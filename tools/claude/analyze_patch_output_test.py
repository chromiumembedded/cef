#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""
Unit tests for analyze_patch_output.py

Running the tests:
  # Run all tests with verbose output
  python3 -m unittest analyze_patch_output_test.py -v

  # Run all tests (simple)
  python3 analyze_patch_output_test.py

  # Run a specific test class
  python3 -m unittest analyze_patch_output_test.TestPatchOutputAnalyzer -v

  # Run a specific test
  python3 -m unittest analyze_patch_output_test.TestPatchOutputAnalyzer.test_failed_hunk_uppercase -v
"""

import unittest
import unittest.mock
import json
from analyze_patch_output import PatchOutputAnalyzer


class TestPatchOutputAnalyzer(unittest.TestCase):
    """Test cases for PatchOutputAnalyzer"""

    def test_successful_patch(self):
        """Test parsing a successful patch"""
        output = """
--> Reading patch file /path/to/patches/gritsettings.patch
--> Reverting changes to /path/to/resource_ids.spec
--> Applying patch to /path/to/src
patching file 'tools/gritsettings/resource_ids.spec'
--> Saving changes to /path/to/patches/gritsettings.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 1)
        patch = analyzer.patches[0]
        self.assertEqual(patch.patch_name, 'gritsettings')
        self.assertEqual(patch.status, 'success')
        self.assertEqual(len(patch.files_processed), 1)
        self.assertEqual(len(patch.files_with_failures), 0)
        self.assertEqual(patch.total_hunks_failed, 0)

    def test_patch_with_offset(self):
        """Test parsing a patch that succeeds with offsets"""
        output = """
--> Reading patch file /path/to/patches/test.patch
--> Applying patch to /path/to/src
patching file 'test.cc'
Hunk #1 succeeded at 100 (offset 5 lines)
Hunk #2 succeeded at 200 with fuzz 2 (offset 10 lines)
--> Saving changes to /path/to/patches/test.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 1)
        patch = analyzer.patches[0]
        self.assertEqual(patch.status, 'offset')

    def test_failed_hunk_uppercase(self):
        """Test parsing failed hunk with uppercase FAILED"""
        output = """
--> Reading patch file /path/to/patches/runhooks.patch
--> Applying patch to /path/to/src
patching file 'build/toolchain/win/setup_toolchain.py'
Hunk #1 FAILED at 166
1 out of 1 hunk FAILED -- saving rejects to file build/toolchain/win/setup_toolchain.py.rej
--> Saving changes to /path/to/patches/runhooks.patch
!!!! WARNING: Failed to apply runhooks
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 1)
        patch = analyzer.patches[0]
        self.assertEqual(patch.status, 'failed')
        self.assertEqual(len(patch.files_with_failures), 1)
        self.assertEqual(patch.total_hunks_failed, 1)

        failure = patch.files_with_failures[0]
        self.assertEqual(failure.file_path, 'build/toolchain/win/setup_toolchain.py')
        self.assertEqual(failure.total_hunks_failed, 1)
        self.assertEqual(failure.reject_file, 'build/toolchain/win/setup_toolchain.py.rej')
        self.assertEqual(len(failure.failed_hunks), 1)
        self.assertEqual(failure.failed_hunks[0].hunk_number, 1)
        self.assertEqual(failure.failed_hunks[0].line_number, 166)

    def test_failed_hunk_lowercase(self):
        """Test parsing failed hunk with lowercase failed"""
        output = """
--> Reading patch file /path/to/patches/test.patch
--> Applying patch to /path/to/src
patching file 'chrome/browser/ui/browser.cc'
No such line 615 in input file, ignoring
1 out of 7 hunks failed--saving rejects to 'chrome/browser/ui/browser.cc.rej'
--> Saving changes to /path/to/patches/test.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 1)
        patch = analyzer.patches[0]
        self.assertEqual(patch.status, 'failed')
        self.assertEqual(len(patch.files_with_failures), 1)

        failure = patch.files_with_failures[0]
        self.assertEqual(failure.file_path, 'chrome/browser/ui/browser.cc')
        self.assertEqual(failure.total_hunks_failed, 1)
        self.assertEqual(failure.reject_file, "'chrome/browser/ui/browser.cc.rej'")

    def test_missing_file_skipping(self):
        """Test parsing missing files that result in 'hunks ignored'"""
        output = """
