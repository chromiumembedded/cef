#!/bin/bash
# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

# Builds only the generated targets needed for CEF (not all of Chromium).
# This avoids building ~39k targets and focuses on ~9k that CEF actually uses.
#
# Usage: Run from chromium/src directory
#   ./cef/tools/clang/scripts/build_cef_gen_targets.sh [build_dir]
#
# Arguments:
#   build_dir - Build directory (default: out/ClangTool)

set -e

BUILD_DIR="${1:-out/ClangTool}"

if [ ! -d "$BUILD_DIR" ]; then
  echo "Error: Build directory '$BUILD_DIR' not found"
  echo "Run 'gn gen $BUILD_DIR' first"
  exit 1
fi

echo "Finding CEF-related gen targets..."

# Use ninja to trace inputs needed for cef target, filter to gen/ source/header files
ninja -C "$BUILD_DIR" -t inputs cef 2>/dev/null \
  | grep '^gen/' \
  | grep -E '\.(h|cc|hpp|cpp|c)$' \
  | sort -u > /tmp/cef_gen_targets.txt

TARGET_COUNT=$(wc -l < /tmp/cef_gen_targets.txt | tr -d ' ')
echo "Found $TARGET_COUNT CEF-related gen targets (vs ~39k for all of Chromium)"

echo "Building cef_make_headers target..."
ninja -C "$BUILD_DIR" cef_make_headers

echo "Building Chromium buildflags targets needed by CEF..."
# These buildflags headers are included by Chromium headers that CEF uses.
# The ninja -t inputs command doesn't catch them because they're transitive
# dependencies through Chromium code, not direct CEF dependencies.
ninja -C "$BUILD_DIR" \
  gen/chrome/browser/buildflags.h \
  gen/chrome/services/speech/buildflags/buildflags.h \
  gen/components/guest_view/buildflags/buildflags.h \
  gen/components/lens/buildflags.h \
  gen/components/paint_preview/buildflags/buildflags.h \
  gen/components/spellcheck/spellcheck_buildflags.h \
  gen/components/variations/service/buildflags.h \
  gen/ui/views/buildflags.h

echo "Building gen targets..."
cat /tmp/cef_gen_targets.txt | xargs ninja -C "$BUILD_DIR"

echo "Done. Generated files are ready for clang tooling."
