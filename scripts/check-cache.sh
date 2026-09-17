#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# AOIProject - is the bundle this machine restored actually in its cache?
#
#   bash scripts/check-cache.sh                     # both configurations
#   bash scripts/check-cache.sh --config=debug
#   bash scripts/check-cache.sh --config=release
#   bash scripts/check-cache.sh --archive out/conan-cache-debug.tgz
#
# Read-only and offline.  It asks `conan list "*#*:*"` what the local cache
# holds and subtracts that from the manifests scripts/package-cache.sh publishes
# (conan/lists/pkglist-<config>.json).  Nothing is restored, built or uploaded,
# no remote is contacted, and the whole thing runs in about a second.
#
# --archive answers the other half of the same question, about the file instead
# of the machine: `conan cache save` writes the work list it was handed into the
# bundle as pkglist.json, so a bundle describes itself.  Reading that back says
# which recipe revisions and which binaries the file carries, and whether it
# arrived whole.  A byte count and a date that came along with the copy are not
# evidence of either -- and the file is read to its end, so a transfer cut short
# is caught here rather than six minutes later inside `conan create`.
#
# Why it exists
# -------------
# `conan cache restore` only ever ADDS to a cache, and it reports nothing about
# what was already there.  Three different mistakes therefore look identical
# from the outside: restoring one archive out of two, restoring into a cache
# that still holds an unrelated state, and a transfer that arrived truncated.
# Each of them leaves a graph that resolves only partially -- and the failure
# then surfaces one step later, inside `conan create`, on whichever dependency
# happens to be reached first.
#
# In this project that dependency is qt.  vtk/9.5.0@aoi/stable requires it, so a
# Debug build whose cache holds only the Release qt stops with a message about
# qt -- while the file that was never restored, or restored incompletely, is a
# different archive altogether.  The question underneath is one line shorter,
# and this prints it.
#
# The two manifests are mutually exclusive by construction: Debug and Release
# are different package ids under the same recipe revisions, so a complete
# Release cache has nothing of Debug in it.  That is what makes the hint below
# possible -- when the requested configuration is incomplete and the other one
# is perfect, the cause is almost always "the wrong archive was restored".
# ---------------------------------------------------------------------------

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_UNIX="$(cd "$SCRIPT_DIR/.." && pwd)"

# shellcheck source=scripts/_env.sh
. "$SCRIPT_DIR/_env.sh"

ROOT="$(AOI_WINPATH "$ROOT_UNIX")"
LISTS_DIR="$ROOT_UNIX/conan/lists"
WORK_DIR="$ROOT_UNIX/out/check-cache"
REF_LIST="$SCRIPT_DIR/pkglist-refs.py"

CONFIG="both"
ARCHIVE=""

usage() {
  cat <<'EOF'
Check the local Conan cache against the manifests in conan/lists/, or read the
manifest an archive carries inside itself.

  bash scripts/check-cache.sh [--config debug|release|both]
  bash scripts/check-cache.sh --archive out/conan-cache-debug.tgz

      --config <c>    which configuration to report on     (default: both)
      --archive <f>   read a .tgz's own pkglist.json instead of the local cache
  -h, --help          this text

The cache is always compared against BOTH manifests, so a failure can say
whether the other configuration's packages are the ones that are present.
--archive ignores --config: the archive itself says which one it holds.
The exit status is 0 only when every requested configuration is complete.
EOF
}

say()  { printf '\n=== %s ===\n' "$*"; }
ok()   { printf '      [ok]   %s\n' "$*"; }
bad()  { printf '      [FAIL] %s\n' "$*" >&2; }
info() { printf '      %s\n' "$*"; }

while [ $# -gt 0 ]; do
  case "$1" in
    --config)    CONFIG="${2:-}"; shift 2 ;;
    --config=*)  CONFIG="${1#*=}"; shift ;;
    --archive)   ARCHIVE="${2:-}"; shift 2 ;;
    --archive=*) ARCHIVE="${1#*=}"; shift ;;
    -h|--help)   usage; exit 0 ;;
    *) printf 'ERROR: unknown argument "%s" (try --help)\n' "$1" >&2; exit 2 ;;
  esac
done

case "$CONFIG" in
  debug|release|both) : ;;
  *) printf 'ERROR: --config must be debug, release or both (got "%s").\n' "$CONFIG" >&2
     exit 2 ;;
esac

