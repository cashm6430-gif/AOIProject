#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# AOIProject - prove that the Debug build works with nothing but the bundle
#
#   bash scripts/verify-debug-bundle.sh [stage ...]
#
# Stages (default: all of them, in this order):
#   home       throwaway CONAN_HOME under out/verify/
#   restore    conan cache restore out/conan-cache-debug.tgz into it
#   check      conan cache check-integrity  (recomputes every manifest)
#   install    conan install --no-remote, and WITHOUT --build=missing
#   build      cmake configure + build in a fresh tree
#   test       ctest in that tree
#   run        Shrimp.exe --selftest out of that tree
#
# Why this exists
# ---------------
# `package-debug-cache.sh` can only tell you what it *put into* the archive.
# That is not the same question as "can somebody else build with it".  Every
# delivery bug this project has hit was invisible on the producing machine:
#
#   * pkglist resolved while Qt was optional     -> no qt/6.8.3 shipped
#   * pkglist without -o "&:with_shrimp=True"    -> no private vtk shipped, so
#                                                   the consumer still has to
#                                                   build VTK for ~50 minutes
#   * pkglist without -c:a skip_binaries=False   -> 12 of 52 binaries
#   * msys2 etc/mtab aborted `cache save`        -> 780 MB truncated archive
#
# In all three cases the archive existed, had a plausible size, and this
# machine -- which has a warm cache -- could still build.  Only a machine with
# an EMPTY cache notices.  So this script becomes that machine: it restores the
# bundle into a throwaway home and runs the consumer's whole Debug build from
# there, under rules that leave no room to cheat:
#
#   * the home starts empty, so nothing from ~/.conan2 can leak in
#   * the install is --no-remote and has no --build=missing, so a missing
#     binary is a hard error instead of a silent "compile it from source"
#   * cmake uses the toolchain the install just generated, in a fresh build
#     tree, and prefers the ninja that came out of the bundle
#
# The developer cache is never touched.  Logs land in out/6-verify.log and
# out/verify/*.log.
# ---------------------------------------------------------------------------

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_UNIX="$(cd "$SCRIPT_DIR/.." && pwd)"

# shellcheck source=scripts/_env.sh
. "$SCRIPT_DIR/_env.sh"

# Conan rejects the shell's "/e/..." form; use "E:/..." for every argument.
ROOT="$(AOI_WINPATH "$ROOT_UNIX")"

PROFILE="$ROOT/conan/profiles/windows-msvc-v143-x64"
LOCKFILE="$ROOT/conan.lock"

ARCHIVE="$ROOT/out/conan-cache-debug.tgz"
ARCHIVE_UNIX="$ROOT_UNIX/out/conan-cache-debug.tgz"
VERIFY_ROOT="$ROOT_UNIX/out/verify"
V_HOME_UNIX="$VERIFY_ROOT/conan-home"
V_HOME="$(AOI_WINPATH "$V_HOME_UNIX")"
V_GEN="$VERIFY_ROOT/conan"
V_BUILD="$VERIFY_ROOT/build"
LOG="$ROOT_UNIX/out/6-verify.log"

# The consumer's graph must match the producer's or the check proves nothing
# about the archive.  Only one thing is conditional -- the private VTK package,
# via with_shrimp -- so that is the only flag to keep in lockstep with the other
# two scripts.  Qt needs no flag: conanfile.py requires qt/6.8.3 unconditionally
# because TaskControl is not headless, so every resolvable graph has it.
# AOI_WITH_SHRIMP=0 falls back to the old core-only check, which then proves less
# (it cannot catch a bundle that ships without VTK).
AOI_WITH_SHRIMP="${AOI_WITH_SHRIMP:-1}"
BUILD_JOBS="${BUILD_JOBS:-}"

say() { printf '\n=== %s ===\n' "$*"; }
ok()  { printf '      [ok]   %s\n' "$*"; }
bad() { printf '      [FAIL] %s\n' "$*" >&2; }

