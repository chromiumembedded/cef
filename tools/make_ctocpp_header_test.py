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
from make_ctocpp_header import make_ctocpp_header
from make_ctocpp_header import make_function_body
from make_ctocpp_header import make_function_body_block
from make_ctocpp_header import write_ctocpp_header


class MakeCToCppHeaderTest(unittest.TestCase):

  def setUp(self):
    self.header = make_fixture_header()

  def test_all_class_outputs_match_goldens_and_cover_inheritance(self):
    for clsname in self.header.get_class_names():
      output = make_ctocpp_header(self.header, clsname)
      self.assertEqual(output, read_golden('make_ctocpp_header',
                                           clsname + '.h'))
    child = self.header.get_class('CefFixtureLibraryChild')
    body = make_function_body(self.header, child, True)
    self.assertIn('CefFixtureLibrary methods', body)
    self.assertIn('std::map', body)
    self.assertIn('std::vector', body)
    self.assertIn('CefFixtureLibraryChild methods',
                  make_function_body_block(child, True))
    output = make_ctocpp_header(self.header, 'CefFixtureScoped')
    self.assertIn('CefCToCppScoped', output)

  def test_missing_class_and_writer_tuple(self):
    with self.assertRaisesRegex(Exception, 'Class does not exist'):
      make_ctocpp_header(self.header, 'CefMissing')
    with tempfile.TemporaryDirectory() as temporary_directory:
      path, contents = write_ctocpp_header(self.header, 'CefFixtureClient',
                                           temporary_directory)
      self.assertEqual(
          path, os.path.join(temporary_directory, 'fixture_client_ctocpp.h'))
      self.assertEqual(contents,
                       read_golden('make_ctocpp_header', 'CefFixtureClient.h'))

  def test_cli_usage_and_success(self):
    invalid = run_generator_script('make_ctocpp_header.py')
    self.assertEqual(invalid.returncode, 0)
    self.assertIn('Usage:', invalid.stderr)
    fixture = testdata_dir('include', 'cef_fixture_scoped.h')
    success = run_generator_script('make_ctocpp_header.py', fixture,
                                   'CefFixtureScopedChild')
    self.assertEqual(success.returncode, 0, success.stderr)
    self.assertEqual(
        success.stdout,
        read_golden('make_ctocpp_header', 'CefFixtureScopedChild.h'))


if __name__ == '__main__':
  unittest.main()
