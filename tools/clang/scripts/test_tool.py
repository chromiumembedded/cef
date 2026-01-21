#!/usr/bin/env python3
# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""Test harness for CEF clang tools.

This is a CEF-specific version of Chromium's test_tool.py that works
correctly with CEF's separate git repository.

Usage:
    # From chromium/src directory
    python3 cef/tools/clang/scripts/test_cef_tool.py cef_cpp_rewriter

    # With verbose output
    python3 cef/tools/clang/scripts/test_cef_tool.py cef_cpp_rewriter --verbose
"""

import argparse
import difflib
import glob
import json
import os
import platform
import shutil
import subprocess
import sys


def _get_chromium_src_dir():
    """Returns the chromium/src directory."""
    # This script is at cef/tools/clang/scripts/test_cef_tool.py
    script_dir = os.path.dirname(os.path.realpath(__file__))
    return os.path.realpath(os.path.join(script_dir, '..', '..', '..', '..'))


def _get_tool_binary_path(chromium_src):
    """Returns the path to the built tool binary."""
    return os.path.join(chromium_src, 'third_party', 'llvm-build',
                        'Release+Asserts', 'bin', 'cef_cpp_rewriter')


def _generate_compile_commands(test_dir, files):
    """Generate compile_commands.json for the test files."""
    commands = []

    # Ensure test_dir is absolute (required for compile command lookup)
    test_dir = os.path.realpath(test_dir)

    # Build the compiler command
    compiler_cmd = 'clang++ -std=c++20'

    # Add macOS SDK path if on macOS
    if platform.system() == 'Darwin':
        try:
            sdk_path = subprocess.check_output(
                ['xcrun', '--show-sdk-path'],
                stderr=subprocess.DEVNULL
            ).decode('utf-8').strip()
            compiler_cmd += f' -isysroot {sdk_path}'
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass  # Fall back to no SDK path

    for f in files:
        basename = os.path.basename(f)
        commands.append({
            'directory': test_dir,
            'command': f'{compiler_cmd} -c {basename}',
            'file': basename
        })

    return json.dumps(commands, indent=2)


def _run_tool(tool_path, test_dir, actual_files, verbose=False):
    """Run the clang tool on the test files."""
    # Disable path filter for tests since test files use relative paths
    # that don't contain '/cef/'
    args = [tool_path, '--disable-path-filter', '-p', test_dir] + actual_files

    if verbose:
        print(f'Running: {" ".join(args)}')

    result = subprocess.run(args, capture_output=True, text=True)

    if result.returncode != 0:
        print(f'Tool failed with exit code {result.returncode}')
        if result.stderr:
            print(f'stderr: {result.stderr}')
        return None

    return result.stdout


def _apply_edits(raw_output, test_dir, verbose=False):
    """Apply edits from the tool's raw output."""
    if not raw_output:
        return True

    # Parse and apply edits
    # Format: r:::filepath:::offset:::length:::replacement
    edits_by_file = {}

    for line in raw_output.strip().split('\n'):
        if not line or not line.startswith('r:::'):
            continue

        parts = line.split(':::', 4)
        if len(parts) != 5:
            continue

        _, filepath, offset, length, replacement = parts
        offset = int(offset)
        length = int(length)
        # Convert null bytes back to newlines (OutputHelper replaces \n with \0)
        replacement = replacement.replace('\0', '\n')

        # Normalize filepath
        if not os.path.isabs(filepath):
            filepath = os.path.join(test_dir, filepath)
        filepath = os.path.realpath(filepath)

        if filepath not in edits_by_file:
            edits_by_file[filepath] = []
        edits_by_file[filepath].append((offset, length, replacement))

    # Apply edits to each file (in reverse order to preserve offsets)
    for filepath, edits in edits_by_file.items():
        if verbose:
            print(f'Applying {len(edits)} edits to {os.path.basename(filepath)}')

        with open(filepath, 'r') as f:
            content = f.read()

        # Sort by offset descending so we can apply without adjusting offsets
        for offset, length, replacement in sorted(edits, key=lambda x: -x[0]):
            content = content[:offset] + replacement + content[offset + length:]

        with open(filepath, 'w') as f:
            f.write(content)

    return True