# The interpreter behind conan.exe; a helper script on somebody else's machine
# may only have a bare `python`, so fall back to whatever is on PATH.  Archive
# mode needs this and nothing else, so it is resolved before conan is required.
PY="${AOI_PYTHON:-}"
if [ -z "$PY" ] || [ ! -x "$PY" ]; then
  if command -v python >/dev/null 2>&1; then PY="$(command -v python)"
  elif command -v py >/dev/null 2>&1; then PY="$(command -v py)"
  else
    printf 'ERROR: no Python interpreter found for scripts/pkglist-refs.py.\n' >&2
    exit 2
  fi
fi

# ---------------------------------------------------------------------------
# --archive: read the bundle's own manifest instead of the local cache.
#
# `conan cache save` writes the work list it was handed into the archive as
# pkglist.json, so an archive describes itself -- which recipe revisions and
# which binaries it carries.  That answers "is the file I am holding the right
# one?" without trusting its name, its byte count or the date somebody copied
# it, and it is the question that matters when two bundles travel separately
# and only one of them arrives.
#
# The whole stream is read, so a truncated transfer is caught here too.  That
# failure is invisible otherwise: `conan cache restore` extracts whatever is
# intact and exits, leaving a cache that resolves part of a graph -- which then
# surfaces inside `conan create` as a missing dependency of something unrelated.
# ---------------------------------------------------------------------------
if [ -n "$ARCHIVE" ]; then
  if [ ! -f "$ARCHIVE" ]; then
    printf 'ERROR: no such file: %s\n' "$ARCHIVE" >&2
    printf '       Pass a path with forward slashes, relative to this directory.\n' >&2
    exit 2
  fi
  say "archive: $ARCHIVE"
  "$PY" - "$ARCHIVE" "$ROOT_UNIX/conan.lock" <<'PYEOF'
import json, os, re, sys, tarfile

path, lock_path = sys.argv[1], sys.argv[2]

def info(m): print("      " + m)
def ok(m):   print("      [ok]   " + m)
def bad(m):  print("      [FAIL] " + m, file=sys.stderr)

info("size       : {:,} bytes".format(os.path.getsize(path)))

payload, members, broken, rc = None, 0, None, 0
try:
    with tarfile.open(path, "r|gz") as tf:
        for member in tf:
            members += 1
            if payload is None and member.name.lstrip("./") == "pkglist.json":
                payload = tf.extractfile(member).read()
except Exception as exc:                 # bad gzip trailer, cut-off stream, ...
    broken = exc

info("members    : {:,}".format(members))
if broken is None:
    ok("streamed to the end -- complete, not truncated")
else:
    # The manifest can be perfectly correct and the transfer still unusable:
    # pkglist.json sits near the front of the stream, so it is read long before
    # the point where the file ends early.  Reporting the contents and stopping
    # there would be the same false green that `conan cache restore` gives.
    rc = 1
    bad("the archive did not decompress cleanly: {}".format(broken))
    print("      hint   the file is cut short.  `conan cache restore` extracts\n"
          "             whatever arrived and exits, so the cache ends up holding\n"
          "             part of a graph -- re-copy the file before trusting it.",
          file=sys.stderr)

if payload is None:
    bad("no pkglist.json inside -- this is not a `conan cache save` archive")
    sys.exit(1)

doc = json.loads(payload)
recipes, binaries, kinds, vtk = 0, 0, set(), []
for ref, entry in doc.items():
    recipes += 1
    for rrev, body in (entry.get("revisions") or {}).items():
        pkgs = body.get("packages") or {}
        binaries += len(pkgs)
        if ref.startswith("vtk/"):
            vtk.append((rrev, len(pkgs)))
        # Read the build type from every package, not just qt.  qt used to be the
        # reliable witness because it is in every full bundle -- but a private
        # bundle carries vtk and nothing else, and asking only qt made that one
        # report "no qt package to read a build type from" while holding both
        # build types of the one package that matters.
        for pbody in pkgs.values():
            settings = ((pbody or {}).get("info") or {}).get("settings") or {}
            if settings.get("build_type"):
                kinds.add(settings["build_type"])

lock = None
if os.path.exists(lock_path):
    found = re.search(r"vtk/9\.5\.0@aoi/stable#([0-9a-f]{32})",
                      open(lock_path, encoding="utf-8").read())
    lock = found.group(1) if found else None

