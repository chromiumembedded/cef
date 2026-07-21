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
from make_cpptoc_header import make_cpptoc_header
from make_cpptoc_header import write_cpptoc_header


class MakeCppToCHeaderTest(unittest.TestCase):

  def setUp(self):
    self.header = make_fixture_header()

  def test_all_class_outputs_match_goldens_and_cover_sides_and_bases(self):
    for clsname in self.header.get_class_names():
      output = make_cpptoc_header(self.header, clsname)
      self.assertEqual(output, read_golden('make_cpptoc_header',
                                           clsname + '.h'))
    library = make_cpptoc_header(self.header, 'CefFixtureLibrary')
    self.assertIn('BUILDING_CEF_SHARED', library)
    self.assertIn('CefCppToCRefCounted', library)
    self.assertIn('CefFixtureLibrary_13302_CppToC', library)
    scoped = make_cpptoc_header(self.header, 'CefFixtureScoped')
    self.assertIn('CefCppToCScoped', scoped)
    client = make_cpptoc_header(self.header, 'CefFixtureClient')
    self.assertIn('WRAPPING_CEF_SHARED', client)

  def test_missing_class_and_writer_tuple(self):
    with self.assertRaisesRegex(Exception, 'Class does not exist'):
      make_cpptoc_header(self.header, 'CefMissing')
    with tempfile.TemporaryDirectory() as temporary_directory:
      path, contents = write_cpptoc_header(self.header, 'CefFixtureScoped',
                                           temporary_directory)
      self.assertEqual(
          path, os.path.join(temporary_directory, 'fixture_scoped_cpptoc.h'))
      self.assertEqual(contents,
                       read_golden('make_cpptoc_header', 'CefFixtureScoped.h'))

  def test_cli_usage_and_success(self):
    invalid = run_generator_script('make_cpptoc_header.py')
    self.assertEqual(invalid.returncode, 0)
    self.assertIn('Usage:', invalid.stderr)
    fixture = testdata_dir('include', 'cef_fixture_scoped.h')
    success = run_generator_script('make_cpptoc_header.py', fixture,
                                   'CefFixtureScopedChild')
    self.assertEqual(success.returncode, 0, success.stderr)
    self.assertEqual(
        success.stdout,
        read_golden('make_cpptoc_header', 'CefFixtureScopedChild.h'))


if __name__ == '__main__':
  unittest.main()