# --- environment shared by every stage -------------------------------------
# Has to run AFTER _env.sh, which deliberately unsets CONAN_HOME so that the
# normal build uses Conan's default home.  Conan wants a native absolute path.
export CONAN_HOME="$V_HOME"

require_conan() {
  if [ -z "${AOI_CONAN:-}" ] || [ ! -x "${AOI_CONAN:-}" ]; then
    printf 'ERROR: conan not found. Set AOI_CONAN=/path/to/conan(.exe).\n' >&2
    exit 2
  fi
}

# Hand cmake/ctest the ninja that lives IN the restored cache: using this
# machine's copy would hide a tool package missing from the bundle.
prefer_bundled_ninja() {
  local n
  n="$(find "$V_HOME_UNIX/p" -maxdepth 5 -path '*ninja*/p/bin/ninja.exe' 2>/dev/null | head -1)"
  if [ -n "$n" ]; then
    export PATH="$(dirname "$n"):$PATH"
    printf '      bundled ninja : %s\n' "$n"
  else
    printf '      [warn] no ninja inside the restored cache; using %s\n' "${AOI_NINJA:-none}"
  fi
}

require_toolchain() {
  local missing=0 pair name path
  for pair in "cmake:${AOI_CMAKE:-}" "ctest:${AOI_CTEST:-}"; do
    name="${pair%%:*}"; path="${pair#*:}"
    if [ -n "$path" ] && [ -e "$path" ]; then printf '      %-6s: %s\n' "$name" "$path"
    else printf '      %-6s: NOT FOUND\n' "$name"; missing=1; fi
  done
  [ "$missing" -eq 0 ] || { printf 'ERROR: cmake/ctest missing.\n' >&2; exit 2; }
}

# --- stages ----------------------------------------------------------------

# Remove a directory left behind by an earlier run, and prove it is gone.
#
# Both trees this script clears are large (a restored Debug home is ~570k files)
# and a cache unpacked by tar can carry read-only entries, so drop the attribute
# and retry.  The errors stay visible on purpose: swallowing them costs a
# diagnostic round, and "could not clear" alone does not say whether it was a
# permission, a lock or a path-length problem.
clear_tree() {
  local dir="$1" out
  [ -e "$dir" ] || return 0
  out=$(rm -rf "$dir" 2>&1 | head -3)
  if [ -e "$dir" ]; then
    printf '      rm failed%s; clearing read-only bits and retrying\n' \
           "${out:+ ($(printf '%s' "$out" | tr '\n' ' '))}"
    chmod -R u+w "$dir" 2>/dev/null || true
    rm -rf "$dir" 2>&1 | head -3
  fi
  if [ -e "$dir" ]; then
    printf '      still present; moving aside instead\n'
    mv "$dir" "$dir.stale_$(date +%H%M%S)" 2>&1 | head -3
  fi
  [ -e "$dir" ] && { bad "could not clear $dir (errors above)"; return 1; }
  return 0
}

stage_home() {
  say "home: create an empty CONAN_HOME"
  # A stale home would let packages leak in from an earlier run and the
  # verification would then pass for the wrong reason.
  clear_tree "$V_HOME_UNIX" || return 1
  mkdir -p "$V_HOME_UNIX" || return 1
  [ "$(ls -A "$V_HOME_UNIX" | wc -l)" -eq 0 ] || { bad "not empty"; return 1; }
  ok "empty: $V_HOME"

  # Any command bootstraps an empty home (settings.yml / global.conf come from
  # Conan's bundled defaults); the archive itself only carries p/.
  "$AOI_CONAN" config home >/dev/null 2>&1 || true
  if [ ! -f "$V_HOME_UNIX/settings.yml" ]; then
    "$AOI_CONAN" profile detect --force >/dev/null 2>&1 || true
  fi
  [ -f "$V_HOME_UNIX/settings.yml" ] || { bad "no settings.yml"; return 1; }
  ok "settings.yml bootstrapped"
}

