#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# AOIProject — pack a Conan cache snapshot for offline transfer
#
#   bash scripts/package-cache.sh                 # Debug   (AOI_CACHE_CONFIG)
#   AOI_CACHE_CONFIG=release bash scripts/package-cache.sh
#
# The corporate JFrog CE instance is not reachable from this machine, so the
# binaries are built locally and shipped as a cache snapshot instead.  On a
# machine that can reach the remote:
#
#   conan cache restore conan-cache-<config>.tgz
#   conan upload --list=pkglist-<config>.json -r <remote> -c
#
# Artefacts written into out/:
#   conan/<config>/graph-<config>.json   dependency graph of that configuration
#   conan/<config>/pkglist-<config>.json exact recipe+package revisions to ship
#   conan-cache-<config>.tgz             the cache snapshot (the file to carry)
#
# One snapshot per configuration, on purpose.  A bundle holds the binaries of
# the build type it was resolved with; Debug and Release are different package
# ids under the same recipe revisions, so a Release build cannot use the Debug
# archive.  The private vtk/9.5.0@aoi/stable exists in no remote, which is why
# the Release snapshot matters too -- see docs/build.md, "The bundle covers one
# build type".
# ---------------------------------------------------------------------------

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# shellcheck source=scripts/_env.sh
. "$SCRIPT_DIR/_env.sh"

# Conan rejects the shell's "/e/..." form; use "E:/..." for every argument.
ROOT="$(AOI_WINPATH "$ROOT")"

CONFIG="${AOI_CACHE_CONFIG:-debug}"
case "$CONFIG" in
  debug)   BUILD_TYPE=Debug ;;
  release) BUILD_TYPE=Release ;;
  *) printf 'ERROR: AOI_CACHE_CONFIG must be "debug" or "release" (got "%s").\n' "$CONFIG" >&2
     exit 2 ;;
esac

PROFILE="$ROOT/conan/profiles/windows-msvc-v143-x64"
LOCKFILE="$ROOT/conan.lock"
OUT="$ROOT/out"
CONAN_OUT="$OUT/conan/$CONFIG"
GRAPH_JSON="$CONAN_OUT/graph-$CONFIG.json"
PKG_JSON="$CONAN_OUT/pkglist-$CONFIG.json"
ARCHIVE="$OUT/conan-cache-$CONFIG.tgz"

# The graph that gets packaged MUST be resolved with the same option the deps
# stage used, or the archive ships the wrong closure and the receiving machine
# still cannot configure.
#
# There is exactly one conditional dependency to get right: with_shrimp adds the
# private vtk/9.5.0@aoi/stable package.  Qt is NOT part of this decision --
# conanfile.py requires qt/6.8.3 unconditionally because TaskControl is not
# headless, so every resolvable graph already contains it.
#
# Shrimp is ON by default (AOI_BUILD_SHRIMP), so the Qt-only graph is no longer
# the closure the build needs: without VTK in the bundle the receiving machine
# has to run `conan create conan/recipes/vtk` itself -- a ~26 minute compile that
# also needs a reachable copy of the 50 MB VTK source archive.
AOI_WITH_SHRIMP="${AOI_WITH_SHRIMP:-1}"

say() { printf '\n=== %s ===\n' "$*"; }

if [ -z "${AOI_CONAN:-}" ] || [ ! -x "${AOI_CONAN:-}" ]; then
  printf 'ERROR: conan not found. Set AOI_CONAN=/path/to/conan(.exe).\n' >&2
  exit 2
fi

mkdir -p "$CONAN_OUT"

set_section="1/5  resolve the $CONFIG dependency graph ($BUILD_TYPE)"
say "$set_section"
# (top level, not a function -- plain assignment, `local` is not allowed here)
# Qt needs no flag here: it is an unconditional require in conanfile.py, so it is
# in every graph this command can produce.
opt_flag=()
if [ "$AOI_WITH_SHRIMP" = "1" ]; then
  opt_flag=(-o "&:with_shrimp=True")
  printf '      shrimp option: ON  (archive carries the private VTK package)\n'
else
  printf '      shrimp option: OFF (archive carries Qt only -- no VTK, so the\n'
  printf '                        receiver still has to `conan create` it)\n'
fi
printf '      build type   : %s\n' "$BUILD_TYPE"

