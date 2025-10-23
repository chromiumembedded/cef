#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""
Analyze and format patch_updater.py output for easier consumption by AI agents
and human developers.

Usage:
  python patch_updater.py 2>&1 | python analyze_patch_output.py
  python patch_updater.py > output.txt 2>&1
  python analyze_patch_output.py output.txt
  python analyze_patch_output.py output.txt --json
"""

import argparse
import json
import re
import sys
from collections import defaultdict
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import List, Dict, Optional


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
    missing: bool = False
    skipped: bool = False

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

    def __post_init__(self):
        if self.files_processed is None:
            self.files_processed = []
        if self.files_with_failures is None:
            self.files_with_failures = []


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
                # Track files being processed
                match = re.search(r'--> (?:Reverting changes to|Applying patch)', line)
                if match and i + 1 < len(lines):
                    # Skip, we'll get file names from patching messages
                    pass

                # Files being patched
                match = re.search(r'^patching file (.+)$', line)
                if match:
                    file_path = match.group(1)
                    if file_path not in self.current_patch.files_processed:
                        self.current_patch.files_processed.append(file_path)
                    continue

                # Hunk failures
                match = re.search(r'^Hunk #(\d+) FAILED at (\d+)', line)
                if match:
                    hunk_num = int(match.group(1))
                    line_num = int(match.group(2))

                    # Look back to find which file this is for
                    file_path = self._find_current_file(lines, i)
                    if file_path:
                        self._add_hunk_failure(file_path, hunk_num, line_num)
                    continue

                # Total hunks failed with reject file
                match = re.search(r'^(\d+) out of (\d+) hunks? FAILED -- saving rejects to file (.+)$', line)
                if match:
                    failed_count = int(match.group(1))
                    reject_file = match.group(3)
                    # Update the most recent file failure with reject path
                    if self.current_patch.files_with_failures:
                        self.current_patch.files_with_failures[-1].reject_file = reject_file
                        self.current_patch.total_hunks_failed += failed_count
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
                return match.group(1)
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
        # Find or create file failure
        file_failure = None
        for ff in self.current_patch.files_with_failures:
            if ff.file_path == file_path:
                file_failure = ff
                break

        if not file_failure:
            file_failure = FileFailure(file_path=file_path)
            self.current_patch.files_with_failures.append(file_failure)

        file_failure.failed_hunks.append(HunkFailure(hunk_num, line_num))

    def _add_missing_file(self, file_path: str, skipped: bool = False):
        """Add a missing file to the current patch."""
        file_failure = FileFailure(
            file_path=file_path,
            missing=True,
            skipped=skipped
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

                for file_failure in patch.files_with_failures:
                    if file_failure.missing:
                        lines.append(f'   {RED}✗{RESET} {file_failure.file_path} (FILE MISSING)')
                        lines.append(f'      {YELLOW}Investigate:{RESET} git log --full-history -1 -- {file_failure.file_path}')
                    else:
                        hunk_count = len(file_failure.failed_hunks)
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

                lines.append(f'   {YELLOW}Fix and resave:{RESET} python patch_updater.py --resave --patch {patch.patch_name}')

            lines.append('')
            lines.append('-' * 80)
            lines.append(f'{BOLD}NEXT STEPS:{RESET}')
            lines.append(f'1. Fix each failed patch by manually editing the Chromium source files')
            lines.append(f'2. Use the commands above to understand what changed in Chromium')
            lines.append(f'3. Apply the CEF changes from the .rej files manually')
            lines.append(f'4. Resave each patch: python patch_updater.py --resave --patch <name>')
            lines.append(f'5. Verify all patches apply: python patch_updater.py')
            lines.append('')
            lines.append(f'{CYAN}For detailed instructions:{RESET} see cef/tmp/FIXPATCHES.md')

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
                'total_hunks_failed': patch.total_hunks_failed,
                'failures': []
            }

            for file_failure in patch.files_with_failures:
                failure_data = {
                    'file_path': file_failure.file_path,
                    'missing': file_failure.missing,
                    'skipped': file_failure.skipped,
                    'reject_file': file_failure.reject_file,
                    'failed_hunks': [
                        {'hunk_number': h.hunk_number, 'line_number': h.line_number}
                        for h in file_failure.failed_hunks
                    ]
                }
                patch_data['failures'].append(failure_data)

            report['patches'].append(patch_data)

        return json.dumps(report, indent=2)

    def generate_fix_plan(self) -> str:
        """Generate a systematic fix plan for AI agents."""
        stats = self.get_statistics()

        if stats['failed'] == 0:
            return "No fixes needed - all patches applied successfully!"

        lines = []
        lines.append("SYSTEMATIC FIX PLAN")
        lines.append("=" * 80)
        lines.append(f"Total patches to fix: {stats['failed']}")
        lines.append(f"Total files requiring manual edits: {stats['total_files_with_failures']}")
        lines.append("")

        for i, patch in enumerate(stats['failed_patches'], 1):
            lines.append(f"PATCH {i}/{stats['failed']}: {patch.patch_name}")
            lines.append("-" * 80)

            for j, file_failure in enumerate(patch.files_with_failures, 1):
                lines.append(f"\n  FILE {i}.{j}: {file_failure.file_path}")

                if file_failure.missing:
                    lines.append(f"    STATUS: Missing file")
                    lines.append(f"    ACTION:")
                    lines.append(f"      1. Investigate: git log --full-history -1 -- {file_failure.file_path}")
                    lines.append(f"      2. Determine if file was moved, deleted, or renamed")
                    lines.append(f"      3. Update patch file paths or remove if no longer needed")
                else:
                    lines.append(f"    STATUS: {len(file_failure.failed_hunks)} hunk(s) failed")
                    lines.append(f"    REJECT FILE: {file_failure.reject_file}")
                    lines.append(f"    ACTION:")
                    lines.append(f"      1. Read reject file: cat {file_failure.reject_file}")
                    lines.append(f"      2. Read current file: cat {file_failure.file_path}")
                    lines.append(f"      3. See what changed in Chromium:")
                    lines.append(f"         git diff --no-prefix refs/tags/{self.old_version}...refs/tags/{self.new_version} -- {file_failure.file_path}")
                    lines.append(f"      4. Manually apply the CEF changes from .rej to the current file")
                    lines.append(f"      5. Verify changes make sense in new context")

            lines.append(f"\n  RESAVE COMMAND:")
            lines.append(f"    python patch_updater.py --resave --patch {patch.patch_name}")
            lines.append("")

        lines.append("=" * 80)
        lines.append("After fixing all patches, verify with: python patch_updater.py")
        lines.append("")

        return '\n'.join(lines)


def main():
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
        '--fix-plan',
        action='store_true',
        help='Generate a systematic fix plan'
    )
    parser.add_argument(
        '--no-color',
        action='store_true',
        help='Disable color output'
    )
    parser.add_argument(
        '--old-version',
        default='OLD_VERSION',
        help='Old Chromium version tag (e.g., 139.0.7258.0)'
    )
    parser.add_argument(
        '--new-version',
        default='NEW_VERSION',
        help='New Chromium version tag (e.g., 140.0.7339.0)'
    )
    parser.add_argument(
        '--project-root',
        help='Path to Chromium source root'
    )

    args = parser.parse_args()

    # Read input
    if args.input_file:
        with open(args.input_file, 'r') as f:
            output_text = f.read()
    else:
        output_text = sys.stdin.read()

    # Analyze
    analyzer = PatchOutputAnalyzer(
        output_text,
        project_root=args.project_root,
        old_version=args.old_version,
        new_version=args.new_version
    )
    analyzer.parse()

    # Generate report
    if args.json:
        print(analyzer.generate_json_report())
    elif args.fix_plan:
        print(analyzer.generate_fix_plan())
    else:
        print(analyzer.generate_summary_report(colorize=not args.no_color))


if __name__ == '__main__':
    main()
