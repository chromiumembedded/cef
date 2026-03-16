# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
"""
Unit tests for version_util.py

Running the tests:
  # Run all tests with verbose output
  python3 -m unittest version_util_test -v

  # Run a specific test class
  python3 -m unittest version_util_test.TestVersionMake -v
"""

import unittest
from unittest import mock
from version_util import (
    version_make,
    version_tracked,
    version_parse,
    version_valid,
    version_valid_for_next,
    read_version_last,
    version_label,
    version_as_numeric,
    version_as_variable,
    version_as_metadata,
    EXP_VERSION,
    EXP_NAME,
    NEXT_VERSION,
    NEXT_NAME,
)


class TestVersionMake(unittest.TestCase):
  """Test cases for version_make function."""

  def test_three_digit_major(self):
    """Major < 1000 produces 5-char string with zero-padded major."""
    self.assertEqual(version_make(146, 1), '14601')

  def test_three_digit_major_zero_revision(self):
    self.assertEqual(version_make(133, 0), '13300')

  def test_three_digit_major_two_digit_revision(self):
    self.assertEqual(version_make(133, 4), '13304')

  def test_four_digit_major(self):
    """Major >= 1000 produces 6-char string."""
    self.assertEqual(version_make(1000, 0), '100000')
    self.assertEqual(version_make(1000, 5), '100005')

  def test_single_digit_major(self):
    self.assertEqual(version_make(1, 0), '00100')

  def test_max_revision(self):
    self.assertEqual(version_make(146, 99), '14699')

  def test_invalid_major_zero(self):
    with self.assertRaises(AssertionError):
      version_make(0, 0)

  def test_invalid_major_too_large(self):
    with self.assertRaises(AssertionError):
      version_make(9999, 0)

  def test_invalid_revision_negative(self):
    with self.assertRaises(AssertionError):
      version_make(146, -1)

  def test_invalid_revision_too_large(self):
    with self.assertRaises(AssertionError):
      version_make(146, 100)


class TestVersionTracked(unittest.TestCase):
  """Test cases for version_tracked function."""

  def test_five_char_tracked(self):
    self.assertTrue(version_tracked('14601'))
    self.assertTrue(version_tracked('13300'))

  def test_six_char_tracked(self):
    self.assertTrue(version_tracked('100005'))

  def test_experimental_not_tracked(self):
    self.assertFalse(version_tracked(EXP_VERSION))

  def test_next_not_tracked(self):
    self.assertFalse(version_tracked(NEXT_VERSION))

  def test_too_short(self):
    self.assertFalse(version_tracked('1234'))

  def test_too_long(self):
    self.assertFalse(version_tracked('1234567'))

  def test_non_numeric(self):
    self.assertFalse(version_tracked('abcde'))
    self.assertFalse(version_tracked('1234a'))


class TestVersionParse(unittest.TestCase):
  """Test cases for version_parse function."""

  def test_five_char_version(self):
    """5-char version splits at position 3."""
    major, revision = version_parse('14601')
    self.assertEqual(major, 146)
    self.assertEqual(revision, 1)

  def test_five_char_zero_revision(self):
    major, revision = version_parse('13300')
    self.assertEqual(major, 133)
    self.assertEqual(revision, 0)

  def test_six_char_version(self):
    """6-char version splits at position 4."""
    major, revision = version_parse('100005')
    self.assertEqual(major, 1000)
    self.assertEqual(revision, 5)

  def test_roundtrip_with_version_make(self):
    """version_parse(version_make(m, r)) == (m, r)."""
    for major, rev in [(146, 1), (133, 0), (1, 99), (1000, 50)]:
      with self.subTest(major=major, rev=rev):
        self.assertEqual(version_parse(version_make(major, rev)), (major, rev))

  def test_untracked_version_raises(self):
    with self.assertRaises(AssertionError):
      version_parse(EXP_VERSION)


class TestVersionValid(unittest.TestCase):
  """Test cases for version_valid function."""

  def test_tracked_version_valid(self):
    self.assertTrue(version_valid('14601'))

  def test_experimental_name_valid(self):
    self.assertTrue(version_valid(EXP_NAME))

  def test_next_name_valid(self):
    self.assertTrue(version_valid(NEXT_NAME))

  def test_experimental_number_not_valid(self):
    """Untracked versions must be referenced by name, not number."""
    self.assertFalse(version_valid(EXP_VERSION))

  def test_next_number_not_valid(self):
    self.assertFalse(version_valid(NEXT_VERSION))

  def test_garbage_not_valid(self):
    self.assertFalse(version_valid('garbage'))


