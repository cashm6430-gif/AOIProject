#!/usr/bin/env python3
"""Read the package lists that `conan list --graph` writes.

Four shapes of the same JSON are needed by the scripts around it.  A pkglist and
the output of `conan list "*#*:*" --format=json` have the same structure --
{"Local Cache": {ref: {"revisions": {rrev: {"packages": {pkgid: ...}}}}} -- so
one can be subtracted from the other:

  rows    one line per binary, "ref#recipe_revision:package_id", so a shell loop
          can ask a remote whether each one arrived.  `conan list` has no
          --list option (only `conan upload` has) and takes a single pattern, so
          the only way to check a whole manifest is one query per entry.

  subset  a pkglist containing only the refs that exist in no remote -- i.e. the
          ones carrying a user/channel such as vtk/9.5.0@aoi/stable.  A JFrog
          repository that proxies ConanCenter is read-only, so uploading the
          third-party part of the graph to it is either refused or unwanted; the
          private package is the only one somebody must push.  The output has the
          same structure as the input, so `conan upload --list=` accepts it.

  missing the rows of a pkglist that the given cache does not hold.  This is
          how scripts/check-cache.sh answers "did the restore really deliver the
          archive?"; it exits 1 when anything is absent, so the caller can use
          it as a test rather than parse its output.

  revs    "recipe_revision<TAB>binary_count" for one reference, so a caller can
          show which revisions of a package the cache carries -- the difference
          between "the locked revision is not here" and "only a stale revision
          of this recipe is here", which need different fixes.

Usage:
    pkglist-refs.py rows    <pkglist.json> [--private-only]
    pkglist-refs.py subset  <pkglist.json> <out.json> [--private-only]
    pkglist-refs.py missing <pkglist.json> <cache.json>
    pkglist-refs.py revs    <cache.json> <ref>
"""

import json
import sys


# Windows Python translates "\n" to "\r\n" on the way out, which is invisible in
# a terminal and poisonous to the caller: a row arrives in bash as
# "ref#rev:pkgid\r", and the trailing carriage return then travels into a
# `grep -q "$pkgid"`.  Git for Windows' grep cannot match a pattern containing
# CR, so every lookup fails and a remote that holds everything reads as empty.
# That is exactly how scripts/upload-cache.sh reported "3 of 3 entries did not
# come back from the remote" for an upload that had in fact succeeded.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(newline="\n")


def load(path):
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def is_private(ref):
    # Conan 1 style reference: name/version@user/channel.  This project's only
    # private package is vtk/9.5.0@aoi/stable; everything else in the graph
    # comes from ConanCenter and has no user/channel.
    return "@" in ref


def keys(doc):
    # (ref, recipe_revision, package_id), the identity Conan keys a binary by.
    # The recipe revision is part of it: a rebuilt recipe leaves its previous
    # binary in the cache under a revision nothing resolves any more, and a
    # revision-blind test would count that binary as the one it needs.
    for cache in doc.values():
        for ref, info in cache.items():
            for rrev, rdata in (info.get("revisions") or {}).items():
                for pkgid in (rdata.get("packages") or {}):
                    yield (ref, rrev, pkgid)


def rows(doc, private_only):
    for ref, rrev, pkgid in keys(doc):
        if private_only and not is_private(ref):
            continue
        yield f"{ref}#{rrev}:{pkgid}"


def missing(doc, cache_doc):
    have = set(keys(cache_doc))
    for key in keys(doc):
        if key not in have:
            yield key


def revs(doc, ref):
    for cache in doc.values():
        info = cache.get(ref)
        if not info:
            continue
        for rrev, rdata in (info.get("revisions") or {}).items():
            yield rrev, len(rdata.get("packages") or {})


def subset(doc, private_only):
    out = {}
    for cache_name, cache in doc.items():
        kept = {r: v for r, v in cache.items() if not private_only or is_private(r)}
        if kept:
            out[cache_name] = kept
    return out


def main(argv):
    if len(argv) < 3:
        print(__doc__.strip(), file=sys.stderr)
        return 2

    mode, src = argv[1], argv[2]
    rest = argv[3:]
    private_only = "--private-only" in rest
    dests = [a for a in rest if not a.startswith("--")]

    doc = load(src)

    if mode == "rows":
        for line in rows(doc, private_only):
            print(line)
        return 0

    if mode == "subset":
        if not dests:
            print("ERROR: subset needs an output path", file=sys.stderr)
            return 2
        if not private_only:
            print("ERROR: subset without --private-only would copy the file", file=sys.stderr)
            return 2
        # newline="\n" for the same reason as the stdout fix above: the file is
        # handed to `conan upload --list=`, and keeping it byte-stable between
        # this machine (CRLF by default) and a Linux CI box removes one more
        # way for two runs of the same command to disagree.
        with open(dests[0], "w", encoding="utf-8", newline="\n") as fh:
            json.dump(subset(doc, private_only), fh, indent=4)
        for line in rows(subset(doc, private_only), private_only):
            print(line)
        return 0

    if mode == "missing":
        if not dests:
            print("ERROR: missing needs the cache json (conan list \"*:*\" --format=json)",
                  file=sys.stderr)
            return 2
        absent = list(missing(doc, load(dests[0])))
        for ref, rrev, pkgid in absent:
            print(f"{ref}#{rrev}:{pkgid}")
        # Exit 1 so `if pkglist-refs.py missing ...` reads as "the cache is
        # incomplete" without the caller having to count lines.
        return 1 if absent else 0

    if mode == "revs":
        if not dests:
            print("ERROR: revs needs the reference to look up", file=sys.stderr)
            return 2
        for rrev, count in revs(doc, dests[0]):
            print(f"{rrev}\t{count}")
        return 0

    print(f"ERROR: unknown mode '{mode}'", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
