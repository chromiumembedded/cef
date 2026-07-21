# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import os
import tempfile
import unittest

from cef_parser import obj_header
from generator_test_util import make_fixture_header
from generator_test_util import read_golden
from generator_test_util import run_generator_script
from generator_test_util import testdata_dir
from make_wrapper_types_header import make_wrapper_types_header
from make_wrapper_types_header import write_wrapper_types_header


class MakeWrapperTypesHeaderTest(unittest.TestCase):

  def test_populated_and_empty_headers_and_writer(self):
    header = make_fixture_header()
    output = make_wrapper_types_header(header)
    self.assertEqual(
        output, read_golden('make_wrapper_types_header', 'wrapper_types.h'))
    self.assertLess(output.index('WT_FIXTURE_CLIENT'),
                    output.index('WT_FIXTURE_LIBRARY'))
    empty = make_wrapper_types_header(obj_header())
    self.assertNotIn('WT_FIXTURE_', empty)
    with tempfile.TemporaryDirectory() as temporary_directory:
      output_path = os.path.join(temporary_directory, 'wrapper_types.h')
      path, contents = write_wrapper_types_header(header, output_path)
      self.assertEqual(path, output_path)
      self.assertEqual(contents, output)

  def test_cli_usage_and_success(self):
    invalid = run_generator_script('make_wrapper_types_header.py')
    self.assertEqual(invalid.returncode, 0)
    self.assertIn('Usage:', invalid.stderr)
    include_dir = testdata_dir('include')
    success = run_generator_script('make_wrapper_types_header.py', include_dir)
    self.assertEqual(success.returncode, 0, success.stderr)
    self.assertEqual(
        success.stdout,
        read_golden('make_wrapper_types_header', 'wrapper_types.h'))


if __name__ == '__main__':
  unittest.main()
