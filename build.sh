#!/usr/bin/env bash
# build.sh — configure and build SdrTaskApi
#
# SdrTaskApi has no sibling-repo dependencies of its own; this script just
# provides a consistent entry point that matches the pattern used by
# SdrResourceManager and AcquisitionApp.
#
# Usage:
#   ./build.sh                  # Release build
#   ./build.sh --debug          # Debug build (ASan + UBSan)
#   ./build.sh --clean          # Remove build/ then rebuild
#   ./build.sh --tests          # Build and run unit tests
#   ./build.sh --debug --tests  # Flags may be combined
#   ./build.sh --help

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${REPO_DIR}/build"
BUILD_TYPE="Release"
RUN_TESTS=0
CLEAN=0

# ── Argument parsing ──────────────────────────────────────────────────────────
for arg in "$@"; do
    case "$arg" in
        --debug)       BUILD_TYPE="Debug" ;;
        --tests|-t)    RUN_TESTS=1 ;;
        --clean|-c)    CLEAN=1 ;;
        --help|-h)
            echo "Usage: $0 [--debug] [--clean] [--tests]"
            echo ""
            echo "  --debug    Build with Debug mode (AddressSanitizer + UBSan)"
            echo "  --clean    Delete build/ before configuring"
            echo "  --tests    Build and run unit tests after a successful build"
            exit 0
            ;;
        *)
            echo "Unknown argument: $arg  (use --help for usage)" >&2
            exit 1
            ;;
    esac
done

# ── Clean ─────────────────────────────────────────────────────────────────────
if [[ $CLEAN -eq 1 && -d "$BUILD_DIR" ]]; then
    echo "[SdrTaskApi] Removing existing build directory..."
    rm -rf "$BUILD_DIR"
fi

# ── Configure ─────────────────────────────────────────────────────────────────
echo ""
echo "╔══════════════════════════════════════════════╗"
echo "║  SdrTaskApi build                            ║"
echo "║  Build type : $BUILD_TYPE                    "
echo "╚══════════════════════════════════════════════╝"
echo ""

cmake -B "$BUILD_DIR" -S "$REPO_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

# ── Build ─────────────────────────────────────────────────────────────────────
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo ""
echo "[SdrTaskApi] Build complete → $BUILD_DIR"

# ── Tests ─────────────────────────────────────────────────────────────────────
if [[ $RUN_TESTS -eq 1 ]]; then
    echo ""
    echo "[SdrTaskApi] Running unit tests..."
    ctest --test-dir "$BUILD_DIR" --output-on-failure -V
    echo "[SdrTaskApi] All tests passed."
fi
