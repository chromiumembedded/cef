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


if __name__ == '__main__':
  unittest.main()
