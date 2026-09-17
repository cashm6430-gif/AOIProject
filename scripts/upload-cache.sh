#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# AOIProject - push a bundle's packages to a Conan remote
#
#   bash scripts/upload-cache.sh -r <remote>                    # both configs
#   bash scripts/upload-cache.sh -r <remote> --config=release
#   bash scripts/upload-cache.sh -r <remote> --only-private      # vtk only
#   bash scripts/upload-cache.sh -r <remote> --dry-run
#
# Run this on a machine that CAN reach the corporate JFrog (the machine that
# produced the bundles cannot).  The packages have to be in the local cache
# already, which is what `conan cache restore` puts there:
#
#   conan cache restore out/conan-cache-debug.tgz
#   conan cache restore out/conan-cache-release.tgz
#   conan remote add company https://<host>/artifactory/api/conan/<repo>
#   conan remote login company <user>            # token, if the repo needs one
#
# What the manifests are, and why they are in the repository
# ----------------------------------------------------------
# `conan upload` can take its work list from a file (`--list=`), and the exact
# list is the pkglist that `package-cache.sh` resolved while writing the archive
# (conan/lists/pkglist-<config>.json).  out/ is gitignored, so the copy the
# receiving machine reads travels with the source tree instead of being one more
# file somebody has to remember to copy next to the archive.
#
# Two configurations, not one
# ---------------------------
# Debug and Release are different package ids under the same recipe revisions.
# The archive that is restored decides what can be uploaded, and the private
# package is the fingerprint that tells them apart: vtk/9.5.0@aoi/stable is the
# only package in the graph that exists in no remote, and its package id is
# 278fb94f... in Debug against b7c91f41... in Release.  So the preflight below
# checks that one entry before anything is pushed, which turns "I restored the
# wrong archive" into a two-second error instead of a half-finished upload.
#
# A third preflight covers the sources cache: `conan upload` also walks
# core.sources:download_cache, and refuses to continue if any blob there lacks
# the "<sha256>.json" metadata file that Conan itself always writes.  That
# failure arrives after the artifacts are already on the server, so it is
# checked -- and can be repaired -- before anything is transferred.
# ---------------------------------------------------------------------------

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_UNIX="$(cd "$SCRIPT_DIR/.." && pwd)"

# shellcheck source=scripts/_env.sh
. "$SCRIPT_DIR/_env.sh"

# Conan rejects the shell's "/e/..." form; use "E:/..." for every argument.
ROOT="$(AOI_WINPATH "$ROOT_UNIX")"

LISTS_DIR="$ROOT_UNIX/conan/lists"
OUT_DIR="$ROOT_UNIX/out"

REMOTE=""
CONFIG="${AOI_CACHE_CONFIG:-both}"
ONLY_PRIVATE=0
DRY_RUN=0
DO_CHECK=0
DO_VERIFY=1
REPAIR_METADATA=0
VERIFY_ONLY=0

usage() {
  cat <<'EOF'
Push a bundle's packages to a Conan remote.

  bash scripts/upload-cache.sh -r <remote> [options]

  -r, --remote <name>   remote to upload to (required)
      --config <c>      debug | release | both          (default: both)
      --only-private    upload only the refs with a user/channel, i.e. the
                        packages no remote can serve (vtk/9.5.0@aoi/stable)
      --repair-source-metadata
                        add the metadata file conan wants beside every blob in
                        the sources cache that lacks one.  Without it, such a
                        blob stops the upload after the artifacts are already on
                        the server
      --dry-run         ask conan what it would do, without uploading
      --check           let conan verify manifests before uploading
      --verify-only     upload nothing; only read the manifest back from the
                        remote, i.e. "did the last upload actually land?"
      --no-verify       skip reading the entries back from the remote
  -h, --help            this text

The packages must already be in the local cache:

  conan cache restore out/conan-cache-debug.tgz
  conan cache restore out/conan-cache-release.tgz
EOF
}

say() { printf '\n=== %s ===\n' "$*"; }
ok()  { printf '      [ok]   %s\n' "$*"; }
bad() { printf '      [FAIL] %s\n' "$*" >&2; }
info(){ printf '      %s\n' "$*"; }