info("recipes    : {}".format(recipes))
info("binaries   : {}".format(binaries))
info("carries    : {}".format(", ".join(sorted(kinds)) or "no package to read a build type from"))
info("conan.lock : {}".format("#" + lock[:12] if lock else "(pins no vtk/9.5.0@aoi/stable)"))
for rrev, count in vtk:
    note = "" if rrev == lock else "   <- NOT what conan.lock pins"
    info("vtk        : #{}{}{}".format(rrev[:12], " ({} binaries)".format(count), note))

print()
if not vtk:
    bad("this archive carries no vtk/9.5.0@aoi/stable at all")
    sys.exit(1)
if lock is None:
    ok("carries vtk; there is no lockfile here to compare it against")
    sys.exit(0)
if all(rrev != lock for rrev, _ in vtk):
    bad("this archive carries {}, but conan.lock pins #{}".format(
        ", ".join("#" + r for r, _ in vtk), lock[:12]))
    print("      hint   a bundle from before the recipe was pinned to the locked\n"
          "             revision.  No amount of restoring makes it usable: the\n"
          "             install reports a missing Debug dependency and then starts\n"
          "             a full VTK rebuild.", file=sys.stderr)
    print("      fix    take the current bundle from the machine that produced it;\n"
          "             scripts/package-cache.sh prints its md5 when it packs.",
          file=sys.stderr)
    sys.exit(1)

ok("this archive is the bundle conan.lock describes")
if kinds == {"Release"}:
    print("      note   Release binaries only -- building Debug needs\n"
          "             out/conan-cache-debug.tgz as well, and the other way round.")
# A private-only bundle is not a smaller full bundle, it is a different thing:
# the packages no remote can serve, for the machine that will upload them.  Said
# out loud because its small size would otherwise read as a truncated transfer.
if recipes and all("@" in ref for ref in doc):
    print("      note   private packages only ({} recipe, {} binaries).  That is what\n"
          "             `scripts/upload-cache.sh -r <remote> --only-private` needs, but\n"
          "             it is NOT enough to build here: the third-party packages come\n"
          "             from the full bundles or from the remote.".format(recipes, binaries))
sys.exit(rc)
PYEOF
  exit $?
fi

if [ -z "${AOI_CONAN:-}" ] || [ ! -x "${AOI_CONAN:-}" ]; then
  printf 'ERROR: conan not found. Set AOI_CONAN=/path/to/conan(.exe).\n' >&2
  exit 2
fi

mkdir -p "$WORK_DIR" || exit 1

for cfg in debug release; do
  if [ ! -f "$LISTS_DIR/pkglist-$cfg.json" ]; then
    printf 'ERROR: missing %s\n' "$LISTS_DIR/pkglist-$cfg.json" >&2
    printf '       That file is tracked in git; if it is absent the checkout is\n' >&2
    printf '       incomplete.  It is written by scripts/package-cache.sh.\n' >&2
    exit 2
  fi
done

# One query for the whole cache.  `conan list` takes a single pattern -- passing
# two is answered with "unrecognized arguments" -- so a per-entry loop would be
# 47 process starts per configuration for exactly this answer.
#
# "*#*:*" and not "*:*": a pattern that names no revision resolves to the LATEST
# one, so "*:*" would omit every revision a recipe has ever had and would report
# a manifest entry pinned to an older revision as absent while it sits right
# there in the cache.  The manifests pin revisions, so the comparison has to see
# them all.
CACHE_JSON="$WORK_DIR/cache.json"
CACHE_ERR="$WORK_DIR/cache.err"
if ! "$AOI_CONAN" list "*#*:*" --format=json -cc core:non_interactive=True \
       > "$CACHE_JSON" 2> "$CACHE_ERR"; then
  bad 'conan list "*#*:*" failed'
  tail -n 5 "$CACHE_ERR" >&2
  exit 1
fi

# Subtract both manifests from that answer up front: the requested ones are what
# gets reported, the other one is what makes the hint possible.
declare -A N_MISSING N_BINARIES N_RECIPES MISSING_FILE
for cfg in debug release; do
  MISSING_FILE[$cfg]="$WORK_DIR/missing-$cfg.txt"
  "$PY" "$REF_LIST" missing "$LISTS_DIR/pkglist-$cfg.json" "$CACHE_JSON" \
      > "${MISSING_FILE[$cfg]}" 2>/dev/null
  N_MISSING[$cfg]="$(wc -l < "${MISSING_FILE[$cfg]}" | tr -d ' ')"
  N_BINARIES[$cfg]="$("$PY" "$REF_LIST" rows "$LISTS_DIR/pkglist-$cfg.json" | wc -l | tr -d ' ')"
  N_RECIPES[$cfg]="$("$PY" "$REF_LIST" rows "$LISTS_DIR/pkglist-$cfg.json" \
                       | cut -d'#' -f1 | sort -u | wc -l | tr -d ' ')"
