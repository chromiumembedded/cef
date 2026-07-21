# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import os
import tempfile
import unittest
from unittest import mock

from generator_test_util import read_golden
from generator_test_util import run_generator_script
from generator_test_util import testdata_dir
from make_config_header import make_config_header
from make_config_header import write_config_header


class MakeConfigHeaderTest(unittest.TestCase):

  def _input(self, directory, contents):
    path = os.path.join(directory, 'args.gn')
    with open(path, 'w', encoding='utf-8') as output:
      output.write(contents)
    return path

  def test_platform_x11_and_v8_sandbox_matrix(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      x64 = self._input(temporary_directory, 'target_cpu="x64"\n')
      with mock.patch('make_config_header.sys.platform', 'linux'):
        output = make_config_header(x64)
        self.assertIn('#define CEF_X11 1', output)
        self.assertIn('#define CEF_V8_ENABLE_SANDBOX 1', output)

      disabled = self._input(
          temporary_directory, 'target_cpu="arm64"\nozone_platform_x11=false\n'
          'v8_enable_sandbox=false\n')
      with mock.patch('make_config_header.sys.platform', 'linux'):
        output = make_config_header(disabled)
        self.assertNotIn('#define CEF_X11 1', output)
        self.assertNotIn('#define CEF_V8_ENABLE_SANDBOX 1', output)
      with mock.patch('make_config_header.sys.platform', 'darwin'):
        self.assertNotIn('#define CEF_X11 1', make_config_header(x64))

  def test_golden_writer_idempotence_missing_input_and_cli(self):
    fixture = testdata_dir('args.gn')
    self.assertEqual(make_config_header(fixture),
                     read_golden('make_config_header', 'cef_config.h'))
    with self.assertRaisesRegex(Exception, 'does not exist'):
      make_config_header('/missing/fixture/args.gn')

    with tempfile.TemporaryDirectory() as temporary_directory:
      output = os.path.join(temporary_directory, 'cef_config.h')
      self.assertTrue(write_config_header(output, fixture))
      os.utime(output, (1000000000, 1000000000))
      self.assertFalse(write_config_header(output, fixture))
      self.assertEqual(os.stat(output).st_mtime_ns, 1000000000000000000)

      success = run_generator_script('make_config_header.py', output, fixture)
      self.assertEqual(success.returncode, 0, success.stderr)
      self.assertEqual(success.stdout, '')

    invalid = run_generator_script('make_config_header.py')
    self.assertEqual(invalid.returncode, 255)
    self.assertIn('Usage:', invalid.stdout)


if __name__ == '__main__':
  unittest.main()