while [ $# -gt 0 ]; do
  case "$1" in
    -r|--remote)  REMOTE="${2:-}"; shift 2 ;;
    --remote=*)   REMOTE="${1#*=}"; shift ;;
    --config)     CONFIG="${2:-}"; shift 2 ;;
    --config=*)   CONFIG="${1#*=}"; shift ;;
    --only-private) ONLY_PRIVATE=1; shift ;;
    --repair-source-metadata) REPAIR_METADATA=1; shift ;;
    --dry-run)    DRY_RUN=1; shift ;;
    --check)      DO_CHECK=1; shift ;;
    --no-verify)  DO_VERIFY=0; shift ;;
    --verify-only) VERIFY_ONLY=1; shift ;;
    -h|--help)    usage; exit 0 ;;
    *) printf 'ERROR: unknown argument "%s" (try --help)\n' "$1" >&2; exit 2 ;;
  esac
done

case "$CONFIG" in
  debug|release|both) : ;;
  *) printf 'ERROR: --config must be debug, release or both (got "%s").\n' "$CONFIG" >&2
     exit 2 ;;
esac

# Distinguishes the work list written by --only-private from the full one, so a
# dry run and a real run of the two modes cannot overwrite each other's file.
PRIVATE_SUFFIX=""
[ "$ONLY_PRIVATE" = "1" ] && PRIVATE_SUFFIX="-private"

if [ -z "$REMOTE" ]; then
  printf 'ERROR: no remote given.  Use -r <remote>; see --help.\n' >&2
  exit 2
fi

if [ -z "${AOI_CONAN:-}" ] || [ ! -x "${AOI_CONAN:-}" ]; then
  printf 'ERROR: conan not found. Set AOI_CONAN=/path/to/conan(.exe).\n' >&2
  exit 2
fi

# The interpreter that backs conan.exe; _env.sh looks for the same one.  Kept
# optional because this script is meant to run on somebody else's machine, where
# a bare `python` may be the only thing on PATH.
PY="${AOI_PYTHON:-}"
if [ -z "$PY" ] || [ ! -x "$PY" ]; then
  if command -v python >/dev/null 2>&1; then PY="$(command -v python)"
  elif command -v py >/dev/null 2>&1; then PY="$(command -v py)"
  else
    printf 'ERROR: no Python interpreter found for scripts/pkglist-refs.py.\n' >&2
    printf '       Set AOI_PYTHON to the interpreter that backs conan.exe.\n' >&2
    exit 2
  fi
fi

# Write the work list of one configuration into out/upload/ and echo its path.
# --private-only narrows it to the refs that carry a user/channel (i.e. the
# packages a proxying JFrog repository cannot serve), which is the whole reason
# somebody has to upload at all.
work_list() {
  local cfg="$1"
  local manifest="$LISTS_DIR/pkglist-$cfg.json"
  local dest
  dest="$OUT_DIR/upload/pkglist-$cfg${PRIVATE_SUFFIX}.json"
  if [ ! -f "$manifest" ]; then
    bad "missing $manifest"
    printf '         That file is tracked in git; if it is absent the checkout is\n' >&2
    printf '         incomplete.  It is written by scripts/package-cache.sh.\n' >&2
    return 1
  fi
  mkdir -p "$OUT_DIR/upload" || return 1
  if [ "$ONLY_PRIVATE" = "1" ]; then
    "$PY" "$SCRIPT_DIR/pkglist-refs.py" subset "$manifest" "$dest" --private-only >/dev/null || return 1
  else
    cp "$manifest" "$dest" || return 1
  fi
  printf '%s' "$dest"
}

# True when the local cache holds "ref#recipe_revision" with exactly "package_id".
# `conan list` exits 0 for a package it cannot find ("ERROR: Recipe ... not
# found"), so its status says nothing -- the 40-hex id has to be looked for.
#
# -cc belongs to the subcommand, not to `conan` itself: `conan -cc x list` is
# answered with "'-cc' is not a Conan command".  core:non_interactive turns a
# missing credential into an error instead of an interactive prompt, which a
# script has to have.
CI=(-cc core:non_interactive=True)