--> Reading patch file /path/to/patches/chrome_runtime_views.patch
--> Reverting changes to /path/to/browser_view.cc
--> Skipping non-existing file /path/to/src/chrome/browser/ui/views/frame/browser_view_layout.cc
--> Skipping non-existing file /path/to/src/chrome/browser/ui/views/frame/browser_view_layout.h
--> Applying patch to /path/to/src
patching file 'chrome/browser/ui/views/frame/browser_view.cc'
No file to patch.  Skipping...
5 out of 5 hunks ignored--saving rejects to 'chrome/browser/ui/views/frame/browser_view_layout.cc.rej'
No file to patch.  Skipping...
2 out of 2 hunks ignored--saving rejects to 'chrome/browser/ui/views/frame/browser_view_layout.h.rej'
--> Saving changes to /path/to/patches/chrome_runtime_views.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 1)
        patch = analyzer.patches[0]
        self.assertEqual(patch.status, 'failed')
        self.assertEqual(len(patch.files_with_failures), 2)

        # Files with "hunks ignored" create FileFailure objects
        self.assertEqual(patch.files_with_failures[0].file_path,
                        'chrome/browser/ui/views/frame/browser_view_layout.cc')
        self.assertEqual(patch.files_with_failures[0].total_hunks_failed, 5)
        self.assertEqual(patch.files_with_failures[1].file_path,
                        'chrome/browser/ui/views/frame/browser_view_layout.h')
        self.assertEqual(patch.files_with_failures[1].total_hunks_failed, 2)

    def test_missing_file_ignored_hunks(self):
        """Test parsing missing files that result in ignored hunks"""
        output = """
--> Reading patch file /path/to/patches/test.patch
--> Applying patch to /path/to/src
patching file 'file1.cc'
No file to patch.  Skipping...
7 out of 7 hunks ignored--saving rejects to 'chrome/browser/ui/views/frame/browser_view_layout.cc.rej'
--> Saving changes to /path/to/patches/test.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        # Note: "ignored" hunks are typically for missing files, but the analyzer
        # currently only catches them via "Skipping non-existing file"
        # This test documents current behavior
        self.assertEqual(len(analyzer.patches), 1)

    def test_multiple_failed_hunks_one_file(self):
        """Test parsing multiple failed hunks in one file"""
        output = """
--> Reading patch file /path/to/patches/chrome_runtime_views.patch
--> Applying patch to /path/to/src
patching file 'chrome/browser/ui/views/frame/browser_view.cc'
Hunk #2 FAILED at 772
Hunk #3 FAILED at 921
Hunk #5 FAILED at 1095
9 out of 20 hunks FAILED -- saving rejects to file chrome/browser/ui/views/frame/browser_view.cc.rej
--> Saving changes to /path/to/patches/chrome_runtime_views.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 1)
        patch = analyzer.patches[0]
        self.assertEqual(patch.status, 'failed')
        self.assertEqual(patch.total_hunks_failed, 9)

        failure = patch.files_with_failures[0]
        self.assertEqual(failure.file_path, 'chrome/browser/ui/views/frame/browser_view.cc')
        self.assertEqual(failure.total_hunks_failed, 9)
        self.assertEqual(len(failure.failed_hunks), 3)  # Individual hunks tracked
        self.assertEqual(failure.failed_hunks[0].hunk_number, 2)
        self.assertEqual(failure.failed_hunks[0].line_number, 772)

    def test_multiple_files_failed_one_patch(self):
        """Test parsing multiple file failures in one patch"""
        output = """
--> Reading patch file /path/to/patches/chrome_browser_browser.patch
--> Applying patch to /path/to/src
patching file 'chrome/browser/devtools/devtools_window.cc'
Hunk #4 FAILED at 1932
1 out of 4 hunks FAILED -- saving rejects to file chrome/browser/devtools/devtools_window.cc.rej
patching file 'chrome/browser/ui/browser.cc'
Hunk #3 FAILED at 686
1 out of 26 hunks FAILED -- saving rejects to file chrome/browser/ui/browser.cc.rej
--> Saving changes to /path/to/patches/chrome_browser_browser.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 1)
        patch = analyzer.patches[0]
        self.assertEqual(patch.status, 'failed')
        self.assertEqual(len(patch.files_with_failures), 2)
        self.assertEqual(patch.total_hunks_failed, 2)

        self.assertEqual(patch.files_with_failures[0].file_path,
                        'chrome/browser/devtools/devtools_window.cc')
        self.assertEqual(patch.files_with_failures[0].total_hunks_failed, 1)

        self.assertEqual(patch.files_with_failures[1].file_path,
                        'chrome/browser/ui/browser.cc')
        self.assertEqual(patch.files_with_failures[1].total_hunks_failed, 1)

    def test_multiple_patches(self):
        """Test parsing multiple patches"""
        output = """
