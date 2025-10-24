#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""
Unit tests for patch_utils.py

Running the tests:
  # Run all tests with verbose output
  python3 -m unittest patch_utils_test.py -v

  # Run all tests (simple)
  python3 patch_utils_test.py

  # Run a specific test
  python3 -m unittest patch_utils_test.TestDetectFileMovement.test_detect_rename_with_r_status -v
"""

import unittest
import unittest.mock
import subprocess
from patch_utils import detect_file_movement


class TestDetectFileMovement(unittest.TestCase):
    """Test cases for detect_file_movement function"""

    def test_detect_rename_with_r_status(self):
        """Test detecting file rename from git diff R status"""
        # Mock subprocess.run to return rename entry
        mock_result = unittest.mock.MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = 'R095\tchrome/browser/ui/views/frame/browser_view.cc\tchrome/browser/ui/views/frame/layout/browser_view.cc\n'

        with unittest.mock.patch('patch_utils.subprocess.run', return_value=mock_result) as mock_run:
            new_location = detect_file_movement(
                'chrome/browser/ui/views/frame/browser_view.cc',
                '142.0.7444.0',
                '143.0.7491.0',
                '/test/chromium/src'
            )

            # Verify git was called with correct arguments
            self.assertTrue(mock_run.called)
            call_args = mock_run.call_args[0][0]
            self.assertIn('git', call_args)
            self.assertIn('diff', call_args)
            self.assertIn('--name-status', call_args)
            self.assertIn('-M', call_args)
            self.assertIn('refs/tags/142.0.7444.0', call_args)
            self.assertIn('refs/tags/143.0.7491.0', call_args)

            # Verify new location was detected
            self.assertEqual(new_location, 'chrome/browser/ui/views/frame/layout/browser_view.cc')

    def test_detect_rename_with_high_similarity(self):
        """Test detecting rename with R100 (perfect match)"""
        mock_result = unittest.mock.MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = 'R100\told/path/file.cc\tnew/path/file.cc\n'

        with unittest.mock.patch('patch_utils.subprocess.run', return_value=mock_result):
            new_location = detect_file_movement(
                'old/path/file.cc',
                '142.0.0.0',
                '143.0.0.0',
                '/test/src'
            )

            self.assertEqual(new_location, 'new/path/file.cc')

    def test_detect_delete_add_pair(self):
        """Test detecting file movement from D/A pair"""
        # File was deleted and added with same basename in new location
        mock_result = unittest.mock.MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = '''D\tchrome/browser/ui/file.cc
A\tchrome/browser/ui/newdir/file.cc
A\tchrome/browser/ui/newdir/other.cc
'''

        with unittest.mock.patch('patch_utils.subprocess.run', return_value=mock_result):
            new_location = detect_file_movement(
                'chrome/browser/ui/file.cc',
                '142.0.0.0',
                '143.0.0.0',
                '/test/src'
            )

            # Should match the added file with same basename
            self.assertEqual(new_location, 'chrome/browser/ui/newdir/file.cc')

    def test_delete_add_pair_prefers_matching_basename(self):
        """Test that D/A matching prefers files with matching basename"""
        mock_result = unittest.mock.MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = '''D\tchrome/browser/ui/foo.cc
A\tchrome/browser/ui/new/bar.cc
A\tchrome/browser/ui/new/foo.cc
A\tchrome/browser/ui/new/baz.cc
'''

        with unittest.mock.patch('patch_utils.subprocess.run', return_value=mock_result):
            new_location = detect_file_movement(
                'chrome/browser/ui/foo.cc',
                '142.0.0.0',
                '143.0.0.0',
                '/test/src'
            )

            # Should match foo.cc, not bar.cc or baz.cc
            self.assertEqual(new_location, 'chrome/browser/ui/new/foo.cc')

    def test_no_movement_detected_file_deleted(self):
        """Test when file is deleted with no replacement"""
        mock_result = unittest.mock.MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = 'D\tchrome/browser/deleted_file.cc\n'

        with unittest.mock.patch('patch_utils.subprocess.run', return_value=mock_result):
            new_location = detect_file_movement(
                'chrome/browser/deleted_file.cc',
                '142.0.0.0',
                '143.0.0.0',
                '/test/src'
            )

            # No matching added file, should return None
            self.assertIsNone(new_location)

    def test_no_movement_detected_different_file(self):
        """Test when requested file is not in git diff output"""
        mock_result = unittest.mock.MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = 'R100\tother/file.cc\tother/new/file.cc\n'

        with unittest.mock.patch('patch_utils.subprocess.run', return_value=mock_result):
            new_location = detect_file_movement(
                'chrome/browser/my_file.cc',
                '142.0.0.0',
                '143.0.0.0',
                '/test/src'
            )

            # Different file in output, should return None
            self.assertIsNone(new_location)

    def test_git_timeout_handling(self):
        """Test graceful handling of git command timeout"""
        with unittest.mock.patch('patch_utils.subprocess.run',
                                side_effect=subprocess.TimeoutExpired('git', 3)):
            new_location = detect_file_movement(
                'chrome/browser/ui/file.cc',
                '142.0.0.0',
                '143.0.0.0',
                '/test/src'
            )

            # Should handle timeout gracefully and return None
            self.assertIsNone(new_location)

    def test_git_command_failure(self):
        """Test handling of git command returning error code"""
        mock_result = unittest.mock.MagicMock()
        mock_result.returncode = 1
        mock_result.stdout = ''

        with unittest.mock.patch('patch_utils.subprocess.run', return_value=mock_result):
            new_location = detect_file_movement(
                'chrome/browser/ui/file.cc',
                '142.0.0.0',
                '143.0.0.0',
                '/test/src'
            )

            # Should handle failure gracefully and return None
            self.assertIsNone(new_location)

    def test_git_not_found(self):
        """Test handling when git command is not found"""
        with unittest.mock.patch('patch_utils.subprocess.run',
                                side_effect=FileNotFoundError('git not found')):
            new_location = detect_file_movement(
                'chrome/browser/ui/file.cc',
                '142.0.0.0',
                '143.0.0.0',
                '/test/src'
            )

            # Should handle missing git gracefully and return None
            self.assertIsNone(new_location)

    def test_missing_old_version_returns_none(self):
        """Test that missing old_version parameter returns None"""
        with unittest.mock.patch('patch_utils.subprocess.run') as mock_run:
            new_location = detect_file_movement(
                'chrome/browser/ui/file.cc',
                None,  # No old version
                '143.0.0.0',
                '/test/src'
            )

            # Should not call git and return None
            self.assertFalse(mock_run.called)
            self.assertIsNone(new_location)

    def test_missing_new_version_returns_none(self):
        """Test that missing new_version parameter returns None"""
        with unittest.mock.patch('patch_utils.subprocess.run') as mock_run:
            new_location = detect_file_movement(
                'chrome/browser/ui/file.cc',
                '142.0.0.0',
                None,  # No new version
                '/test/src'
            )

            # Should not call git and return None
            self.assertFalse(mock_run.called)
            self.assertIsNone(new_location)

    def test_missing_project_root_returns_none(self):
        """Test that missing project_root parameter returns None"""
        with unittest.mock.patch('patch_utils.subprocess.run') as mock_run:
            new_location = detect_file_movement(
                'chrome/browser/ui/file.cc',
                '142.0.0.0',
                '143.0.0.0',
                None  # No project root
            )

            # Should not call git and return None
            self.assertFalse(mock_run.called)
            self.assertIsNone(new_location)

    def test_multiple_renames_in_output(self):
        """Test parsing when git diff contains multiple renames"""
        mock_result = unittest.mock.MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = '''R100\tchrome/browser/ui/file1.cc\tchrome/browser/ui/new/file1.cc
R095\tchrome/browser/ui/file2.cc\tchrome/browser/ui/new/file2.cc
R090\tchrome/browser/ui/file3.cc\tchrome/browser/ui/new/file3.cc
'''

        with unittest.mock.patch('patch_utils.subprocess.run', return_value=mock_result):
            new_location = detect_file_movement(
                'chrome/browser/ui/file2.cc',
                '142.0.0.0',
                '143.0.0.0',
                '/test/src'
            )

            # Should find the correct rename for file2.cc
            self.assertEqual(new_location, 'chrome/browser/ui/new/file2.cc')

    def test_wildcard_pattern_in_git_command(self):
        """Test that git command uses wildcard pattern for subdirectory search"""
        mock_result = unittest.mock.MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = 'R100\tchrome/browser/ui/views/file.cc\tchrome/browser/ui/views/layout/file.cc\n'

        with unittest.mock.patch('patch_utils.subprocess.run', return_value=mock_result) as mock_run:
            new_location = detect_file_movement(
                'chrome/browser/ui/views/file.cc',
                '142.0.0.0',
                '143.0.0.0',
                '/test/src'
            )

            # Verify wildcard pattern was used
            call_args = mock_run.call_args[0][0]
            pattern_arg = None
            for i, arg in enumerate(call_args):
                if arg == '--' and i + 1 < len(call_args):
                    pattern_arg = call_args[i + 1]
                    break

            self.assertIsNotNone(pattern_arg)
            self.assertIn('*', pattern_arg)
            self.assertIn('file.cc', pattern_arg)

            # Verify detection worked
            self.assertEqual(new_location, 'chrome/browser/ui/views/layout/file.cc')

    def test_empty_git_output(self):
        """Test handling of empty git output"""
        mock_result = unittest.mock.MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = ''

        with unittest.mock.patch('patch_utils.subprocess.run', return_value=mock_result):
            new_location = detect_file_movement(
                'chrome/browser/ui/file.cc',
                '142.0.0.0',
                '143.0.0.0',
                '/test/src'
            )

            # Should return None for empty output
            self.assertIsNone(new_location)

    def test_whitespace_only_git_output(self):
        """Test handling of whitespace-only git output"""
        mock_result = unittest.mock.MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = '   \n\n  \t  \n'

        with unittest.mock.patch('patch_utils.subprocess.run', return_value=mock_result):
            new_location = detect_file_movement(
                'chrome/browser/ui/file.cc',
                '142.0.0.0',
                '143.0.0.0',
                '/test/src'
            )

            # Should return None for whitespace-only output
            self.assertIsNone(new_location)

    def test_malformed_git_output(self):
        """Test handling of malformed git diff output"""
        mock_result = unittest.mock.MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = 'INVALID OUTPUT WITHOUT TABS\n'

        with unittest.mock.patch('patch_utils.subprocess.run', return_value=mock_result):
            new_location = detect_file_movement(
                'chrome/browser/ui/file.cc',
                '142.0.0.0',
                '143.0.0.0',
                '/test/src'
            )

            # Should handle malformed output gracefully
            self.assertIsNone(new_location)


if __name__ == '__main__':
    unittest.main()
