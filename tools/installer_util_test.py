#!/usr/bin/env python3
# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
"""
Unit tests for installer_util.py

Running the tests:
  # Run all tests with verbose output
  python3 -m unittest installer_util_test -v

  # Run all tests (simple)
  python3 installer_util_test.py

  # Run a specific test class
  python3 -m unittest installer_util_test.TestComputeVmin -v
"""

import json
import os
import tempfile
import unittest

from installer_util import (
    compute_vmin,
    format_vmin,
    get_api_version_last,
    load_api_versions,
    write_json_output,
    write_rc_file,
)


class TestLoadApiVersions(unittest.TestCase):
  """Test cases for load_api_versions."""

  def test_valid_file(self):
    data = {'last': '14601', 'min': '13300'}
    with tempfile.NamedTemporaryFile(mode='w', suffix='.json',
                                     delete=False) as f:
      json.dump(data, f)
      f.flush()
      path = f.name
    try:
      result = load_api_versions(path)
      self.assertIsNotNone(result)
      self.assertEqual(result['last'], '14601')
    finally:
      os.unlink(path)

  def test_missing_file(self):
    result = load_api_versions('/nonexistent/path.json')
    self.assertIsNone(result)

  def test_invalid_json(self):
    with tempfile.NamedTemporaryFile(mode='w', suffix='.json',
                                     delete=False) as f:
      f.write('not valid json {')
      f.flush()
      path = f.name
    try:
      result = load_api_versions(path)
      self.assertIsNone(result)
    finally:
      os.unlink(path)


class TestGetApiVersionLast(unittest.TestCase):
  """Test cases for get_api_version_last."""

  def test_present(self):
    self.assertEqual(get_api_version_last({'last': '14601'}), '14601')

  def test_missing(self):
    self.assertIsNone(get_api_version_last({}))


class TestComputeVmin(unittest.TestCase):
  """Test cases for compute_vmin."""

  def test_from_last(self):
    api_versions = {'last': '14601'}
    result = compute_vmin(api_versions)
    self.assertEqual(result, (146, 1))

  def test_from_last_zero_minor(self):
    api_versions = {'last': '14600'}
    result = compute_vmin(api_versions)
    self.assertEqual(result, (146, 0))

  def test_from_last_double_digit_minor(self):
    api_versions = {'last': '14610'}
    result = compute_vmin(api_versions)
    self.assertEqual(result, (146, 10))

  def test_with_override(self):
    api_versions = {'last': '14601'}
    result = compute_vmin(api_versions, api_version_override='14500')
    self.assertEqual(result, (145, 0))

  def test_override_takes_precedence(self):
    api_versions = {'last': '14601'}
    result = compute_vmin(api_versions, api_version_override='13300')
    self.assertEqual(result, (133, 0))

  def test_invalid_override(self):
    api_versions = {'last': '14601'}
    result = compute_vmin(api_versions, api_version_override='abc')
    self.assertIsNone(result)

  def test_missing_last(self):
    result = compute_vmin({})
    self.assertIsNone(result)


class TestFormatVmin(unittest.TestCase):
  """Test cases for format_vmin."""

  def test_basic(self):
    self.assertEqual(format_vmin(146, 1), '146.1')

  def test_zero_minor(self):
    self.assertEqual(format_vmin(146, 0), '146.0')

  def test_double_digit_minor(self):
    self.assertEqual(format_vmin(146, 10), '146.10')


class TestWriteJsonOutput(unittest.TestCase):
  """Test cases for write_json_output."""

  def test_creates_file(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      path = os.path.join(tmpdir, 'output.json')
      write_json_output(path, '{"key": "value"}')
      self.assertTrue(os.path.exists(path))
      with open(path, 'r') as f:
        self.assertEqual(f.read(), '{"key": "value"}')

  def test_creates_parent_dirs(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      path = os.path.join(tmpdir, 'sub', 'dir', 'output.json')
      write_json_output(path, '{}')
      self.assertTrue(os.path.exists(path))


class TestWriteRcFile(unittest.TestCase):
  """Test cases for write_rc_file."""

  def test_creates_rc(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      path = os.path.join(tmpdir, 'output.rc')
      write_rc_file(path, 'CEF_INSTALLER_CONFIG', 'config.json')
      self.assertTrue(os.path.exists(path))
      with open(path, 'r') as f:
        content = f.read()
      self.assertEqual(content, 'CEF_INSTALLER_CONFIG RCDATA "config.json"\n')

  def test_different_resource_name(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      path = os.path.join(tmpdir, 'output.rc')
      write_rc_file(path, 'CEF_REVOCATION_LIST', 'revocation_list.json')
      with open(path, 'r') as f:
        content = f.read()
      self.assertEqual(content,
                       'CEF_REVOCATION_LIST RCDATA "revocation_list.json"\n')

  def test_creates_parent_dirs(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      path = os.path.join(tmpdir, 'sub', 'output.rc')
      write_rc_file(path, 'RES', 'data.json')
      self.assertTrue(os.path.exists(path))


if __name__ == '__main__':
  unittest.main()
