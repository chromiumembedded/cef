#!/bin/bash
# CEF Local Build Pipeline
# Usage:
#   ./tools/claude/build.sh setup    # First-time setup: fetch depot_tools + chromium
#   ./tools/claude/build.sh build    # Build CEF (debug)
#   ./tools/claude/build.sh release  # Build CEF (release)
#   ./tools/claude/build.sh test     # Run ceftests
#   ./tools/claude/build.sh quick    # Incremental build + test (fastest iteration)
#   ./tools/claude/build.sh check    # Syntax/compilation check only (no link)
#   ./tools/claude/build.sh all      # Full build + test

set -euo pipefail

CEF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# Standard CEF build expects:
#   chromium_src/
#   chromium_src/cef/   (symlinked or cloned)
#   chromium_src/out/Debug_GN_x64/
CHROMIUM_DIR="${CHROMIUM_DIR:-$(dirname "$CEF_DIR")}"
DEPOT_TOOLS="${DEPOT_TOOLS:-$HOME/depot_tools}"
BUILD_DIR_DEBUG="${CHROMIUM_DIR}/out/Debug_GN_x64"
BUILD_DIR_RELEASE="${CHROMIUM_DIR}/out/Release_GN_x64"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 8)}"

log() { echo "==> $*"; }
err() { echo "ERROR: $*" >&2; exit 1; }

ensure_depot_tools() {
  if [ ! -d "$DEPOT_TOOLS" ]; then
    log "Fetching depot_tools..."
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS"
  fi
  export PATH="$DEPOT_TOOLS:$PATH"
}

cmd_setup() {
  log "Setting up CEF build environment"
  ensure_depot_tools

  log "Running automate-git.py to fetch/update Chromium + build..."
  log "This will take a LONG time on first run (downloading ~30GB of source)"
  log ""
  log "If you already have a Chromium checkout, set CHROMIUM_DIR to point to it"
  log "and ensure cef/ is symlinked inside it."
  log ""

  # Create the build directories
  mkdir -p "$BUILD_DIR_DEBUG"

  # Verify the CEF directory is accessible from Chromium
  if [ ! -d "$CHROMIUM_DIR/cef" ] && [ ! -L "$CHROMIUM_DIR/cef" ]; then
    if [ "$CHROMIUM_DIR" != "$(dirname "$CEF_DIR")" ]; then
      log "Symlinking $CEF_DIR -> $CHROMIUM_DIR/cef"
      ln -sf "$CEF_DIR" "$CHROMIUM_DIR/cef"
    fi
  fi

  # Write args.gn for debug build
  cat > "$BUILD_DIR_DEBUG/args.gn" << 'ARGS'
is_debug = true
is_component_build = true
symbol_level = 1
dcheck_always_on = true
use_sysroot = true
use_allocator = "none"
treat_warnings_as_errors = false
ARGS

  log "Running GN gen..."
  if command -v gn &>/dev/null; then
    gn gen "$BUILD_DIR_DEBUG"
  else
    log "gn not found. Ensure depot_tools is in PATH or run:"
    log "  export PATH=\$HOME/depot_tools:\$PATH"
    log "  gn gen $BUILD_DIR_DEBUG"
  fi

  log "Setup complete. Run: ./tools/claude/build.sh build"
}

cmd_build() {
  ensure_depot_tools
  log "Building CEF (debug)..."

  if [ ! -f "$BUILD_DIR_DEBUG/args.gn" ]; then
    err "Build directory not set up. Run: ./tools/claude/build.sh setup"
  fi

  autoninja -C "$BUILD_DIR_DEBUG" cef cefclient cefsimple ceftests -j "$JOBS"
  log "Build complete."
}