cached_package() {
  # One `local` per line: `local ref="$1" rest="$a/$ref"` in a single statement
  # reads the *new* local, which is unset at that moment, and dies under
  # `set -u` with "ref: unbound variable" (bash 5.3).
  local expect="$1"   # ref#rrev:pkgid
  local ref="${expect%%#*}"
  local rest="${expect#*#}"
  local rrev="${rest%%:*}"
  local pkgid="${rest#*:}"
  "$AOI_CONAN" list "$ref#$rrev:*" -c "${CI[@]}" 2>/dev/null | grep -q "$pkgid"
}

remote_package() {
  local expect="$1"
  local ref="${expect%%#*}"
  local rest="${expect#*#}"
  local rrev="${rest%%:*}"
  local pkgid="${rest#*:}"
  "$AOI_CONAN" list "$ref#$rrev:*" -r "$REMOTE" "${CI[@]}" 2>/dev/null | grep -q "$pkgid"
}

# The private package is the one entry that cannot be obtained anywhere else, so
# it is also the proof that the *right* archive was restored: its package id is
# different in Debug and in Release.
fingerprint_row() {
  local cfg="$1"
  # tr -d '\r' is belt and braces: scripts/pkglist-refs.py pins its own stdout to
  # "\n", but a stray carriage return here would end up inside a `grep -q` and
  # Git for Windows' grep never matches a pattern containing one.
  "$PY" "$SCRIPT_DIR/pkglist-refs.py" rows "$LISTS_DIR/pkglist-$cfg.json" --private-only 2>/dev/null \
    | head -1 | tr -d '\r'
}

upload_one() {
  local cfg="$1" build_type list_file row ref_private

  case "$cfg" in
    debug)   build_type=Debug ;;
    release) build_type=Release ;;
  esac

  say "$cfg: $([ "$VERIFY_ONLY" = 1 ] && echo "read back from remote '$REMOTE'" || echo "upload to remote '$REMOTE'") ($([ "$ONLY_PRIVATE" = 1 ] && echo 'private packages only' || echo 'whole graph'))"

  if [ ! -f "$LISTS_DIR/pkglist-$cfg.json" ]; then
    bad "manifest missing: conan/lists/pkglist-$cfg.json"
    printf '         It is tracked in git and written by scripts/package-cache.sh.\n' >&2
    return 1
  fi

  # --- 1. is the matching archive restored? -------------------------------
  # Skipped for --verify-only, whose whole point is to ask the remote a question
  # that must not depend on what this machine happens to hold.
  ref_private="$(fingerprint_row "$cfg")"
  if [ -z "$ref_private" ]; then
    bad "no private package in conan/lists/pkglist-$cfg.json"
    return 1
  fi
  if [ "$VERIFY_ONLY" = "1" ]; then
    :
  elif cached_package "$ref_private"; then
    ok "restored : ${ref_private%%#*} (${ref_private##*:} )"
  else
    bad "the $cfg packages are not in the local cache"
    printf '         Expected %s\n' "$ref_private" >&2
    printf '         Restore the matching archive first:\n' >&2
    printf '             conan cache restore %s\n' "$(AOI_WINPATH "$OUT_DIR")/conan-cache-$cfg.tgz" >&2
    printf '         The private VTK package is the fingerprint: Debug and Release\n' >&2
    printf '         carry different package ids under the same recipe revision, so\n' >&2
    printf '         this also catches having restored the wrong archive.\n' >&2
    return 1
  fi

  # --- 2. is the remote there at all? ------------------------------------
  # `conan list -r <unknown remote>` exits 1; a reachable remote that simply
  # lacks the recipe exits 0, which is why this probe asks for a package that is
  # expected to be absent.
  local probe_out probe_rc
  probe_out="$("$AOI_CONAN" list "${ref_private%%#*}" -r "$REMOTE" "${CI[@]}" 2>&1)"
  probe_rc=$?
  if [ "$probe_rc" -ne 0 ]; then
    bad "remote '$REMOTE' is not usable"
    printf '%s\n' "$probe_out" | sed 's/^/         /' >&2
    printf '         If it exists but is not logged in:\n' >&2
    printf '             conan remote list\n' >&2
    printf '             conan remote login %s <user>\n' "$REMOTE" >&2
    return 1
  fi
  ok "remote   : $REMOTE reachable"

  # --- 3. upload ----------------------------------------------------------
  list_file="$(work_list "$cfg")" || return 1
  local -a cmd=("$AOI_CONAN" upload
                --list="$(AOI_WINPATH "$list_file")" -r "$REMOTE" --confirm "${CI[@]}")
  [ "$DRY_RUN" = "1" ] && cmd+=(--dry-run)
  [ "$DO_CHECK" = "1" ] && cmd+=(--check)
  info "entries  : $("$PY" "$SCRIPT_DIR/pkglist-refs.py" rows "$list_file" | wc -l | tr -d ' ')"
  info "work list: $list_file"
  if [ "$VERIFY_ONLY" = "1" ]; then
    info "verify only: nothing is uploaded, the remote is only asked what it has"
  else
    printf '\n'
    "${cmd[@]}"
    local rc=$?
    [ "$rc" -eq 0 ] || { bad "conan upload failed (exit $rc)"; return 1; }
    ok "upload command returned 0"
  fi

  # --- 4. did it actually arrive? ----------------------------------------
  # `conan upload` is happy to report success for artifacts the server already
  # had, and a repository can accept the recipe while refusing the binaries
  # (permissions, quota, a read-only proxy).  Ask the remote for every entry.
  if [ "$DRY_RUN" = "1" ]; then
    info "dry run   : nothing was transferred, so there is nothing to read back"
    return 0
  fi
  [ "$DO_VERIFY" = "1" ] || return 0

  say "$cfg: read the entries back from '$REMOTE'"
  local total=0 found=0 missing_list="" row
  while IFS= read -r row; do
    row="${row%$'\r'}"
    [ -n "$row" ] || continue
    total=$((total + 1))
    if remote_package "$row"; then
      found=$((found + 1))
    else
      missing_list="$missing_list
         $row"
    fi
  done < <("$PY" "$SCRIPT_DIR/pkglist-refs.py" rows "$list_file")

  if [ -n "$missing_list" ]; then
    bad "$((total - found)) of $total entries did not come back from the remote"
    printf '%s\n' "$missing_list" >&2
    if [ "$VERIFY_ONLY" = "1" ]; then
      printf '         Nothing was uploaded by this run.  Either the upload never\n' >&2
      printf '         happened, or -r names the wrong remote.\n' >&2
    else
      printf '         The upload reported success, so the remote accepted the\n' >&2
      printf '         recipe and refused the binaries.  A proxy repository takes\n' >&2
      printf '         reads and rejects writes; the private package then needs a\n' >&2
      printf '         hosted repository.  Re-run with --only-private against that one.\n' >&2
    fi
    return 1
  fi
  ok "$found/$total entries present on the remote"
  return 0
}

