# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import json
import os
import tempfile
import unittest
import warnings

from generator_test_util import read_fixture
from generator_test_util import read_golden
from generator_test_util import run_generator_script
from generator_test_util import testdata_dir
from make_api_versions_header import make_api_versions_header
from make_api_versions_header import make_api_versions_inc
from make_api_versions_header import write_api_versions
from version_util import read_version_files


class MakeApiVersionsHeaderTest(unittest.TestCase):

  def setUp(self):
    self.data = json.loads(read_fixture('api_versions.json'))

  def test_header_and_include_match_goldens_and_preserve_order(self):
    header = make_api_versions_header(self.data)
    include = make_api_versions_inc(self.data)
    self.assertEqual(
        header, read_golden('make_api_versions_header', 'cef_api_versions.h'))
    self.assertEqual(
        include, read_golden('make_api_versions_header',
                             'cef_api_versions.inc'))
    self.assertLess(header.index('CEF_API_VERSION_13300'),
                    header.index('CEF_API_VERSION_13302'))
    self.assertIn('Fixture legacy version', header)
    for platform in ('OS_WIN', 'OS_MAC', 'OS_LINUX'):
      self.assertIn('CEF_API_HASH_13302', header)
      self.assertIn(platform, header)
    self.assertIn('CEF_API_VERSION_MIN CEF_API_VERSION_13300', header)
    self.assertIn('CEF_API_VERSION_LAST CEF_API_VERSION_13302', header)

  def test_writer_first_write_and_unchanged_contract_and_mtime(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      header = os.path.join(temporary_directory, 'versions.h')
      include = os.path.join(temporary_directory, 'versions.inc')
      self.assertTrue(write_api_versions(header, include, self.data))
      os.utime(header, (1000000000, 1000000000))
      os.utime(include, (1000000000, 1000000000))
      self.assertFalse(write_api_versions(header, include, self.data))
      self.assertEqual(os.stat(header).st_mtime_ns, 1000000000000000000)
      self.assertEqual(os.stat(include).st_mtime_ns, 1000000000000000000)

  def test_missing_fields_and_cli_usage_and_success(self):
    with self.assertRaises(KeyError):
      make_api_versions_header({'hashes': {}})
    unresolved = make_api_versions_header({
        'min': '1',
        'last': '1',
        'hashes': {
            '1': {
                'comment': 'incomplete'
            }
        }
    })
    self.assertIn('$WINDOWS$', unresolved)

    invalid = run_generator_script('make_api_versions_header.py')
    self.assertEqual(invalid.returncode, 255)
    self.assertIn('Usage:', invalid.stdout)
    self.assertEqual(invalid.stderr, '')
    with tempfile.TemporaryDirectory() as temporary_directory:
      header_path = os.path.join(temporary_directory, 'versions.h')
      include_path = os.path.join(temporary_directory, 'versions.inc')
      success = run_generator_script('make_api_versions_header.py', header_path,
                                     include_path,
                                     testdata_dir('api_versions.json'),
                                     testdata_dir('api_untracked.json'))
      self.assertEqual(success.returncode, 0, success.stderr)
      with warnings.catch_warnings():
        warnings.simplefilter('ignore', DeprecationWarning)
        combined = read_version_files(testdata_dir('api_versions.json'),
                                      testdata_dir('api_untracked.json'),
                                      initialize=True,
                                      combine=True)[0]
      with open(header_path, encoding='utf-8') as output:
        self.assertEqual(output.read(), make_api_versions_header(combined))
      with open(include_path, encoding='utf-8') as output:
        self.assertEqual(output.read(), make_api_versions_inc(combined))


if __name__ == '__main__':
  unittest.main()
