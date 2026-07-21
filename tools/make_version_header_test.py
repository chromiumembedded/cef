# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import os
import tempfile
import unittest
from unittest import mock

from generator_test_util import read_golden
from generator_test_util import run_generator_script
from make_version_header import main
from make_version_header import make_version_header
from make_version_header import write_version_header


class FakeVersionFormatter:

  def get_version_string(self):
    return '123.4.5+gabcdef0+chromium-123.0.4567.89'

  def get_cef_commit_components(self):
    return {'HASH': 'abcdef0123456789abcdef0123456789abcdef01', 'NUMBER': 42}

  def get_version_parts(self):
    return {'MAJOR': 123, 'MINOR': 4, 'PATCH': 5}

  def get_chrome_version_components(self):
    return {'MAJOR': 123, 'MINOR': 0, 'BUILD': 4567, 'PATCH': 89}


class MakeVersionHeaderTest(unittest.TestCase):

  def _patches(self):
    return mock.patch.multiple('make_version_header',
                               VersionFormatter=FakeVersionFormatter,
                               get_year=mock.DEFAULT,
                               calculate_sandbox_compat_hash=mock.DEFAULT)

  def test_fixed_version_content_empty_and_populated_sandbox_hash(self):
    with self._patches() as patched:
      patched['get_year'].return_value = '2026'
      patched['calculate_sandbox_compat_hash'].return_value = (
          '0123456789abcdef')
      output = make_version_header('unused', ['a', 'b'])
      self.assertEqual(output,
                       read_golden('make_version_header', 'cef_version.h'))
      self.assertIn('#define CEF_COMMIT_NUMBER 42', output)
      self.assertIn('#define CHROME_VERSION_BUILD 4567', output)
      self.assertEqual(patched['calculate_sandbox_compat_hash'].call_count, 1)
      self.assertIn('#define CEF_SANDBOX_COMPAT_HASH ""',
                    make_version_header('unused'))

  def test_writer_idempotence_main_sandbox_parsing_and_usage(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      output = os.path.join(temporary_directory, 'cef_version.h')
      with self._patches() as patched:
        patched['get_year'].return_value = '2026'
        patched['calculate_sandbox_compat_hash'].return_value = (
            '0123456789abcdef')
        self.assertTrue(write_version_header(output, ['sandbox']))
        os.utime(output, (1000000000, 1000000000))
        self.assertFalse(write_version_header(output, ['sandbox']))
        self.assertEqual(os.stat(output).st_mtime_ns, 1000000000000000000)
        main(['make_version_header.py', output, '--sandbox-files', 'a', 'b'])
        patched['calculate_sandbox_compat_hash'].assert_called_with(['a', 'b'])

    invalid = run_generator_script('make_version_header.py')
    self.assertEqual(invalid.returncode, 255)
    self.assertIn('Usage:', invalid.stdout)
    self.assertEqual(invalid.stderr, '')

  def test_sandbox_hash_failure_propagates(self):
    with self._patches() as patched:
      patched['get_year'].return_value = '2026'
      patched['calculate_sandbox_compat_hash'].side_effect = RuntimeError(
          'hash failed')
      with self.assertRaisesRegex(RuntimeError, 'hash failed'):
        make_version_header('unused', ['sandbox'])


if __name__ == '__main__':
  unittest.main()
