# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
"""
Unit tests for cef_version.py

Running the tests:
  # Run all tests with verbose output
  python3 -m unittest cef_version_test -v

  # Run a specific test class
  python3 -m unittest cef_version_test.TestComputeVersion -v
"""

import unittest
from unittest import mock
import os

from cef_version import VersionFormatter


def _create_formatter(chrome_version=None, cef_version=None, cef_commit=None):
  """Create a VersionFormatter with mocked filesystem checks and
  pre-populated version components to avoid real I/O."""
  chrome_version = chrome_version or {
      'MAJOR': '146',
      'MINOR': '0',
      'BUILD': '7680',
      'PATCH': '0'
  }
  cef_version = cef_version or {'CEF_MAJOR': '3'}
  cef_commit = cef_commit or {'HASH': 'f70f340abcdef', 'NUMBER': '3508'}

  with mock.patch('os.path.isdir', return_value=True):
    formatter = VersionFormatter('/fake/chromium/src')

  # Pre-populate cached state to avoid filesystem/git calls.
  formatter._chrome_version = chrome_version
  formatter._cef_version = cef_version
  formatter._cef_commit = cef_commit
  return formatter


class TestComputeVersionMaster(unittest.TestCase):
  """Test _compute_version behavior on the master branch."""

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_minor_from_api_version(self, mock_read_last, mock_git):
    """On master, MINOR should come from the last API version."""
    mock_git.is_ancestor.return_value = False
    mock_git.get_branch_name.return_value = 'master'
    mock_read_last.return_value = '14601'

    formatter = _create_formatter()
    parts = formatter.get_version_parts()

    self.assertEqual(parts['MAJOR'], 146)
    self.assertEqual(parts['MINOR'], 1)
    self.assertEqual(parts['PATCH'], 0)

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_minor_zero_when_no_versions_file(self, mock_read_last, mock_git):
    """On master, MINOR should be 0 when versions file returns None."""
    mock_git.is_ancestor.return_value = False
    mock_git.get_branch_name.return_value = 'master'
    mock_read_last.return_value = None

    formatter = _create_formatter()
    parts = formatter.get_version_parts()

    self.assertEqual(parts['MINOR'], 0)
    self.assertEqual(parts['PATCH'], 0)

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_minor_zero_when_major_mismatch(self, mock_read_last, mock_git):
    """On master, MINOR should be 0 when API version major doesn't match Chrome major."""
    mock_git.is_ancestor.return_value = False
    mock_git.get_branch_name.return_value = 'master'
    # API version 14501 -> major=145, but Chrome major is 146.
    mock_read_last.return_value = '14501'

    formatter = _create_formatter()
    parts = formatter.get_version_parts()

    self.assertEqual(parts['MINOR'], 0)

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_version_string_format(self, mock_read_last, mock_git):
    """Master version string should include branch name and minor from API."""
    mock_git.is_ancestor.return_value = False
    mock_git.get_branch_name.return_value = 'master'
    mock_read_last.return_value = '14601'

    formatter = _create_formatter()
    version = formatter.get_version_string()

    self.assertEqual(version,
                     '146.1.0-master.3508+gf70f340+chromium-146.0.7680.0')

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_version_string_zero_minor(self, mock_read_last, mock_git):
    """Master version string with no API version should have minor 0."""
    mock_git.is_ancestor.return_value = False
    mock_git.get_branch_name.return_value = 'master'
    mock_read_last.return_value = None

    formatter = _create_formatter()
    version = formatter.get_version_string()

    self.assertEqual(version,
                     '146.0.0-master.3508+gf70f340+chromium-146.0.7680.0')

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_short_version_string(self, mock_read_last, mock_git):
    mock_git.is_ancestor.return_value = False
    mock_git.get_branch_name.return_value = 'master'
    mock_read_last.return_value = '14601'

    formatter = _create_formatter()
    self.assertEqual(formatter.get_short_version_string(), '146.1.0')

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_higher_revision(self, mock_read_last, mock_git):
    """API version with higher revision should be reflected."""
    mock_git.is_ancestor.return_value = False
    mock_git.get_branch_name.return_value = 'master'
    mock_read_last.return_value = '14610'

    formatter = _create_formatter()
    parts = formatter.get_version_parts()

    self.assertEqual(parts['MINOR'], 10)

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_origin_master_prefix_stripped(self, mock_read_last, mock_git):
    """Branch name 'origin/master' should be split to 'master'."""
    mock_git.is_ancestor.return_value = False
    mock_git.get_branch_name.return_value = 'origin/master'
    mock_read_last.return_value = '14601'

    formatter = _create_formatter()
    parts = formatter.get_version_parts()

    self.assertEqual(parts['MINOR'], 1)


