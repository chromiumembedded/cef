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
from make_libcef_dll_dylib_impl import make_libcef_dll_dylib_impl
from make_libcef_dll_dylib_impl import make_libcef_dll_dylib_impl_parts
from make_libcef_dll_dylib_impl import write_libcef_dll_dylib_impl


class MakeLibcefDllDylibImplTest(unittest.TestCase):

  def test_parts_cover_void_nonvoid_zero_and_multiple_arguments(self):
    declare, initialize, implementation = make_libcef_dll_dylib_impl_parts(
        'cef_fixture', 'int', ['int left', 'const char* right'])
    self.assertEqual(declare, 'decltype(&cef_fixture) cef_fixture;\n')
    self.assertEqual(initialize, 'INIT_ENTRY(cef_fixture);\n')
    self.assertIn('return g_libcef_pointers.cef_fixture(left, right);',
                  implementation)
    _, _, implementation = make_libcef_dll_dylib_impl_parts(
        'cef_fixture_void', 'void', [])
    self.assertIn('g_libcef_pointers.cef_fixture_void();', implementation)
    self.assertNotIn('return g_libcef', implementation)

  def test_complete_output_auxiliary_headers_and_writer_match_golden(self):
    header = make_fixture_header()
    output = make_libcef_dll_dylib_impl(header)
    self.assertEqual(
        output, read_golden('make_libcef_dll_dylib_impl',
                            'libcef_dll_dylib.cc'))
    self.assertEqual(output.count('#include "include/capi/cef_fixture_capi.h"'),
                     1)
    self.assertIn('decltype(&cef_api_hash) cef_api_hash;', output)
    self.assertIn('#if CEF_API_ADDED(13302)', output)
    with tempfile.TemporaryDirectory() as temporary_directory:
      output_path = os.path.join(temporary_directory, 'dylib.cc')
      path, contents = write_libcef_dll_dylib_impl(header, output_path)
      self.assertEqual(path, output_path)
      self.assertEqual(contents, output)

  def test_cli_usage_and_success(self):
    invalid = run_generator_script('make_libcef_dll_dylib_impl.py')
    self.assertEqual(invalid.returncode, 0)
    self.assertIn('Usage:', invalid.stderr)
    success = run_generator_script('make_libcef_dll_dylib_impl.py',
                                   testdata_dir('include'))
    self.assertEqual(success.returncode, 0, success.stderr)
    self.assertEqual(
        success.stdout,
        read_golden('make_libcef_dll_dylib_impl', 'libcef_dll_dylib.cc'))


if __name__ == '__main__':
  unittest.main()
