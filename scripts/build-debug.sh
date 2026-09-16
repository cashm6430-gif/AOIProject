#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# AOIProject — Debug build driver (Windows / MSVC v143 / Ninja)
#
#   bash scripts/build-debug.sh [stage ...]
#
# Stages (default: all, in this order):
#   vtk        conan create conan/recipes/vtk -> vtk/9.5.0@aoi/stable (Debug)
#   deps       conan install --build=missing  (retries transient failures only)
#   configure  cmake --preset debug
#   build      cmake --build --preset build-debug
#   test       ctest in the debug build tree
#   run        Shrimp.exe --selftest (headless end-to-end check of the GUI)
#   package    conan cache save -> out/conan-cache-debug.tgz
#
# The deps stage is resilient on purpose: this machine sits behind a wireless
# link that drops without warning.  When a source download is truncated Conan
# renames the source folder to "<pkg>/s.dirty" and then refuses to continue
# until that path disappears.  Deleting it trips the host's bulk-delete guard,
# so the folder is *renamed* instead and Conan simply re-fetches it.  Packages
# that already built stay in the cache, which makes each retry incremental.
# MSBuild-based packages additionally need the duplicate-case proxy variables
# removed; scripts/_env.sh does that.
#
# Retrying is not free, though, and it is pointless for the failures that have
# actually cost time here.  pcre2's LNK1120, winflexbison's missing win_flex.exe
# and libiconv's D8003 were all reproducible verbatim, and every one of them
# burned attempts until the log was read by hand.  So a failed attempt is
# classified first (see DETERMINISTIC_RE / TRANSIENT_RE below) and only the
# transient class is retried; a build error aborts the stage on the spot with
# the offending lines printed.
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
CONAN_OUT="$ROOT/out/conan/debug"
CONAN_OUT_REL="out/conan/debug"
BUILD_DIR="$ROOT/out/build/debug"
LOG_DIR="$ROOT/out"
DEPS_LOG="$LOG_DIR/conan-debug-deps.log"

# Retries exist for one reason only: this machine's wireless link drops, and a
# truncated download is worth another attempt.
#
# A *build* failure is not worth another attempt.  Conan re-runs the identical
# recipe with the identical inputs, so it produces the identical error; the
# only thing 40 attempts buy is 40x the wall clock.  The driver therefore
# classifies every failed attempt and stops immediately on the deterministic
# ones, printing the offending lines instead of burying them in the log.
#   MAX_ATTEMPTS       bound for transient/unknown failures only
#   AOI_RETRY_ALL=1    ignore classification, retry everything (old behaviour)
MAX_ATTEMPTS="${MAX_ATTEMPTS:-3}"
RETRY_WAIT="${RETRY_WAIT:-10}"
AOI_RETRY_ALL="${AOI_RETRY_ALL:-0}"
BUILD_JOBS="${BUILD_JOBS:-}"

# A failure the recipe will reproduce verbatim.  Only *errors* are matched --
# LNK4098 is a warning that shows up in healthy builds, so the pattern needs
# "error LNK", not just "LNK".
DETERMINISTIC_RE='(: Error in (build|configure|generate|package|validate|source)\(\) method|Error [0-9]+ while executing|error LNK[0-9]{4}|error C[0-9]{4}|error D[0-9]{4}|fatal error [CU][0-9]{4}|LINK : fatal error|ninja: build stopped|CMake Error at|ERROR: Program .* not found|meson\.build:[0-9]+:[0-9]+: ERROR|No rule to make target|collect2: error|build failed)'

# A failure that another attempt can plausibly clear: the link, the proxy, or
# the ConanCenter front end.  Checked *second*: a package that dies in build()
# may well have logged a timeout while fetching something earlier in the same
# attempt, and the build error is the one that matters.
TRANSIENT_RE='(Timeout|timed out|Read timed out|ConnectionError|Connection reset|Connection aborted|Connection refused|SSLError|ProxyError|ClientPayloadError|ServerDisconnectedError|Failed to download|Could not connect|Temporary failure in name resolution|requests\.exceptions|IncompleteRead|chunked encoding|EOF occurred|Remote service unavailable|HTTPError|No route to host|Network is unreachable)'

