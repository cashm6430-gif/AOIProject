#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# AOIProject — pack the private packages alone, for the machine that uploads
#
#   bash scripts/package-private.sh          # -> out/conan-cache-private.tgz
#
# scripts/package-cache.sh exists so that a machine with NO remote can build;
# its snapshots carry the whole closure and are 2+ GB each.  This script exists
# for the opposite machine -- the one that can reach the corporate remote and
# only has to push what no remote can serve.  That is the private
# vtk/9.5.0@aoi/stable and nothing else, so the archive is one recipe instead of
# 47 and small enough to hand-carry over a remote-desktop file transfer.
#
# Measured on this cache (2026-09-17):
#   out/conan-cache-debug.tgz     2,852,120,829 B   47 recipes / 53 binaries
#   out/conan-cache-release.tgz   2,613,791,722 B   47 recipes / 47 binaries
#   out/conan-cache-private.tgz     194,118,406 B    1 recipe  /  2 binaries
#
# Both build types in ONE archive, on purpose.  Debug and Release are two
# package ids under the same recipe revision, so a single `conan cache save -l`
# carries both -- there is no reason to make the receiving machine restore two
# files in order to upload one package.  `pkglist-refs.py merge --private-only`
# unions the two tracked manifests into exactly that list; the union is what
# makes this correct rather than convenient, because a plain concatenation would
# let the Release entry's package id replace the Debug one.
#
# On the machine that can reach the remote:
#
#   conan cache restore out/conan-cache-private.tgz
#   bash scripts/upload-cache.sh -r <remote> --only-private
#
# Artefacts written into out/:
#   pkglist-private.json        the union this script packs
#   conan-cache-private.tgz     the archive to carry
#
# Verify the archive before carrying it, and again after it arrives:
#   bash scripts/check-cache.sh --archive out/conan-cache-private.tgz
# ---------------------------------------------------------------------------

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# shellcheck source=scripts/_env.sh
. "$SCRIPT_DIR/_env.sh"

# Conan rejects the shell's "/e/..." form; use "E:/..." for every argument.
ROOT="$(AOI_WINPATH "$ROOT")"

OUT="$ROOT/out"
PKG_JSON="$OUT/pkglist-private.json"
ARCHIVE="$OUT/conan-cache-private.tgz"
LISTS_DIR="$SCRIPT_DIR/../conan/lists"

say() { printf '\n=== %s ===\n' "$*"; }

if [ -z "${AOI_CONAN:-}" ] || [ ! -x "${AOI_CONAN:-}" ]; then
  printf 'ERROR: conan not found. Set AOI_CONAN=/path/to/conan(.exe).\n' >&2
  exit 2
fi
if [ -z "${AOI_PYTHON:-}" ]; then
  printf 'ERROR: no Python interpreter found for scripts/pkglist-refs.py.\n' >&2
  printf '       Point AOI_PYTHON at the interpreter that backs conan.exe.\n' >&2
  exit 2
fi

for f in "$LISTS_DIR/pkglist-debug.json" "$LISTS_DIR/pkglist-release.json"; do
  if [ ! -f "$f" ]; then
    printf 'ERROR: %s is missing.\n' "$f" >&2
    printf '       It is tracked in git and written by scripts/package-cache.sh;\n' >&2
    printf '       without it there is nothing to take the private packages from.\n' >&2
    exit 2
  fi
done

mkdir -p "$OUT" || exit 1

say "1/4  union the private packages of both build types"
# The traced manifests are the source, not the live graph: they are what
# scripts/upload-cache.sh reads, and a bundle built from anything else would
# describe a set of packages the upload step does not expect.  If they are stale,
# `git status` says so.
"$AOI_PYTHON" "$SCRIPT_DIR/pkglist-refs.py" merge "$PKG_JSON" \
    "$LISTS_DIR/pkglist-debug.json" "$LISTS_DIR/pkglist-release.json" \
    --private-only || exit 1

# --- completeness gate -----------------------------------------------------
# Same reason as the one in package-cache.sh: a private bundle that is missing
# the Debug binary, or that pins a revision conan.lock no longer resolves, still
# compresses to a plausible file and still restores without an error.  It fails
# at the far end of a hand-carried transfer, as a 26-minute VTK rebuild.
say "2/4  check the union against conan.lock"
"$AOI_PYTHON" - "$PKG_JSON" "$ROOT/conan.lock" <<'PYEOF' || exit 1
import json, re, sys

