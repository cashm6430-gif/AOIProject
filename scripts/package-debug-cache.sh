#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# AOIProject — pack the **Debug** Conan cache for offline transfer.
#
# Thin wrapper: the implementation moved to scripts/package-cache.sh when the
# Release snapshot became necessary, so that both configurations share one copy
# of the completeness gate, the reparse-point pass and the gzip check.  This
# name is kept because build-debug.sh's `package` stage, docs/build.md and
# whatever calls it by hand all refer to it.
#
#   bash scripts/package-cache.sh                      # Debug (default)
#   bash scripts/package-cache.sh                      # AOI_CACHE_CONFIG=release for Release
# ---------------------------------------------------------------------------
exec env AOI_CACHE_CONFIG=debug bash "$(dirname "${BASH_SOURCE[0]}")/package-cache.sh" "$@"
