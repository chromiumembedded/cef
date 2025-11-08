#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""
Verify that a regenerated patch includes all files that failed.

Uses patch_output.txt to determine which files failed for the specific patch,
then checks if those file paths are present in the regenerated patch.

IMPORTANT: This tool checks FILE COVERAGE (are all the files included?), not
CONTENT CORRECTNESS (are the changes right?). Manual review of patch content
is still required to ensure CEF modifications are correct in the new context.

Includes automatic detection of moved/renamed files using git diff.

Usage:
  python3 verify_patch.py patch_output.txt --patch chrome_runtime_views \
    --old-version 142.0.7444.0 --new-version 143.0.7491.0

  python3 verify_patch.py patch_output.txt --patch views_widget \
    --old-version 142.0.7444.0 --new-version 143.0.7491.0 \
    --project-root /path/to/chromium/src
"""

import argparse
import os
import re
import sys
from typing import List, Dict, Set, Optional

from patch_utils import detect_file_movement
from analyze_patch_output import PatchOutputAnalyzer


class PatchVerifier:
    """Verifies that regenerated patches include all changes from reject files."""

    def __init__(self, patch_name: str, patch_output_text: str,
                 patch_dir: str = None, project_root: str = None,
                 old_version: str = None, new_version: str = None):
        self.patch_name = patch_name
        self.patch_output_text = patch_output_text
        # If patch_dir not specified, calculate relative to script location
        if not patch_dir:
            script_dir = os.path.dirname(os.path.abspath(__file__))
            # Script is in chromium/src/cef/tools/claude/
            # Patches are in chromium/src/cef/patch/patches/
            # Go up: claude/ -> tools/ -> cef/, then to patch/patches/
            cef_dir = os.path.dirname(os.path.dirname(script_dir))
            patch_dir = os.path.join(cef_dir, 'patch', 'patches')
        self.patch_dir = patch_dir
        self.patch_file = os.path.join(self.patch_dir, f"{patch_name}.patch")
        self.project_root = project_root
        self.old_version = old_version
        self.new_version = new_version

    def get_patch_files(self) -> Set[str]:
        """Extract list of files modified in the patch."""
        files = set()
        try:
            with open(self.patch_file, 'r') as f:
                for line in f:
                    # Match "diff --git a/path b/path" or "diff --git path path"
                    match = re.match(r'^diff --git (?:a/)?(\S+)', line)
                    if match:
                        file_path = match.group(1)
                        # Remove a/ or b/ prefix if present
                        file_path = re.sub(r'^[ab]/', '', file_path)
                        files.add(file_path)
        except FileNotFoundError:
            print(f"ERROR: Patch file not found: {self.patch_file}")
            sys.exit(1)

        return files

    def get_failed_files_from_patch_output(self) -> List[Dict]:
        """Parse patch output to get failed files for this specific patch."""
        # Use PatchOutputAnalyzer to parse the output
        analyzer = PatchOutputAnalyzer(
            self.patch_output_text,
            project_root=self.project_root,
            old_version=self.old_version,
            new_version=self.new_version
        )
        analyzer.parse()

        # Find the patch we're interested in
        for patch in analyzer.patches:
            if patch.patch_name == self.patch_name:
                # Return list of file failures with their details
                failed_files = []
                for file_failure in patch.files_with_failures:
                    failed_files.append({
                        'file_path': file_failure.file_path,
                        'reject_file': file_failure.reject_file,
                        'missing': file_failure.missing,
                        'new_location': file_failure.new_location,
                        'total_hunks_failed': file_failure.total_hunks_failed,
                        'failed_hunks': file_failure.failed_hunks
                    })
                return failed_files

        # Patch not found in output
        return []

    def check_patch_coverage(self) -> bool:
        """Check if the patch covers all files that failed for this specific patch."""
        print(f"Verifying patch: {self.patch_name}")
        print("=" * 80)

        # Get files in the patch
        patch_files = self.get_patch_files()
        print(f"\nFiles in patch ({len(patch_files)}):")
        for f in sorted(patch_files):
            print(f"  ✓ {f}")

        # Get failed files from patch output for this specific patch
        failed_files = self.get_failed_files_from_patch_output()
        if not failed_files:
            print("\n✓ No failed files found - patch appears complete!")
            return True

        print(f"\nFailed files from patch output ({len(failed_files)}):")

        missing_files = []
        all_clear = True

        for file_info in failed_files:
            file_path = file_info['file_path']
            new_location = file_info['new_location']
            total_hunks = file_info['total_hunks_failed']
            is_missing = file_info['missing']

            print(f"\n  {file_path} ({total_hunks} hunks)")

            # Check if any form of this file is in the patch
            file_in_patch = any(
                file_path in pf or pf in file_path or
                os.path.basename(file_path) == os.path.basename(pf)
                for pf in patch_files
            )

            if file_in_patch:
                print(f"    ✓ File found in patch")
                # Show hunk info if available
                if file_info['failed_hunks']:
                    for hunk in file_info['failed_hunks']:
                        hunk_num = hunk.hunk_number
                        line_num = hunk.line_number if hunk.line_number else '?'
                        print(f"      Hunk {hunk_num} at line {line_num}")
            else:
                # File not in patch - check if it was moved
                if new_location:
                    # Check if the new location is in the patch
                    new_in_patch = any(
                        new_location in pf or pf in new_location or
                        os.path.basename(new_location) == os.path.basename(pf)
                        for pf in patch_files
                    )
                    if new_in_patch:
                        print(f"    ✓ File moved to {new_location} and found in patch")
                        if file_info['failed_hunks']:
                            for hunk in file_info['failed_hunks']:
                                hunk_num = hunk.hunk_number
                                line_num = hunk.line_number if hunk.line_number else '?'
                                print(f"      Hunk {hunk_num} at line {line_num}")
                    else:
                        print(f"    ✗ WARNING: File moved to {new_location} but NOT in patch!")
                        all_clear = False
                        missing_files.append(f"{file_path} → {new_location}")
                else:
                    print(f"    ✗ WARNING: File NOT in patch!")
                    all_clear = False
                    missing_files.append(file_path)

        print("\n" + "=" * 80)

        if all_clear:
            print("✓ Verification PASSED: All reject files are covered by the patch")
            return True
        else:
            print("✗ Verification FAILED: Some reject files are not in the patch")
            print(f"\nMissing files ({len(missing_files)}):")
            for f in missing_files:
                print(f"  - {f}")
            print("\nPossible reasons:")
            print("  1. File was moved/renamed - check if it's in the patch under a different name")
            print("  2. File was not included when regenerating the patch")
            print("  3. Changes were applied but git diff wasn't run on that file")
            return False


def main():
    parser = argparse.ArgumentParser(
        description='Verify that a regenerated patch includes all changes from failed files'
    )
    parser.add_argument(
        'patch_output_file',
        help='Path to patch_output.txt file from patch_updater.py'
    )
    parser.add_argument(
        '--patch',
        required=True,
        help='Patch name (without .patch extension)'
    )
    parser.add_argument(
        '--patch-dir',
        help='Directory containing patch files (default: ../../patch/patches)'
    )
    parser.add_argument(
        '--old-version',
        required=True,
        help='Old Chromium version tag (e.g., 142.0.7444.0)'
    )
    parser.add_argument(
        '--new-version',
        required=True,
        help='New Chromium version tag (e.g., 143.0.7491.0)'
    )
    parser.add_argument(
        '--project-root',
        help='Path to Chromium source root (inferred if not specified)'
    )

    args = parser.parse_args()

    # Read patch output file
    with open(args.patch_output_file, 'r') as f:
        patch_output_text = f.read()

    # Infer project root if not specified
    # Script is in chromium/src/cef/tools/claude/, so go up 3 levels to chromium/src
    project_root = args.project_root
    if not project_root:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        # Go up: claude/ -> tools/ -> cef/ -> src/
        project_root = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))

    verifier = PatchVerifier(
        patch_name=args.patch,
        patch_output_text=patch_output_text,
        patch_dir=args.patch_dir,
        old_version=args.old_version,
        new_version=args.new_version,
        project_root=project_root
    )

    success = verifier.check_patch_coverage()
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
