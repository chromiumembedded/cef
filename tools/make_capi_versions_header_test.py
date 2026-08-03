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
from make_capi_versions_header import _version_finder
from make_capi_versions_header import make_capi_versions_header
from make_capi_versions_header import write_capi_versions_header

_LIBRARY_DATA = '''
#include "include/cef_base.h"
///
/// Library-side object.
///
/*--cef(source=library)--*/
class CefFixtureLibrary : public CefBaseRefCounted {
 public:
  ///
  /// Return a value.
  ///
  /*--cef()--*/
  virtual int GetValue() = 0;

  IMPLEMENT_REFCOUNTING(CefFixtureLibrary);
};
'''

_UTIL_DATA = '''
#include "include/cef_fixture_library.h"
///
/// Create a library-side object.
///
/*--cef()--*/
CefRefPtr<CefFixtureLibrary> CefFixtureCreateLibrary();

///
/// Consume a library-side object.
///
/*--cef()--*/
void CefFixtureConsumeLibrary(CefRefPtr<CefFixtureLibrary> library);
'''


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

  def test_global_function_type_includes(self):
    header = obj_header()
    header.add_data('cef_fixture_library.h', _LIBRARY_DATA)
    header.add_data('cef_fixture_util.h', _UTIL_DATA)
    output = make_capi_versions_header(header, 'cef_fixture_util.h')
    self.assertIn('#include "include/capi/cef_fixture_library_capi_versions.h"',
                  output)

  def test_cli_resolves_cross_file_types(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      include_directory = os.path.join(temporary_directory, 'include')
      os.makedirs(include_directory)
      util_path = os.path.join(include_directory, 'cef_fixture_util.h')
      with open(os.path.join(include_directory, 'cef_fixture_library.h'),
                'w',
                encoding='utf-8') as library_file:
        library_file.write(_LIBRARY_DATA)
      with open(util_path, 'w', encoding='utf-8') as util_file:
        util_file.write(_UTIL_DATA)

      # The CLI must load enough context to resolve CefFixtureLibrary from
      # cef_fixture_library.h (issue #4123).
      success = run_generator_script('make_capi_versions_header.py', util_path)
      self.assertEqual(success.returncode, 0, success.stderr)
      self.assertIn(
          '#include "include/capi/cef_fixture_library_capi_versions.h"',
          success.stdout)
      self.assertIn('_cef_fixture_library_0_t', success.stdout)

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
