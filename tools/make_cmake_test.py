# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import os
import shutil
import tempfile
import unittest
from unittest import mock

from generator_test_util import read_golden
from generator_test_util import run_generator_script
from generator_test_util import testdata_dir
from make_cmake import format_cmake_group
from make_cmake import format_cmake_library
from make_cmake import format_cmake_set
from make_cmake import get_files_for_variable
from make_cmake import process_cmake_template
from make_cmake import process_cmake_template_segment
from make_cmake import read_gypi_variables


class MakeCmakeTest(unittest.TestCase):

  def test_format_helpers_relative_paths_platforms_and_template_golden(self):
    variables = {
        'common': ['include/a.h', 'libcef_dll/b.cc'],
        'win': ['libcef_dll/b_win.cc']
    }
    with tempfile.TemporaryDirectory() as temporary_directory:
      fixture_root = os.path.join(temporary_directory, 'fixture_root')
      os.mkdir(fixture_root)
      shutil.copy2(testdata_dir('cef_paths.gypi'),
                   os.path.join(fixture_root, 'cef_paths.gypi'))
      template = os.path.join(fixture_root, 'cmake.template')
      shutil.copy2(testdata_dir('cmake.template'), template)
      with mock.patch('make_cmake.cef_dir', fixture_root):
        paths = get_files_for_variable(
            os.path.join(fixture_root, 'CMakeLists.txt'), variables, 'common')
        self.assertEqual(paths, ['include/a.h', 'libcef_dll/b.cc'])
        fixture_variables = read_gypi_variables('cef_paths')
        output = os.path.join(temporary_directory, 'CMakeLists.txt')
        process_cmake_template(template, output, fixture_variables, quiet=True)
        with open(output, encoding='utf-8') as generated:
          self.assertEqual(generated.read(),
                           read_golden('make_cmake', 'CMakeLists.txt'))
        os.utime(output, (1000000000, 1000000000))
        process_cmake_template(template, output, fixture_variables, quiet=True)
        self.assertEqual(os.stat(output).st_mtime_ns, 1000000000000000000)

    self.assertEqual(format_cmake_set('FILES', ['a', 'b']),
                     'set(FILES\n  a\n  b\n  )\n')
    self.assertIn(
        'APPEND_CUSTOM(FILES)',
        format_cmake_group(os.path.join('tmp', 'lib', 'CMakeLists.txt'),
                           'FILES', ['a.cc', 'b.cc:WIN'], ':', 'APPEND_CUSTOM'))
    self.assertEqual(format_cmake_library('lib', ['A', 'B']),
                     'add_library(lib\n  ${A}\n  ${B}\n  )\n\n')

  def test_missing_variables_files_and_malformed_segments(self):
    with self.assertRaisesRegex(Exception, 'Variable missing does not exist'):
      get_files_for_variable(os.path.join('tmp', 'CMakeLists.txt'), {},
                             'missing')
    with mock.patch('make_cmake.cef_dir', '/definitely/missing'):
      with self.assertRaisesRegex(Exception, 'does not exist'):
        read_gypi_variables('missing')
    with self.assertRaisesRegex(Exception, 'Missing prefix'):
      process_cmake_template_segment("'includes': ['x']", 1,
                                     os.path.join('tmp', 'a'), {'x': ['a']})
    with self.assertRaisesRegex(Exception, 'Missing includes'):
      process_cmake_template_segment("'prefix': 'x'", 1,
                                     os.path.join('tmp', 'a'), {})
    with tempfile.TemporaryDirectory() as temporary_directory:
      output = os.path.join(temporary_directory, 'out')
      with self.assertRaisesRegex(Exception, 'does not exist'):
        process_cmake_template('/missing/template', output, {}, quiet=True)

  def test_python3_cli_success_override_and_unchanged_output(self):
    invalid = run_generator_script('make_cmake.py')
    self.assertEqual(invalid.returncode, 0)
    self.assertIn('Usage:', invalid.stderr)
    with tempfile.TemporaryDirectory() as temporary_directory:
      fixture_root = os.path.join(temporary_directory, 'fixture_root')
      tools_directory = os.path.join(fixture_root, 'tools')
      os.makedirs(tools_directory)
      source_tools = os.path.dirname(__file__)
      for filename in ('make_cmake.py', 'file_util.py'):
        shutil.copy2(os.path.join(source_tools, filename), tools_directory)
      shutil.copy2(testdata_dir('cef_paths.gypi'), fixture_root)
      template = os.path.join(fixture_root, 'cmake.template')
      shutil.copy2(testdata_dir('cmake.template'), template)
      with open(os.path.join(fixture_root, 'cef_paths2.gypi'),
                'w',
                encoding='utf-8') as output_file:
        output_file.write("""{
  'variables': {
    'fixture_common': [
      'include/cef_override.h',
      'libcef_dll/fixture.cc',
    ],
  },
}
""")

      script = os.path.join(tools_directory, 'make_cmake.py')
      output = os.path.join(temporary_directory, 'CMakeLists.txt')
      expected_stdout = 'Processing "%s" to "%s"...\n' % (template, output)
      expected_output = read_golden('make_cmake', 'CMakeLists.txt').replace(
          'include/cef_fixture.h', 'include/cef_override.h')

      result = run_generator_script(script, template, output)
      self.assertEqual(result.returncode, 0, result.stderr)
      self.assertEqual(result.stdout, expected_stdout)
      self.assertEqual(result.stderr, '')
      with open(output, encoding='utf-8') as output_file:
        self.assertEqual(output_file.read(), expected_output)

      os.utime(output, (1000000000, 1000000000))
      result = run_generator_script(script, template, output)
      self.assertEqual(result.returncode, 0, result.stderr)
      self.assertEqual(result.stdout, expected_stdout)
      self.assertEqual(result.stderr, '')
      with open(output, encoding='utf-8') as output_file:
        self.assertEqual(output_file.read(), expected_output)
      self.assertEqual(os.stat(output).st_mtime_ns, 1000000000000000000)


if __name__ == '__main__':
  unittest.main()
