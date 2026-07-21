# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import os
import tempfile
import unittest

from generator_test_util import make_fixture_header
from generator_test_util import read_golden
from generator_test_util import run_generator_script
from generator_test_util import testdata_dir
from make_capi_versions_header import _version_finder
from make_capi_versions_header import make_capi_versions_header
from make_capi_versions_header import write_capi_versions_header


class MakeCapiVersionsHeaderTest(unittest.TestCase):

  def setUp(self):
    self.header = make_fixture_header()

  def test_complete_outputs_and_versioned_members(self):
    for filename in self.header.get_file_names():
      self.assertEqual(make_capi_versions_header(self.header, filename),
                       read_golden('make_capi_versions_header', filename))
    output = make_capi_versions_header(self.header, 'cef_fixture.h')
    self.assertIn('cef_fixture_library_0_t', output)
    self.assertIn('cef_fixture_library_13302_t', output)
    self.assertIn('get_versioned_value_removed', output)
    self.assertIn('get_versioned_value_v2', output)
    self.assertEqual(_version_finder(self.header, 'cef_fixture_library_t'),
                     'cef_fixture_library_0_t')

  def test_writer_path_and_contents(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      output_directory = os.path.join(temporary_directory, 'include', 'capi')
      path, contents = write_capi_versions_header(self.header, output_directory,
                                                  'cef_fixture.h')
      self.assertEqual(
          path, os.path.join(output_directory, 'cef_fixture_capi_versions.h'))
      self.assertEqual(
          contents, read_golden('make_capi_versions_header', 'cef_fixture.h'))

  def test_cli_usage_and_successful_stdout(self):
    invalid = run_generator_script('make_capi_versions_header.py')
    self.assertEqual(invalid.returncode, 0)
    self.assertEqual(invalid.stdout, '')
    self.assertIn('Usage:', invalid.stderr)

    fixture = testdata_dir('include', 'cef_fixture_scoped.h')
    success = run_generator_script('make_capi_versions_header.py', fixture)
    self.assertEqual(success.returncode, 0, success.stderr)
    self.assertEqual(success.stderr, '')
    self.assertEqual(
        success.stdout,
        read_golden('make_capi_versions_header', 'cef_fixture_scoped.h'))


if __name__ == '__main__':
  unittest.main()