# --- sources cache: one bare blob aborts the whole upload -------------------
# `conan upload` walks core.sources:download_cache/s/ to decide which downloaded
# sources to push as *backup sources*, and raises
#     ERROR: Missing metadata file for backup source <path>
# on the first blob that has no "<sha256>.json" beside it
# (conan/internal/rest/download_cache.py, get_backup_sources_files).  It raises
# even when that blob belongs to a package nobody is uploading, so the failure
# has nothing to do with what is being pushed -- and it arrives AFTER the
# artifacts are already on the server, which makes it read as a network or
# permission problem.
#
# A cache filled by Conan itself always has the metadata.  One filled by hand --
# scripts/prefetch-sources.sh, or curl -- did not, which is how this machine's
# cache came to hold exactly one such blob (xz_utils, fetched 2026-09-15) and
# how every upload failed until it was given one.
source_cache_dir() {
  local line v
  # `conan config show <key>` -- there is no `conan config get` in Conan 2.32
  # (it answers with the usage text and exit code 2), and `conan config list`
  # prints the key's description rather than its value.
  line="$("$AOI_CONAN" config show core.sources:download_cache 2>/dev/null | tr -d '\r' | head -1)"
  case "$line" in
    core.sources:download_cache:*) v="${line#core.sources:download_cache:}" ;;
    *) return 1 ;;
  esac
  v="${v#"${v%%[![:space:]]*}"}"
  [ -n "$v" ] || return 1
  # A relative value would be relative to the Conan home, the way the setting is
  # written in global.conf when somebody pins it per project.
  case "$v" in
    /*|[A-Za-z]:*) : ;;
    *) v="$("$AOI_CONAN" config home 2>/dev/null | tr -d '\r')/$v" ;;
  esac
  if command -v cygpath >/dev/null 2>&1; then
    cygpath -u "$v" 2>/dev/null || printf '%s' "$v"
  else
    printf '%s' "$v"
  fi
}

# Write the metadata Conan's scan insists on.  `references` stays empty because
# the blob's owner cannot be recovered from the cache: the sha256 is the only
# key, and a guessed reference would be filtered out by
# get_backup_sources_files() anyway, since it matches those keys against the
# references being uploaded.  An empty map keeps the entry inert instead of
# mis-attributed; the sources themselves stay usable for building, because
# Conan looks them up by sha256 and never reads this file.
repair_source_metadata() {
  local f
  for f in "$@"; do
    printf '{"references": {}, "timestamp": %s}\n' "$(date +%s)" > "$f.json" || return 1
  done
}

preflight_sources_cache() {
  local dir f orphans=() total=0
  if ! dir="$(source_cache_dir)"; then
    info "sources  : no core.sources:download_cache is configured, nothing to scan"
    return 0
  fi
  [ -d "$dir/s" ] || return 0
  for f in "$dir"/s/*; do
    [ -f "$f" ] || continue
    case "$f" in *.json) continue ;; esac
    total=$((total + 1))
    [ -f "$f.json" ] && continue
    orphans+=("$f")
  done
  if [ "${#orphans[@]}" -eq 0 ]; then
    [ "$total" -gt 0 ] && info "sources  : $total cached source blob(s), every one has its metadata"
    return 0
  fi

  if [ "$REPAIR_METADATA" = "1" ]; then
    repair_source_metadata "${orphans[@]}" || { bad "could not write the metadata files"; return 1; }
    ok "sources  : gave ${#orphans[@]} orphan blob(s) the metadata conan requires"
    return 0
  fi

  bad "${#orphans[@]} blob(s) in the sources cache have no metadata, and conan"
  printf '         aborts the upload when it meets one:\n' >&2
  printf '             ERROR: Missing metadata file for backup source <path>\n' >&2
  local o
  for o in "${orphans[@]}"; do printf '           %s (%s B)\n' "$(basename "$o")" "$(stat -c %s "$o")" >&2; done
  printf '         It would fail only after every artifact has been pushed.  Either:\n' >&2
  printf '             bash scripts/%s -r %s --repair-source-metadata\n' "$(basename "${BASH_SOURCE[0]}")" "$REMOTE" >&2
  printf '         which adds the missing (deliberately inert) metadata file, or\n' >&2
  printf '         re-run scripts/prefetch-sources.sh, which now writes it as it fetches.\n' >&2
  return 1
}

rc=0
# --verify-only transfers nothing, so the sources cache cannot get in its way;
# blocking it there would be a false alarm.
if [ "$VERIFY_ONLY" != "1" ]; then
  preflight_sources_cache || rc=$?
  [ "$rc" -eq 0 ] || { printf '\nNothing was uploaded.\n' >&2; exit "$rc"; }
fi

for cfg in $(if [ "$CONFIG" = "both" ]; then echo "debug release"; else echo "$CONFIG"; fi); do
  upload_one "$cfg" || { rc=$?; break; }
done

if [ "$rc" -eq 0 ]; then
  printf '\n=====================================================================\n'
  if [ "$VERIFY_ONLY" = "1" ]; then
    printf 'Verified: %s -- every entry of the manifest is on the remote\n' \
           "$([ "$CONFIG" = "both" ] && echo 'debug + release' || echo "$CONFIG")"
  else
    printf 'Uploaded: %s\n' "$([ "$CONFIG" = "both" ] && echo 'debug + release' || echo "$CONFIG")"
  fi
  printf 'Remote  : %s\n' "$REMOTE"
  printf 'Recipients can now drop the archive and resolve the same graph with\n'
  printf '  conan install . --lockfile=conan.lock --build=missing -o "&:with_shrimp=True"\n'
  printf '=====================================================================\n'
else
  printf '\nUpload did not complete.  Nothing else was touched.\n' >&2
fi
exit "$rc"