def _compare_files(expected_path, actual_path):
    """Compare expected and actual files, returning True if they match."""
    with open(expected_path, 'r') as f:
        expected = f.readlines()
    with open(actual_path, 'r') as f:
        actual = f.readlines()

    if expected == actual:
        return True, None

    diff = list(difflib.unified_diff(
        expected, actual,
        fromfile=os.path.basename(expected_path),
        tofile=os.path.basename(actual_path)
    ))
    return False, diff


def main(argv):
    parser = argparse.ArgumentParser(description='Test CEF clang tools')
    parser.add_argument('tool_name', help='Name of the tool to test')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Enable verbose output')
    parser.add_argument('--test-filter', default='*',
                        help='Glob pattern to filter test files')
    args = parser.parse_args(argv)

    chromium_src = _get_chromium_src_dir()
    tool_path = _get_tool_binary_path(chromium_src)

    # Check tool exists
    if not os.path.exists(tool_path):
        print(f'Error: Tool not found at {tool_path}')
        print('Run ./cef/tools/clang/scripts/build_clang_rewriter.sh first')
        return 1

    # Find test directory
    test_dir = os.path.join(chromium_src, 'cef', 'tools', 'clang',
                            args.tool_name, 'tests')
    if not os.path.isdir(test_dir):
        print(f'Error: Test directory not found: {test_dir}')
        return 1

    # Find test files
    pattern = os.path.join(test_dir, f'{args.test_filter}-original.cc')
    source_files = sorted(glob.glob(pattern))

    if not source_files:
        print(f'No test files matching {pattern}')
        return 1

    print(f'\nTesting {args.tool_name}')
    print(f'Found {len(source_files)} test file(s)\n')

    passed = 0
    failed = 0

    for source_file in source_files:
        test_name = os.path.basename(source_file).rsplit('-', 1)[0]
        actual_file = os.path.join(test_dir, f'{test_name}-actual.cc')
        expected_file = os.path.join(test_dir, f'{test_name}-expected.cc')

        print(f'[ RUN      ] {test_name}')

        # Check expected file exists
        if not os.path.exists(expected_file):
            print(f'[  FAILED  ] {test_name} - missing expected file')
            failed += 1
            continue

        # Copy original to actual
        shutil.copyfile(source_file, actual_file)

        # Generate compile_commands.json
        compile_db_path = os.path.join(test_dir, 'compile_commands.json')
        compile_db = _generate_compile_commands(test_dir, [actual_file])
        with open(compile_db_path, 'w') as f:
            f.write(compile_db)

        if args.verbose:
            print(f'Generated compile_commands.json')

        # Run the tool
        raw_output = _run_tool(tool_path, test_dir, [actual_file], args.verbose)

        if raw_output is None:
            print(f'[  FAILED  ] {test_name} - tool execution failed')
            failed += 1
            continue

        if args.verbose and raw_output:
            print(f'Tool output:\n{raw_output}')

        # Apply edits
        if not _apply_edits(raw_output, test_dir, args.verbose):
            print(f'[  FAILED  ] {test_name} - failed to apply edits')
            failed += 1
            continue

        # Compare results
        match, diff = _compare_files(expected_file, actual_file)

        if match:
            print(f'[       OK ] {test_name}')
            passed += 1
            # Clean up on success
            os.remove(actual_file)
        else:
            print(f'[  FAILED  ] {test_name}')
            if diff:
                sys.stdout.writelines(diff)
            failed += 1
            # Keep actual file for inspection

    # Always clean up compile_commands.json
    compile_db_path = os.path.join(test_dir, 'compile_commands.json')
    if os.path.exists(compile_db_path):
        os.remove(compile_db_path)

    print()
    print(f'[==========] {len(source_files)} test(s) ran.')
    if passed > 0:
        print(f'[  PASSED  ] {passed} test(s).')
    if failed > 0:
        print(f'[  FAILED  ] {failed} test(s).')
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
