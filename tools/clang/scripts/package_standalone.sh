#!/bin/bash
# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

# Package the C++ rewriter tool as a standalone distribution.
#
# Usage:
#   ./package_standalone.sh /path/to/target/directory
#
# This creates a standalone distribution with:
#   <target>/
#   ├── bin/
#   │   └── cpp_rewriter           # The compiled tool binary
#   ├── scripts/
#   │   ├── apply_edits.py
#   │   ├── process_edits.py
#   │   └── run_rewriter.py
#   └── README.md

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CEF_CLANG_DIR="$(dirname "$SCRIPT_DIR")"
CHROMIUM_SRC="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

# Check arguments
if [ -z "$1" ]; then
    echo "Usage: $0 <target_directory>"
    echo ""
    echo "Example:"
    echo "  $0 ~/my-project/cpp_rewriter"
    exit 1
fi

TARGET_DIR="$1"

# Check if the tool binary exists
TOOL_BINARY="$CHROMIUM_SRC/third_party/llvm-build/Release+Asserts/bin/cef_cpp_rewriter"
if [ ! -f "$TOOL_BINARY" ]; then
    echo "Error: Tool binary not found at:"
    echo "  $TOOL_BINARY"
    echo ""
    echo "Build it first with:"
    echo "  ./cef/tools/clang/scripts/build_clang_rewriter.sh"
    exit 1
fi

# Find the clang resource directory (needed for built-in headers like stdarg.h)
CLANG_RESOURCE_DIR="$CHROMIUM_SRC/third_party/llvm-build/Release+Asserts/lib/clang"
CLANG_VERSION=$(ls "$CLANG_RESOURCE_DIR" | head -1)
if [ -z "$CLANG_VERSION" ]; then
    echo "Error: Could not find clang version in $CLANG_RESOURCE_DIR"
    exit 1
fi

# Create target directory structure
echo "Creating standalone distribution at: $TARGET_DIR"
mkdir -p "$TARGET_DIR/bin"
mkdir -p "$TARGET_DIR/scripts"
mkdir -p "$TARGET_DIR/lib/clang/$CLANG_VERSION"

# Copy the binary (renamed to cpp_rewriter)
echo "  Copying binary..."
cp "$TOOL_BINARY" "$TARGET_DIR/bin/cpp_rewriter"

# Copy the clang resource directory (headers needed for parsing)
echo "  Copying clang headers (lib/clang/$CLANG_VERSION/)..."
cp -R "$CLANG_RESOURCE_DIR/$CLANG_VERSION/include" "$TARGET_DIR/lib/clang/$CLANG_VERSION/"

# Copy scripts
echo "  Copying scripts..."
cp "$CEF_CLANG_DIR/standalone/run_rewriter.py" "$TARGET_DIR/scripts/"
cp "$SCRIPT_DIR/apply_edits.py" "$TARGET_DIR/scripts/"
cp "$SCRIPT_DIR/process_edits.py" "$TARGET_DIR/scripts/"

# Make scripts executable
chmod +x "$TARGET_DIR/scripts/"*.py

# Copy documentation
echo "  Copying documentation..."
cp "$CEF_CLANG_DIR/standalone/README.md" "$TARGET_DIR/"
cp "$CEF_CLANG_DIR/TRANSFORMS.md" "$TARGET_DIR/"

echo ""
echo "Done! Standalone distribution created at: $TARGET_DIR"
echo ""
echo "Usage:"
echo "  cd $TARGET_DIR"
echo "  ./scripts/run_rewriter.py -p /path/to/build src/ | ./scripts/apply_edits.py --base-dir /path/to/build"
echo ""
echo "See README.md for full documentation."
