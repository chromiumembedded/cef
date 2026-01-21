#!/usr/bin/env python3
# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""
Simple edit applicator for cef_cpp_rewriter output.

Unlike Chromium's apply_edits.py, this script:
- Does not require git
- Works with relative or absolute paths
- Can resolve files relative to a base directory

Usage:
  # Apply edits from stdin, resolving relative paths from current directory
  cat edits.txt | python3 apply_edits.py

  # Apply edits with a base directory for relative paths
  cat edits.txt | python3 apply_edits.py --base-dir /path/to/chromium/src

  # Apply edits from file
  python3 apply_edits.py edits.txt --base-dir /path/to/chromium/src

The input format is the standard clang tool edit format:
  r:::<file>:::<offset>:::<length>:::<replacement>
"""

import argparse
import os
import sys
from collections import defaultdict


def parse_edit(line):
    """Parse an edit line into its components."""
    line = line.strip()
    if not line or not line.startswith('r:::'):
        return None

    parts = line.split(':::')
    if len(parts) < 5:
        return None

    replacement = parts[4] if len(parts) == 5 else ':::'.join(parts[4:])
    # Convert null bytes back to newlines (the tool uses \0 to encode \n)
    replacement = replacement.replace('\x00', '\n')

    return {
        'file': parts[1],
        'offset': int(parts[2]),
        'length': int(parts[3]),
        'replacement': replacement
    }


def resolve_path(filepath, base_dir):
    """Resolve a file path, trying various strategies."""
    # If absolute and exists, use it
    if os.path.isabs(filepath) and os.path.exists(filepath):
        return filepath

    # Try relative to base_dir
    if base_dir:
        candidate = os.path.join(base_dir, filepath)
        if os.path.exists(candidate):
            return candidate

    # Try relative to cwd
    if os.path.exists(filepath):
        return filepath

    return None


def apply_edits_to_file(filepath, edits):
    """Apply a list of edits to a single file."""
    with open(filepath, 'r') as f:
        content = f.read()

    # Sort edits by offset in reverse order (apply from end to start)
    sorted_edits = sorted(edits, key=lambda e: e['offset'], reverse=True)

    for edit in sorted_edits:
        offset = edit['offset']
        length = edit['length']
        replacement = edit['replacement']

        # Apply the edit
        content = content[:offset] + replacement + content[offset + length:]

    with open(filepath, 'w') as f:
        f.write(content)

    return len(sorted_edits)


def main():
    parser = argparse.ArgumentParser(description='Apply clang tool edits to files')
    parser.add_argument('input_file', nargs='?', help='Input file with edits (default: stdin)')
    parser.add_argument('--base-dir', '-d', help='Base directory for resolving relative paths')
    args = parser.parse_args()

    # Read edits (as binary to handle null bytes correctly)
    if args.input_file:
        with open(args.input_file, 'rb') as f:
            data = f.read()
    else:
        data = sys.stdin.buffer.read()

    # Split by newlines
    lines = data.decode('utf-8', errors='replace').split('\n')

    # Group edits by file, deduplicating by offset+length+replacement
    edits_by_file = defaultdict(dict)  # file -> {(offset, length, replacement) -> edit}
    for line in lines:
        edit = parse_edit(line)
        if edit:
            key = (edit['offset'], edit['length'], edit['replacement'])
            edits_by_file[edit['file']][key] = edit

    # Convert back to lists
    edits_by_file = {f: list(edits.values()) for f, edits in edits_by_file.items()}

    if not edits_by_file:
        print('No edits to apply', file=sys.stderr)
        return 0

    # Apply edits to each file
    total_edits = 0
    for filepath, edits in edits_by_file.items():
        resolved = resolve_path(filepath, args.base_dir)
        if not resolved:
            print(f'Warning: Could not find file: {filepath}', file=sys.stderr)
            continue

        count = apply_edits_to_file(resolved, edits)
        total_edits += count
        print(f'Applied {count} edits to {resolved}')

    print(f'Total: {total_edits} edits applied to {len(edits_by_file)} files')
    return 0


if __name__ == '__main__':
    sys.exit(main())