# Offline by default.  Every recipe the lockfile pins is already in the local
# cache and every source tarball is prefetched (scripts/prefetch-sources.sh),
# so `--no-remote` removes the last reason for the build to touch a network
# that is not reliable here.  Set AOI_OFFLINE=0 when a recipe has to be
# refreshed from ConanCenter.  If an offline attempt fails because something is
# genuinely missing from the cache, the driver flips itself online and says so.
AOI_OFFLINE="${AOI_OFFLINE:-1}"

# Tests are opt-in in CMake; the test stage turns them on.
ENABLE_TESTS="${AOI_BUILD_TASKCONTROL_TESTS:-ON}"

# Qt is an *unconditional* Conan dependency of this project, not a switch:
# TaskControl is not headless.  It stores its runtime data in QVariant/QHash
# (core/AlgorithmContext.h), parses recipes with QJsonArray
# (core/AlgorithmIO.h), and TaskControl/CMakeLists.txt line 1 is
#   find_package(Qt6 REQUIRED COMPONENTS Core)
# Core only -- TaskControl has no GUI type in it; Qt6::Gui/Widgets belong to
# Shrimp.  Runtime links AOI::TaskControl PUBLIC and both test executables link
# it too, so every target here needs Qt and conanfile.py requires qt/6.8.3
# without asking.  There is deliberately no AOI_WITH_QT: an option that can only
# ever be 1 is a trap, and with_qt=False used to resolve a Qt-less graph and then
# die at TaskControl/CMakeLists.txt:1 with
#   Could not find a package configuration file provided by "Qt6"
#
# Shrimp is the project's application -- the Qt GUI that renders point clouds
# and drives the measurement recipes -- so the driver builds it by default.
# That needs the private VTK package, which the "vtk" stage creates; set
# AOI_WITH_SHRIMP=0 for a core-only loop (TaskControl + Runtime + tests) that
# still links Qt but keeps VTK out of the graph entirely.
AOI_WITH_SHRIMP="${AOI_WITH_SHRIMP:-1}"
AOI_FORCE_VTK="${AOI_FORCE_VTK:-0}"

say() { printf '\n=== %s ===\n' "$*"; }

# --- helpers ---------------------------------------------------------------

# Move every leftover "dirty" *source* marker out of the way so Conan can retry.
#
# Only "s.dirty" is touched, and that restriction matters.  Conan uses the same
# ".dirty" suffix for two very different things:
#
#   <pkg>/s.dirty   a source tree whose download/checksum failed.  Conan refuses
#                   to continue until the path is gone, and deleting it trips the
#                   host's bulk-delete guard, so renaming it is the cheap fix.
#
#   <pkg>/p.dirty   a *binary package* Conan has judged corrupted and wants to
#                   delete.  Renaming that one is actively harmful: the recipe
#                   and its "d" folder stay in the cache, so on the next install
#                   Conan believes the package is present and then dies with
#                       AssertionError: Pkg '<ref>' folder must exist: ...\<pkg>\p
#                   An earlier version of this function matched "*.dirty" and
#                   bricked msys2/cci.latest and strawberryperl/5.32.1.1 exactly
#                   that way (they sat as p.dirty_bad_HHMMSS for a whole day).
#                   If you ever see a p.dirty, let Conan remove it -- or clear it
#                   with `conan remove "<pkg>/*" -c`, never with mv.
clean_dirty_sources() {
  local moved=0 d
  while IFS= read -r d; do
    [ -e "$d" ] || continue
    if mv "$d" "${d}_bad_$(date +%H%M%S)" 2>/dev/null; then
      printf '  [clean] %s\n' "$d"
      moved=$((moved + 1))
    fi
  done < <(find "$AOI_CONAN_HOME/p" -maxdepth 2 -name 's.dirty*' 2>/dev/null)
  [ "$moved" -gt 0 ] && printf '  [clean] %d dirty source folder(s) moved aside\n' "$moved"
  return 0
}

