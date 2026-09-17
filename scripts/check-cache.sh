#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# AOIProject - is the bundle this machine restored actually in its cache?
#
#   bash scripts/check-cache.sh                     # both configurations
#   bash scripts/check-cache.sh --config=debug
#   bash scripts/check-cache.sh --config=release
#
# Read-only and offline.  It asks `conan list "*#*:*"` what the local cache
# holds and subtracts that from the manifests scripts/package-cache.sh publishes
# (conan/lists/pkglist-<config>.json).  Nothing is restored, built or uploaded,
# no remote is contacted, and the whole thing runs in about a second.
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

usage() {
  cat <<'EOF'
Check the local Conan cache against the manifests in conan/lists/.

  bash scripts/check-cache.sh [--config debug|release|both]

      --config <c>   which configuration to report on      (default: both)
  -h, --help         this text

The cache is always compared against BOTH manifests, so a failure can say
whether the other configuration's packages are the ones that are present.
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
    -h|--help)   usage; exit 0 ;;
    *) printf 'ERROR: unknown argument "%s" (try --help)\n' "$1" >&2; exit 2 ;;
  esac
done

case "$CONFIG" in
  debug|release|both) : ;;
  *) printf 'ERROR: --config must be debug, release or both (got "%s").\n' "$CONFIG" >&2
     exit 2 ;;
esac

if [ -z "${AOI_CONAN:-}" ] || [ ! -x "${AOI_CONAN:-}" ]; then
  printf 'ERROR: conan not found. Set AOI_CONAN=/path/to/conan(.exe).\n' >&2
  exit 2
fi

# The interpreter behind conan.exe; a helper script on somebody else's machine
# may only have a bare `python`, so fall back to whatever is on PATH.
PY="${AOI_PYTHON:-}"
if [ -z "$PY" ] || [ ! -x "$PY" ]; then
  if command -v python >/dev/null 2>&1; then PY="$(command -v python)"
  elif command -v py >/dev/null 2>&1; then PY="$(command -v py)"
  else
    printf 'ERROR: no Python interpreter found for scripts/pkglist-refs.py.\n' >&2
    exit 2
  fi
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