class TestComputeVersionReleaseBranch(unittest.TestCase):
  """Test _compute_version behavior on release branches."""

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_release_branch_uses_branch_version(self, mock_read_last, mock_git):
    """On a release branch, MINOR/PATCH come from branch version components."""
    mock_git.is_ancestor.return_value = True
    # Simulate 3 commits since branching: 2nd one changed the API versions file.
    mock_git.get_branch_hashes.return_value = ['aaa', 'bbb', 'ccc']
    mock_git.get_changed_files.side_effect = [
        ['some_file.cc'],
        ['cef_api_versions.json', 'other.cc'],
        ['another.cc'],
    ]
    mock_read_last.return_value = '14601'

    formatter = _create_formatter()
    parts = formatter.get_version_parts()

    # Commit aaa: no API change -> bugfix=1. Commit bbb: API change -> minor=1, bugfix=0.
    # Commit ccc: no API change -> bugfix=1.
    # read_version_last returns '14601' -> major=146, revision=1.
    # revision(1) == minor(1), so no override.
    self.assertEqual(parts['MAJOR'], 146)
    self.assertEqual(parts['MINOR'], 1)
    self.assertEqual(parts['PATCH'], 1)

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_release_branch_version_string_format(self, mock_read_last, mock_git):
    """Release branch version string has no branch name suffix."""
    mock_git.is_ancestor.return_value = True
    mock_git.get_branch_hashes.return_value = ['aaa']
    mock_git.get_changed_files.return_value = ['some_file.cc']
    mock_read_last.return_value = None

    formatter = _create_formatter()
    version = formatter.get_version_string()

    # minor=0, patch=1 (one non-API commit)
    self.assertEqual(version, '146.0.1+gf70f340+chromium-146.0.7680.0')

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_release_branch_override_from_versions_file(self, mock_read_last,
                                                      mock_git):
    """When the versions file has a higher revision, it overrides minor."""
    mock_git.is_ancestor.return_value = True
    # One commit, no API file change -> computed minor=0.
    mock_git.get_branch_hashes.return_value = ['aaa']
    mock_git.get_changed_files.return_value = ['some_file.cc']
    # But versions file says revision=5, which is > computed minor(0).
    mock_read_last.return_value = '14605'

    formatter = _create_formatter()
    parts = formatter.get_version_parts()

    self.assertEqual(parts['MINOR'], 5)
    self.assertEqual(parts['PATCH'], 1)


class TestComputeVersionDetachedHead(unittest.TestCase):
  """Test _compute_version behavior in detached HEAD state."""

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_detached_head_minor_from_api_version(self, mock_read_last, mock_git):
    """Detached HEAD should get MINOR from API version (same as master)."""
    mock_git.is_ancestor.return_value = False
    mock_git.get_branch_name.return_value = 'HEAD'
    mock_read_last.return_value = '14601'

    formatter = _create_formatter()
    parts = formatter.get_version_parts()

    self.assertEqual(parts['MAJOR'], 146)
    self.assertEqual(parts['MINOR'], 1)
    self.assertEqual(parts['PATCH'], 0)

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_detached_head_zero_when_no_versions_file(self, mock_read_last,
                                                    mock_git):
    """Detached HEAD should have MINOR=0 when versions file returns None."""
    mock_git.is_ancestor.return_value = False
    mock_git.get_branch_name.return_value = 'HEAD'
    mock_read_last.return_value = None

    formatter = _create_formatter()
    parts = formatter.get_version_parts()

    self.assertEqual(parts['MINOR'], 0)
    self.assertEqual(parts['PATCH'], 0)

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_detached_head_version_string(self, mock_read_last, mock_git):
    mock_git.is_ancestor.return_value = False
    mock_git.get_branch_name.return_value = 'HEAD'
    mock_read_last.return_value = '14601'

    formatter = _create_formatter()
    version = formatter.get_version_string()

    self.assertEqual(version,
                     '146.1.0-HEAD.3508+gf70f340+chromium-146.0.7680.0')


