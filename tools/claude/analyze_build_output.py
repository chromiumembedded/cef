#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""
Analyze and format ninja build output for easier consumption by AI agents.
Creates a concise index of errors with line number references.

Usage:
  python3 analyze_build_output.py build_output.txt \
    --old-version 142.0.7444.0 --new-version 143.0.7499.0

  python3 analyze_build_output.py build_output.txt --json

  autoninja -C out/Debug_GN_arm64 cef 2>&1 | tee build_output.txt
  python3 analyze_build_output.py build_output.txt \
    --old-version 142.0.7444.0 --new-version 143.0.7499.0
"""

import argparse
import json
import os
import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Set


@dataclass
class ErrorReference:
    """Minimal error information with reference to build_output.txt line."""
    file_path: str
    line_number: int  # Line in source file
    error_line: int   # Line number in build_output.txt where error appears


@dataclass
class FileErrors:
    """Errors grouped by file."""
    file_path: str
    error_count: int = 0
    errors: List[ErrorReference] = field(default_factory=list)
    build_target: Optional[str] = None  # ninja build target (e.g., obj/cef/libcef_static/file.o)


@dataclass
class BuildAnalysis:
    """Complete build analysis with minimal references."""
    total_errors: int = 0
    total_files: int = 0
    files: Dict[str, FileErrors] = field(default_factory=dict)
    build_output_file: str = "build_output.txt"


class BuildOutputAnalyzer:
    """Analyzes ninja build output and creates an error index."""

    def __init__(self, build_output_file: str, old_version: str = None, new_version: str = None):
        self.build_output_file = build_output_file
        self.old_version = old_version or "OLD_VERSION"
        self.new_version = new_version or "NEW_VERSION"
        self.analysis = BuildAnalysis(build_output_file=build_output_file)

    def parse(self, output_text: str):
        """Parse build output and create error index."""
        lines = output_text.split('\n')

        # First pass: extract build targets from FAILED lines
        # Map from source file basename to build target
        target_map: Dict[str, str] = {}
        for line in lines:
            # FAILED: <uuid> "<target>" CXX <target>
            # Example: FAILED: 70ba6b60-775a-4ac0-8912-69b8f3de807e "./obj/cef/libcef_static/iothread_state.o" CXX obj/cef/libcef_static/iothread_state.o
            failed_match = re.search(r'FAILED:.*?"([^"]+)"', line)
            if failed_match:
                target = failed_match.group(1)
                # Extract basename without extension (e.g., iothread_state from iothread_state.o)
                basename = os.path.splitext(os.path.basename(target))[0]
                target_map[basename] = target.strip('./')

        # Second pass: process errors
        for line_idx, line in enumerate(lines, 1):
            # Match error lines: file:line:col: error: message
            error_match = re.match(r'^([^:]+):(\d+):(?:\d+:)? error: (.+)$', line)
            if error_match:
                file_path = error_match.group(1)
                source_line = int(error_match.group(2))
                error_msg = error_match.group(3)

                # Normalize file path
                file_path = re.sub(r'^(?:\.\./)+', '', file_path)

                # Create error reference
                error_ref = ErrorReference(
                    file_path=file_path,
                    line_number=source_line,
                    error_line=line_idx
                )

                # Add to file errors
                if file_path not in self.analysis.files:
                    # Try to find build target for this file
                    file_basename = os.path.splitext(os.path.basename(file_path))[0]
                    build_target = target_map.get(file_basename)
                    self.analysis.files[file_path] = FileErrors(
                        file_path=file_path,
                        build_target=build_target
                    )

                self.analysis.files[file_path].errors.append(error_ref)
                self.analysis.files[file_path].error_count += 1
                self.analysis.total_errors += 1

        self.analysis.total_files = len(self.analysis.files)

    def generate_summary_report(self, colorize: bool = True) -> str:
        """Generate human-readable summary with line references."""
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
        lines.append(f'{BOLD}CEF BUILD ERROR INDEX{RESET}')
        lines.append('=' * 80)
        lines.append(f'Build output file: {CYAN}{self.analysis.build_output_file}{RESET}')
        lines.append(f'Total errors: {RED}{self.analysis.total_errors}{RESET}')
        lines.append(f'Files with errors: {RED}{self.analysis.total_files}{RESET}')
        lines.append('')

        if self.analysis.total_errors == 0:
            lines.append(f'{GREEN}{BOLD}✓ BUILD SUCCESSFUL - No errors found!{RESET}')
            lines.append('=' * 80)
            return '\n'.join(lines)

        lines.append(f'{BOLD}ERRORS BY FILE (ordered by error count):{RESET}')
        lines.append('-' * 80)

        # Sort files by error count (most errors first)
        sorted_files = sorted(
            self.analysis.files.values(),
            key=lambda f: f.error_count,
            reverse=True
        )

        for i, file_errors in enumerate(sorted_files, 1):
            lines.append(f'\n{BOLD}{i}.{RESET} {CYAN}{file_errors.file_path}{RESET} - {RED}{file_errors.error_count} error(s){RESET}')
            if file_errors.build_target:
                lines.append(f'   {BOLD}Rebuild:{RESET} autoninja -C out/Debug_GN_arm64 {file_errors.build_target}')

            for error in file_errors.errors:
                lines.append(f'   • Line {error.line_number} '
                           f'{BOLD}→ {self.analysis.build_output_file}:{error.error_line}{RESET}')

        lines.append('')
        lines.append('-' * 80)
        lines.append(f'{BOLD}WORKFLOW:{RESET}')
        lines.append('')
        lines.append(f'1. Read error details: {CYAN}Read {self.analysis.build_output_file} at line <error_line>{RESET}')
        lines.append('')
        lines.append(f'2. Investigate Chromium changes:')
        lines.append(f'   {CYAN}git diff --no-prefix refs/tags/{self.old_version}...refs/tags/{self.new_version} -- <file_path>{RESET}')
        lines.append('')
        lines.append(f'3. Fix the error(s) in the file')
        lines.append('')
        lines.append(f'4. Rebuild just that file using the command shown above')
        lines.append(f'   {CYAN}Example: autoninja -C out/Debug_GN_arm64 obj/cef/libcef_static/file.o{RESET}')
        lines.append('')
        lines.append(f'5. If successful, move to next file and repeat')
        lines.append('')
        lines.append(f'6. After all files compile successfully, rebuild all:')
        lines.append(f'   {CYAN}autoninja -C out/Debug_GN_arm64 cef{RESET}')
        lines.append('')
        lines.append(f'{CYAN}Detailed instructions:{RESET} cef/tools/claude/CLAUDE_BUILD_INSTRUCTIONS.md')
        lines.append('=' * 80)
        lines.append('')

        return '\n'.join(lines)

    def generate_json_report(self) -> str:
        """Generate JSON report with line references."""
        report = {
            'build_output_file': self.analysis.build_output_file,
            'statistics': {
                'total_errors': self.analysis.total_errors,
                'total_files': self.analysis.total_files
            },
            'files': []
        }

        for file_errors in sorted(self.analysis.files.values(), key=lambda f: f.error_count, reverse=True):
            file_data = {
                'file_path': file_errors.file_path,
                'error_count': file_errors.error_count,
                'build_target': file_errors.build_target,
                'errors': [
                    {
                        'source_line': e.line_number,
                        'build_output_line': e.error_line
                    }
                    for e in file_errors.errors
                ]
            }
            report['files'].append(file_data)

        return json.dumps(report, indent=2)


def main():
    parser = argparse.ArgumentParser(
        description='Analyze ninja build output and create error index with line references'
    )
    parser.add_argument(
        'input_file',
        help='Build output file (e.g., build_output.txt)'
    )
    parser.add_argument(
        '--json',
        action='store_true',
        help='Output JSON format'
    )
    parser.add_argument(
        '--no-color',
        action='store_true',
        help='Disable color output'
    )
    parser.add_argument(
        '--old-version',
        help='Old Chromium version (e.g., 142.0.7444.0)'
    )
    parser.add_argument(
        '--new-version',
        help='New Chromium version (e.g., 143.0.7499.0)'
    )

    args = parser.parse_args()

    # Read input file
    with open(args.input_file, 'r') as f:
        output_text = f.read()

    # Analyze
    analyzer = BuildOutputAnalyzer(
        build_output_file=args.input_file,
        old_version=args.old_version,
        new_version=args.new_version
    )
    analyzer.parse(output_text)

    # Generate report
    if args.json:
        print(analyzer.generate_json_report())
    else:
        print(analyzer.generate_summary_report(colorize=not args.no_color))


if __name__ == '__main__':
    main()
