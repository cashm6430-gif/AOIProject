#!/usr/bin/env python3
"""Restore encrypted Git-tracked text files from the current Git revision.

Some endpoint encryption products transform files after ``git pull`` writes
them to the worktree.  Git still has the original text in its object database,
but compilers and Conan then receive ciphertext.  This helper obtains the
tracked blob from Git and writes it back through the local plaintext override.

By default only files that no longer look like UTF-8 text are restored, so
normal local source edits are preserved.  ``--all`` intentionally replaces all
supported tracked text files with the selected revision and should only be used
with a clean worktree.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


TEXT_SUFFIXES = {
    ".bat", ".cc", ".cmake", ".cmd", ".cpp", ".cxx", ".h", ".hh", ".hpp",
    ".hxx", ".inl", ".ini", ".json", ".md", ".natvis", ".ps1", ".py",
    ".qrc", ".qss", ".sh", ".toml", ".txt", ".ui", ".yaml", ".yml",
}


def git(repo: Path, *args: str, text: bool = False) -> bytes | str:
    return subprocess.check_output(["git", "-C", str(repo), *args], text=text)


def looks_like_utf8_text(data: bytes) -> bool:
    """Reject encrypted/binary data while accepting ordinary UTF-8 source."""
    try:
        decoded = data.decode("utf-8-sig")
    except UnicodeDecodeError:
        return False
    if "\x00" in decoded:
        return False
    controls = sum(ord(char) < 32 and char not in "\t\n\r" for char in decoded)
    return controls <= max(2, len(decoded) // 100)


def tracked_paths(repo: Path) -> list[Path]:
    output = git(repo, "ls-files", "-z")
    assert isinstance(output, bytes)
    return [Path(item.decode("utf-8")) for item in output.split(b"\0") if item]


def worktree_blob(repo: Path, ref: str, relative_path: Path) -> bytes:
    """Read a tracked blob after Git applies this worktree's filters.

    ``git show`` returns the repository blob verbatim, which is LF-normalized
    for ordinary text files.  Writing those bytes on a Windows checkout with
    ``core.autocrlf`` enabled repairs the source but leaves a misleading
    whole-file diff.  ``cat-file --filters`` returns exactly what Git would
    write to this checkout, including its configured end-of-line conversion.
    """
    blob = git(
        repo,
        "cat-file",
        "--filters",
        f"--path={relative_path.as_posix()}",
        f"{ref}:{relative_path.as_posix()}",
    )
    assert isinstance(blob, bytes)
    return blob


def same_text_with_normalized_eol(left: bytes, right: bytes) -> bool:
    """Compare text while treating CRLF and LF as the same line ending."""
    return left.replace(b"\r\n", b"\n") == right.replace(b"\r\n", b"\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--ref", default="HEAD", help="Git revision to restore from (default: HEAD)")
    parser.add_argument("--all", action="store_true", help="replace every supported tracked text file")
    parser.add_argument(
        "--normalize-eol",
        action="store_true",
        help="rewrite files that differ from Git only by LF/CRLF line endings",
    )
    parser.add_argument("--check", action="store_true", help="report files without writing them")
    args = parser.parse_args()

    repo = Path(git(args.repo, "rev-parse", "--show-toplevel", text=True).strip())
    repaired: list[Path] = []
    skipped = 0
    for relative_path in tracked_paths(repo):
        if relative_path.suffix.lower() not in TEXT_SUFFIXES:
            continue
        destination = repo / relative_path
        if not destination.is_file():
            continue
        current = destination.read_bytes()
        current_is_text = looks_like_utf8_text(current)
        blob = None
        if not args.all and current_is_text:
            if not args.normalize_eol:
                skipped += 1
                continue
            candidate = worktree_blob(repo, args.ref, relative_path)
            if current == candidate or not same_text_with_normalized_eol(current, candidate):
                skipped += 1
                continue
            blob = candidate
        if blob is None:
            blob = worktree_blob(repo, args.ref, relative_path)
        if not looks_like_utf8_text(blob):
            print(f"skip non-text Git blob: {relative_path}", file=sys.stderr)
            continue
        repaired.append(relative_path)
        print(("would restore" if args.check else "restored") + f": {relative_path}")
        if not args.check:
            destination.write_bytes(blob)

    print(f"{len(repaired)} file(s) {'need' if args.check else ''} restoration; {skipped} normal text file(s) untouched.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