# Two options here are mandatory, and neither is about this machine needing to
# build anything (the deps stage already did).  They are what make the exported
# pkglist describe the closure the *receiving* machine needs.
#
#   --build=missing
#   -c:a tools.graph:skip_binaries=False
#
# `tools.graph:skip_binaries` defaults to True
# (conan/internal/model/conf.py).  With it on, conan marks every node whose
# files are not needed *by this command* as "binary": "Skip" -- and on a warm
# cache that is nearly the whole graph, because the direct dependencies are
# already sitting there.  `conan list --graph` then drops those nodes
# unconditionally (conan/api/model/list.py:
#     if binary in (BINARY_SKIP, BINARY_INVALID, BINARY_MISSING): continue )
# so the pkglist silently collapses.  Measured on this graph:
#     default                  : 11 Cache / 246 Skip           -> 12 binaries
#     -c:a skip_binaries=False : 256 Cache / 1 Download        -> 52 binaries
# Both lists carry the same package names and both produce a multi-hundred-
# megabyte archive, so nothing looks wrong here.  It breaks on a machine with an
# EMPTY cache: there those nodes stop being skippable, need binaries and do not
# find them, because the bundle never contained them.
#
# -c:a is required rather than plain -c: plain -c only reaches the host context
# and leaves all 223 build-context nodes skipped, i.e. it fixes half the graph.
#
# --output-folder must be the long spelling: with a bare "-o" also on the
# command line conan parses "-of=E:/..." as "-o f=E:/..." and then aborts with
# "option 'f' doesn't exist. Possible options are [...]".
"$AOI_CONAN" install "$ROOT" \
    --lockfile="$LOCKFILE" \
    --build=missing \
    -c:a tools.graph:skip_binaries=False \
    "${opt_flag[@]}" \
    -pr:h="$PROFILE" -pr:b="$PROFILE" \
    -s:h build_type="$BUILD_TYPE" \
    --output-folder="$CONAN_OUT" \
    --format=json > "$GRAPH_JSON" || exit 1
printf '      -> %s\n' "$GRAPH_JSON"

say "2/5  list the exact recipe + package revisions"
"$AOI_CONAN" list --graph="$GRAPH_JSON" --format=json > "$PKG_JSON" || exit 1
printf '      -> %s\n' "$PKG_JSON"

# --- completeness gate -----------------------------------------------------
# A pkglist is only useful if it carries *binaries*.  Every delivery bug this
# project has hit produced a pkglist that looked plausible and shipped an
# unusable archive:
#   * resolved back when Qt was optional       -> no qt/6.8.3 at all
#   * resolved without -o "&:with_shrimp=True"    -> no private vtk/9.5.0@aoi/stable
#   * resolved without -c:a skip_binaries=False   -> 12 binaries instead of 52
# so the list is checked before the archive is written, rather than discovered
# at the far end of the transfer.
#
# True when the pkglist holds at least one binary package for "<name>/...".
# Top-level package keys sit at 8 spaces and a binary always contributes a
# "packages": block inside that key's revision.
has_binary() {
  awk -v want="        \"$1/" '
    index($0, want) == 1      { inside = 1; next }
    inside && /^        "/   { inside = 0 }
    inside && /"packages":/   { print "yes"; exit }
  ' "$PKG_JSON" | grep -q yes
}

# Everything the build actually links.  Deliberately not the whole graph:
# alternates such as cmake/4.4.3, b2, nasm and the OpenEXR/JPEG/TIFF chain that
# the prebuilt OpenCV does not need in Debug are not part of the consumer's
# closure, and their absence has been checked to be harmless.
#
# vtk is listed first because it is the one entry nothing else can substitute
# for: it is a *private* package (@aoi/stable), so it exists in no remote and on
# no other machine.  Leaving it out of the pkglist does not fail here -- it fails
# as a 26-minute `conan create` on the receiving side.
MUST_HAVE="vtk qt pcre2 zlib opencv pcl catch2 boost fmt spdlog openssl sqlite3
           freetype libpng harfbuzz glib libpq eigen taskflow double-conversion
           brotli md4c lz4 bzip2 libiconv libffi"

say "3/5  verify the package list carries binaries"
printf '      binaries in pkglist : %s\n' \
       "$(grep -cE '^ {24}"[0-9a-f]{40}": \{$' "$PKG_JSON" || true)"
missing=""
for name in $MUST_HAVE; do
  has_binary "$name" || missing="$missing $name"
done
if [ -n "$missing" ]; then
  printf '\nERROR: no binary in the package list for:%s\n' "$missing" >&2
  printf '       Nothing was written.  Three causes, in order of likelihood:\n' >&2
  printf '         1. the requested build type was never installed for this\n' >&2
  printf '            configuration -- run the deps stage for it first:\n' >&2
  printf '              conan install . --lockfile=conan.lock --build=missing \\\n' >&2
  printf '                -o "&:with_shrimp=True" -s:h build_type=%s \\\n' "$BUILD_TYPE" >&2
  printf '                --output-folder=out/conan/%s\n' "$CONFIG" >&2
  printf '         2. the graph was resolved without the option the deps stage\n' >&2
  printf '            used (-o "&:with_shrimp=True"), so the conditional package\n' >&2
  printf '            is not in the graph at all;\n' >&2
  printf '         3. "-c:a tools.graph:skip_binaries=False" was missing: conan then\n' >&2
  printf '            marks every node it does not need for this command as "Skip",\n' >&2
  printf '            and `conan list --graph` drops Skip nodes, so the archive would\n' >&2
  printf '            ship recipes without the binaries an empty-cache machine needs.\n' >&2
  exit 1
