# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import os
import shutil
import tempfile
import unittest

from generator_test_util import read_golden
from generator_test_util import run_generator_script
from generator_test_util import testdata_dir
from make_colorids_header import MakeFile
from make_colorids_header import MakeFileSegment


class MakeColorIdsHeaderTest(unittest.TestCase):

  def _stable_inputs(self, temporary_directory):
    root = os.path.join(temporary_directory, 'color_work')
    working_directory = os.path.join(root, 'a', 'b')
    os.makedirs(working_directory)
    shutil.copy2(testdata_dir('color_a.inc'), os.path.join(root, 'color_a.inc'))
    shutil.copy2(testdata_dir('color_b.inc'), os.path.join(root, 'color_b.inc'))
    return working_directory, ['../../color_a.inc', '../../color_b.inc']

  def test_segment_substitutions_deduplication_golden_and_idempotence(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      working_directory, inputs = self._stable_inputs(temporary_directory)
      output = os.path.join(temporary_directory, 'cef_color_ids.h')
      previous_directory = os.getcwd()
      try:
        os.chdir(working_directory)
        segment = MakeFileSegment(inputs[0])
        self.assertIn('From color_a.inc', segment)
        self.assertIn('#define COLOR_IDS', segment)
        self.assertIsNone(MakeFile(output, inputs))
        with open(output, encoding='utf-8') as generated:
          contents = generated.read()
        os.utime(output, (1000000000, 1000000000))
        MakeFile(output, inputs)
      finally:
        os.chdir(previous_directory)
      self.assertEqual(contents,
                       read_golden('make_colorids_header', 'cef_color_ids.h'))
      self.assertEqual(contents.count('#undef COLOR_IDS'), 2)
      self.assertIn('CEF_ColorAlpha', contents)
      self.assertIn('defined(OS_', contents)
      self.assertEqual(os.stat(output).st_mtime_ns, 1000000000000000000)

  def test_malformed_input_cli_usage_and_success(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      working_directory, inputs = self._stable_inputs(temporary_directory)
      malformed = os.path.join(temporary_directory, 'malformed.inc')
      with open(malformed, 'w', encoding='utf-8') as output:
        output.write('// no segment\n')
      with self.assertRaises(AssertionError):
        MakeFileSegment(malformed)

      destination = os.path.join(working_directory, 'cef_color_ids.h')
      success = run_generator_script('make_colorids_header.py',
                                     destination,
                                     *inputs,
                                     cwd=working_directory)
      self.assertEqual(success.returncode, 0, success.stderr)
      with open(destination, encoding='utf-8') as output:
        self.assertEqual(output.read(),
                         read_golden('make_colorids_header', 'cef_color_ids.h'))

    invalid = run_generator_script('make_colorids_header.py')
    self.assertEqual(invalid.returncode, 255)
    self.assertIn('Usage:', invalid.stdout)


if __name__ == '__main__':
  unittest.main()