# `conan install` APPENDS its generated preset file to the project's
# CMakeUserPresets.json -- it never replaces the list.  Two installs into
# different --output-folder values therefore leave two includes that both define
# "conan-debug", and the very next
#     cmake --preset debug
# dies with
#     CMake Error: Could not read presets from ...: Duplicate preset: "conan-debug"
# Nothing in CMakePresets.json references Conan's generated presets (the
# project's own "debug" / "build-debug" carry their toolchain path themselves),
# so pin the file to the folder this stage just installed into.  It is a
# generated, git-ignored convenience file, rewritten on every `deps` run.
normalize_user_presets() {
  local f="$ROOT_UNIX/CMakeUserPresets.json"
  local inc="$CONAN_OUT_REL/build/Debug/generators/CMakePresets.json"
  local want
  want="$(printf '{\n    "version": 4,\n    "vendor": {\n        "conan": {}\n    },\n    "include": [\n        "%s"\n    ]\n}' "$inc")"
  if [ -f "$f" ] && [ "$(cat "$f")" = "$want" ]; then
    return 0
  fi
  printf '%s\n' "$want" > "$f" || return 1
  printf '  [presets] CMakeUserPresets.json pinned to %s\n' "$inc"
  return 0
}

# --- recipe revision guards -------------------------------------------------
# `vtk/9.5.0@aoi/stable` is the one dependency whose *bytes* this repository
# supplies: Conan hashes the conanfile.py it reads off disk into the recipe
# revision, and conan.lock pins that revision.  Anything that changes those
# bytes -- a real edit, or just an editor rewriting the file with CRLF line
# endings -- changes which build the lockfile describes.

# The vtk revision conan.lock pins, or empty when it pins none -- which stage_vtk
# treats as an error rather than as a reason to skip the check.
#
# The revision is 32 hex characters (Conan hashes with md5), and the lockfile
# entry is "vtk/9.5.0@aoi/stable#<revision>%<timestamp>", so take everything
# between '#' and '%'.  Do NOT hardcode the length: an earlier version of this
# helper asked for 40 characters, matched nothing, and silently turned both this
# guard and the skip test below into no-ops -- which cost a 26 minute rebuild
# that nothing had asked for.
locked_vtk_revision() {
  sed -n 's/.*vtk\/9\.5\.0@aoi\/stable#\([0-9a-f]*\)%.*/\1/p' "$LOCKFILE" 2>/dev/null \
    | head -1
}

# True when the file contains at least one carriage return.
#
# Deliberately not `grep -q $'\r' "$f"`: Git for Windows' grep treats CR as a
# line terminator, so it answers "no match" for a file `od -c` shows is full of
# \r\n.  Measured on this very recipe, while it was CRLF:
#     grep -c $'\r'   -> 0        grep -cU $'\r'  -> 104
# Counting bytes cannot be misread, and needs no --binary/GNU-only flag.
file_has_cr() {
  [ "$(wc -c < "$1")" -ne "$(tr -d '\r' < "$1" | wc -c)" ]
}

# Put CRLF back to LF before Conan hashes the file.
#
# `.gitattributes` declares `*.py text eol=lf`, so a clone always checks this
# file out with LF.  What it does NOT do is report a worktree that has drifted
# back to CRLF: git normalizes the comparison away, so `git diff` prints nothing
# and `git hash-object` returns the blob the commit already stores, while
# `conan export` produces a different revision and the pinned one resolves to
# nothing.  Measured on this very file: LF exports as 983c7acf..., CRLF as
# 4b2e39c8... .  The file is small and ASCII, and dropping carriage returns is
# what git does on the way in anyway, so this is a no-op for the committed
# content -- it just makes the build agree with a fresh clone.
normalize_recipe_eol() {
  local f rel
  for f in "$@"; do
    [ -f "$f" ] || continue
    file_has_cr "$f" || continue
    rel="${f#"$ROOT_UNIX"/}"
    sed -i 's/\r$//' "$f" || return 1
    printf '  [eol] %s: CRLF -> LF (conan hashes these bytes into the recipe revision)\n' "$rel"
  done
  return 0
}

require_conan() {
  if [ -z "${AOI_CONAN:-}" ] || [ ! -x "${AOI_CONAN:-}" ]; then
    printf 'ERROR: conan not found. Set AOI_CONAN=/path/to/conan(.exe).\n' >&2
    exit 2
  fi
  printf 'conan : %s\n' "$AOI_CONAN"
  printf 'root  : %s\n' "$ROOT"
}

