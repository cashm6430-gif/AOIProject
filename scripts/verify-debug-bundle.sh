#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# AOIProject - verify the **Debug** bundle (thin wrapper).
#
# The implementation moved to scripts/verify-bundle.sh when the Release snapshot
# became necessary, so that both configurations share one copy of the
# empty-home / no-remote / no-build rules and of the fresh-tree build check.
# This name is kept because docs/build.md and habit refer to it.
#
#   bash scripts/verify-bundle.sh                            # Debug (default)
#   AOI_CACHE_CONFIG=release bash scripts/verify-bundle.sh   # Release
# ---------------------------------------------------------------------------
exec env AOI_CACHE_CONFIG=debug bash "$(dirname "${BASH_SOURCE[0]}")/verify-bundle.sh" "$@"