count_binaries() {
  local json="$VERIFY_ROOT/restored-pkglist.json"
  "$AOI_CONAN" list '*:*' -c --format=json > "$json" 2>/dev/null || true
  # Parse the JSON instead of grepping for indentation: byte-level matching on
  # pretty-printed output breaks silently the day Conan reformats it.
  "${AOI_PYTHON:-python}" -c '
import json, sys
data = json.load(open(sys.argv[1], encoding="utf-8"))
total = 0
for cache in data.values():
    for info in cache.values():
        for rev in info.get("revisions", {}).values():
            total += len(rev.get("packages", {}))
print(total)
' "$json" 2>/dev/null || echo 0
}

stage_restore() {
  say "restore: conan cache restore"
  [ -f "$ARCHIVE_UNIX" ] || { bad "archive not found: $ARCHIVE_UNIX"; return 1; }
  printf '      archive : %s\n' "$ARCHIVE_UNIX"
  printf '      bytes   : %s\n' "$(stat -c %s "$ARCHIVE_UNIX" 2>/dev/null || echo '?')"
  printf '      md5     : %s\n' "$(md5sum "$ARCHIVE_UNIX" | cut -d' ' -f1)"
  [ -f "$V_HOME_UNIX/settings.yml" ] || { bad "run the 'home' stage first"; return 1; }
  "$AOI_CONAN" cache restore "$ARCHIVE" || { bad "restore failed"; return 1; }

  local n; n="$(count_binaries)"
  printf '      binaries in the restored cache : %s\n' "$n"
  [ "${n:-0}" -ge 1 ] || { bad "the restored cache has no binaries at all"; return 1; }
  ok "restored"
}

stage_check() {
  say "check: conan cache check-integrity (recomputes every manifest)"
  # The same code path that used to raise OSError(22) on msys2's etc/mtab, so
  # this doubles as the regression test for the reparse-point normalization.
  if "$AOI_CONAN" cache check-integrity '*:*' > "$VERIFY_ROOT/check-integrity.log" 2>&1; then
    ok "integrity ok ($(grep -c 'Integrity check: ok' "$VERIFY_ROOT/check-integrity.log" || true) packages)"
  else
    bad "integrity check failed -- see $VERIFY_ROOT/check-integrity.log"
    tail -20 "$VERIFY_ROOT/check-integrity.log" >&2
    return 1
  fi
}

