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
from make_capi_header import make_capi_header
from make_capi_header import write_capi_header


class MakeCapiHeaderTest(unittest.TestCase):

  def setUp(self):
    self.header = make_fixture_header()

  def test_complete_outputs_match_pre_refactor_goldens(self):
    for filename in self.header.get_file_names():
      output = make_capi_header(self.header, filename)
      self.assertEqual(output, read_golden('make_capi_header', filename))
    output = make_capi_header(self.header, 'cef_fixture.h')
    self.assertIn('cef_string_userfree_free()', output)
    self.assertIn('get_versioned_value_removed', output)
    self.assertIn('#if CEF_API_REMOVED(13302)', output)
    self.assertIn('#include "include/internal/cef_types_geometry.h"', output)

  def test_writer_returns_capi_path_and_exact_contents(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      output_directory = os.path.join(temporary_directory, 'include', 'capi')
      path, contents = write_capi_header(self.header, output_directory,
                                         'cef_fixture.h')
      self.assertEqual(path, os.path.join(output_directory,
                                          'cef_fixture_capi.h'))
      self.assertEqual(contents, read_golden('make_capi_header',
                                             'cef_fixture.h'))

  def test_unknown_forward_declaration_and_cpp_base_include_fail(self):
    data = '''
#include "include/cef_base.h"
class CefMissing;
///
/// Test class.
///
/*--cef(source=library)--*/
class CefBad : public CefBaseRefCounted {
 public:
  ///
  /// Test method.
  ///
  /*--cef()--*/
  virtual void Run() = 0;
};
'''
    header = obj_header()
    header.add_data('cef_bad.h', data)
    with self.assertRaisesRegex(Exception, 'Unknown class: CefMissing'):
      make_capi_header(header, 'cef_bad.h')

    header = obj_header()
    header.add_data(
        'cef_bad.h',
        data.replace('#include "include/cef_base.h"',
                     '#include "include/base/cef_callback.h"'))
    with self.assertRaisesRegex(Exception, 'Disallowed include'):
      make_capi_header(header, 'cef_bad.h')

  def test_global_function_type_includes(self):
    library_data = '''
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
    util_data = '''
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
    header = obj_header()
    header.add_data('cef_fixture_library.h', library_data)
    header.add_data('cef_fixture_util.h', util_data)
    output = make_capi_header(header, 'cef_fixture_util.h')
    self.assertIn('#include "include/capi/cef_fixture_library_capi.h"', output)

  def test_cli_usage_and_successful_stdout(self):
    invalid = run_generator_script('make_capi_header.py')
    self.assertEqual(invalid.returncode, 0)
    self.assertEqual(invalid.stdout, '')
    self.assertIn('Usage:', invalid.stderr)

    fixture = testdata_dir('include', 'cef_fixture_scoped.h')
    success = run_generator_script('make_capi_header.py', fixture)
    self.assertEqual(success.returncode, 0, success.stderr)
    self.assertEqual(success.stderr, '')
    self.assertEqual(success.stdout,
                     read_golden('make_capi_header', 'cef_fixture_scoped.h'))


if __name__ == '__main__':
  unittest.main()