class TestVersionValidForNext(unittest.TestCase):
  """Test cases for version_valid_for_next function."""

  def test_experimental_allowed_by_default(self):
    self.assertTrue(version_valid_for_next(EXP_NAME, '14601'))

  def test_experimental_disallowed(self):
    """With allow_exp=False, EXPERIMENTAL falls through to version_parse which asserts."""
    with self.assertRaises(AssertionError):
      version_valid_for_next(EXP_NAME, '14601', allow_exp=False)

  def test_next_never_valid(self):
    self.assertFalse(version_valid_for_next(NEXT_NAME, '14601'))

  def test_same_version_valid(self):
    self.assertTrue(version_valid_for_next('14601', '14601'))

  def test_higher_revision_valid(self):
    self.assertTrue(version_valid_for_next('14602', '14601'))

  def test_lower_version_invalid(self):
    self.assertFalse(version_valid_for_next('14600', '14601'))

  def test_different_major_invalid(self):
    """Must have the same major version number."""
    self.assertFalse(version_valid_for_next('14701', '14601'))

  def test_case_insensitive(self):
    self.assertTrue(version_valid_for_next('experimental', '14601'))
    self.assertTrue(version_valid_for_next('EXPERIMENTAL', '14601'))


class TestReadVersionLast(unittest.TestCase):
  """Test cases for read_version_last function."""

  @mock.patch('version_util.read_json_file')
  def test_returns_last_value(self, mock_read_json):
    mock_read_json.return_value = {
        'hashes': {},
        'last': '14601',
        'min': '13300'
    }
    self.assertEqual(read_version_last('/fake/path.json'), '14601')

  @mock.patch('version_util.read_json_file')
  def test_returns_none_for_empty_file(self, mock_read_json):
    mock_read_json.return_value = {}
    self.assertIsNone(read_version_last('/fake/path.json'))

  @mock.patch('version_util.read_json_file')
  def test_returns_none_for_none(self, mock_read_json):
    mock_read_json.return_value = None
    self.assertIsNone(read_version_last('/fake/path.json'))

  @mock.patch('version_util.read_json_file')
  def test_asserts_on_missing_last_key(self, mock_read_json):
    mock_read_json.return_value = {'hashes': {}, 'min': '13300'}
    with self.assertRaises(AssertionError):
      read_version_last('/fake/path.json')


class TestVersionLabel(unittest.TestCase):
  """Test cases for version_label function."""

  def test_experimental_version(self):
    self.assertEqual(version_label(EXP_VERSION), 'experimental version')

  def test_experimental_name(self):
    self.assertEqual(version_label(EXP_NAME), 'experimental version')

  def test_next_version(self):
    self.assertEqual(version_label(NEXT_VERSION), 'next version')

  def test_next_name(self):
    self.assertEqual(version_label(NEXT_NAME), 'next version')

  def test_tracked_version(self):
    self.assertEqual(version_label('14601'), 'version 14601')


class TestVersionAsNumeric(unittest.TestCase):
  """Test cases for version_as_numeric function."""

  def test_tracked_version(self):
    self.assertEqual(version_as_numeric('14601'), 14601)

  def test_experimental_name(self):
    self.assertEqual(version_as_numeric(EXP_NAME), int(EXP_VERSION))

  def test_next_name(self):
    self.assertEqual(version_as_numeric(NEXT_NAME), int(NEXT_VERSION))

  def test_case_insensitive(self):
    self.assertEqual(version_as_numeric('experimental'), int(EXP_VERSION))

  def test_invalid_raises(self):
    with self.assertRaises(AssertionError):
      version_as_numeric('garbage')


class TestVersionAsVariable(unittest.TestCase):
  """Test cases for version_as_variable function."""

  def test_tracked_version(self):
    self.assertEqual(version_as_variable('14601'), '14601')

  def test_experimental_name(self):
    self.assertEqual(version_as_variable(EXP_NAME), 'CEF_EXPERIMENTAL')

  def test_next_name(self):
    self.assertEqual(version_as_variable(NEXT_NAME), 'CEF_NEXT')

  def test_case_insensitive(self):
    self.assertEqual(version_as_variable('experimental'), 'CEF_EXPERIMENTAL')


class TestVersionAsMetadata(unittest.TestCase):
  """Test cases for version_as_metadata function."""

  def test_tracked_version(self):
    self.assertEqual(version_as_metadata('14601'), '14601')

  def test_experimental_name(self):
    self.assertEqual(version_as_metadata(EXP_NAME), 'experimental')

  def test_next_name(self):
    self.assertEqual(version_as_metadata(NEXT_NAME), 'next')


if __name__ == '__main__':
  unittest.main()
