# Project scripts

Run the shell scripts from **Git Bash**. They share the VS2022, CMake, Ninja
and Conan discovery in `_env.sh`; that file is sourced by the entry points and
is not intended to be run directly.

## Daily development

| Entry point | Purpose |
|---|---|
| `build-debug.sh` | Build the Debug VTK dependency and the project. Individual stages can be named, for example `bash scripts/build-debug.sh deps configure build test run`. |
| `prefetch-sources.sh` | Warm Conan's sources cache before an offline build. |
| `restore_plaintext_from_git.py` | Restore Git-tracked UTF-8 source files that endpoint encryption has replaced with ciphertext. Use `--check` first. |

## Conan cache delivery

| Entry point | Purpose |
|---|---|
| `package-cache.sh` | Create the Debug cache bundle; set `AOI_CACHE_CONFIG=release` for Release. |
| `verify-bundle.sh` | Restore a bundle into an empty cache, then build, test and run it. Set `AOI_CACHE_CONFIG=release` for Release. |
| `upload-cache.sh` | Upload packages recorded in `conan/lists/pkglist-<config>.json` to a Conan remote. |
| `pkglist-refs.py` | Convert a package-list JSON file to exact Conan references. Used by `upload-cache.sh`. |
| `normalize-cache-reparse.py` | Normalize cache reparse points before `conan cache save`. Called by `package-cache.sh`. |

## Compatibility entry points

`package-debug-cache.sh` and `verify-debug-bundle.sh` remain as Debug-only
wrappers for existing local commands. New documentation and automation should
use `package-cache.sh` and `verify-bundle.sh` so Debug and Release follow the
same path.

For exact toolchain requirements, cache hand-off rules and troubleshooting, see
[`docs/build.md`](../docs/build.md).