--> Reading patch file /path/to/patches/patch1.patch
--> Applying patch to /path/to/src
patching file 'file1.cc'
--> Saving changes to /path/to/patches/patch1.patch

--> Reading patch file /path/to/patches/patch2.patch
--> Applying patch to /path/to/src
patching file 'file2.cc'
Hunk #1 FAILED at 100
1 out of 1 hunk FAILED -- saving rejects to file file2.cc.rej
--> Saving changes to /path/to/patches/patch2.patch

--> Reading patch file /path/to/patches/patch3.patch
--> Applying patch to /path/to/src
patching file 'file3.cc'
--> Saving changes to /path/to/patches/patch3.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 3)
        self.assertEqual(analyzer.patches[0].status, 'success')
        self.assertEqual(analyzer.patches[1].status, 'failed')
        self.assertEqual(analyzer.patches[2].status, 'success')

    def test_statistics_calculation(self):
        """Test statistics calculation"""
        output = """
--> Reading patch file /path/to/patches/success.patch
--> Applying patch to /path/to/src
patching file 'file1.cc'
--> Saving changes to /path/to/patches/success.patch

--> Reading patch file /path/to/patches/offset.patch
--> Applying patch to /path/to/src
patching file 'file2.cc'
Hunk #1 succeeded at 100 (offset 5 lines)
--> Saving changes to /path/to/patches/offset.patch

--> Reading patch file /path/to/patches/failed.patch
--> Applying patch to /path/to/src
patching file 'file3.cc'
Hunk #1 FAILED at 200
1 out of 2 hunks FAILED -- saving rejects to file file3.cc.rej
--> Saving changes to /path/to/patches/failed.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()
        stats = analyzer.get_statistics()

        self.assertEqual(stats['total_patches'], 3)
        self.assertEqual(stats['successful'], 1)
        self.assertEqual(stats['with_offsets'], 1)
        self.assertEqual(stats['failed'], 1)
        self.assertEqual(stats['total_files_with_failures'], 1)
        self.assertEqual(stats['total_hunks_failed'], 1)
        self.assertAlmostEqual(stats['success_rate'], 33.3, places=1)

    def test_json_output(self):
        """Test JSON report generation"""
        output = """