require_toolchain() {
  local missing=0
  for pair in "cmake:${AOI_CMAKE:-}" "ninja:${AOI_NINJA:-}" "ctest:${AOI_CTEST:-}"; do
    name="${pair%%:*}"
    path="${pair#*:}"
    if [ -n "$path" ] && [ -e "$path" ]; then
      printf '%-6s: %s\n' "$name" "$path"
    else
      printf '%-6s: NOT FOUND\n' "$name"
      missing=1
    fi
  done
  if [ "$missing" -ne 0 ]; then
    printf 'ERROR: cmake / ninja / ctest must all be present before configuring.\n' >&2
    exit 2
  fi
}

# --- stages ----------------------------------------------------------------

stage_deps() {
  say "deps: conan install --build=missing (Debug)"
  require_conan
  mkdir -p "$CONAN_OUT"
  : > "$DEPS_LOG"

  local i rc before
  local -a remote_flag=() opt_flag=()
  if [ "$AOI_OFFLINE" = "1" ]; then
    remote_flag=(--no-remote)
    printf 'mode  : OFFLINE (--no-remote)\n'
  else
    printf 'mode  : ONLINE (ConanCenter)\n'
  fi
  # Qt is not an option -- it arrives with the unconditional require in
  # conanfile.py -- so the only choice left here is whether the private VTK
  # package joins the graph.
  opt_flag=()
  if [ "$AOI_WITH_SHRIMP" = "1" ]; then
    opt_flag=(-o "&:with_shrimp=True")
    printf 'shrimp: ON  (-o &:with_shrimp=True; adds the private VTK package)\n'
  else
    printf 'shrimp: OFF (core-only loop: TaskControl + Runtime + tests; Qt stays)\n'
  fi

  for ((i = 1; i <= MAX_ATTEMPTS; i++)); do
    clean_dirty_sources
    before=$(wc -l < "$DEPS_LOG" 2>/dev/null || echo 0)
    { printf '\n===== attempt %d / %d  %s =====\n' "$i" "$MAX_ATTEMPTS" "$(date '+%F %T')"; } | tee -a "$DEPS_LOG"

    # --output-folder must be spelled in the long form: with a bare "-o" also
    # present, conan's parser splits "-of=E:/..." as "-o f=E:/..." and then dies
    # with "option 'f' doesn't exist. Possible options are [...]".
    "$AOI_CONAN" install "$ROOT" \
        --lockfile="$LOCKFILE" \
        --build=missing \
        "${remote_flag[@]}" \
        "${opt_flag[@]}" \
        -pr:h="$PROFILE" -pr:b="$PROFILE" \
        -s:h build_type=Debug \
        --output-folder="$CONAN_OUT" 2>&1 | tee -a "$DEPS_LOG"
    rc=${PIPESTATUS[0]}

    if [ "$rc" -eq 0 ]; then
      printf '\n===== deps OK on attempt %d =====\n' "$i" | tee -a "$DEPS_LOG"
      normalize_user_presets
      return 0
    fi

    # A missing recipe is the one failure that retrying cannot fix: drop back
    # to an online resolve instead of spinning.  Only *this* attempt's output is
    # inspected, and only lines that are genuine "not available locally" errors
    # count -- an informational line such as
    #   boost/1.83.0: Compatible configurations not found in cache, checking servers
    # is printed for every missing binary package and used to flip the driver
    # online on failures that had nothing to do with the cache.
    #
    # The exports_sources case is the important one: Conan only fetches a
    # recipe's exported sources when it needs to build that package from source,
    # and it does so by looping over the *remotes* list.  With --no-remote that
    # list is empty, so the loop body never runs and Conan raises
    #   ERROR: The 'openexr/3.2.3' package has 'exports_sources' but sources not
    #          found in local cache.
    # even though the recipe is sitting right there in the cache and the source
    # tarball is already prefetched.  The recipes that hit this are exactly the
    # ones that ship patches (openexr, libtiff, opencv, pcl, ...), which is why
    # the failure always appears part-way through the dependency list.
    local attempt_out
    attempt_out="$(tail -n "+$((before + 1))" "$DEPS_LOG" 2>/dev/null || true)"
    if [ "${#remote_flag[@]}" -gt 0 ] && \
       printf '%s\n' "$attempt_out" | \
         grep -qE "^ERROR: (Unable to find .* in remotes|Missing prebuilt package|Package .* not resolved|Recipe .* not found|The .* package has .exports_sources. but sources not found in local cache)"; then
      printf '\n===== offline resolve failed; switching to ONLINE for the rest =====\n' | tee -a "$DEPS_LOG"
      remote_flag=()
    fi

    printf '\n===== attempt %d failed (exit %d) =====\n' "$i" "$rc" | tee -a "$DEPS_LOG"

    local kind=unknown
    if printf '%s\n' "$attempt_out" | grep -qaE "$DETERMINISTIC_RE"; then
      kind=deterministic
    elif printf '%s\n' "$attempt_out" | grep -qaE "$TRANSIENT_RE"; then
      kind=transient
    fi

    if [ "$kind" = "deterministic" ] && [ "$AOI_RETRY_ALL" != "1" ]; then
      printf '===== deterministic build failure -- NOT retrying =====\n' | tee -a "$DEPS_LOG"
      printf 'conan would re-run the same recipe and print the same error; attempts 2..%d would only cost time.\n' \
             "$MAX_ATTEMPTS" | tee -a "$DEPS_LOG"
      printf -- '---- error lines ----\n' | tee -a "$DEPS_LOG"
      printf '%s\n' "$attempt_out" | grep -aE "$DETERMINISTIC_RE" | tail -n 15 | tee -a "$DEPS_LOG"
      printf -- '--------------------\n' | tee -a "$DEPS_LOG"
      printf 'full log : %s\n' "$DEPS_LOG" >&2
      printf 'retry any: AOI_RETRY_ALL=1 bash scripts/build-debug.sh deps\n' >&2
      return 1
    fi

    printf '\n===== attempt %d failed (%s), retry in %ss =====\n' "$i" "$kind" "$RETRY_WAIT" | tee -a "$DEPS_LOG"
    sleep "$RETRY_WAIT"
  done

  printf 'ERROR: deps gave up after %d attempts. See %s\n' "$MAX_ATTEMPTS" "$DEPS_LOG" >&2
  return 1
}