done

REPORT_SHOWN=10

report() {
  local cfg="$1" other
  say "check-cache: $cfg"
  info "manifest : conan/lists/pkglist-$cfg.json (${N_RECIPES[$cfg]} recipes / ${N_BINARIES[$cfg]} binaries)"

  if [ "${N_MISSING[$cfg]}" -eq 0 ]; then
    ok "every one of the ${N_BINARIES[$cfg]} binaries is in this machine's cache"
    return 0
  fi

  bad "${N_MISSING[$cfg]} of ${N_BINARIES[$cfg]} binaries are NOT in this machine's cache"
  local shown=0 row
  while IFS= read -r row; do
    [ -n "$row" ] || continue
    shown=$((shown + 1))
    [ "$shown" -le "$REPORT_SHOWN" ] || break
    printf '           %s\n' "$row" >&2
  done < "${MISSING_FILE[$cfg]}"
  if [ "${N_MISSING[$cfg]}" -gt "$REPORT_SHOWN" ]; then
    printf '           ... and %d more\n' "$((N_MISSING[$cfg] - REPORT_SHOWN))" >&2
  fi

  other="$([ "$cfg" = "debug" ] && printf 'release' || printf 'debug')"
  if [ "${N_MISSING[$other]}" -eq 0 ]; then
    printf '\n      hint     the %s manifest is complete against this same cache, so the\n' "$other" >&2
    printf '               likely cause is that out/conan-cache-%s.tgz was restored\n' "$other" >&2
    printf '               and out/conan-cache-%s.tgz was not.  `conan cache restore`\n' "$cfg" >&2
    printf '               only ever adds to a cache, so restoring it now is safe and\n' >&2
    printf '               needs no cleanup first.\n' >&2
  fi

  printf '\n      fix      conan cache restore out/conan-cache-%s.tgz\n' "$cfg" >&2
  printf '               then re-run this script; it is read-only and takes a second.\n' >&2
  return 1
}

rc=0
for cfg in $(if [ "$CONFIG" = "both" ]; then echo "debug release"; else echo "$CONFIG"; fi); do
  report "$cfg" || rc=1
done

# Which revision of the private package does the worktree resolve, and which
# ones does the cache hold?  A cache carrying only a stale revision looks the
# same as an empty one until this says so -- and the fix is different: no
# restore helps, the package has to be built.
say "vtk recipe revision"
LOCK_REV="$(grep -o 'vtk/9\.5\.0@aoi/stable#[0-9a-f]\{32\}' "$ROOT_UNIX/conan.lock" 2>/dev/null | head -1)"
LOCK_REV="${LOCK_REV#*#}"
if [ -z "$LOCK_REV" ]; then
  info "conan.lock : pins no vtk/9.5.0@aoi/stable revision (or there is no lockfile)"
else
  info "conan.lock : #${LOCK_REV:0:12}"
fi

VTK_REVS="$WORK_DIR/vtk-revs.txt"
"$PY" "$REF_LIST" revs "$CACHE_JSON" "vtk/9.5.0@aoi/stable" > "$VTK_REVS" 2>/dev/null
if [ ! -s "$VTK_REVS" ]; then
  info "cache      : no vtk/9.5.0@aoi/stable at all"
else
  while IFS=$'\t' read -r rrev count; do
    [ -n "$rrev" ] || continue
    if [ "$rrev" = "$LOCK_REV" ]; then
      info "cache      : #${rrev:0:12} ($count binaries)  <- the revision conan.lock pins"
    else
      info "cache      : #${rrev:0:12} ($count binaries)  <- unused; nothing resolves this revision"
    fi
  done < "$VTK_REVS"
fi

if [ "$rc" -eq 0 ]; then
  printf '\n=====================================================================\n'
  printf 'Cache is complete for: %s\n' \
         "$([ "$CONFIG" = "both" ] && echo 'debug + release' || echo "$CONFIG")"
  printf 'The graph conan.lock describes resolves offline:\n'
  printf '  conan install . --lockfile=conan.lock --no-remote -o "&:with_shrimp=True"\n'
  printf '=====================================================================\n'
else
  printf '\nAt least one configuration is incomplete.  Nothing was changed.\n' >&2
fi

exit "$rc"