--> Reading patch file /path/to/patches/test.patch
--> Applying patch to /path/to/src
patching file 'test.cc'
Hunk #1 FAILED at 100
1 out of 1 hunk FAILED -- saving rejects to file test.cc.rej
--> Saving changes to /path/to/patches/test.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        json_output = analyzer.generate_json_report()
        data = json.loads(json_output)

        self.assertEqual(data['status'], 'partial_failure')
        self.assertEqual(data['statistics']['total_patches'], 1)
        self.assertEqual(data['statistics']['failed'], 1)
        self.assertEqual(len(data['patches']), 1)

        patch_data = data['patches'][0]
        self.assertEqual(patch_data['patch_name'], 'test')
        self.assertEqual(patch_data['status'], 'failed')
        self.assertEqual(len(patch_data['failures']), 1)

        failure = patch_data['failures'][0]
        self.assertEqual(failure['file_path'], 'test.cc')
        self.assertEqual(failure['total_hunks_failed'], 1)
        self.assertEqual(failure['reject_file'], 'test.cc.rej')
        self.assertFalse(failure['missing'])

    def test_summary_report_generation(self):
        """Test summary report generation (basic smoke test)"""
        output = """
--> Reading patch file /path/to/patches/test.patch
--> Applying patch to /path/to/src
patching file 'test.cc'
--> Saving changes to /path/to/patches/test.patch
"""
        analyzer = PatchOutputAnalyzer(output,
                                      old_version='139.0.0.0',
                                      new_version='140.0.0.0')
        analyzer.parse()

        summary = analyzer.generate_summary_report(colorize=False)

        self.assertIn('PATCH UPDATE SUMMARY', summary)
        self.assertIn('Total patches: 1', summary)
        self.assertIn('Successful: 1', summary)
        self.assertIn('ALL PATCHES APPLIED SUCCESSFULLY', summary)

    def test_fix_plan_generation(self):
        """Test fix plan generation"""
        output = """
--> Reading patch file /path/to/patches/test.patch
--> Applying patch to /path/to/src
patching file 'test.cc'
Hunk #1 FAILED at 100
1 out of 1 hunk FAILED -- saving rejects to file test.cc.rej
--> Saving changes to /path/to/patches/test.patch
"""
        analyzer = PatchOutputAnalyzer(output,
                                      old_version='139.0.0.0',
                                      new_version='140.0.0.0')
        analyzer.parse()

        fix_plan = analyzer.generate_fix_plan()

        self.assertIn('SYSTEMATIC FIX PLAN', fix_plan)
        self.assertIn('PATCH 1/1: test', fix_plan)
        self.assertIn('FILE 1.1: test.cc', fix_plan)
        self.assertIn('1 hunk(s) failed', fix_plan)
        self.assertIn('test.cc.rej', fix_plan)

    def test_mixed_failures(self):
        """Test patch with both missing files and hunk failures"""
        output = """
--> Reading patch file /path/to/patches/mixed.patch
--> Skipping non-existing file /path/to/src/missing.cc
--> Applying patch to /path/to/src
No file to patch.  Skipping...
3 out of 3 hunks ignored--saving rejects to 'missing.cc.rej'
patching file 'exists.cc'
Hunk #1 FAILED at 50
1 out of 2 hunks FAILED -- saving rejects to file exists.cc.rej
--> Saving changes to /path/to/patches/mixed.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 1)
        patch = analyzer.patches[0]
        self.assertEqual(patch.status, 'failed')
        self.assertEqual(len(patch.files_with_failures), 2)

        # First failure should be the missing file (from "hunks ignored")
        self.assertEqual(patch.files_with_failures[0].file_path, 'missing.cc')
        self.assertEqual(patch.files_with_failures[0].total_hunks_failed, 3)

        # Second failure should be the hunk failure
        self.assertEqual(patch.files_with_failures[1].file_path, 'exists.cc')
        self.assertEqual(patch.files_with_failures[1].total_hunks_failed, 1)

    def test_files_processed_tracking(self):
        """Test that files_processed list is populated correctly"""
        output = """
--> Reading patch file /path/to/patches/multifile.patch
--> Applying patch to /path/to/src
patching file 'file1.cc'
patching file 'file2.h'
patching file 'file3.cc'
--> Saving changes to /path/to/patches/multifile.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 1)
        patch = analyzer.patches[0]
        self.assertEqual(len(patch.files_processed), 3)
        self.assertIn("'file1.cc'", patch.files_processed)
        self.assertIn("'file2.h'", patch.files_processed)
        self.assertIn("'file3.cc'", patch.files_processed)

    def test_warning_detection(self):
        """Test that WARNING messages set failed status"""
        output = """
--> Reading patch file /path/to/patches/test.patch
--> Applying patch to /path/to/src
patching file 'test.cc'
--> Saving changes to /path/to/patches/test.patch
!!!! WARNING: Failed to apply test
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 1)
        patch = analyzer.patches[0]
        self.assertEqual(patch.status, 'failed')

    def test_empty_output(self):
        """Test handling of empty output"""
        analyzer = PatchOutputAnalyzer("")
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 0)
        stats = analyzer.get_statistics()
        self.assertEqual(stats['total_patches'], 0)
        self.assertEqual(stats['success_rate'], 0)

    def test_patch_name_extraction(self):
        """Test that patch names are extracted correctly from paths"""
        output = """
--> Reading patch file /Users/test/chromium/src/cef/patch/patches/my_test_patch.patch
--> Applying patch to /path/to/src
patching file 'test.cc'
--> Saving changes to /Users/test/chromium/src/cef/patch/patches/my_test_patch.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 1)
        self.assertEqual(analyzer.patches[0].patch_name, 'my_test_patch')
        self.assertEqual(analyzer.patches[0].patch_file, 'cef/patch/patches/my_test_patch.patch')


