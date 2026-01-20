#!/usr/bin/env python3
# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""
Post-processing script for cef_cpp_rewriter output.

This script:
1. Reads raw edit directives from stdin or files
2. Deduplicates edits (same replacement at same location)
3. Adds BEGIN/END EDITS markers

Usage:
  # Pipe from tool output, then apply with CEF's apply_edits.py
  cef_cpp_rewriter ... | python3 process_edits.py | python3 apply_edits.py --base-dir out/ClangTool

  # Or process saved files
  python3 process_edits.py raw_output1.txt raw_output2.txt > edits.txt
  python3 apply_edits.py edits.txt --base-dir out/ClangTool
"""

import os
import sys


def process_edits(input_lines):
    """Process edit lines, deduplicating and filtering."""
    seen_edits = set()
    edits = []

    for line in input_lines:
        line = line.strip()
        if not line:
            continue

        # Skip non-edit lines (processing messages, errors, etc.)
        if not line.startswith('r:::') and not line.startswith('include-'):
            continue

        parts = line.split(':::')
        if len(parts) < 2:
            continue

        # Keep the path as-is (don't normalize away directory components)
        path = parts[1]

        # Reconstruct the line
        normalized_line = ':::'.join(parts)

        # Deduplicate
        if normalized_line in seen_edits:
            continue
        seen_edits.add(normalized_line)

        edits.append(normalized_line)

    return sorted(edits)


def main():
    # Read from files if provided, otherwise stdin
    if len(sys.argv) > 1:
        lines = []
        for filename in sys.argv[1:]:
            with open(filename) as f:
                lines.extend(f.readlines())
    else:
        lines = sys.stdin.readlines()

    edits = process_edits(lines)

    # Output with markers
    print('==== BEGIN EDITS ====')
    for edit in edits:
        print(edit)
    print('==== END EDITS ====')


if __name__ == '__main__':
    main()
