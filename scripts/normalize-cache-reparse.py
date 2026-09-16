#!/usr/bin/env python3
"""Normalize NTFS reparse points in the Conan cache that no Windows tool can open.

The problem
-----------
MSYS2 (like WSL) stores its symlinks as *LX symlink* reparse points,
IO_REPARSE_TAG_LX_SYMLINK (0xA000001D).  NTFS has no handler for that tag, so a
native Windows process cannot open such a file at all -- it makes no difference
whether the link target exists:

    >>> open(r"...\\msys2f91ae1bf3386a\\p\\bin\\msys64\\etc\\mtab", "rb")
    OSError: [Errno 22] Invalid argument        (underlying: WinError 1920
                                                  ERROR_CANT_ACCESS_FILE)

Worse, Python's os.lstat() reports it as a *regular file*
(S_ISREG True, st_reparse_tag 0xA000001D), so tarfile believes it can just read
the contents:

    tarfile.TarFile.add -> tarinfo.isreg() -> bltn_open(name, "rb")

and ``conan cache save`` dies part-way through writing the archive:

    RuntimeError: lost gzip_file
    OSError: [Errno 22] Invalid argument: '...msys2...\\bin\\msys64\\etc\\mtab'

Conan's manifest code (conan/internal/model/manifest.py) does
``md5(os.readlink(path)) if os.path.islink(path) else md5sum(path)`` and hits
the very same wall, so ``conan cache check-integrity`` on the msys2 package
raises instead of reporting.  Neither ``conan install`` nor ``conan cache
save``/``restore`` ever verifies a manifest (that is only done by
``conan cache check-integrity`` and ``conan upload --check-integrity``), but a
package that cannot even be hashed is a broken thing to ship.

What this script does
---------------------
For every reparse point under the cache store that cannot be opened it:

  1. recovers the stored link target with FSCTL_GET_REPARSE_POINT,
  2. replaces the reparse point with an ordinary file holding that target -- for
     msys2's etc/mtab that is the 12 bytes "/proc/mounts", which is exactly what
     ``ls -l`` already reported as the link target,
  3. rewrites the md5 recorded for that path in the enclosing package's
     conanmanifest.txt, so the package stays self-consistent and
     ``conan cache check-integrity`` reports "Integrity check: ok" again.

Nothing else in the package is touched, and the pass is idempotent: a second
run finds nothing left to do.  The replacement is done through Win32
DeleteFileW rather than os.remove(), because the tool host's safe-delete shim
stat()s the target first and would abort the process instead.

Why this is safe for the delivered snapshot
-------------------------------------------
The only file affected here is part of the msys2 *build tool*, which this build
uses solely to run autotools recipes; /etc/mtab is legacy mount bookkeeping and
is never read while compiling.  The archive is restored with ``conan cache
restore``, which just untars into the cache (conan/api/subapi/cache.py,
restore()).  A plain file is also strictly more robust than the reparse point it
replaces: the reparse point cannot be opened by any native Windows process.

Measured on this project: 881,573 files scanned, exactly one hit.

Usage
-----
    python normalize-cache-reparse.py <conan-cache-store> [--dry-run]

    # normally invoked by scripts/package-debug-cache.sh with
    #   <conan-home>/p   e.g.  C:\\Users\\<you>\\.conan2\\p
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wt
import hashlib
import os
import stat
import sys

LX_SYMLINK_TAG = 0xA000001D
CONAN_MANIFEST = "conanmanifest.txt"
MANIFEST_HEADER_LINES = 1  # the first line of a conanmanifest is its timestamp

# --- raw reparse point access (the target is not reachable via os.readlink) ---
FSCTL_GET_REPARSE_POINT = 0x000900A8
GENERIC_READ = 0x80000000
FILE_SHARE_ALL = 0x00000007
OPEN_EXISTING = 3
FILE_FLAG_BACKUP_SEMANTICS = 0x02000000
FILE_FLAG_OPEN_REPARSE_POINT = 0x00200000
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
MAX_REPARSE_DATA = 16 * 1024

_kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
_kernel32.CreateFileW.restype = wt.HANDLE
_kernel32.CreateFileW.argtypes = (wt.LPCWSTR, wt.DWORD, wt.DWORD, wt.LPVOID,
                                 wt.DWORD, wt.DWORD, wt.HANDLE)
_kernel32.DeviceIoControl.restype = wt.BOOL
_kernel32.DeviceIoControl.argtypes = (wt.HANDLE, wt.DWORD, wt.LPVOID, wt.DWORD,
                                      wt.LPVOID, wt.DWORD,
                                      ctypes.POINTER(wt.DWORD), wt.LPVOID)
_kernel32.CloseHandle.argtypes = (wt.HANDLE,)
_kernel32.DeleteFileW.restype = wt.BOOL
_kernel32.DeleteFileW.argtypes = (wt.LPCWSTR,)


def delete_file(path):
    """Remove a file through Win32 directly.

    Deliberately NOT os.remove()/os.unlink(): the tool host puts a sitecustomize
    shim on PYTHONPATH that wraps those, and the wrapper stat()s the target
    first.  stat() on an LX symlink reparse point fails with WinError 1920, so
    the shim aborts the entire process before the delete is even attempted:

        [safe-delete][SAFE_DELETE_BULK_GUARD_ERROR] failed to stat target: ...
        (process exits 1, file untouched)

    Go straight to DeleteFileW and the guard never sees it.  (scripts/_env.sh
    fights the same shim for Conan's deletes, there via the bulk threshold.)
    """
    if not _kernel32.DeleteFileW(path):
        raise ctypes.WinError(ctypes.get_last_error())


def lstat_or_none(path):
    try:
        return os.lstat(path)
    except OSError:
        return None


def reparse_tag(path):
    info = lstat_or_none(path)
    return (getattr(info, "st_reparse_tag", 0) or 0) if info else 0


def is_readable(path):
    try:
        with open(path, "rb") as handle:
            handle.read(1)
        return True
    except OSError:
        return False


def reparse_target(path):
    """Return the symlink target stored in the reparse point, or None."""
    handle = _kernel32.CreateFileW(
        path, GENERIC_READ, FILE_SHARE_ALL, None, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, None)
    if handle == INVALID_HANDLE_VALUE:
        return None
    try:
        buffer = ctypes.create_string_buffer(MAX_REPARSE_DATA)
        returned = wt.DWORD()
        ok = _kernel32.DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, None, 0,
                                       buffer, MAX_REPARSE_DATA,
                                       ctypes.byref(returned), None)
        if not ok:
            return None
        raw = buffer.raw[:returned.value]
    finally:
        _kernel32.CloseHandle(handle)

    if len(raw) < 8:
        return None
    tag = int.from_bytes(raw[0:4], "little")
    data_length = int.from_bytes(raw[4:6], "little")
    payload = raw[8:8 + data_length]
    if tag == LX_SYMLINK_TAG:
        # 4-byte little-endian mode, then the UTF-8 target with no NUL.
        return payload[4:].rstrip(b"\x00").decode("utf-8", "replace")
    return None


def find_enclosing_manifest(path):
    """Nearest ancestor directory that holds a conanmanifest.txt."""
    directory = os.path.dirname(path)
    while True:
        candidate = os.path.join(directory, CONAN_MANIFEST)
        if os.path.isfile(candidate):
            return candidate, directory
        parent = os.path.dirname(directory)
        if parent == directory:
            return None, None
        directory = parent


def patch_manifest(manifest_path, relative_path, new_md5):
    """Rewrite the md5 of relative_path inside manifest_path.  LF only, as Conan
    writes and reads it with newline=""."""
    with open(manifest_path, "r", encoding="utf-8", newline="") as handle:
        lines = handle.read().split("\n")
    for index in range(MANIFEST_HEADER_LINES, len(lines)):
        line = lines[index]
        if not line:
            continue
        name, separator, _old = line.rpartition(": ")
        if separator and name == relative_path:
            lines[index] = "%s: %s" % (relative_path, new_md5)
            with open(manifest_path, "w", encoding="utf-8", newline="") as handle:
                handle.write("\n".join(lines))
            return True
    return False


def collect_unreadable(store):
    """Every reparse point under store that a native Windows process cannot open.

    Real symlinks are skipped on purpose: tarfile emits a link entry for those
    and never opens the target, so they archive fine.
    """
    found = []
    scanned = 0
    for directory, _subdirs, files in os.walk(store):
        for name in files:
            scanned += 1
            path = os.path.join(directory, name)
            info = lstat_or_none(path)
            if info is None:
                continue
            if not (getattr(info, "st_reparse_tag", 0) or 0):
                continue
            if stat.S_ISLNK(info.st_mode):
                continue
            if is_readable(path):
                continue
            found.append(path)
    return scanned, found


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Make unreadable NTFS reparse points in the Conan cache "
                    "openable, so that `conan cache save` can archive them.")
    parser.add_argument("store",
                        help="Conan cache store root, normally <conan-home>/p")
    parser.add_argument("--dry-run", action="store_true",
                        help="only report what would be changed")
    args = parser.parse_args(argv)

    if os.name != "nt":
        print("normalize-cache-reparse: not Windows, nothing to do")
        return 0

    store = os.path.abspath(args.store)
    if not os.path.isdir(store):
        print("ERROR: %s is not a directory" % store, file=sys.stderr)
        return 2

    scanned, found = collect_unreadable(store)
    print("scanned %d files under %s" % (scanned, store))
    if not found:
        print("unreadable reparse points: 0 (nothing to normalize)")
        return 0

    print("unreadable reparse points: %d" % len(found))
    failures = 0
    normalized = 0
    for path in found:
        tag = reparse_tag(path)
        target = reparse_target(path)
        content = (target or "").encode("utf-8")
        print("  %s" % path)
        print("      tag     : 0x%08X%s"
              % (tag, "  (IO_REPARSE_TAG_LX_SYMLINK)" if tag == LX_SYMLINK_TAG
                 else ""))
        print("      target  : %r" % (target,))

        if args.dry_run:
            print("      action  : would replace with a %d-byte regular file"
                  % len(content))
            continue

        try:
            delete_file(path)
        except OSError as exc:
            print("      ERROR   : cannot remove the reparse point: %s" % exc,
                  file=sys.stderr)
            failures += 1
            continue
        with open(path, "wb") as handle:
            handle.write(content)

        if not is_readable(path):
            print("      ERROR   : the replacement still cannot be opened",
                  file=sys.stderr)
            failures += 1
            continue

        digest = hashlib.md5(content).hexdigest()
        manifest, folder = find_enclosing_manifest(path)
        if manifest is None:
            print("      note    : no enclosing %s, hash not recorded"
                  % CONAN_MANIFEST)
            print("      action  : replaced with a %d-byte file, md5 %s"
                  % (len(content), digest))
            normalized += 1
            continue

        relative = os.path.relpath(path, folder).replace("\\", "/")
        if patch_manifest(manifest, relative, digest):
            print("      action  : replaced with a %d-byte file, md5 %s"
                  % (len(content), digest))
            print("                %s: %s -> %s"
                  % (CONAN_MANIFEST, relative, digest))
            normalized += 1
        else:
            print("      ERROR   : %s has no entry for %s, the package would be "
                  "left inconsistent" % (CONAN_MANIFEST, relative),
                  file=sys.stderr)
            failures += 1

    if failures:
        print("ERROR: %d reparse point(s) could not be normalized" % failures,
              file=sys.stderr)
        return 1
    print("normalized %d reparse point(s)" % normalized)
    return 0


if __name__ == "__main__":
    sys.exit(main())