# Build the private VTK package that the Shrimp target links.
#
# ConanCenter has no VTK recipe in the configuration this project needs (shared,
# Qt-enabled, same MSVC / Qt ABI), so conan/recipes/vtk builds it from source and
# publishes it as vtk/9.5.0@aoi/stable.  It is by far the longest step in the
# pipeline (~26 minutes measured on this machine), hence the skip when the cache
# already holds a matching Debug package (AOI_FORCE_VTK=1 rebuilds it).
#
# "Matching" means matching the recipe revision conan.lock pins, not merely
# "a Debug vtk exists".  Conan keys binaries by (recipe revision, package id),
# so a rebuilt recipe leaves the previous binary in the cache under a revision
# nothing resolves any more: the old revision-blind test answered yes and
# skipped the rebuild the new lockfile required, and the failure then surfaced
# much later in `deps` as "Package 'vtk/9.5.0@aoi/stable' not resolved".
stage_vtk() {
  say "vtk: conan create conan/recipes/vtk -> vtk/9.5.0@aoi/stable (Debug)"
  require_conan

  # Both files Conan reads out of this repository.  Each has been CRLF in the
  # working tree at some point while git stored LF for it.
  normalize_recipe_eol "$ROOT_UNIX/conan/recipes/vtk/conanfile.py" \
                       "$ROOT_UNIX/conanfile.py" || return 1

  # Export first and compare with the lockfile, so a mismatch costs a second
  # instead of a compile: `conan export` is idempotent and prints the revision
  # whether or not it is already in the cache.
  local export_log="$LOG_DIR/vtk-export.log" locked_rev disk_rev export_rc=0
  # Capture the status with `|| rc=$?` rather than `if ! cmd`: inside the then
  # block of `if ! cmd` the `!` has already reset $? to 0, so the message below
  # would have reported a failure as "exit 0".
  "$AOI_CONAN" export "$ROOT/conan/recipes/vtk" \
      --user=aoi --channel=stable --no-remote > "$export_log" 2>&1 || export_rc=$?
  if [ "$export_rc" -ne 0 ]; then
    printf 'ERROR: conan export failed (exit %d). See %s\n' "$export_rc" "$export_log" >&2
    tail -n 5 "$export_log" >&2
    return 1
  fi
  disk_rev="$(sed -n 's/^Exported: vtk\/9\.5\.0@aoi\/stable#\([0-9a-f]*\) .*/\1/p' \
                  "$export_log" | tail -1)"
  locked_rev="$(locked_vtk_revision)"

  # A guard that cannot read one of the two revisions must stop the stage, not
  # wave it through: `conan create` would then run for ~26 minutes and anything
  # that depends on the revision would be decided by accident.
  if [ -z "$locked_rev" ]; then
    printf 'ERROR: conan.lock has no vtk/9.5.0@aoi/stable revision to compare against.\n' >&2
    printf '       A lockfile resolved with -o "&:with_shrimp=True" always has one, so\n' >&2
    printf '       this file is not the lockfile the build expects -- regenerate it as\n' >&2
    printf '       docs/build.md describes ("Editing this recipe invalidates conan.lock").\n' >&2
    return 1
  fi
  if [ -z "$disk_rev" ]; then
    printf 'ERROR: could not read the exported revision out of %s.\n' "$export_log" >&2
    printf '       Expected a line "Exported: vtk/9.5.0@aoi/stable#<revision>".\n' >&2
    tail -n 5 "$export_log" >&2
    return 1
  fi

  if [ "$locked_rev" != "$disk_rev" ]; then
    printf '\nERROR: conan.lock pins a different vtk recipe revision than this worktree.\n' >&2
    printf '       conan.lock : vtk/9.5.0@aoi/stable#%s\n' "$locked_rev" >&2
    printf '       this tree  : vtk/9.5.0@aoi/stable#%s\n' "$disk_rev" >&2
    printf '       The revision is a checksum of conan/recipes/vtk/conanfile.py as it sits\n' >&2
    printf '       on disk, so a real edit to that file lands here -- regenerate the\n' >&2
    printf '       lockfile as docs/build.md describes ("Editing this recipe invalidates\n' >&2
    printf '       conan.lock"), after this stage has built the new package.\n' >&2
    printf '       If the recipe was NOT edited, the worktree had stray CRLF line endings:\n' >&2
    printf '       this stage strips them before exporting, so simply re-run it.\n' >&2
    return 1
  fi
  printf 'recipe revision: %s (matches conan.lock)\n' "$disk_rev"

  # Only the locked revision counts as "already built".
  if [ "$AOI_FORCE_VTK" != "1" ]; then
    if "$AOI_CONAN" list "vtk/9.5.0@aoi/stable#${locked_rev}:*" -c 2>/dev/null | grep -q 'build_type: Debug'; then
      printf 'vtk/9.5.0@aoi/stable#%s (Debug) is already in the cache -- skipping.\n' "${locked_rev:0:12}"
      printf 'AOI_FORCE_VTK=1 rebuilds it.\n'
      return 0
    fi
  fi
  local -a remote_flag=()
  [ "$AOI_OFFLINE" = "1" ] && remote_flag=(--no-remote)
  local rc
  "$AOI_CONAN" create "$ROOT/conan/recipes/vtk" --user=aoi --channel=stable \
      "${remote_flag[@]}" \
      -pr:h="$PROFILE" -pr:b="$PROFILE" \
      -s:h build_type=Debug 2>&1 | tee "$LOG_DIR/vtk-create-debug.log"
  rc=${PIPESTATUS[0]}
  [ "$rc" -eq 0 ] || printf 'ERROR: conan create failed (exit %d). See %s\n' "$rc" "$LOG_DIR/vtk-create-debug.log" >&2
  return "$rc"
}