cmd_release() {
  ensure_depot_tools
  log "Building CEF (release)..."

  mkdir -p "$BUILD_DIR_RELEASE"
  if [ ! -f "$BUILD_DIR_RELEASE/args.gn" ]; then
    cat > "$BUILD_DIR_RELEASE/args.gn" << 'ARGS'
is_debug = false
is_component_build = false
symbol_level = 0
is_official_build = true
use_sysroot = true
treat_warnings_as_errors = false
ARGS
    gn gen "$BUILD_DIR_RELEASE"
  fi

  autoninja -C "$BUILD_DIR_RELEASE" cef cefclient cefsimple -j "$JOBS"
  log "Release build complete."
}

cmd_test() {
  log "Running ceftests..."

  local test_binary="$BUILD_DIR_DEBUG/ceftests"
  if [ ! -f "$test_binary" ]; then
    err "ceftests binary not found. Run: ./tools/claude/build.sh build"
  fi

  # Run with gtest filter if provided
  local filter="${1:-*}"
  "$test_binary" --gtest_filter="$filter" --no-sandbox 2>&1 | tee /tmp/ceftests.log

  log "Test results saved to /tmp/ceftests.log"
}

cmd_test_perf() {
  log "Running performance optimization tests only..."

  local test_binary="$BUILD_DIR_DEBUG/ceftests"
  if [ ! -f "$test_binary" ]; then
    err "ceftests binary not found. Run: ./tools/claude/build.sh build"
  fi

  "$test_binary" \
    --gtest_filter="StateJournal.*:PageModelCache.*:AgentScheduler.*:SecurityPolicy.*:DirtyTracking.*:SessionPool.*" \
    --no-sandbox 2>&1 | tee /tmp/ceftests-perf.log

  log "Results saved to /tmp/ceftests-perf.log"
}

cmd_quick() {
  ensure_depot_tools
  log "Quick incremental build + test..."

  # Build only ceftests target for fastest iteration
  autoninja -C "$BUILD_DIR_DEBUG" ceftests -j "$JOBS"
  cmd_test_perf
}

cmd_check() {
  ensure_depot_tools
  log "Compilation check (no link)..."

  # Build just the object files for our new sources
  local new_files=(
    "obj/cef/libcef/browser/agent_scheduler.o"
    "obj/cef/libcef/browser/automation_program_impl.o"
    "obj/cef/libcef/browser/page_model_cache.o"
    "obj/cef/libcef/browser/session_pool.o"
    "obj/cef/libcef/browser/state_journal.o"
  )

  autoninja -C "$BUILD_DIR_DEBUG" "${new_files[@]}" -j "$JOBS" 2>&1 || true
  log "Check complete."
}

cmd_all() {
  cmd_build
  cmd_test
}

cmd_help() {
  echo "CEF Local Build Pipeline"
  echo ""
  echo "Usage: ./tools/claude/build.sh <command> [args]"
  echo ""
  echo "Commands:"
  echo "  setup      First-time setup (fetch depot_tools, create build dirs)"
  echo "  build      Debug build (cef + cefclient + cefsimple + ceftests)"
  echo "  release    Release/official build"
  echo "  test [F]   Run ceftests (optional gtest filter F)"
  echo "  test-perf  Run only performance optimization tests"
  echo "  quick      Incremental build + run perf tests (fastest iteration)"
  echo "  check      Compile-only check for new files (no link)"
  echo "  all        Full build + test"
  echo "  help       This message"
  echo ""
  echo "Environment variables:"
  echo "  CHROMIUM_DIR  Path to Chromium source (default: parent of CEF dir)"
  echo "  DEPOT_TOOLS   Path to depot_tools (default: ~/depot_tools)"
  echo "  JOBS          Parallel build jobs (default: nproc)"
}

# Main dispatch
case "${1:-help}" in
  setup)     cmd_setup ;;
  build)     cmd_build ;;
  release)   cmd_release ;;
  test)      cmd_test "${2:-*}" ;;
  test-perf) cmd_test_perf ;;
  quick)     cmd_quick ;;
  check)     cmd_check ;;
  all)       cmd_all ;;
  help|*)    cmd_help ;;
esac
