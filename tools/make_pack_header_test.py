# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import os
import tempfile
import unittest

from generator_test_util import read_golden
from generator_test_util import run_generator_script
from generator_test_util import testdata_dir
from make_pack_header import _get_cpp_var_name
from make_pack_header import _get_defines
from make_pack_header import make_pack_header
from make_pack_header import make_pack_inc
from make_pack_header import write_pack_header


class MakePackHeaderTest(unittest.TestCase):

  def test_define_parsing_duplicate_suffixes_content_and_cpp_name(self):
    names = {}
    first = _get_defines(testdata_dir('resources_a.h'), names)
    second = _get_defines(testdata_dir('resources_b.h'), names)
    self.assertEqual(first['IDR_ALPHA'], '100')
    self.assertEqual(second['IDR_BETA'], '200')
    self.assertEqual(second['IDR_DUPLICATE_2'], '201')
    all_files = {'resources_a.h': first, 'resources_b.h': second}
    header_path = os.path.join('tmp', 'cef_resources.h')
    include_path = os.path.join('tmp', 'cef_resources.inc')
    self.assertEqual(make_pack_header(header_path, all_files),
                     read_golden('make_pack_header', 'cef_resources.h'))
    self.assertEqual(make_pack_inc(include_path, all_files),
                     read_golden('make_pack_header', 'cef_resources.inc'))
    self.assertEqual(_get_cpp_var_name(include_path), 'Resources')

  def test_writer_sorting_idempotence_and_current_inc_only_return_bug(self):
    inputs = [testdata_dir('resources_b.h'), testdata_dir('resources_a.h')]
    with tempfile.TemporaryDirectory() as temporary_directory:
      header = os.path.join(temporary_directory, 'cef_resources.h')
      include = os.path.join(temporary_directory, 'cef_resources.inc')
      self.assertTrue(write_pack_header(header, include, inputs))
      os.utime(header, (1000000000, 1000000000))
      os.utime(include, (1000000000, 1000000000))
      self.assertFalse(write_pack_header(header, include, inputs))
      self.assertEqual(os.stat(header).st_mtime_ns, 1000000000000000000)
      self.assertEqual(os.stat(include).st_mtime_ns, 1000000000000000000)

      with open(include, 'w', encoding='utf-8') as output:
        output.write('corrupt include')
      # Characterize the known bug: the include is repaired but its write
      # result is ignored while the unchanged header result is returned.
      self.assertFalse(write_pack_header(header, include, inputs))
      with open(include, encoding='utf-8') as output:
        self.assertEqual(output.read(),
                         read_golden('make_pack_header', 'cef_resources.inc'))

  def test_duplicate_filenames_and_cli_usage_and_success(self):
    inputs = [testdata_dir('resources_a.h'), testdata_dir('resources_a.h')]
    with tempfile.TemporaryDirectory() as temporary_directory:
      with self.assertRaises(AssertionError):
        write_pack_header(os.path.join(temporary_directory, 'out.h'),
                          os.path.join(temporary_directory, 'out.inc'), inputs)
      header = os.path.join(temporary_directory, 'cef_resources.h')
      include = os.path.join(temporary_directory, 'cef_resources.inc')
      success = run_generator_script('make_pack_header.py', header, include,
                                     testdata_dir('resources_b.h'),
                                     testdata_dir('resources_a.h'))
      self.assertEqual(success.returncode, 0, success.stderr)
      with open(header, encoding='utf-8') as output:
        self.assertEqual(output.read(),
                         read_golden('make_pack_header', 'cef_resources.h'))

    invalid = run_generator_script('make_pack_header.py')
    self.assertEqual(invalid.returncode, 255)
    self.assertIn('Usage:', invalid.stdout)


if __name__ == '__main__':
  unittest.main()