stage_configure() {
  say "configure: cmake --preset debug"
  require_toolchain
  local -a extra=()
  [ -n "$ENABLE_TESTS" ] && extra+=("-DAOI_BUILD_TASKCONTROL_TESTS=$ENABLE_TESTS")
  # Pass the option explicitly in BOTH directions.  AOI_BUILD_SHRIMP defaults to
  # ON in the top-level CMakeLists, so simply omitting the flag for
  # AOI_WITH_SHRIMP=0 leaves Shrimp enabled and the configure step then dies in
  # the find_package(VTK) guard -- i.e. the core-only loop could not be
  # configured at all.  The two switches have to be turned together or not at
  # all; saying it out loud here is what keeps them in step.
  if [ "$AOI_WITH_SHRIMP" = "1" ]; then
    extra+=("-DAOI_BUILD_SHRIMP=ON")
    printf 'shrimp: ON  (-DAOI_BUILD_SHRIMP=ON; requires the vtk stage)\n'
  else
    extra+=("-DAOI_BUILD_SHRIMP=OFF")
    printf 'shrimp: OFF (-DAOI_BUILD_SHRIMP=OFF; core-only build, no VTK)\n'
  fi
  "$AOI_CMAKE" --preset debug "${extra[@]}"
}

stage_build() {
  say "build: cmake --build --preset build-debug"
  require_toolchain
  if [ -n "$BUILD_JOBS" ]; then
    "$AOI_CMAKE" --build --preset build-debug --parallel "$BUILD_JOBS"
  else
    "$AOI_CMAKE" --build --preset build-debug
  fi
}