class TestRealWorldSamples(unittest.TestCase):
    """Test cases based on real patch_updater.py output samples"""

    def test_chrome_runtime_views_from_patch_output_txt(self):
        """Test parsing chrome_runtime_views from patch_output.txt format"""
        output = """
--> Reading patch file /path/to/patches/chrome_runtime_views.patch
--> Reverting changes to /path/to/chrome/browser/ui/browser_command_controller.cc
--> Skipping non-existing file /path/to/src/chrome/browser/ui/views/frame/browser_view_layout.cc
--> Skipping non-existing file /path/to/src/chrome/browser/ui/views/frame/browser_view_layout.h
--> Skipping non-existing file /path/to/src/chrome/browser/ui/views/frame/browser_view_layout_delegate.h
--> Applying patch to /path/to/src
No file to patch.  Skipping...
7 out of 7 hunks ignored--saving rejects to 'chrome/browser/ui/views/frame/browser_view_layout.cc.rej'
No file to patch.  Skipping...
2 out of 2 hunks ignored--saving rejects to 'chrome/browser/ui/views/frame/browser_view_layout.h.rej'
No file to patch.  Skipping...
1 out of 1 hunks ignored--saving rejects to 'chrome/browser/ui/views/frame/browser_view_layout_delegate.h.rej'
patching file 'chrome/browser/ui/views/frame/browser_widget.cc'
No such line 615 in input file, ignoring
1 out of 7 hunks failed--saving rejects to 'chrome/browser/ui/views/frame/browser_widget.cc.rej'
--> Saving changes to /path/to/patches/chrome_runtime_views.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 1)
        patch = analyzer.patches[0]
        self.assertEqual(patch.patch_name, 'chrome_runtime_views')
        self.assertEqual(patch.status, 'failed')
        self.assertEqual(len(patch.files_with_failures), 4)

        # Check that we have 3 missing files (from "hunks ignored") + 1 hunk failure
        self.assertEqual(patch.files_with_failures[0].total_hunks_failed, 7)
        self.assertEqual(patch.files_with_failures[1].total_hunks_failed, 2)
        self.assertEqual(patch.files_with_failures[2].total_hunks_failed, 1)
        self.assertEqual(patch.files_with_failures[3].total_hunks_failed, 1)

    def test_viz_osr_2575_multiple_files(self):
        """Test parsing viz_osr_2575 with many file failures"""
        output = """
--> Reading patch file /path/to/patches/viz_osr_2575.patch
--> Applying patch to /path/to/src
patching file 'components/viz/host/host_display_client.cc'
2 out of 2 hunks failed--saving rejects to 'components/viz/host/host_display_client.cc.rej'
patching file 'components/viz/host/host_display_client.h'
1 out of 1 hunks failed--saving rejects to 'components/viz/host/host_display_client.h.rej'
patching file 'components/viz/service/BUILD.gn'
1 out of 1 hunks failed--saving rejects to 'components/viz/service/BUILD.gn.rej'
--> Saving changes to /path/to/patches/viz_osr_2575.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 1)
        patch = analyzer.patches[0]
        self.assertEqual(patch.status, 'failed')
        self.assertEqual(len(patch.files_with_failures), 3)
        self.assertEqual(patch.total_hunks_failed, 4)  # 2 + 1 + 1

        self.assertEqual(patch.files_with_failures[0].total_hunks_failed, 2)
        self.assertEqual(patch.files_with_failures[1].total_hunks_failed, 1)
        self.assertEqual(patch.files_with_failures[2].total_hunks_failed, 1)


class TestFileMovementDetection(unittest.TestCase):
    """Test cases for file movement detection feature"""

    def test_original_files_includes_missing_files(self):
        """Test that original_files list includes both existing and missing files"""
        output = """
--> Reading patch file /path/to/patches/test.patch
--> Reverting changes to /path/to/src/chrome/browser/ui/file1.cc
--> Skipping non-existing file /path/to/src/chrome/browser/ui/file2.cc
--> Reverting changes to /path/to/src/chrome/browser/ui/file3.cc
--> Skipping non-existing file /path/to/src/chrome/browser/ui/file4.cc
--> Applying patch to /path/to/src
patching file 'chrome/browser/ui/file1.cc'
patching file 'chrome/browser/ui/file3.cc'
--> Saving changes to /path/to/patches/test.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        self.assertEqual(len(analyzer.patches), 1)
        patch = analyzer.patches[0]

        # Should have 4 files in original_files (2 existing + 2 missing)
        self.assertEqual(len(patch.original_files), 4)
        self.assertIn('chrome/browser/ui/file1.cc', patch.original_files)
        self.assertIn('chrome/browser/ui/file2.cc', patch.original_files)
        self.assertIn('chrome/browser/ui/file3.cc', patch.original_files)
        self.assertIn('chrome/browser/ui/file4.cc', patch.original_files)

    def test_new_location_field_exists(self):
        """Test that new_location field is present in FileFailure for missing files"""
        output = """
