#!/bin/bash
# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

# Builds the CEF C++20 rewriter tool.
#
# This tool is not regularly tested and may need updates when:
# - Chromium updates its clang tooling infrastructure
# - CEF adopts a new C++ standard (e.g., C++23)
# - LLVM/Clang APIs change
#
# Usage: Run from chromium/src directory
#   ./cef/tools/clang/scripts/build_clang_rewriter.sh

set -e
cd "$(dirname "$0")/../../../.."  # chromium/src

CEF_TOOL_DIR="cef/tools/clang/cef_cpp_rewriter"
CHROMIUM_TOOL_DIR="tools/clang/cef_cpp_rewriter"

if [ ! -d "$CEF_TOOL_DIR" ]; then
  echo "Error: $CEF_TOOL_DIR not found"
  exit 1
fi

# Symlink CEF tool into Chromium's clang tools directory
echo "Symlinking $CEF_TOOL_DIR -> $CHROMIUM_TOOL_DIR"
ln -sfn "$(pwd)/$CEF_TOOL_DIR" "$CHROMIUM_TOOL_DIR"

# Bootstrap clang with the tool (takes ~30 minutes)
echo "Bootstrapping clang with cef_cpp_rewriter..."
python3 tools/clang/scripts/build.py \
  --bootstrap \
  --without-android \
  --without-fuchsia \
  --extra-tools cef_cpp_rewriter

# Create cr_build_revision file if missing (GN requires this)
# The bootstrap doesn't create it, but GN expects it to exist
LLVM_BUILD_DIR="third_party/llvm-build/Release+Asserts"
if [ ! -f "$LLVM_BUILD_DIR/cr_build_revision" ]; then
  echo "Creating cr_build_revision file..."
  # Extract version from update.py
  CLANG_REVISION=$(grep "^CLANG_REVISION = " tools/clang/scripts/update.py | cut -d"'" -f2)
  CLANG_SUB_REVISION=$(grep "^CLANG_SUB_REVISION = " tools/clang/scripts/update.py | awk '{print $3}')
  echo "${CLANG_REVISION}-${CLANG_SUB_REVISION}" > "$LLVM_BUILD_DIR/cr_build_revision"
fi

echo "Done. Tool available at: third_party/llvm-build/Release+Asserts/bin/cef_cpp_rewriter"
