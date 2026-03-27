#!/bin/bash
# Run performance optimization tests
# Usage:
#   ./tools/claude/test-perf.sh              # All perf tests
#   ./tools/claude/test-perf.sh StateJournal  # Just journal tests
#   ./tools/claude/test-perf.sh -v           # Verbose output

set -euo pipefail

CEF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CHROMIUM_DIR="${CHROMIUM_DIR:-$(dirname "$CEF_DIR")}"
BUILD_DIR="${CHROMIUM_DIR}/out/Debug_GN_x64"
TEST_BINARY="$BUILD_DIR/ceftests"

PERF_SUITES=(
  "StateJournal"
  "PageModelCache"
  "AgentScheduler"
  "SecurityPolicy"
  "DirtyTracking"
  "SessionPool"
)

if [ ! -f "$TEST_BINARY" ]; then
  echo "ERROR: ceftests not found at $TEST_BINARY"
  echo "Run: ./tools/claude/build.sh build"
  exit 1
fi

VERBOSE=""
SUITE_FILTER=""

for arg in "$@"; do
  case "$arg" in
    -v|--verbose) VERBOSE="--gtest_print_time=1" ;;
    *)
      # Check if it's a known suite
      for suite in "${PERF_SUITES[@]}"; do
        if [ "$arg" = "$suite" ]; then
          SUITE_FILTER="$suite.*"
          break
        fi
      done
      if [ -z "$SUITE_FILTER" ]; then
        SUITE_FILTER="$arg"
      fi
      ;;
  esac
done

if [ -z "$SUITE_FILTER" ]; then
  # All perf suites
  FILTER=$(IFS=":"; echo "${PERF_SUITES[*]/%/.*}")
  FILTER="${FILTER// /}"
else
  FILTER="$SUITE_FILTER"
fi

echo "Running: $TEST_BINARY --gtest_filter=$FILTER $VERBOSE --no-sandbox"
echo "---"

"$TEST_BINARY" --gtest_filter="$FILTER" $VERBOSE --no-sandbox 2>&1

echo "---"
echo "Done."