pkg_path, lock_path = sys.argv[1], sys.argv[2]
doc = json.load(open(pkg_path, encoding="utf-8"))
lock = None
try:
    found = re.search(r"vtk/9\.5\.0@aoi/stable#([0-9a-f]{32})",
                      open(lock_path, encoding="utf-8").read())
    lock = found.group(1) if found else None
except OSError:
    pass

recipes = [ref for cache in doc.values() for ref in cache]
if recipes != ["vtk/9.5.0@aoi/stable"]:
    print("ERROR: the union is not exactly the private vtk package: {}".format(recipes),
          file=sys.stderr)
    sys.exit(1)

entry = next(iter(doc.values()))[recipes[0]]
revs = entry.get("revisions") or {}
if len(revs) != 1:
    print("ERROR: expected one recipe revision, found {}".format(list(revs)), file=sys.stderr)
    sys.exit(1)

rrev, body = next(iter(revs.items()))
ids = list(body.get("packages") or {})
if len(ids) != 2:
    print("ERROR: expected 2 binaries (one per build type), found {}".format(len(ids)),
          file=sys.stderr)
    print("       A manifest holding one of them usually means the merge overwrote\n"
          "       rather than unioned -- check the `packages` update in\n"
          "       pkglist-refs.py merge().", file=sys.stderr)
    sys.exit(1)
if lock and rrev != lock:
    print("ERROR: the manifests describe vtk#{} but conan.lock pins #{}".format(rrev[:12], lock[:12]),
          file=sys.stderr)
    print("       Re-pack the full bundles first (scripts/package-cache.sh); they\n"
          "       are what carry the correct list.", file=sys.stderr)
    sys.exit(1)

types = []
for pid in ids:
    info = (body["packages"][pid] or {}).get("info") or {}
    types.append((info.get("settings") or {}).get("build_type") or "?")
print("      vtk/9.5.0@aoi/stable#{}".format(rrev[:12]))
for pid, bt in zip(ids, types):
    print("        {}  build_type={}".format(pid[:16] + "...", bt))
print("      matches conan.lock" if lock else "      no lockfile to compare against")
PYEOF

say "3/4  save the private snapshot"
# Same stale-marker trap as package-cache.sh: an interrupted `cache save` leaves
# "<archive>.dirty" behind and every later run refuses to start.
if [ -e "$ARCHIVE.dirty" ]; then
  rm -f "$ARCHIVE.dirty"
  printf '      cleared stale marker: %s.dirty\n' "$ARCHIVE"
fi
"$AOI_CONAN" cache save --list="$PKG_JSON" --file="$ARCHIVE" || exit 1
printf '      -> %s\n' "$ARCHIVE"

say "4/4  verify the gzip stream"
# A truncated archive is the failure that costs a whole round of hand-carrying:
# this one is small enough that it will be sent whole, and a cut-off file
# restores without an error on the far side.
gzip -t "$ARCHIVE" || {
  printf '\nERROR: %s is corrupt or truncated -- do not carry it.\n' "$ARCHIVE" >&2
  exit 1
}
printf '      stream ok\n'

printf '\nArchive size     : %s (%s bytes)\n' \
       "$(du -h "$ARCHIVE" | cut -f1)" "$(stat -c %s "$ARCHIVE")"
printf 'Recipe / binaries: %s / %s\n' \
       "$(grep -cE '^ {8}"' "$PKG_JSON" || true)" \
       "$(grep -cE '^ {24}"[0-9a-f]{40}": \{$' "$PKG_JSON" || true)"
# Printed because this is the file that gets copied by hand: a checksum taken
# here is the only way to tell a complete transfer from a stopped one before
# spending time on a restore.
printf 'md5              : %s\n' "$(md5sum "$ARCHIVE" | cut -d' ' -f1)"
printf '\nOn the machine that can reach the remote:\n'
printf '  conan cache restore out/conan-cache-private.tgz\n'
printf '  bash scripts/upload-cache.sh -r <remote> --only-private\n'
