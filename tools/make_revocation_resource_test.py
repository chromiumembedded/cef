#!/usr/bin/env python3
# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
"""
Unit tests for make_revocation_resource.py

Running the tests:
  # Run all tests with verbose output
  python3 -m unittest make_revocation_resource_test -v

  # Run all tests (simple)
  python3 make_revocation_resource_test.py
"""

import json
import os
import tempfile
import unittest
from unittest import mock

from generator_test_util import read_golden
from generator_test_util import run_generator_script
from generator_test_util import testdata_dir
from make_revocation_resource import main

from make_revocation_resource import (
    parse_version,
    prune_revocation_list,
    version_at_least,
)


class TestParseVersion(unittest.TestCase):
  """Test cases for parse_version."""

  def test_three_part(self):
    self.assertEqual(parse_version('148.2.0'), (148, 2, 0))

  def test_two_part(self):
    self.assertEqual(parse_version('146.0'), (146, 0))

  def test_four_part(self):
    self.assertEqual(parse_version('1.2.3.4'), (1, 2, 3, 4))

  def test_single_part(self):
    self.assertIsNone(parse_version('148'))

  def test_empty(self):
    self.assertIsNone(parse_version(''))

  def test_non_numeric(self):
    self.assertIsNone(parse_version('abc.def'))

  def test_mixed(self):
    self.assertIsNone(parse_version('148.abc.0'))


class TestVersionAtLeast(unittest.TestCase):
  """Test cases for version_at_least."""

  def test_equal(self):
    self.assertTrue(version_at_least((146, 1, 0), 146, 1))

  def test_greater_major(self):
    self.assertTrue(version_at_least((147, 0, 0), 146, 1))

  def test_greater_minor(self):
    self.assertTrue(version_at_least((146, 2, 0), 146, 1))

  def test_less_major(self):
    self.assertFalse(version_at_least((145, 9, 0), 146, 1))

  def test_less_minor(self):
    self.assertFalse(version_at_least((146, 0, 0), 146, 1))

  def test_none_version(self):
    self.assertFalse(version_at_least(None, 146, 1))


class TestPruneRevocationList(unittest.TestCase):
  """Test cases for prune_revocation_list."""

  def test_empty_list(self):
    self.assertEqual(prune_revocation_list([], 146, 0), [])

  def test_all_above_vmin(self):
    entries = [
        {
            'version': '147.0.0',
            'reason': 'test'
        },
        {
            'version': '148.2.0',
            'reason': 'test'
        },
    ]
    result = prune_revocation_list(entries, 146, 0)
    self.assertEqual(len(result), 2)

  def test_all_below_vmin(self):
    entries = [
        {
            'version': '130.0.0',
            'reason': 'old'
        },
        {
            'version': '140.5.0',
            'reason': 'old'
        },
    ]
    result = prune_revocation_list(entries, 146, 0)
    self.assertEqual(len(result), 0)

  def test_mixed(self):
    entries = [
        {
            'version': '130.0.0',
            'reason': 'old'
        },
        {
            'version': '146.0.0',
            'reason': 'at_boundary'
        },
        {
            'version': '148.2.0',
            'reason': 'new'
        },
    ]
    result = prune_revocation_list(entries, 146, 0)
    self.assertEqual(len(result), 2)
    self.assertEqual(result[0]['version'], '146.0.0')
    self.assertEqual(result[1]['version'], '148.2.0')

  def test_unparseable_version_kept(self):
    entries = [
        {
            'version': 'not_a_version',
            'reason': 'unparseable'
        },
        {
            'version': '148.0.0',
            'reason': 'valid'
        },
    ]
    result = prune_revocation_list(entries, 146, 0)
    self.assertEqual(len(result), 2)

  def test_minor_version_boundary(self):
    entries = [
        {
            'version': '146.0.0',
            'reason': 'below'
        },
        {
            'version': '146.1.0',
            'reason': 'at'
        },
        {
            'version': '146.2.0',
            'reason': 'above'
        },
    ]
    result = prune_revocation_list(entries, 146, 1)
    self.assertEqual(len(result), 2)
    self.assertEqual(result[0]['version'], '146.1.0')
    self.assertEqual(result[1]['version'], '146.2.0')

  def test_main_end_to_end(self):
    """Test the main() function via subprocess to verify CLI arg handling."""
    import subprocess
    import sys

    with tempfile.TemporaryDirectory() as tmpdir:
      # Write input revoked.json with mixed versions.
      revoked = {
          'revoked_versions': [
              {
                  'version': '130.0.0',
                  'reason': 'old',
                  'revoked_at': '2025-01-01T00:00:00Z'
              },
              {
                  'version': '148.2.0',
                  'reason': 'CVE-2026-XXXXX',
                  'revoked_at': '2026-03-05T00:00:00Z'
              },
          ]
      }
      revoked_path = os.path.join(tmpdir, 'revoked.json')
      with open(revoked_path, 'w') as f:
        json.dump(revoked, f)

      # Write minimal api_versions.json.
      api_versions = {'last': '14601', 'min': '13300', 'hashes': {}}
      api_path = os.path.join(tmpdir, 'cef_api_versions.json')
      with open(api_path, 'w') as f:
        json.dump(api_versions, f)

      output_json = os.path.join(tmpdir, 'revocation_list.json')
      output_rc = os.path.join(tmpdir, 'revocation_list.rc')

      script_dir = os.path.dirname(os.path.abspath(__file__))
      script = os.path.join(script_dir, 'make_revocation_resource.py')

      result = subprocess.run([
          sys.executable, script, '--revoked-json', revoked_path,
          '--api-versions', api_path, '--output-json', output_json,
          '--output-rc', output_rc, '--resource-name', 'CEF_REVOCATION_LIST'
      ],
                              capture_output=True,
                              text=True)
      self.assertEqual(result.returncode, 0, result.stderr)

      # Verify JSON output: only version >= 146.1 should remain.
      with open(output_json, 'r') as f:
        output = json.load(f)
      self.assertEqual(len(output['revoked_versions']), 1)
      self.assertEqual(output['revoked_versions'][0]['version'], '148.2.0')

      # Verify RC output.
      with open(output_rc, 'r') as f:
        rc_content = f.read()
      self.assertEqual(rc_content,
                       'CEF_REVOCATION_LIST RCDATA "revocation_list.json"\n')


