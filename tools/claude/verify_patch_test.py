#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""
Unit tests for verify_patch.py

Running the tests:
  # Run all tests with verbose output
  python3 -m unittest verify_patch_test.py -v

  # Run all tests (simple)
  python3 verify_patch_test.py

  # Run a specific test class
  python3 -m unittest verify_patch_test.TestPatchVerifier -v

  # Run a specific test
  python3 -m unittest verify_patch_test.TestPatchVerifier.test_patch_with_no_rejects -v
"""

import unittest
import unittest.mock
import os
import tempfile
import shutil
import io
import sys
from verify_patch import PatchVerifier


class TestPatchVerifier(unittest.TestCase):
    """Test cases for PatchVerifier"""

    def setUp(self):
        """Create temporary directories for test fixtures."""
        self.temp_dir = tempfile.mkdtemp(prefix='verify_patch_test_')
        self.patch_dir = os.path.join(self.temp_dir, 'patches')
        self.rej_dir = os.path.join(self.temp_dir, 'rejects')
        os.makedirs(self.patch_dir)
        os.makedirs(self.rej_dir)

    def tearDown(self):
        """Clean up test fixtures."""
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def _check_coverage_quietly(self, verifier):
        """Helper to check patch coverage while suppressing output."""
        with io.StringIO() as buf, unittest.mock.patch('sys.stdout', buf):
            return verifier.check_patch_coverage()

    def _create_patch_file(self, patch_name, files):
        """Helper to create a patch file with specified files.

        Args:
            patch_name: Name of the patch (without .patch extension)
            files: List of file paths to include in the patch
        """
        patch_path = os.path.join(self.patch_dir, f"{patch_name}.patch")
        with open(patch_path, 'w') as f:
            for file_path in files:
                f.write(f"diff --git a/{file_path} b/{file_path}\n")
                f.write(f"index 1234567..abcdefg 100644\n")
                f.write(f"--- a/{file_path}\n")
                f.write(f"+++ b/{file_path}\n")
                f.write("@@ -1,3 +1,4 @@\n")
                f.write(" unchanged line\n")
                f.write("+new line\n")
                f.write(" another line\n")

    def _create_rej_file(self, file_path, hunks):
        """Helper to create a .rej file with specified hunks.

        Args:
            file_path: Path to the file (without .rej extension)
            hunks: List of tuples (old_start, new_start, context)
        """
        full_path = os.path.join(self.rej_dir, file_path)
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        rej_path = full_path + '.rej'

        with open(rej_path, 'w') as f:
            f.write(f"--- {file_path}\n")
            f.write(f"+++ {file_path}\n")
            for old_start, new_start, context in hunks:
                f.write(f"@@ -{old_start},3 +{new_start},4 @@ {context}\n")
                f.write(" unchanged line\n")
                f.write("+rejected line\n")
                f.write(" another line\n")

    def _create_patch_output(self, patch_name, failed_files):
        """Helper to create mock patch_output.txt content.

        Args:
            patch_name: Name of the patch
            failed_files: List of dicts with 'file_path', 'missing' (bool), 'new_location' (str or None), 'hunks' (int)
        """
        lines = []
        lines.append(f"--> Reading patch file /path/to/cef/patch/patches/{patch_name}.patch")
        lines.append(f"--> Applying patch to {self.rej_dir}")

        for file_info in failed_files:
            file_path = file_info['file_path']
            is_missing = file_info.get('missing', False)
            hunks = file_info.get('hunks', 1)

            if is_missing:
                lines.append(f"--> Skipping non-existing file {self.rej_dir}/{file_path}")
            else:
                lines.append(f"patching file {file_path}")
                lines.append(f"Hunk #1 FAILED at 10.")
                lines.append(f"{hunks} out of {hunks} hunks FAILED -- saving rejects to file {file_path}.rej")

        return '\n'.join(lines)

    def test_patch_with_no_rejects(self):
        """Test verification when there are no failed files"""
        self._create_patch_file('test_patch', ['file1.cc', 'file2.h'])
        patch_output = self._create_patch_output('test_patch', [])

        verifier = PatchVerifier('test_patch', patch_output, self.patch_dir)
        result = self._check_coverage_quietly(verifier)

        self.assertTrue(result)

    def test_patch_covers_all_rejects(self):
        """Test verification when patch covers all failed files"""
        self._create_patch_file('test_patch', [
            'chrome/browser/ui/file1.cc',
            'chrome/browser/ui/file2.cc'
        ])
        self._create_rej_file('chrome/browser/ui/file1.cc', [(10, 10, 'context1')])
        self._create_rej_file('chrome/browser/ui/file2.cc', [(20, 20, 'context2')])

        patch_output = self._create_patch_output('test_patch', [
            {'file_path': 'chrome/browser/ui/file1.cc', 'hunks': 1},
            {'file_path': 'chrome/browser/ui/file2.cc', 'hunks': 1}
        ])

        verifier = PatchVerifier('test_patch', patch_output, self.patch_dir)
        result = self._check_coverage_quietly(verifier)

        self.assertTrue(result)

    def test_patch_missing_reject_files(self):
        """Test verification when patch is missing files from failed files"""
        self._create_patch_file('test_patch', ['chrome/browser/ui/file1.cc'])
        self._create_rej_file('chrome/browser/ui/file1.cc', [(10, 10, 'context1')])
        self._create_rej_file('chrome/browser/ui/file2.cc', [(20, 20, 'context2')])
        self._create_rej_file('chrome/browser/ui/file3.cc', [(30, 30, 'context3')])

        patch_output = self._create_patch_output('test_patch', [
            {'file_path': 'chrome/browser/ui/file1.cc', 'hunks': 1},
            {'file_path': 'chrome/browser/ui/file2.cc', 'hunks': 1},
            {'file_path': 'chrome/browser/ui/file3.cc', 'hunks': 1}
        ])

        verifier = PatchVerifier('test_patch', patch_output, self.patch_dir)
        result = self._check_coverage_quietly(verifier)

        self.assertFalse(result)

    def test_get_patch_files(self):
        """Test extraction of files from patch"""
        self._create_patch_file('test_patch', [
            'chrome/browser/ui/file1.cc',
            'chrome/browser/ui/file2.h',
            'content/public/browser/file3.cc'
        ])

        patch_output = self._create_patch_output('test_patch', [])
        verifier = PatchVerifier('test_patch', patch_output, self.patch_dir)
        files = verifier.get_patch_files()

        self.assertEqual(len(files), 3)
        self.assertIn('chrome/browser/ui/file1.cc', files)
        self.assertIn('chrome/browser/ui/file2.h', files)
        self.assertIn('content/public/browser/file3.cc', files)

    def test_find_reject_files(self):
        """Test getting failed files from patch output"""
        self._create_rej_file('chrome/browser/ui/file1.cc', [(10, 10, 'context')])
        self._create_rej_file('chrome/browser/views/file2.cc', [(20, 20, 'context')])
        self._create_rej_file('content/file3.cc', [(30, 30, 'context')])

        # Create a dummy patch file
        self._create_patch_file('test_patch', [])

        patch_output = self._create_patch_output('test_patch', [
            {'file_path': 'chrome/browser/ui/file1.cc', 'hunks': 1},
            {'file_path': 'chrome/browser/views/file2.cc', 'hunks': 1},
            {'file_path': 'content/file3.cc', 'hunks': 1}
        ])

        verifier = PatchVerifier('test_patch', patch_output, self.patch_dir)
        failed_files = verifier.get_failed_files_from_patch_output()

        self.assertEqual(len(failed_files), 3)
        # Check that all failed files were found
        file_paths = [f['file_path'] for f in failed_files]
        self.assertIn('chrome/browser/ui/file1.cc', file_paths)
        self.assertIn('chrome/browser/views/file2.cc', file_paths)
        self.assertIn('content/file3.cc', file_paths)

    def test_patch_file_not_found(self):
        """Test handling of missing patch file"""
        # Don't create a patch file, but try to verify it
        patch_output = self._create_patch_output('nonexistent_patch', [])
        verifier = PatchVerifier('nonexistent_patch', patch_output, self.patch_dir)

        # This should exit with error code 1
        # Suppress the error message output during testing
        with self.assertRaises(SystemExit) as cm:
            with io.StringIO() as buf, unittest.mock.patch('sys.stdout', buf):
                verifier.get_patch_files()

        self.assertEqual(cm.exception.code, 1)

    def test_multiple_hunks_in_reject(self):
        """Test handling of multiple hunks in a single reject file"""
        self._create_rej_file('multi_hunk.cc', [
            (10, 10, 'hunk1'),
            (20, 21, 'hunk2'),
            (30, 33, 'hunk3'),
            (40, 45, 'hunk4')
        ])

        self._create_patch_file('test_patch', ['multi_hunk.cc'])

        patch_output = self._create_patch_output('test_patch', [
            {'file_path': 'multi_hunk.cc', 'hunks': 4}
        ])

        verifier = PatchVerifier('test_patch', patch_output, self.patch_dir)
        result = self._check_coverage_quietly(verifier)

        self.assertTrue(result)

    def test_basename_matching(self):
        """Test that file matching works with basename matching"""
        # Patch might have different path than failed file
        self._create_patch_file('test_patch', ['browser/ui/views/file.cc'])
        self._create_rej_file('chrome/browser/ui/views/file.cc', [(10, 10, 'context')])

        patch_output = self._create_patch_output('test_patch', [
            {'file_path': 'chrome/browser/ui/views/file.cc', 'hunks': 1}
        ])

        verifier = PatchVerifier('test_patch', patch_output, self.patch_dir)
        result = self._check_coverage_quietly(verifier)

        # Should pass because basename matches
        self.assertTrue(result)

    def test_nested_reject_directories(self):
        """Test handling of failed files in nested directory structures"""
        self._create_rej_file('a/b/c/d/file1.cc', [(10, 10, 'context')])
        self._create_rej_file('x/y/z/file2.cc', [(20, 20, 'context')])

        self._create_patch_file('test_patch', [
            'a/b/c/d/file1.cc',
            'x/y/z/file2.cc'
        ])

        patch_output = self._create_patch_output('test_patch', [
            {'file_path': 'a/b/c/d/file1.cc', 'hunks': 1},
            {'file_path': 'x/y/z/file2.cc', 'hunks': 1}
        ])

        verifier = PatchVerifier('test_patch', patch_output, self.patch_dir)
        result = self._check_coverage_quietly(verifier)

        self.assertTrue(result)

    def test_file_movement_detected_and_in_patch(self):
        """Test when file has moved and new location is in patch"""
        # Create patch with the new location
        self._create_patch_file('test_patch', [
            'chrome/browser/ui/views/frame/layout/browser_view.cc'
        ])

        # Create reject file for old location
        self._create_rej_file('chrome/browser/ui/views/frame/browser_view.cc', [(10, 10, 'context')])

        patch_output = self._create_patch_output('test_patch', [
            {'file_path': 'chrome/browser/ui/views/frame/browser_view.cc', 'hunks': 1}
        ])

        verifier = PatchVerifier(
            'test_patch',
            patch_output,
            self.patch_dir,
            project_root='/test/chromium/src',
            old_version='142.0.0.0',
            new_version='143.0.0.0'
        )

        # Mock git diff to show file was renamed
        mock_result = unittest.mock.MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = 'R100\tchrome/browser/ui/views/frame/browser_view.cc\tchrome/browser/ui/views/frame/layout/browser_view.cc\n'

        with unittest.mock.patch('patch_utils.subprocess.run', return_value=mock_result):
            result = self._check_coverage_quietly(verifier)

        # Should pass because new location is in patch
        self.assertTrue(result)

    def test_file_movement_detected_but_not_in_patch(self):
        """Test when file has moved but new location is NOT in patch"""
        # Create patch with different file
        self._create_patch_file('test_patch', [
            'chrome/browser/ui/other/file.cc'
        ])

        # Create reject file for old location
        self._create_rej_file('chrome/browser/ui/views/frame/browser_view.cc', [(10, 10, 'context')])

        patch_output = self._create_patch_output('test_patch', [
            {'file_path': 'chrome/browser/ui/views/frame/browser_view.cc', 'hunks': 1}
        ])

        verifier = PatchVerifier(
            'test_patch',
            patch_output,
            self.patch_dir,
            project_root='/test/chromium/src',
            old_version='142.0.0.0',
            new_version='143.0.0.0'
        )

        # Mock git diff to show file was renamed
        mock_result = unittest.mock.MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = 'R100\tchrome/browser/ui/views/frame/browser_view.cc\tchrome/browser/ui/views/frame/layout/browser_view.cc\n'

        with unittest.mock.patch('patch_utils.subprocess.run', return_value=mock_result):
            result = self._check_coverage_quietly(verifier)

        # Should fail because new location is NOT in patch
        self.assertFalse(result)

    def test_no_version_info_no_movement_detection(self):
        """Test that without version info, basic checking is still done"""
        # Create patch with a file
        self._create_patch_file('test_patch', ['chrome/browser/ui/file1.cc'])

        # Create reject file for different location
        self._create_rej_file('chrome/browser/ui/file2.cc', [(10, 10, 'context')])

        patch_output = self._create_patch_output('test_patch', [
            {'file_path': 'chrome/browser/ui/file2.cc', 'hunks': 1}
        ])

        # No version info provided
        verifier = PatchVerifier(
            'test_patch',
            patch_output,
            self.patch_dir
        )

        with unittest.mock.patch('patch_utils.subprocess.run') as mock_run:
            result = self._check_coverage_quietly(verifier)

            # Git should not be called without version info
            self.assertFalse(mock_run.called)

            # Should fail because file2.cc is not in patch
            self.assertFalse(result)


if __name__ == '__main__':
    unittest.main()