stage_install() {
  say "install: conan install (--no-remote, no --build=missing)"
  local -a opt_flag=()
  if [ "$AOI_WITH_SHRIMP" = "1" ]; then
    opt_flag=(-o "&:with_shrimp=True"); printf '      shrimp option: ON\n'
  else
    printf '      shrimp option: OFF (NOT the shape of the shipped archive)\n'
  fi

  # `conan install` APPENDS an include to the project's CMakeUserPresets.json
  # (it never replaces the list).  Installing a second time into a different
  # --output-folder therefore leaves two includes that both define the preset
  # "conan-debug", and afterwards every plain
  #     cmake --preset debug
  # in this repository dies with
  #     CMake Error: Could not read presets: Duplicate preset: "conan-debug"
  # The verify build does not use those presets -- it passes the toolchain file
  # explicitly -- so put the developer-facing file back exactly as it was.
  local presets="$ROOT_UNIX/CMakeUserPresets.json"
  local presets_bak="$VERIFY_ROOT/CMakeUserPresets.json.bak"
  local had_presets=0
  if [ -f "$presets" ]; then had_presets=1; cp "$presets" "$presets_bak" || return 1; fi
  restore_user_presets() {
    if [ "$had_presets" -eq 1 ]; then cp "$presets_bak" "$presets" 2>/dev/null || true
    else rm -f "$presets"; fi
  }

  # No --build=missing and no remotes, on purpose.  This is the question the
  # bundle has to answer: is the cache on its own enough?  With --build=missing
  # a missing binary quietly becomes "compile it from source", which on an empty
  # home without prefetched sources fails somewhere far less readable.
  if ! "$AOI_CONAN" install "$ROOT" \
        --lockfile="$LOCKFILE" \
        --no-remote \
        "${opt_flag[@]}" \
        -pr:h="$PROFILE" -pr:b="$PROFILE" \
        -s:h build_type=Debug \
        --output-folder="$V_GEN" \
        > "$VERIFY_ROOT/install.log" 2>&1; then
    restore_user_presets
    bad "the cache is NOT self-sufficient -- see $VERIFY_ROOT/install.log"
    grep -nE 'ERROR|Missing prebuilt|not found in local cache|Unable to find' \
         "$VERIFY_ROOT/install.log" | head -20 >&2
    printf '       Every ref listed above is missing from the archive.\n' >&2
    return 1
  fi
  restore_user_presets
  printf '      CMakeUserPresets.json left untouched\n'
  ok "graph resolved with no remote and nothing to build"
  [ -f "$V_GEN/build/Debug/generators/conan_toolchain.cmake" ] \
    || { bad "no conan_toolchain.cmake under $V_GEN"; return 1; }
  ok "conan_toolchain.cmake generated"
}

stage_build() {
  say "build: cmake configure + build (fresh tree, restored toolchain)"
  require_toolchain
  prefer_bundled_ninja

  # Empty bin/ even when the build tree itself is reused.  Every executable and
  # its whole DLL closure land there, so a closure left over from an earlier run
  # would let the `test` and `run` stages succeed with DLLs the bundle never
  # provided -- precisely the leak this script exists to catch.  Ninja notices
  # the missing outputs and relinks them, which re-runs the deploy step against
  # what the bundle actually contains, so this costs a relink, not a recompile.
  clear_tree "$V_BUILD/bin" || return 1
  mkdir -p "$V_BUILD/bin" || return 1
  # Shrimp is ON by default in the top-level CMakeLists, but say so explicitly:
  # this stage is a check on the *bundle*, and "the default happened to be on"
  # would let a bundle without VTK pass quietly if that default ever changed.
  local -a extra=()
  if [ "$AOI_WITH_SHRIMP" = "1" ]; then
    extra+=("-DAOI_BUILD_SHRIMP=ON")
    printf '      shrimp        : ON (Shrimp + VTK come out of the bundle)\n'
  fi
  "$AOI_CMAKE" -S "$ROOT" -B "$(AOI_WINPATH "$V_BUILD")" -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
      -DCMAKE_TOOLCHAIN_FILE="$(AOI_WINPATH "$V_GEN")/build/Debug/generators/conan_toolchain.cmake" \
      -DAOI_BUILD_TASKCONTROL_TESTS=ON \
      "${extra[@]}" \
      > "$VERIFY_ROOT/configure.log" 2>&1 || {
    bad "configure failed -- see $VERIFY_ROOT/configure.log"
    tail -25 "$VERIFY_ROOT/configure.log" >&2
    return 1
  }
  ok "configured"
  local -a jobs=(); [ -n "$BUILD_JOBS" ] && jobs=(--parallel "$BUILD_JOBS")
  "$AOI_CMAKE" --build "$(AOI_WINPATH "$V_BUILD")" "${jobs[@]}" \
      > "$VERIFY_ROOT/build.log" 2>&1 || {
    bad "build failed -- see $VERIFY_ROOT/build.log"
    grep -nE 'error [A-Z]+[0-9]{4}|fatal error|ninja: build stopped' \
         "$VERIFY_ROOT/build.log" | head -15 >&2
    return 1
  }
  ok "built"
}