class TestRevocationResourceFailuresAndBytes(unittest.TestCase):

  def _arguments(self, directory, revoked=None, api_versions=None):
    return [
        '--revoked-json', revoked or testdata_dir('revoked.json'),
        '--api-versions', api_versions or testdata_dir('api_versions.json'),
        '--output-json',
        os.path.join(directory, 'revoked.json'), '--output-rc',
        os.path.join(directory,
                     'revoked.rc'), '--resource-name', 'CEF_REVOCATION_LIST'
    ]

  def test_exact_bytes_override_missing_list_and_stable_rewrite(self):
    with tempfile.TemporaryDirectory() as directory:
      arguments = self._arguments(directory)
      result = run_generator_script('make_revocation_resource.py', *arguments)
      self.assertEqual(result.returncode, 0, result.stderr)
      json_path = os.path.join(directory, 'revoked.json')
      rc_path = os.path.join(directory, 'revoked.rc')
      with open(json_path, encoding='utf-8') as output:
        first_json = output.read()
      with open(rc_path, encoding='utf-8') as output:
        first_rc = output.read()
      self.assertEqual(first_json,
                       read_golden('make_revocation_resource', 'revoked.json'))
      self.assertEqual(first_rc,
                       read_golden('make_revocation_resource', 'revoked.rc'))
      self.assertEqual(
          run_generator_script('make_revocation_resource.py',
                               *arguments).returncode, 0)
      with open(json_path, encoding='utf-8') as output:
        self.assertEqual(output.read(), first_json)

      override = run_generator_script('make_revocation_resource.py', *arguments,
                                      '--api-version', '13300')
      self.assertEqual(override.returncode, 0, override.stderr)
      with open(json_path, encoding='utf-8') as output:
        self.assertEqual(len(json.load(output)['revoked_versions']), 1)

      empty_input = os.path.join(directory, 'empty.json')
      with open(empty_input, 'w', encoding='utf-8') as output:
        json.dump({}, output)
      missing_list = run_generator_script(
          'make_revocation_resource.py',
          *self._arguments(directory, revoked=empty_input))
      self.assertEqual(missing_list.returncode, 0, missing_list.stderr)
      with open(json_path, encoding='utf-8') as output:
        self.assertEqual(json.load(output), {'revoked_versions': []})

  def test_malformed_unreadable_invalid_api_and_argparse_leave_no_outputs(self):
    invalid = run_generator_script('make_revocation_resource.py')
    self.assertEqual(invalid.returncode, 2)
    self.assertIn('required', invalid.stderr)
    with tempfile.TemporaryDirectory() as directory:
      malformed = os.path.join(directory, 'malformed.json')
      with open(malformed, 'w', encoding='utf-8') as output:
        output.write('{invalid')
      for revoked, api_versions in ((malformed,
                                     testdata_dir('api_versions.json')),
                                    ('/missing/revoked.json',
                                     testdata_dir('api_versions.json')),
                                    (testdata_dir('revoked.json'),
                                     malformed), (testdata_dir('revoked.json'),
                                                  '/missing/api.json')):
        with self.subTest(revoked=revoked, api_versions=api_versions):
          result = run_generator_script(
              'make_revocation_resource.py',
              *self._arguments(directory, revoked, api_versions))
          self.assertEqual(result.returncode, 1)
          self.assertFalse(os.path.exists(os.path.join(directory,
                                                       'revoked.rc')))

  def test_rc_failure_leaves_json_partial_output(self):
    with tempfile.TemporaryDirectory() as directory:
      argv = ['make_revocation_resource.py', *self._arguments(directory)]
      with mock.patch('make_revocation_resource.sys.argv', argv), mock.patch(
          'make_revocation_resource.write_rc_file',
          side_effect=OSError('fixture rc failure')):
        with self.assertRaisesRegex(OSError, 'fixture rc failure'):
          main()
      self.assertTrue(os.path.isfile(os.path.join(directory, 'revoked.json')))
      self.assertFalse(os.path.exists(os.path.join(directory, 'revoked.rc')))


if __name__ == '__main__':
  unittest.main()
