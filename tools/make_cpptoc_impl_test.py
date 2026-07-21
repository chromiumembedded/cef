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
from make_cpptoc_impl import make_cpptoc_class_impl
from make_cpptoc_impl import make_cpptoc_global_impl
from make_cpptoc_impl import make_cpptoc_impl_proto
from make_cpptoc_impl import write_cpptoc_impl


class MakeCppToCImplTest(unittest.TestCase):

  def setUp(self):
    self.header = make_fixture_header()

  def test_all_class_and_global_outputs_match_pre_refactor_goldens(self):
    output, customized = make_cpptoc_global_impl(self.header, '')
    self.assertFalse(customized)
    self.assertEqual(output, read_golden('make_cpptoc_impl', 'global.cc'))
    self.assertIn('CEF_API_ADDED(13302)', output)
    for clsname in self.header.get_class_names():
      output, customized = make_cpptoc_class_impl(self.header, clsname, '')
      self.assertFalse(customized, clsname)
      self.assertEqual(output, read_golden('make_cpptoc_impl', clsname + '.cc'),
                       clsname)

    library, _ = make_cpptoc_class_impl(self.header, 'CefFixtureLibrary', '')
    self.assertIn('shutdown_checker::AssertNotShutdown()', library)
    self.assertIn('// Verify param: index', library)
    self.assertIn('transfer_string_list_contents', library)
    self.assertIn('transfer_string_map_contents', library)
    self.assertIn('// Return type: string', library)
    self.assertIn('// Return type: refptr_diff', library)
    self.assertIn('UnwrapDerived', library)

  def test_proto_existing_manual_body_and_writer_paths(self):
    func = self.header.get_funcs()[0]
    parts = func.get_capi_parts(self.header.get_defined_structs(), True)
    self.assertEqual(make_cpptoc_impl_proto('cef_fixture_add', func, parts),
                     'CEF_EXPORT int cef_fixture_add(int left, int right)')

    golden = read_golden('make_cpptoc_impl', 'CefFixtureClient.cc')
    manual = golden.replace(
        '  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING',
        '  // MANUAL FIXTURE BODY', 1)
    with contextlib.redirect_stdout(io.StringIO()) as stdout:
      regenerated, customized = make_cpptoc_class_impl(self.header,
                                                       'CefFixtureClient',
                                                       manual)
    self.assertTrue(customized)
    self.assertIn('// MANUAL FIXTURE BODY', regenerated)
    self.assertEqual(stdout.getvalue(),
                     '  NOTE: fixture_client_get_value has manual edits\n')

    with tempfile.TemporaryDirectory() as temporary_directory:
      missing_target = os.path.join(temporary_directory, 'missing.cc')
      path, output, customized = write_cpptoc_impl(self.header, None,
                                                   missing_target)
      self.assertEqual(path, missing_target)
      self.assertFalse(customized)
      self.assertEqual(output, read_golden('make_cpptoc_impl', 'global.cc'))

      existing_target = os.path.join(temporary_directory,
                                     'fixture_client_cpptoc.cc')
      with open(existing_target, 'w', encoding='utf-8') as destination:
        destination.write(manual)
      with contextlib.redirect_stdout(io.StringIO()) as stdout:
        path, output, customized = write_cpptoc_impl(self.header,
                                                     'CefFixtureClient',
                                                     temporary_directory)
      self.assertEqual(path, existing_target)
      self.assertTrue(customized)
      self.assertIn('// MANUAL FIXTURE BODY', output)
      self.assertEqual(
          stdout.getvalue(), 'In %s:\n'
          '  NOTE: fixture_client_get_value has manual edits\n' %
          existing_target)
      self.assertIsNone(cef_parser._NOTIFY_CONTEXT)

      with mock.patch('make_cpptoc_impl.make_cpptoc_class_impl',
                      side_effect=RuntimeError('fixture generation failure')):
        with self.assertRaisesRegex(RuntimeError, 'fixture generation failure'):
          write_cpptoc_impl(self.header, 'CefFixtureClient',
                            temporary_directory)
      # Characterize the known failure-path leak for a separate fix.
      self.assertEqual(cef_parser._NOTIFY_CONTEXT, existing_target)
      cef_parser.set_notify_context(None)

  def test_missing_class_and_successful_cli(self):
    with self.assertRaisesRegex(Exception, 'Class does not exist'):
      make_cpptoc_class_impl(self.header, 'CefMissing', '')

    invalid = run_generator_script('make_cpptoc_impl.py')
    self.assertEqual(invalid.returncode, 0)
    self.assertIn('Usage:', invalid.stderr)

    fixture = testdata_dir('include', 'cef_fixture_scoped.h')
    existing = testdata_dir('golden', 'make_cpptoc_impl', 'CefFixtureScoped.cc')
    with open(fixture, 'rb') as input_file:
      fixture_before = input_file.read()
    with open(existing, 'rb') as input_file:
      existing_before = input_file.read()

    result = run_generator_script('make_cpptoc_impl.py', fixture,
                                  'CefFixtureScoped', existing)
    self.assertEqual(result.returncode, 0, result.stderr)
    self.assertEqual(result.stdout,
                     read_golden('make_cpptoc_impl', 'CefFixtureScoped.cc'))
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
    output, _ = make_cpptoc_global_impl(special, '')
    self.assertIn('shutdown_checker::SetIsShutdown()', output)
    self.assertIn('NO_STACK_PROTECTOR', output)

    argument = self.header.get_funcs()[0].get_arguments()[0]
    with mock.patch.object(argument,
                           'get_arg_type',
                           return_value='fixture_unsupported'):
      with self.assertRaisesRegex(
          Exception, 'Unsupported argument type fixture_unsupported'):
        make_cpptoc_global_impl(self.header, '')

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
      make_cpptoc_class_impl(unsupported, 'CefUnsupported', '')


if __name__ == '__main__':
  unittest.main()
