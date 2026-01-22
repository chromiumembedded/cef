#!/usr/bin/env python3
# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""Standalone C++ rewriter runner for any project.

This is a simplified wrapper that runs cpp_rewriter on any C++ project with
a compile_commands.json file.

Usage:
    # Run on all files in compile_commands.json
    python3 run_rewriter.py -p /path/to/build

    # Run on specific source directories
    python3 run_rewriter.py -p /path/to/build src/ include/

    # Filter by path pattern (regex)
    python3 run_rewriter.py -p /path/to/build --path-filter "src/core/.*"

    # Disable specific transformations
    python3 run_rewriter.py -p /path/to/build --tool-args="--structured-bindings=false"

    # Pipe output to apply_edits.py
    python3 run_rewriter.py -p /path/to/build src/ | python3 apply_edits.py --base-dir /path/to/build
"""

import argparse
import json
import multiprocessing
import os
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor, as_completed


def _get_script_dir():
    """Returns the directory containing this script."""
    return os.path.dirname(os.path.realpath(__file__))


def _get_tool_path():
    """Returns the path to the cpp_rewriter binary."""
    script_dir = _get_script_dir()
    # Try ../bin/cpp_rewriter (standalone distribution)
    standalone_path = os.path.join(script_dir, '..', 'bin', 'cpp_rewriter')
    if os.path.exists(standalone_path):
        return os.path.realpath(standalone_path)

    # Try the original name (when running from source)
    cef_path = os.path.join(script_dir, '..', 'bin', 'cef_cpp_rewriter')
    if os.path.exists(cef_path):
        return os.path.realpath(cef_path)

    return None


def _get_resource_dir():
    """Returns the path to the clang resource directory (for built-in headers)."""
    script_dir = _get_script_dir()
    # Look for ../lib/clang/<version>/ in standalone distribution
    lib_clang = os.path.join(script_dir, '..', 'lib', 'clang')
    if os.path.isdir(lib_clang):
        versions = os.listdir(lib_clang)
        if versions:
            return os.path.realpath(os.path.join(lib_clang, versions[0]))
    return None


def _load_compile_commands(build_dir):
    """Load and parse compile_commands.json."""
    compile_db_path = os.path.join(build_dir, 'compile_commands.json')
    if not os.path.exists(compile_db_path):
        return None
    with open(compile_db_path, 'r') as f:
        return json.load(f)


def _filter_entries(entries, path_filters=None):
    """Filter compile_commands entries by optional path prefixes.

    Args:
        entries: List of compile_commands.json entries
        path_filters: Optional list of path prefixes to filter by.
                     If None, includes all files.
    """
    if not path_filters:
        return entries

    filtered = []
    for entry in entries:
        file_path = entry.get('file', '')
        directory = entry.get('directory', '')

        # Get the full path
        if os.path.isabs(file_path):
            full_path = os.path.realpath(file_path)
        else:
            full_path = os.path.realpath(os.path.join(directory, file_path))

        # Check path filters
        matches_filter = any(pf in full_path for pf in path_filters)
        if matches_filter:
            filtered.append(entry)

    return filtered


def _run_tool_on_file(args):
    """Run the clang tool on a single file. Used by process pool."""
    tool_path, build_dir, file_path, tool_args, path_filter, resource_dir = args

    cmd = [tool_path, '-p', build_dir]

    # Add path filter if specified
    if path_filter:
        cmd.extend(['--path-filter', path_filter])
    else:
        # No filter means process all files
        cmd.append('--disable-path-filter')

    if tool_args:
        cmd.extend(tool_args.split())

    # Add resource directory for clang built-in headers (stdarg.h, etc.)
    if resource_dir:
        cmd.extend(['--extra-arg', f'-resource-dir={resource_dir}'])

    cmd.append(file_path)

    try:
        result = subprocess.run(cmd, capture_output=True, timeout=120)
        stdout = result.stdout.decode('utf-8', errors='replace') if result.stdout else ''
        stderr = result.stderr.decode('utf-8', errors='replace') if result.stderr else ''
        return stdout, stderr, result.returncode
    except subprocess.TimeoutExpired:
        return '', f'Timeout processing {file_path}', 1
    except Exception as e:
        return '', f'Error processing {file_path}: {e}', 1


def main(argv):
    parser = argparse.ArgumentParser(
        description='Run C++ rewriter on source files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)
    parser.add_argument('-p', '--build-dir', required=True,
                        help='Build directory containing compile_commands.json')
    parser.add_argument('--path-filter', default='',
                        help='Regex pattern to filter files by path')
    parser.add_argument('-j', '--jobs', type=int,
                        default=multiprocessing.cpu_count(),
                        help='Number of parallel jobs')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Print progress to stderr')
    parser.add_argument('--tool-args', default='',
                        help='Additional arguments to pass to the tool')
    parser.add_argument('paths', nargs='*',
                        help='Optional path filters (substring match). '
                             'If not specified, processes all files.')
    args = parser.parse_args(argv)

    tool_path = _get_tool_path()
    if not tool_path:
        print('Error: cpp_rewriter binary not found', file=sys.stderr)
        print('Expected at: scripts/../bin/cpp_rewriter', file=sys.stderr)
        return 1

    # Get resource directory for clang built-in headers
    resource_dir = _get_resource_dir()
    if args.verbose and resource_dir:
        print(f'Using clang resource directory: {resource_dir}', file=sys.stderr)

    # Make build_dir absolute
    if not os.path.isabs(args.build_dir):
        args.build_dir = os.path.realpath(args.build_dir)

    # Load compile_commands.json
    entries = _load_compile_commands(args.build_dir)
    if entries is None:
        print(f'Error: compile_commands.json not found in {args.build_dir}',
              file=sys.stderr)
        print('Generate it with:', file=sys.stderr)
        print('  CMake: cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..', file=sys.stderr)
        print('  Bear:  bear -- make', file=sys.stderr)
        return 1

    # Filter entries by path if specified
    filtered_entries = _filter_entries(entries, args.paths if args.paths else None)

    if not filtered_entries:
        if args.paths:
            print(f'No files found matching: {" ".join(args.paths)}',
                  file=sys.stderr)
        else:
            print('No files found in compile_commands.json', file=sys.stderr)
        return 1

    if args.verbose:
        filter_msg = f' (filtered by {" ".join(args.paths)})' if args.paths else ''
        print(f'Found {len(filtered_entries)} files to process{filter_msg}',
              file=sys.stderr)

    # Get unique file paths
    file_paths = []
    seen = set()
    for entry in filtered_entries:
        file_path = entry.get('file', '')
        directory = entry.get('directory', '')
        if not os.path.isabs(file_path):
            file_path = os.path.join(directory, file_path)
        file_path = os.path.realpath(file_path)
        if file_path not in seen:
            seen.add(file_path)
            file_paths.append(file_path)

    if args.verbose:
        print(f'Processing {len(file_paths)} unique files with {args.jobs} workers',
              file=sys.stderr)

    # Prepare work items
    work_items = [(tool_path, args.build_dir, f, args.tool_args, args.path_filter, resource_dir)
                  for f in file_paths]

    # Process files in parallel
    completed = 0
    errors = 0

    with ProcessPoolExecutor(max_workers=args.jobs) as executor:
        futures = {executor.submit(_run_tool_on_file, item): item
                   for item in work_items}

        for future in as_completed(futures):
            stdout, stderr, returncode = future.result()
            completed += 1

            if stdout:
                print(stdout, end='')

            if returncode != 0:
                errors += 1
                if stderr and args.verbose:
                    print(stderr, file=sys.stderr)

            if args.verbose and completed % 100 == 0:
                print(f'Progress: {completed}/{len(file_paths)} files',
                      file=sys.stderr)

    if args.verbose:
        print(f'Completed: {completed} files, {errors} errors', file=sys.stderr)

    return 0 if errors == 0 else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