stage_test() {
  say "test: ctest"
  require_toolchain
  if [ ! -d "$BUILD_DIR" ]; then
    printf 'ERROR: %s does not exist. Run the configure stage first.\n' "$BUILD_DIR" >&2
    return 1
  fi
  "$AOI_CTEST" --test-dir "$BUILD_DIR" --build-config Debug --output-on-failure
}

# Prove the application actually runs, not merely that it links.
#
# Shrimp has a --selftest entry point (Shrimp/main.cpp) that builds a synthetic
# point cloud, drives the real "frame a ROI -> measure -> fill the result table"
# path through PointCloudWidget, and prints the table's row count.  It is the
# only end-to-end check of the GUI that does not need somebody clicking, so the
# driver runs it and fails the stage when the pipeline produced no rows --
# which is exactly what a missing VTK/Qt runtime DLL or a broken OpenGL context
# would cause.
stage_run() {
  say "run: Shrimp.exe --selftest"
  local exe="$BUILD_DIR/bin/Shrimp.exe"
  if [ ! -f "$exe" ]; then
    printf 'ERROR: %s not found.\n' "$exe" >&2
    printf '       Shrimp is built when AOI_WITH_SHRIMP=1; run "vtk configure build" first.\n' >&2
    return 1
  fi
  local out rc
  out="$("$exe" --selftest 2>&1)"
  rc=$?
  printf '%s\n' "$out"
  if ! printf '%s\n' "$out" | grep -q 'END-TO-END PASS'; then
    printf 'ERROR: Shrimp --selftest did not pass (exit %d).\n' "$rc" >&2
    return 1
  fi
  return 0
}

stage_package() {
  say "package: conan cache save"
  bash "$SCRIPT_DIR/package-debug-cache.sh"
}

# --- main ------------------------------------------------------------------

if [ "$#" -eq 0 ]; then
  set -- vtk deps configure build test run package
fi

rc=0
for stage in "$@"; do
  case "$stage" in
    vtk)       stage_vtk       || { rc=$?; break; } ;;
    deps)      stage_deps      || { rc=$?; break; } ;;
    configure) stage_configure || { rc=$?; break; } ;;
    build)     stage_build     || { rc=$?; break; } ;;
    test)      stage_test      || { rc=$?; break; } ;;
    run)       stage_run       || { rc=$?; break; } ;;
    package)   stage_package   || { rc=$?; break; } ;;
    all)       for s in vtk deps configure build test run package; do
                 "stage_$s" || { rc=$?; break 2; }
               done ;;
    *) printf 'ERROR: unknown stage "%s"\n' "$stage" >&2; exit 2 ;;
  esac
done

if [ "$rc" -eq 0 ]; then
  say "DONE — debug build pipeline succeeded"
else
  say "FAILED at the stage above (exit $rc)"
fi
exit "$rc"
