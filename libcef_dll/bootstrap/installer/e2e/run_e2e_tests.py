#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runner for CEF installer E2E tests.

Usage:
    python3 run_e2e_tests.py --build-dir=out/Debug_GN_x64
    python3 run_e2e_tests.py --build-dir=out/Debug_GN_x64 -k test_install_flow
    python3 run_e2e_tests.py --build-dir=out/Debug_GN_x64 \
        -k "test_launch_success and not clean_exit"
    python3 run_e2e_tests.py --build-dir=out/Debug_GN_x64 --verbose
"""

import argparse
import os
import sys
import unittest


def main():
  parser = argparse.ArgumentParser(description='Run CEF installer E2E tests')
  parser.add_argument(
      '--build-dir',
      required=True,
      help='Path to the build directory (e.g., out/Debug_GN_x64)')
  parser.add_argument(
      '-k',
      '--filter',
      default=None,
      help='Filter tests by a pytest-style keyword expression (and/or/not, '
      'parentheses; bare keywords are case-insensitive substrings of the '
      'test id).')
  parser.add_argument('--verbose',
                      '-v',
                      action='store_true',
                      help='Verbose test output')
  args = parser.parse_args()

  # Resolve build dir relative to chromium/src.
  build_dir = os.path.abspath(args.build_dir)
  if not os.path.isdir(build_dir):
    print(f'Error: build directory not found: {build_dir}', file=sys.stderr)
    return 1

  # Set environment variable for the test base to find build artifacts.
  os.environ['CEF_E2E_BUILD_DIR'] = build_dir

  # Discover tests in the e2e directory.
  test_dir = os.path.dirname(os.path.abspath(__file__))
  # Add test_dir to sys.path so `from e2e_test_base import ...` works.
  if test_dir not in sys.path:
    sys.path.insert(0, test_dir)
  loader = unittest.TestLoader()
  suite = loader.discover(test_dir, pattern='test_*.py')

  if args.filter:
    # Import here: e2e_filter lives in test_dir, which was just added to
    # sys.path above.
    from e2e_filter import compile_filter, flatten_suite
    match = compile_filter(args.filter)
    suite = unittest.TestSuite(t for t in flatten_suite(suite) if match(str(t)))

  verbosity = 2 if args.verbose else 1
  runner = unittest.TextTestRunner(verbosity=verbosity)
  result = runner.run(suite)
  return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
  sys.exit(main())