stage_test() {
  say "test: ctest"
  require_toolchain
  prefer_bundled_ninja
  "$AOI_CTEST" --test-dir "$(AOI_WINPATH "$V_BUILD")" \
      --build-config Debug --output-on-failure > "$VERIFY_ROOT/ctest.log" 2>&1
  local rc=$?
  grep -E '^ *Test +#|tests passed|tests failed out of' "$VERIFY_ROOT/ctest.log" | sed 's/^/      /'
  [ "$rc" -eq 0 ] || { bad "ctest failed -- see $VERIFY_ROOT/ctest.log"; return 1; }
  ok "all tests passed"
}

stage_run() {
  say "run: Shrimp.exe --selftest (out of the restored bundle)"
  local exe="$V_BUILD/bin/Shrimp.exe"
  if [ ! -f "$exe" ]; then
    bad "$exe missing -- the bundle did not build Shrimp"
    printf '       Check %s for the find_package(VTK) failure.\n' \
           "$VERIFY_ROOT/configure.log" >&2
    return 1
  fi
  local out rc
  out="$("$exe" --selftest 2>&1)"
  rc=$?
  printf '%s\n' "$out" | sed 's/^/      /'
  if ! printf '%s\n' "$out" | grep -q 'END-TO-END PASS'; then
    bad "Shrimp --selftest did not pass (exit $rc)"
    printf '       Compiling and ctest can both succeed while the VTK/Qt runtime\n' >&2
    printf '       DLLs or the OpenGL context are wrong.  This is the stage that\n' >&2
    printf '       notices, which is why the bundle is not called verified without it.\n' >&2
    return 1
  fi
  ok "the application runs from the bundle alone"
}

# --- main ------------------------------------------------------------------

mkdir -p "$VERIFY_ROOT"
require_conan

if [ "$#" -eq 0 ]; then
  set -- home restore check install build test run
fi

for stage in "$@"; do
  {
    printf '\n########## %s  stage=%s ##########\n' "$(date '+%F %T')" "$stage"
    case "$stage" in
      home|restore|check|install|build|test|run) "stage_$stage" ;;
      *) printf 'ERROR: unknown stage "%s"\n' "$stage" >&2; exit 2 ;;
    esac
  } 2>&1 | tee -a "$LOG"
  # ${PIPESTATUS[0]} is the stage function's status; tee itself always succeeds.
  rc="${PIPESTATUS[0]}"
  if [ "$rc" -ne 0 ]; then
    printf '\n=== VERIFY FAILED at stage "%s" (exit %s) ===\n' "$stage" "$rc" | tee -a "$LOG"
    exit "$rc"
  fi
done

{
  printf '\n%s\n' "-------------------------------------------------------------"
  # Only the full chain earns the strong claim: three of the six stages leaving
  # `install` or `test` out would prove far less than the sentence suggests.
  # `run` is what separates "it compiles" from "it works", so the strongest
  # sentence needs all three.
  has_install=0; has_test=0; has_run=0
  printf '%s\n' "$*" | grep -q 'install' && has_install=1
  printf '%s\n' "$*" | grep -q 'test'    && has_test=1
  printf '%s\n' "$*" | grep -q 'run'     && has_run=1
  if [ "$has_install" -eq 1 ] && [ "$has_test" -eq 1 ] && [ "$has_run" -eq 1 ]; then
    printf 'VERIFIED: the bundle alone builds, tests and runs the Debug project\n'
  elif [ "$has_install" -eq 1 ] && [ "$has_test" -eq 1 ]; then
    printf 'VERIFIED: the bundle alone builds and tests the Debug project\n'
  else
    printf 'stages passed (this is a partial run, not a full verification)\n'
  fi
  printf '  stages : %s\n' "$*"
  printf '  home   : %s\n' "$V_HOME"
  printf '  logs   : out/verify/*.log and out/6-verify.log\n'
  printf '%s\n' "-------------------------------------------------------------"
} | tee -a "$LOG"
exit 0