--> Reading patch file /path/to/patches/test.patch
--> Applying patch to /path/to/src
No file to patch.  Skipping...
5 out of 5 hunks ignored--saving rejects to 'chrome/browser/ui/moved.cc.rej'
--> Saving changes to /path/to/patches/test.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        patch = analyzer.patches[0]
        self.assertEqual(len(patch.files_with_failures), 1)
        failure = patch.files_with_failures[0]

        # new_location should exist (even if None)
        self.assertTrue(hasattr(failure, 'new_location'))
        # For "hunks ignored" lines, new_location won't be set by _add_missing_file
        # so it will be None
        self.assertIsNone(failure.new_location)

    def test_new_location_in_json_output(self):
        """Test that new_location appears in JSON output"""
        output = """
--> Reading patch file /path/to/patches/test.patch
--> Applying patch to /path/to/src
No file to patch.  Skipping...
3 out of 3 hunks ignored--saving rejects to 'chrome/browser/ui/old_path.cc.rej'
--> Saving changes to /path/to/patches/test.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        # Manually set new_location for testing JSON serialization
        analyzer.patches[0].files_with_failures[0].new_location = 'chrome/browser/ui/new_path.cc'

        json_output = analyzer.generate_json_report()
        data = json.loads(json_output)

        self.assertIn('new_location', data['patches'][0]['failures'][0])
        self.assertEqual(data['patches'][0]['failures'][0]['new_location'],
                        'chrome/browser/ui/new_path.cc')

    def test_original_files_in_json_output(self):
        """Test that original_files appears in JSON output"""
        output = """
--> Reading patch file /path/to/patches/test.patch
--> Reverting changes to /path/to/src/file1.cc
--> Skipping non-existing file /path/to/src/file2.cc
--> Applying patch to /path/to/src
patching file 'file1.cc'
--> Saving changes to /path/to/patches/test.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        json_output = analyzer.generate_json_report()
        data = json.loads(json_output)

        self.assertIn('original_files', data['patches'][0])
        self.assertEqual(len(data['patches'][0]['original_files']), 2)
        self.assertIn('file1.cc', data['patches'][0]['original_files'])
        self.assertIn('file2.cc', data['patches'][0]['original_files'])

    def test_chrome_runtime_views_complete_file_list(self):
        """Test that chrome_runtime_views shows complete original file list including moved files"""
        output = """
--> Reading patch file /path/to/patches/chrome_runtime_views.patch
--> Reverting changes to /path/to/src/chrome/browser/ui/browser_command_controller.cc
--> Reverting changes to /path/to/src/chrome/browser/ui/toolbar/app_menu_model.cc
--> Skipping non-existing file /path/to/src/chrome/browser/ui/views/frame/browser_view_layout.cc
--> Skipping non-existing file /path/to/src/chrome/browser/ui/views/frame/browser_view_layout.h
--> Skipping non-existing file /path/to/src/chrome/browser/ui/views/frame/browser_view_layout_delegate.h
--> Reverting changes to /path/to/src/chrome/browser/ui/views/frame/browser_view.cc
--> Applying patch to /path/to/src
patching file 'chrome/browser/ui/browser_command_controller.cc'
patching file 'chrome/browser/ui/toolbar/app_menu_model.cc'
patching file 'chrome/browser/ui/views/frame/browser_view.cc'
--> Saving changes to /path/to/patches/chrome_runtime_views.patch
"""
        analyzer = PatchOutputAnalyzer(output)
        analyzer.parse()

        patch = analyzer.patches[0]

        # Should include all 6 files (3 existing + 3 missing)
        self.assertEqual(len(patch.original_files), 6)

        # Check all files are present
        self.assertIn('chrome/browser/ui/browser_command_controller.cc', patch.original_files)
        self.assertIn('chrome/browser/ui/toolbar/app_menu_model.cc', patch.original_files)
        self.assertIn('chrome/browser/ui/views/frame/browser_view_layout.cc', patch.original_files)
        self.assertIn('chrome/browser/ui/views/frame/browser_view_layout.h', patch.original_files)
        self.assertIn('chrome/browser/ui/views/frame/browser_view_layout_delegate.h', patch.original_files)
        self.assertIn('chrome/browser/ui/views/frame/browser_view.cc', patch.original_files)


if __name__ == '__main__':
    unittest.main()
