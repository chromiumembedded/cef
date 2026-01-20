#!/usr/bin/env python3
# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""Run CEF clang tools on CEF source files.

This is a CEF-specific wrapper that filters compile_commands.json to only
include CEF files, avoiding the git-based file discovery in Chromium's
run_tool.py which doesn't work for CEF (separate git repo).

Usage:
    # From chromium/src directory - run on all CEF files
    python3 cef/tools/clang/scripts/run_cef_tool.py -p out/ClangTool

    # Run on specific directories only
    python3 cef/tools/clang/scripts/run_cef_tool.py -p out/ClangTool cef/libcef cef/tests

    # With specific tool (default: cef_cpp_rewriter)
    python3 cef/tools/clang/scripts/run_cef_tool.py -p out/ClangTool --tool cef_cpp_rewriter

    # Generate compile_commands.json first
    python3 cef/tools/clang/scripts/run_cef_tool.py -p out/ClangTool --generate-compdb
"""

import argparse
import json
import multiprocessing
import os
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor, as_completed


def _get_chromium_src_dir():
    """Returns the chromium/src directory."""
    script_dir = os.path.dirname(os.path.realpath(__file__))
    return os.path.realpath(os.path.join(script_dir, '..', '..', '..', '..'))


def _get_tool_path(chromium_src, tool_name):
    """Returns the path to the built tool binary."""
    return os.path.join(chromium_src, 'third_party', 'llvm-build',
                        'Release+Asserts', 'bin', tool_name)


def _load_compile_commands(build_dir):
    """Load and parse compile_commands.json."""
    compile_db_path = os.path.join(build_dir, 'compile_commands.json')
    if not os.path.exists(compile_db_path):
        return None
    with open(compile_db_path, 'r') as f:
        return json.load(f)


def _filter_cef_entries(entries, path_filters=None):
    """Filter compile_commands entries to only include CEF files.

    Args:
        entries: List of compile_commands.json entries
        path_filters: Optional list of path prefixes to filter by (e.g., ['cef/libcef'])
                     If None, includes all CEF files.
    """
    cef_entries = []

    # Normalize path filters to absolute paths
    if path_filters:
        chromium_src = _get_chromium_src_dir()
        abs_filters = []
        for pf in path_filters:
            if os.path.isabs(pf):
                abs_filters.append(os.path.realpath(pf))
            else:
                abs_filters.append(os.path.realpath(os.path.join(chromium_src, pf)))
    else:
        abs_filters = None

    for entry in entries:
        file_path = entry.get('file', '')
        directory = entry.get('directory', '')

        # Get the full path
        if os.path.isabs(file_path):
            full_path = os.path.realpath(file_path)
        else:
            full_path = os.path.realpath(os.path.join(directory, file_path))

        # Check if it's a CEF file
        if '/cef/' not in full_path and '\\cef\\' not in full_path:
            continue

        # Check path filters if specified
        if abs_filters:
            matches_filter = any(full_path.startswith(pf) for pf in abs_filters)
            if not matches_filter:
                continue

        cef_entries.append(entry)

    return cef_entries


def _run_tool_on_file(args):
    """Run the clang tool on a single file. Used by process pool."""
    tool_path, build_dir, file_path = args

    cmd = [tool_path, '-p', build_dir, file_path]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        return result.stdout, result.stderr, result.returncode
    except subprocess.TimeoutExpired:
        return '', f'Timeout processing {file_path}', 1
    except Exception as e:
        return '', f'Error processing {file_path}: {e}', 1


def _generate_compile_commands(build_dir):
    """Generate compile_commands.json using ninja."""
    print(f'Generating compile_commands.json in {build_dir}...', file=sys.stderr)

    # Use Chromium's compile_db module
    chromium_src = _get_chromium_src_dir()
    sys.path.insert(0, os.path.join(chromium_src, 'tools', 'clang', 'pylib'))
    from clang import compile_db

    commands = compile_db.GenerateWithNinja(build_dir)

    compile_db_path = os.path.join(build_dir, 'compile_commands.json')
    with open(compile_db_path, 'w') as f:
        json.dump(commands, f, indent=2)

    print(f'Generated {len(commands)} compile commands', file=sys.stderr)
    return commands


def main(argv):
    parser = argparse.ArgumentParser(description='Run CEF clang tools')
    parser.add_argument('-p', '--build-dir', required=True,
                        help='Build directory containing compile_commands.json')
    parser.add_argument('--tool', default='cef_cpp_rewriter',
                        help='Tool name (default: cef_cpp_rewriter)')
    parser.add_argument('--generate-compdb', action='store_true',
                        help='Generate compile_commands.json before running')
    parser.add_argument('-j', '--jobs', type=int,
                        default=multiprocessing.cpu_count(),
                        help='Number of parallel jobs')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Print progress to stderr')
    parser.add_argument('paths', nargs='*',
                        help='Optional path filters (e.g., cef/libcef cef/tests). '
                             'If not specified, processes all CEF files.')
    args = parser.parse_args(argv)

    chromium_src = _get_chromium_src_dir()
    tool_path = _get_tool_path(chromium_src, args.tool)

    # Check tool exists
    if not os.path.exists(tool_path):
        print(f'Error: Tool not found at {tool_path}', file=sys.stderr)
        print('Run ./cef/tools/clang/scripts/build_clang_rewriter.sh first',
              file=sys.stderr)
        return 1

    # Make build_dir absolute
    if not os.path.isabs(args.build_dir):
        args.build_dir = os.path.join(chromium_src, args.build_dir)

    # Generate compile_commands.json if requested
    if args.generate_compdb:
        entries = _generate_compile_commands(args.build_dir)
    else:
        entries = _load_compile_commands(args.build_dir)
        if entries is None:
            print(f'Error: compile_commands.json not found in {args.build_dir}',
                  file=sys.stderr)
            print('Run with --generate-compdb or run: '
                  'ninja -C {build_dir} -t compdb > compile_commands.json',
                  file=sys.stderr)
            return 1

    # Filter to CEF files only (with optional path filters)
    path_filters = args.paths if args.paths else None
    cef_entries = _filter_cef_entries(entries, path_filters)

    if not cef_entries:
        if path_filters:
            print(f'No CEF files found matching: {" ".join(path_filters)}',
                  file=sys.stderr)
        else:
            print('No CEF files found in compile_commands.json', file=sys.stderr)
        return 1

    if args.verbose:
        filter_msg = f' (filtered by {" ".join(path_filters)})' if path_filters else ''
        print(f'Found {len(cef_entries)} CEF files to process{filter_msg}',
              file=sys.stderr)

    # Get unique file paths
    file_paths = []
    seen = set()
    for entry in cef_entries:
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
    work_items = [(tool_path, args.build_dir, f) for f in file_paths]

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
