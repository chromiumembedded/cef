# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import contextlib
import io
import os
import tempfile
import unittest
from unittest import mock

import cef_parser
from cef_parser import obj_header
from generator_test_util import make_fixture_header
from generator_test_util import read_golden
from generator_test_util import run_generator_script
from generator_test_util import testdata_dir
from make_ctocpp_impl import make_ctocpp_class_impl
from make_ctocpp_impl import make_ctocpp_global_impl
from make_ctocpp_impl import make_ctocpp_impl_proto
from make_ctocpp_impl import make_ctocpp_version_wrappers
from make_ctocpp_impl import _version_finder
from make_ctocpp_impl import write_ctocpp_impl


class MakeCToCppImplTest(unittest.TestCase):

  def setUp(self):
    self.header = make_fixture_header()

  def test_all_class_and_global_outputs_match_pre_refactor_goldens(self):
    output, customized = make_ctocpp_global_impl(self.header, '')
    self.assertFalse(customized)
    self.assertEqual(output, read_golden('make_ctocpp_impl', 'global.cc'))
    for clsname in self.header.get_class_names():
      output, customized = make_ctocpp_class_impl(self.header, clsname, '')
      self.assertFalse(customized, clsname)
      self.assertEqual(output, read_golden('make_ctocpp_impl', clsname + '.cc'),
                       clsname)

    library, _ = make_ctocpp_class_impl(self.header, 'CefFixtureLibrary', '')
    self.assertIn('cef_string_list_alloc()', library)
    self.assertIn('cef_string_map_alloc()', library)
    self.assertIn('AttachToUserFree', library)
    self.assertIn('// Return type: refptr_diff', library)
    self.assertIn('UnwrapDerived', library)
    self.assertIn('CEF_API_ADDED(13302)', library)
    cls = self.header.get_class('CefFixtureLibrary')
    wrappers = make_ctocpp_version_wrappers(self.header, cls, False,
                                            cls.get_all_versions())
    self.assertIn('CefFixtureLibrary_13302_CToCpp::Wrap', wrappers)
    self.assertEqual(
        _version_finder(self.header, 13302, 'cef_fixture_library_t'),
        'cef_fixture_library_0_t')

  def test_proto_existing_manual_body_and_writer_paths(self):
    func = self.header.get_funcs()[0]
    parts = func.get_cpp_parts(True)
    self.assertEqual(
        make_ctocpp_impl_proto(None, 'CefFixtureAdd', func, parts),
        'NO_SANITIZE("cfi-icall") CEF_GLOBAL int CefFixtureAdd(int left, int right)'
    )

    golden = read_golden('make_ctocpp_impl', 'CefFixtureLibrary.cc')
    manual = golden.replace(
        '  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING',
        '  // MANUAL FIXTURE BODY', 1)
    with contextlib.redirect_stdout(io.StringIO()) as stdout:
      regenerated, customized = make_ctocpp_class_impl(self.header,
                                                       'CefFixtureLibrary',
                                                       manual)
    self.assertTrue(customized)
    self.assertIn('// MANUAL FIXTURE BODY', regenerated)
    self.assertEqual(
        stdout.getvalue(),
        '  NOTE: CefFixtureLibraryCToCpp::Create has manual edits\n')

    with tempfile.TemporaryDirectory() as temporary_directory:
      missing_target = os.path.join(temporary_directory, 'missing.cc')
      path, output, customized = write_ctocpp_impl(self.header, None,
                                                   missing_target)
      self.assertEqual(path, missing_target)
      self.assertFalse(customized)
      self.assertEqual(output, read_golden('make_ctocpp_impl', 'global.cc'))

      existing_target = os.path.join(temporary_directory,
                                     'fixture_library_ctocpp.cc')
      with open(existing_target, 'w', encoding='utf-8') as destination:
        destination.write(manual)
      with contextlib.redirect_stdout(io.StringIO()) as stdout:
        path, output, customized = write_ctocpp_impl(self.header,
                                                     'CefFixtureLibrary',
                                                     temporary_directory)
      self.assertEqual(path, existing_target)
      self.assertTrue(customized)
      self.assertIn('// MANUAL FIXTURE BODY', output)
      self.assertEqual(
          stdout.getvalue(), 'In %s:\n'
          '  NOTE: CefFixtureLibraryCToCpp::Create has manual edits\n' %
          existing_target)
      self.assertIsNone(cef_parser._NOTIFY_CONTEXT)

      with mock.patch('make_ctocpp_impl.make_ctocpp_class_impl',
                      side_effect=RuntimeError('fixture generation failure')):
        with self.assertRaisesRegex(RuntimeError, 'fixture generation failure'):
          write_ctocpp_impl(self.header, 'CefFixtureLibrary',
                            temporary_directory)
      # Characterize the known failure-path leak for a separate fix.
      self.assertEqual(cef_parser._NOTIFY_CONTEXT, existing_target)
      cef_parser.set_notify_context(None)

  def test_missing_class_and_successful_cli(self):
    with self.assertRaisesRegex(Exception, 'Class does not exist'):
      make_ctocpp_class_impl(self.header, 'CefMissing', '')

    invalid = run_generator_script('make_ctocpp_impl.py')
    self.assertEqual(invalid.returncode, 0)
    self.assertIn('Usage:', invalid.stderr)

    fixture = testdata_dir('include', 'cef_fixture_scoped.h')
    existing = testdata_dir('golden', 'make_ctocpp_impl', 'CefFixtureScoped.cc')
    with open(fixture, 'rb') as input_file:
      fixture_before = input_file.read()
    with open(existing, 'rb') as input_file:
      existing_before = input_file.read()

    result = run_generator_script('make_ctocpp_impl.py', fixture,
                                  'CefFixtureScoped', existing)
    self.assertEqual(result.returncode, 0, result.stderr)
    self.assertEqual(result.stdout,
                     read_golden('make_ctocpp_impl', 'CefFixtureScoped.cc'))
    self.assertEqual(result.stderr, '')
    with open(fixture, 'rb') as input_file:
      self.assertEqual(input_file.read(), fixture_before)
    with open(existing, 'rb') as input_file:
      self.assertEqual(input_file.read(), existing_before)

  def test_shutdown_no_stack_protector_and_unsupported_types(self):
    special = obj_header()
    special.add_data(
        'cef_special.h', '''
///
/// Shut down the fixture.
///
/*--cef()--*/
void CefShutdown();

///
/// Run without a stack protector.
///
/*--cef(no_stack_protector)--*/
void CefNoStack();
''')
    output, _ = make_ctocpp_global_impl(special, '')
    self.assertIn('shutdown_checker::SetIsShutdown()', output)
    self.assertIn('NO_STACK_PROTECTOR', output)

    argument = self.header.get_funcs()[0].get_arguments()[0]
    with mock.patch.object(argument,
                           'get_arg_type',
                           return_value='fixture_unsupported'):
      with self.assertRaisesRegex(
          Exception, 'Unsupported argument type fixture_unsupported'):
        make_ctocpp_global_impl(self.header, '')

    unsupported = obj_header()
    unsupported.add_data(
        'cef_unsupported.h', '''
#include "include/cef_base.h"
///
/// Unsupported scoped fixture.
///
/*--cef(source=library)--*/
class CefUnsupported : public CefBaseScoped {
 public:
  ///
  /// Return a raw pointer on the same side.
  ///
  /*--cef()--*/
  virtual CefRawPtr<CefUnsupported> GetRaw() = 0;
};
''')
    with self.assertRaisesRegex(Exception,
                                'Unsupported return type rawptr_same'):
      make_ctocpp_class_impl(unsupported, 'CefUnsupported', '')


if __name__ == '__main__':
  unittest.main()
