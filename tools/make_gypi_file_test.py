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
from make_gypi_file import make_gypi_file
from make_gypi_file import write_gypi_file


class MakeGypiFileTest(unittest.TestCase):

  def test_sorted_lists_sides_and_writer_tuple_match_golden(self):
    header = make_fixture_header()
    output = make_gypi_file(header)
    self.assertEqual(output, read_golden('make_gypi_file', 'cef_paths.gypi'))
    self.assertLess(output.index('include/cef_fixture.h'),
                    output.index('include/cef_fixture_scoped.h'))
    self.assertIn('libcef_dll/cpptoc/fixture_library_cpptoc.cc', output)
    self.assertIn('libcef_dll/ctocpp/fixture_client_ctocpp.cc', output)
    with tempfile.TemporaryDirectory() as temporary_directory:
      output_path = os.path.join(temporary_directory, 'cef_paths.gypi')
      path, contents = write_gypi_file(header, output_path)
      self.assertEqual(path, output_path)
      self.assertEqual(contents, output)

  def test_empty_header_and_cli_contract(self):
    output = make_gypi_file(obj_header())
    self.assertIn("'autogen_cpp_includes': [\n    ]", output)
    invalid = run_generator_script('make_gypi_file.py')
    self.assertEqual(invalid.returncode, 0)
    self.assertIn('Usage:', invalid.stderr)
    fixture = testdata_dir('include', 'cef_fixture_scoped.h')
    success = run_generator_script('make_gypi_file.py', fixture)
    self.assertEqual(success.returncode, 0, success.stderr)
    header = obj_header()
    header.add_file(fixture)
    self.assertEqual(success.stdout, make_gypi_file(header))


if __name__ == '__main__':
  unittest.main()