class TestComputeVersionNamedBranch(unittest.TestCase):
  """Test _compute_version behavior on a named non-master branch."""

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_named_branch_uses_branch_version(self, mock_read_last, mock_git):
    """Named branches compute MINOR/PATCH from branch version components."""
    mock_git.is_ancestor.return_value = False
    mock_git.get_branch_name.return_value = 'my-feature'
    mock_git.get_branch_hashes.return_value = ['aaa', 'bbb']
    mock_git.get_changed_files.side_effect = [
        ['cef_api_versions.json'],
        ['file.cc'],
    ]
    mock_read_last.return_value = '14601'

    formatter = _create_formatter()
    parts = formatter.get_version_parts()

    # aaa: API change -> minor=1, bugfix=0. bbb: no API change -> bugfix=1.
    # revision(1) == minor(1), no override.
    self.assertEqual(parts['MINOR'], 1)
    self.assertEqual(parts['PATCH'], 1)

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_named_branch_version_string(self, mock_read_last, mock_git):
    mock_git.is_ancestor.return_value = False
    mock_git.get_branch_name.return_value = 'my-feature'
    mock_git.get_branch_hashes.return_value = []
    mock_read_last.return_value = None

    formatter = _create_formatter()
    version = formatter.get_version_string()

    self.assertEqual(version,
                     '146.0.0-my-feature.3508+gf70f340+chromium-146.0.7680.0')

  @mock.patch('cef_version.git')
  @mock.patch('cef_version.read_version_last')
  def test_named_branch_parts_match_string(self, mock_read_last, mock_git):
    """version_parts should match what appears in the version string."""
    mock_git.is_ancestor.return_value = False
    mock_git.get_branch_name.return_value = 'my-feature'
    mock_git.get_branch_hashes.return_value = ['aaa']
    mock_git.get_changed_files.return_value = ['cef_api_versions.json']
    mock_read_last.return_value = '14601'

    formatter = _create_formatter()
    parts = formatter.get_version_parts()
    version = formatter.get_version_string()

    # minor=1 (one API change), patch=0
    self.assertEqual(parts['MINOR'], 1)
    self.assertEqual(parts['PATCH'], 0)
    self.assertTrue(version.startswith('146.1.0-'))


class TestComputeOldVersion(unittest.TestCase):
  """Test _compute_old_version behavior."""

  def test_old_version_parts(self):
    formatter = _create_formatter()
    parts = formatter.get_version_parts(oldFormat=True)

    self.assertEqual(parts['MAJOR'], 3)
    self.assertEqual(parts['MINOR'], 7680)
    self.assertEqual(parts['PATCH'], 3508)

  def test_old_version_string(self):
    formatter = _create_formatter()
    version = formatter.get_version_string(oldFormat=True)

    self.assertEqual(version, '3.7680.3508.gf70f340')


class TestFormatCommitHash(unittest.TestCase):
  """Test _format_commit_hash static method."""

  def test_truncates_to_seven_chars(self):
    self.assertEqual(VersionFormatter._format_commit_hash('f70f340abcdef1234'),
                     'gf70f340')

  def test_short_hash(self):
    self.assertEqual(VersionFormatter._format_commit_hash('abc'), 'gabc')


class TestChromiumVersionString(unittest.TestCase):
  """Test get_chromium_version_string."""

  def test_format(self):
    formatter = _create_formatter()
    self.assertEqual(formatter.get_chromium_version_string(), '146.0.7680.0')


if __name__ == '__main__':
  unittest.main()