fi
printf '      all %s required packages have a binary\n' "$(printf '%s' "$MUST_HAVE" | wc -w)"

say "4/5  normalize unreadable NTFS reparse points in the cache"
# msys2 ships bin/msys64/etc/mtab as an *LX symlink* reparse point
# (IO_REPARSE_TAG_LX_SYMLINK = 0xA000001D).  NTFS has no handler for that tag,
# so no native Windows process can open the file at all -- whether the link
# target exists is irrelevant (WinError 1920 / ERROR_CANT_ACCESS_FILE), and
# os.path.islink() is False for it.  Python's os.lstat() therefore reports it as
# a REGULAR file, tarfile goes straight to bltn_open(), and `conan cache save`
# dies part-way through writing the archive:
#
#     RuntimeError: lost gzip_file
#     OSError: [Errno 22] Invalid argument: '...msys2...\bin\msys64\etc\mtab'
#
# Conan's manifest code (conan/internal/model/manifest.py:
#     md5(os.readlink(f)) if os.path.islink(f) else md5sum(f))
# fails identically, which is why `conan cache check-integrity "msys2/*"`
# crashes instead of reporting.
#
# A whole-store scan of this cache (881,573 files) found exactly one such file.
# The helper replaces it with a plain file holding the link target and re-hashes
# that path in the package's conanmanifest.txt, so the package stays
# self-consistent.  It is idempotent -- run it twice and the second pass finds
# nothing.  Only /etc/mtab of the msys2 *build tool* is affected; it is legacy
# mount bookkeeping and is never read while compiling.
if [ -z "${AOI_PYTHON:-}" ]; then
  printf 'ERROR: no Python interpreter found for the reparse-point pass.\n' >&2
  printf '       Point AOI_PYTHON at the interpreter that backs conan.exe.\n' >&2
  exit 2
fi
"$AOI_PYTHON" "$SCRIPT_DIR/normalize-cache-reparse.py" \
    "$(AOI_WINPATH "$AOI_CONAN_HOME/p")" || exit 1

# Cheap proof that the cache is consistent before spending minutes compressing a
# multi-gigabyte archive: this recomputes every manifest (the very code path that
# used to raise) and exits non-zero if anything mismatches.  Its text formatter is
# empty, so silence means success.
"$AOI_CONAN" cache check-integrity "msys2/*" >/dev/null || {
  printf '\nERROR: the msys2 package is still inconsistent, refusing to package.\n' >&2
  printf '       Re-run to see the mismatch:\n' >&2
  printf '           conan cache check-integrity "msys2/*"\n' >&2
  exit 1
}
printf '      msys2 package: integrity ok\n'

say "5/5  save the cache snapshot"
# An interrupted `conan cache save` leaves a zero-byte "<archive>.dirty" marker
# beside the archive, and every later run then aborts with
#     ERROR: Folder '<archive>' is already dirty
# even though nothing is actually wrong -- the archive is rewritten from
# scratch.  Clear that marker, but only for the archive this script owns.
if [ -e "$ARCHIVE.dirty" ]; then
  rm -f "$ARCHIVE.dirty"
  printf '      cleared stale marker: %s.dirty\n' "$ARCHIVE"
fi

"$AOI_CONAN" cache save --list="$PKG_JSON" --file="$ARCHIVE" || exit 1
printf '      -> %s\n' "$ARCHIVE"

# `cache save` writes through a "<archive>.dirty" marker that it removes on
# success -- but a crash mid-write leaves a *truncated* archive that looks
# entirely plausible from the outside.  That is exactly how the msys2/etc/mtab
# failure presented: 780 MB of what should have been a 2.7 GB file, marker
# gone, no error visible to anyone who only checks that the file exists.  Walk
# the whole deflate stream and its CRC before calling this a success.
printf '      verifying the gzip stream (~30 s)\n'
gzip -t "$ARCHIVE" || {
  printf '\nERROR: %s is corrupt or truncated -- do not ship it.\n' "$ARCHIVE" >&2
  printf '       Re-run this stage.  An interrupted `cache save` leaves a short\n' >&2
  printf '       archive that still looks plausible from the outside.\n' >&2
  exit 1
}

printf '\nConfiguration    : %s (%s)\n' "$CONFIG" "$BUILD_TYPE"
printf 'Packages in list : %s\n' "$(grep -cE '^ {8}"' "$PKG_JSON" || true)"
printf 'Binaries in list : %s\n' "$(grep -cE '^ {24}"[0-9a-f]{40}": \{$' "$PKG_JSON" || true)"
printf 'Archive size     : %s\n' "$(du -h "$ARCHIVE" | cut -f1)"
