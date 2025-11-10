#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""
Analyze and format patch_updater.py output for easier consumption by AI agents
and human developers.

Includes automatic detection of moved/renamed files using git diff.

Usage:
  python3 analyze_patch_output.py output.txt \
    --old-version 142.0.7444.0 --new-version 143.0.7491.0

  python3 analyze_patch_output.py output.txt \
    --old-version 142.0.7444.0 --new-version 143.0.7491.0 \
    --json

  python3 patch_updater.py 2>&1 | python3 analyze_patch_output.py \
    --old-version 142.0.7444.0 --new-version 143.0.7491.0
"""

import argparse
import json
import os
import re
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import List, Dict, Optional

from patch_utils import detect_file_movement


@dataclass
class HunkFailure:
    """Represents a failed hunk in a patch."""
    hunk_number: int
    line_number: Optional[int] = None


@dataclass
class FileFailure:
    """Represents a file with patch failures."""
    file_path: str
    reject_file: Optional[str] = None
    failed_hunks: List[HunkFailure] = None
    total_hunks_failed: int = 0
    missing: bool = False
    skipped: bool = False
    new_location: Optional[str] = None  # Detected new location if file was moved

    def __post_init__(self):
        if self.failed_hunks is None:
            self.failed_hunks = []


@dataclass
class PatchResult:
    """Represents the result of applying a single patch."""
    patch_name: str
    patch_file: str
    status: str  # 'success', 'offset', 'failed', 'missing_files'
    files_processed: List[str] = None
    files_with_failures: List[FileFailure] = None
    total_hunks_failed: int = 0
    original_files: List[str] = None  # Complete file list from the patch

    def __post_init__(self):
        if self.files_processed is None:
            self.files_processed = []
        if self.files_with_failures is None:
            self.files_with_failures = []
        if self.original_files is None:
            self.original_files = []


class PatchOutputAnalyzer:
    """Analyzes patch_updater.py output and generates structured reports."""

    def __init__(self, output_text: str, project_root: str = None,
                 old_version: str = None, new_version: str = None):
        self.output_text = output_text
        self.project_root = project_root or "/path/to/chromium/src"
        self.old_version = old_version or "OLD_VERSION"
        self.new_version = new_version or "NEW_VERSION"
        self.patches: List[PatchResult] = []
        self.current_patch: Optional[PatchResult] = None

    def parse(self):
        """Parse the patch updater output."""
        lines = self.output_text.split('\n')

        for i, line in enumerate(lines):
            # Reading patch file
            match = re.search(r'--> Reading patch file .*[/\\]patches[/\\](.+?)\.patch', line)
            if match:
                if self.current_patch:
                    self.patches.append(self.current_patch)

                patch_name = match.group(1)
                self.current_patch = PatchResult(
                    patch_name=patch_name,
                    patch_file=f"cef/patch/patches/{patch_name}.patch",
                    status='success'
                )
                continue

            # Reverting/applying files
            if self.current_patch:
                # Track original file list from "Reverting changes to" lines
                match = re.search(r'--> Reverting changes to (.+)$', line)
                if match:
                    file_path = match.group(1)
                    # Extract just the relative path from the full path
                    file_path = re.sub(r'^.*[/\\]src[/\\]', '', file_path)
                    if file_path not in self.current_patch.original_files:
                        self.current_patch.original_files.append(file_path)
                    continue

                # Also track missing files from "Skipping non-existing file" lines
                match = re.search(r'--> Skipping non-existing file (.+)$', line)
                if match:
                    file_path = match.group(1)
                    # Extract just the relative path from the full path
                    file_path = re.sub(r'^.*[/\\]src[/\\]', '', file_path)
                    if file_path not in self.current_patch.original_files:
                        self.current_patch.original_files.append(file_path)
                    continue

                # Skip "Applying patch" lines
                match = re.search(r'--> Applying patch', line)
                if match:
                    # Skip, we'll get file names from patching messages
                    pass

                # Files being patched
                match = re.search(r'^patching file (.+)$', line)
                if match:
                    file_path = match.group(1)
                    # Store in files_processed with original format (may include quotes)
                    if file_path not in self.current_patch.files_processed:
                        self.current_patch.files_processed.append(file_path)
                    # Also track the normalized path for lookups
                    normalized_path = file_path.strip("'\"")
                    if not hasattr(self, '_current_file'):
                        self._current_file = normalized_path
                    else:
                        self._current_file = normalized_path
                    continue

                # Hunk failures
                match = re.search(r'^Hunk #(\d+) (?:FAILED|failed) at (\d+)', line)
                if match:
                    hunk_num = int(match.group(1))
                    line_num = int(match.group(2))

                    # Look back to find which file this is for
                    file_path = self._find_current_file(lines, i)
                    if file_path:
                        self._add_hunk_failure(file_path, hunk_num, line_num)
                    continue

                # Total hunks failed/ignored with reject file
                # "failed" = hunk couldn't apply to existing file
                # "ignored" = file is missing (with "No file to patch"), OR patch already applied
                # Handles both "failed -- saving rejects to file X" and "FAILED -- saving rejects to file X"
                match = re.search(r'^(\d+) out of (\d+) hunks? (failed|FAILED|ignored)\s*--\s*saving rejects to (?:file )?(.+)$', line)
                if match:
                    failed_count = int(match.group(1))
                    status_word = match.group(3)  # "failed", "FAILED", or "ignored"
                    reject_file = match.group(4)

                    # Extract the source file path from reject file (remove .rej, quotes, etc.)
                    file_path = reject_file.strip("'\"").replace('.rej', '')

                    # Determine if this is a missing file
                    # "ignored" can mean two things:
                    # 1. "No file to patch. Skipping..." (file is missing)
                    # 2. "Ignoring previously applied (or reversed) patch" (file exists, patch already applied)
                    # Look back at recent lines to determine which
                    is_missing = False
                    if status_word == 'ignored':
                        # Check the last few lines for "No file to patch"
                        for j in range(max(0, i-5), i):
                            if 'No file to patch' in lines[j]:
                                is_missing = True
                                break

                    # Find or create file failure entry
                    file_failure = None
                    for ff in self.current_patch.files_with_failures:
                        if ff.file_path == file_path:
                            file_failure = ff
                            break

                    if not file_failure:
                        # Create new file failure entry
                        # If file is missing, try to detect new location
                        new_location = detect_file_movement(
                            file_path, self.old_version, self.new_version, self.project_root
                        ) if is_missing else None
                        file_failure = FileFailure(
                            file_path=file_path,
                            missing=is_missing,
                            new_location=new_location
                        )
                        self.current_patch.files_with_failures.append(file_failure)
                    else:
                        # Update existing entry
                        if is_missing:
                            file_failure.missing = True
                            if not file_failure.new_location:
                                file_failure.new_location = detect_file_movement(
                                    file_path, self.old_version, self.new_version, self.project_root
                                )

                    # Update the reject file path and count
                    file_failure.reject_file = reject_file
                    # Set the total from the summary line (this is the authoritative count)
                    file_failure.total_hunks_failed = failed_count
                    # Only add to patch total if not already counted
                    # (Check if this file's hunks were already added to the total)
                    if not hasattr(file_failure, '_counted'):
                        self.current_patch.total_hunks_failed += failed_count
                        file_failure._counted = True
                    self.current_patch.status = 'failed'
                    continue

                # Missing files
                match = re.search(r'--> Skipping non-existing file (.+)$', line)
                if match:
                    file_path = match.group(1)
                    # Extract just the relative path
                    file_path = re.sub(r'^.*[/\\]src[/\\]', '', file_path)
                    self._add_missing_file(file_path)
                    continue

                # Can't find file to patch
                match = re.search(r"^can't find file to patch at input line", line)
                if match:
                    # Look ahead for the file path
                    file_path = self._find_missing_file_path(lines, i)
                    if file_path:
                        self._add_missing_file(file_path, skipped=True)
                    continue

                # Hunk succeeded with offset (not a failure, but notable)
                match = re.search(r'^Hunk #\d+ succeeded at \d+ .*offset', line)
                if match and self.current_patch.status == 'success':
                    self.current_patch.status = 'offset'
                    continue

                # Warning about failed patch
                match = re.search(r'!!!! WARNING: Failed to apply (\w+)', line)
                if match:
                    self.current_patch.status = 'failed'
                    continue

        # Don't forget the last patch
        if self.current_patch:
            self.patches.append(self.current_patch)

    def _find_current_file(self, lines: List[str], current_idx: int) -> Optional[str]:
        """Look backward to find the current file being patched."""
        for i in range(current_idx - 1, max(0, current_idx - 20), -1):
            match = re.search(r'^patching file (.+)$', lines[i])
            if match:
                # Return normalized path (without quotes) for consistency
                return match.group(1).strip("'\"")
        return None

    def _find_missing_file_path(self, lines: List[str], current_idx: int) -> Optional[str]:
        """Look ahead to find the missing file path."""
        for i in range(current_idx + 1, min(len(lines), current_idx + 10)):
            match = re.search(r'^\|--- (.+)$', lines[i])
            if match:
                path = match.group(1)
                # Clean up the path
                path = re.sub(r'^.*[/\\]src[/\\]', '', path)
                return path
        return None

    def _add_hunk_failure(self, file_path: str, hunk_num: int, line_num: int):
        """Add a hunk failure to the current patch."""
        # Normalize the file path
        normalized_path = file_path.strip("'\"")

        # Find or create file failure
        file_failure = None
        for ff in self.current_patch.files_with_failures:
            if ff.file_path == normalized_path:
                file_failure = ff
                break

        if not file_failure:
            file_failure = FileFailure(file_path=normalized_path)
            self.current_patch.files_with_failures.append(file_failure)

        file_failure.failed_hunks.append(HunkFailure(hunk_num, line_num))

    def _add_missing_file(self, file_path: str, skipped: bool = False):
        """Add a missing file to the current patch."""
        # Try to detect if the file was moved
        new_location = detect_file_movement(
            file_path, self.old_version, self.new_version, self.project_root
        )

        file_failure = FileFailure(
            file_path=file_path,
            missing=True,
            skipped=skipped,
            new_location=new_location
        )
        self.current_patch.files_with_failures.append(file_failure)
        if self.current_patch.status == 'success':
            self.current_patch.status = 'missing_files'

    def get_statistics(self) -> Dict:
        """Calculate statistics about the patch results."""
        total = len(self.patches)
        successful = sum(1 for p in self.patches if p.status == 'success')
        offset = sum(1 for p in self.patches if p.status == 'offset')
        failed = sum(1 for p in self.patches if p.status in ('failed', 'missing_files'))

        total_files_failed = sum(len(p.files_with_failures) for p in self.patches)
        total_hunks_failed = sum(p.total_hunks_failed for p in self.patches)

        # Categorize failures
        failed_patches = [p for p in self.patches if p.status in ('failed', 'missing_files')]

        return {
            'total_patches': total,
            'successful': successful,
            'with_offsets': offset,
            'failed': failed,
            'total_files_with_failures': total_files_failed,
            'total_hunks_failed': total_hunks_failed,
            'success_rate': round(successful / total * 100, 1) if total > 0 else 0,
            'failed_patches': failed_patches
        }

    def generate_summary_report(self, colorize: bool = True) -> str:
        """Generate a human-readable summary report."""
        stats = self.get_statistics()

        # ANSI color codes
        if colorize:
            RED = '\033[91m'
            GREEN = '\033[92m'
            YELLOW = '\033[93m'
            CYAN = '\033[96m'
            BOLD = '\033[1m'
            RESET = '\033[0m'
        else:
            RED = GREEN = YELLOW = CYAN = BOLD = RESET = ''

        lines = []
        lines.append('')
        lines.append('=' * 80)
        lines.append(f'{BOLD}PATCH UPDATE SUMMARY{RESET}')
        lines.append('=' * 80)
        lines.append(f'Total patches: {stats["total_patches"]}')
        lines.append(f'{GREEN}✓ Successful: {stats["successful"]}{RESET}')
        if stats['with_offsets'] > 0:
            lines.append(f'{YELLOW}⚠ With offsets: {stats["with_offsets"]} (auto-corrected){RESET}')
        lines.append(f'{RED}✗ Failed: {stats["failed"]}{RESET}')
        lines.append(f'Success rate: {stats["success_rate"]}%')
        lines.append('')

        if stats['failed'] > 0:
            lines.append(f'{BOLD}FAILED PATCHES REQUIRING MANUAL FIXES:{RESET}')
            lines.append('-' * 80)

            for i, patch in enumerate(stats['failed_patches'], 1):
                lines.append(f'\n{BOLD}{i}. {patch.patch_name}{RESET} ({len(patch.files_with_failures)} file(s) with problems)')
                lines.append(f'   Patch file: {CYAN}{patch.patch_file}{RESET}')

                # Show original file list
                if patch.original_files:
                    lines.append(f'   {BOLD}Original files in patch ({len(patch.original_files)} total):{RESET}')
                    for orig_file in patch.original_files:
                        # Mark files that have problems
                        has_problem = any(ff.file_path == orig_file for ff in patch.files_with_failures)
                        if has_problem:
                            lines.append(f'      {RED}✗{RESET} {orig_file}')
                        else:
                            lines.append(f'      {GREEN}✓{RESET} {orig_file}')
                    lines.append('')

                for file_failure in patch.files_with_failures:
                    if file_failure.missing:
                        hunk_count = file_failure.total_hunks_failed if file_failure.total_hunks_failed > 0 else 0
                        hunk_info = f" - {hunk_count} hunk(s) to apply" if hunk_count > 0 else ""
                        lines.append(f'   {RED}✗{RESET} {file_failure.file_path} (FILE MISSING){hunk_info}')
                        if file_failure.new_location:
                            lines.append(f'      {GREEN}→ Detected new location:{RESET} {file_failure.new_location}')
                            if file_failure.reject_file:
                                lines.append(f'      ')
                                lines.append(f'      {BOLD}Try automatic application:{RESET}')
                                lines.append(f'        patch {file_failure.new_location} < {file_failure.reject_file}')
                                lines.append(f'      ')
                                lines.append(f'      {BOLD}If successful, verify changes:{RESET}')
                                lines.append(f'        git diff {file_failure.new_location}')
                                lines.append(f'      ')
                                lines.append(f'      {BOLD}Manual application (if patch fails):{RESET}')
                                lines.append(f'        cat {file_failure.reject_file}')
                        else:
                            lines.append(f'      {YELLOW}Find what happened:{RESET} git log --full-history -1 --stat -- {file_failure.file_path}')
                            if file_failure.reject_file:
                                lines.append(f'      {YELLOW}Apply changes from:{RESET} cat {file_failure.reject_file}')
                        lines.append(f'      {YELLOW}Regenerate patch:{RESET} Use git diff with all original files at their new locations')
                    else:
                        # Use total_hunks_failed if available, otherwise count failed_hunks list
                        hunk_count = file_failure.total_hunks_failed if file_failure.total_hunks_failed > 0 else len(file_failure.failed_hunks)
                        hunk_desc = f"{hunk_count} hunk(s) failed"
                        if file_failure.failed_hunks:
                            lines_desc = ', '.join(f"#{h.hunk_number} at line {h.line_number}"
                                                   for h in file_failure.failed_hunks if h.line_number)
                            if lines_desc:
                                hunk_desc += f" ({lines_desc})"

                        lines.append(f'   {RED}✗{RESET} {file_failure.file_path} - {hunk_desc}')
                        if file_failure.reject_file:
                            lines.append(f'      Reject: {CYAN}{file_failure.reject_file}{RESET}')

                        # Suggest commands
                        lines.append(f'      {YELLOW}View reject:{RESET} cat {file_failure.reject_file}')
                        lines.append(f'      {YELLOW}View file:{RESET} cat {file_failure.file_path}')
                        lines.append(f'      {YELLOW}See changes:{RESET} git diff --no-prefix '
                                   f'refs/tags/{self.old_version}...refs/tags/{self.new_version} '
                                   f'-- {file_failure.file_path}')

                lines.append(f'   {YELLOW}Fix and resave:{RESET} python3 patch_updater.py --resave --patch {patch.patch_name}')

            lines.append('')
            lines.append('-' * 80)
            lines.append(f'{BOLD}NEXT STEPS:{RESET}')
            lines.append(f'1. Fix each failed patch by manually editing the Chromium source files')
            lines.append(f'2. Use the commands above to understand what changed in Chromium')
            lines.append(f'3. Apply the CEF changes from the .rej files manually')
            lines.append(f'4. Resave each patch: python3 patch_updater.py --resave --patch <name>')
            lines.append(f'5. Verify: See Step 5 (Final Verification) in cef/tools/claude/CLAUDE_PATCH_INSTRUCTIONS.md')
            lines.append('')
            lines.append(f'{CYAN}For detailed instructions:{RESET} see cef/tools/claude/CLAUDE_PATCH_INSTRUCTIONS.md')

        else:
            lines.append(f'{GREEN}{BOLD}✓ ALL PATCHES APPLIED SUCCESSFULLY!{RESET}')
            if stats['with_offsets'] > 0:
                lines.append(f'\n{YELLOW}Note:{RESET} {stats["with_offsets"]} patch(es) had offset corrections (this is normal)')

        lines.append('=' * 80)
        lines.append('')

        return '\n'.join(lines)

    def generate_json_report(self) -> str:
        """Generate a JSON report for machine consumption."""
        stats = self.get_statistics()

        report = {
            'status': 'success' if stats['failed'] == 0 else 'partial_failure',
            'statistics': {
                'total_patches': stats['total_patches'],
                'successful': stats['successful'],
                'with_offsets': stats['with_offsets'],
                'failed': stats['failed'],
                'success_rate': stats['success_rate'],
                'total_files_with_failures': stats['total_files_with_failures'],
                'total_hunks_failed': stats['total_hunks_failed']
            },
            'patches': []
        }

        for patch in self.patches:
            patch_data = {
                'patch_name': patch.patch_name,
                'patch_file': patch.patch_file,
                'status': patch.status,
                'files_processed': patch.files_processed,
                'original_files': patch.original_files,
                'total_hunks_failed': patch.total_hunks_failed,
                'failures': []
            }

            for file_failure in patch.files_with_failures:
                failure_data = {
                    'file_path': file_failure.file_path,
                    'missing': file_failure.missing,
                    'skipped': file_failure.skipped,
                    'new_location': file_failure.new_location,
                    'reject_file': file_failure.reject_file,
                    'total_hunks_failed': file_failure.total_hunks_failed,
                    'failed_hunks': [
                        {'hunk_number': h.hunk_number, 'line_number': h.line_number}
                        for h in file_failure.failed_hunks
                    ]
                }
                patch_data['failures'].append(failure_data)

            report['patches'].append(patch_data)

        return json.dumps(report, indent=2)


def main():
    # Fix Windows encoding for Unicode output
    if sys.platform == 'win32':
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

    parser = argparse.ArgumentParser(
        description='Analyze patch_updater.py output and generate formatted reports'
    )
    parser.add_argument(
        'input_file',
        nargs='?',
        help='Input file containing patch_updater.py output (stdin if not specified)'
    )
    parser.add_argument(
        '--json',
        action='store_true',
        help='Output JSON format instead of human-readable summary'
    )
    parser.add_argument(
        '--no-color',
        action='store_true',
        help='Disable color output'
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
        help='Path to Chromium source root (inferred from paths if not specified)'
    )

    args = parser.parse_args()

    # Read input
    if args.input_file:
        with open(args.input_file, 'r') as f:
            output_text = f.read()
    else:
        output_text = sys.stdin.read()

    # Infer project root if not specified
    # Script is in chromium/src/cef/tools/claude/, so go up 3 levels to chromium/src
    project_root = args.project_root
    if not project_root:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        # Go up: claude/ -> tools/ -> cef/ -> src/
        project_root = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))

    # Analyze
    analyzer = PatchOutputAnalyzer(
        output_text,
        project_root=project_root,
        old_version=args.old_version,
        new_version=args.new_version
    )
    analyzer.parse()

    # Generate report
    if args.json:
        print(analyzer.generate_json_report())
    else:
        print(analyzer.generate_summary_report(colorize=not args.no_color))


if __name__ == '__main__':
    main()
