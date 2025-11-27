#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""
Apply Bitbucket wiki formatting guidelines to markdown files.

Automatically fixes common Bitbucket wiki formatting issues:
- Adds blank lines before lists after paragraphs/subsections
- Ensures 4-space indentation for nested list items
- Ensures 4-space indentation for code block fences in lists

Usage:
  # Check formatting without modifying file
  python3 format_wiki.py CLAUDE_TEST_INSTRUCTIONS.md --check

  # Format file in place
  python3 format_wiki.py CLAUDE_TEST_INSTRUCTIONS.md --in-place

  # Output formatted content to stdout
  python3 format_wiki.py CLAUDE_TEST_INSTRUCTIONS.md

  # Format multiple files
  python3 format_wiki.py file1.md file2.md --in-place

  # Format all markdown files
  python3 format_wiki.py "*.md" --check

  # Format all CLAUDE instruction files
  python3 format_wiki.py "CLAUDE_*.md" --in-place

See CLAUDE_WIKI_INSTRUCTIONS.md for complete formatting guidelines.
"""

import argparse
import glob as glob_module
import os
import re
import sys
from dataclasses import dataclass, field
from typing import List, Tuple, Optional


@dataclass
class FormatIssue:
    """Represents a formatting issue found in the markdown."""
    line_number: int
    issue_type: str
    description: str
    original_line: str
    fixed_line: Optional[str] = None


@dataclass
class FormatResult:
    """Results from formatting a markdown file."""
    formatted_lines: List[str]
    issues: List[FormatIssue] = field(default_factory=list)
    changes_made: bool = False


class WikiFormatter:
    """Formats markdown files according to Bitbucket wiki guidelines."""

    # Patterns for detecting markdown elements
    SUBSECTION_HEADER_PATTERN = re.compile(r'^\*\*[^*]+:\*\*$')
    LIST_ITEM_PATTERN = re.compile(r'^(\s*)[-*]\s')
    NUMBERED_LIST_PATTERN = re.compile(r'^(\s*)\d+\.\s')
    CODE_FENCE_PATTERN = re.compile(r'^(\s*)```')
    BLANK_LINE_PATTERN = re.compile(r'^\s*$')

    def __init__(self):
        self.issues = []

    def format_file(self, content: str) -> FormatResult:
        """
        Format markdown content according to Bitbucket wiki guidelines.

        Args:
            content: The markdown content to format

        Returns:
            FormatResult with formatted lines and detected issues
        """
        lines = content.split('\n')
        formatted_lines = []
        self.issues = []

        i = 0
        in_code_block = False
        opening_fence_indent = 0  # Track ORIGINAL opening fence indentation
        while i < len(lines):
            line = lines[i]
            next_line = lines[i + 1] if i + 1 < len(lines) else None

            # Check if this is a code fence
            fence_match = self.CODE_FENCE_PATTERN.match(line)
            is_code_fence = fence_match is not None

            # Determine if this fence opens/closes a code block
            # Only fences at the same ORIGINAL indentation as opening fence can close it
            if is_code_fence:
                fence_indent = len(fence_match.group(1))
                if in_code_block:
                    # Inside block: only closing fence at same ORIGINAL indent is processed
                    is_closing_fence = (fence_indent == opening_fence_indent)
                    if not is_closing_fence:
                        # This is text content that looks like a fence, preserve it
                        formatted_lines.append(line)
                        i += 1
                        continue
                else:
                    # Not in block: this is an opening fence
                    # Remember ORIGINAL indent (before any fixing)
                    opening_fence_indent = fence_indent

            # Skip formatting list/paragraph rules inside code blocks
            # But still process code fences themselves for indentation
            if in_code_block and not is_code_fence:
                formatted_lines.append(line)
                i += 1
                continue

            # Toggle code block state after processing the fence
            if is_code_fence:
                # Fix code fence indentation
                fixed_line, indentation_issue = self._fix_list_indentation(line, i)
                if indentation_issue:
                    self.issues.append(indentation_issue)
                    formatted_lines.append(fixed_line)
                else:
                    formatted_lines.append(line)

                # Toggle state
                in_code_block = not in_code_block
                i += 1
                continue

            # Rule 1: Add blank line before lists after paragraphs/subsections
            if next_line and self._is_list_start(next_line):
                if self._needs_blank_line_before_list(line, next_line):
                    self._add_issue(
                        i + 1,
                        'missing_blank_line',
                        'Missing blank line before list',
                        next_line
                    )
                    formatted_lines.append(line)
                    formatted_lines.append('')  # Add blank line
                    i += 1
                    continue

            # Rule 2: Fix list indentation to 4 spaces
            fixed_line, indentation_issue = self._fix_list_indentation(line, i)
            if indentation_issue:
                self.issues.append(indentation_issue)
                formatted_lines.append(fixed_line)
            else:
                formatted_lines.append(line)

            i += 1

        result = FormatResult(
            formatted_lines=formatted_lines,
            issues=self.issues,
            changes_made=len(self.issues) > 0
        )
        return result

    def _is_list_start(self, line: str) -> bool:
        """Check if line is the start of a list (bullet or numbered)."""
        return (self.LIST_ITEM_PATTERN.match(line) is not None or
                self.NUMBERED_LIST_PATTERN.match(line) is not None)

    def _needs_blank_line_before_list(self, prev_line: str, list_line: str) -> bool:
        """
        Determine if a blank line is needed before the list.

        Blank lines are needed after:
        - Paragraph text (non-empty, non-list lines)
        - Subsection headers (**Header:**)

        Blank lines are NOT needed:
        - When list is already preceded by a blank line
        - When list item is a direct sub-item (indented under parent)
        - After section headers (##)
        """
        # Already has blank line
        if self.BLANK_LINE_PATTERN.match(prev_line):
            return False

        # Section headers don't need blank line
        if prev_line.strip().startswith('#'):
            return False

        # Check if this is a sub-item directly under a parent list item
        list_match = self.LIST_ITEM_PATTERN.match(list_line) or \
                     self.NUMBERED_LIST_PATTERN.match(list_line)
        if list_match:
            list_indent = len(list_match.group(1))
            # If indented (4+ spaces), it's a sub-item - check if previous line is parent
            if list_indent >= 4:
                prev_is_list = (self.LIST_ITEM_PATTERN.match(prev_line) or
                               self.NUMBERED_LIST_PATTERN.match(prev_line))
                if prev_is_list:
                    # Direct sub-item under parent - no blank line needed
                    return False

        # Subsection headers need blank line before list
        if self.SUBSECTION_HEADER_PATTERN.match(prev_line):
            return True

        # Regular paragraph text (non-empty, non-list) needs blank line
        if prev_line.strip() and not self._is_list_start(prev_line):
            return True

        return False

    def _fix_list_indentation(self, line: str, line_num: int) -> Tuple[str, Optional[FormatIssue]]:
        """
        Fix list item indentation to use 4 spaces.

        Also fixes code fence indentation to match list nesting.

        Returns:
            Tuple of (fixed_line, issue or None)
        """
        # Check for list items with 2 or 3 space indentation
        list_match = self.LIST_ITEM_PATTERN.match(line) or \
                     self.NUMBERED_LIST_PATTERN.match(line)
        if list_match:
            indent = list_match.group(1)
            indent_len = len(indent)
            # Check if indentation is not a multiple of 4
            if indent_len > 0 and indent_len % 4 != 0:
                # Round up to nearest multiple of 4
                correct_indent = ((indent_len + 3) // 4) * 4
                fixed_line = ' ' * correct_indent + line[indent_len:]
                issue = FormatIssue(
                    line_number=line_num + 1,
                    issue_type='incorrect_list_indent',
                    description=f'List item has {indent_len} space(s), should be {correct_indent}',
                    original_line=line,
                    fixed_line=fixed_line
                )
                return fixed_line, issue

        # Check for code fences with incorrect indentation
        fence_match = self.CODE_FENCE_PATTERN.match(line)
        if fence_match:
            indent = fence_match.group(1)
            indent_len = len(indent)
            # Code fences in lists should be indented to multiples of 4
            if indent_len > 0 and indent_len % 4 != 0:
                correct_indent = ((indent_len + 3) // 4) * 4
                fixed_line = ' ' * correct_indent + line[indent_len:]
                issue = FormatIssue(
                    line_number=line_num + 1,
                    issue_type='incorrect_code_fence_indent',
                    description=f'Code fence has {indent_len} space(s), should be {correct_indent}',
                    original_line=line,
                    fixed_line=fixed_line
                )
                return fixed_line, issue

        return line, None

    def _add_issue(self, line_num: int, issue_type: str, description: str, line: str):
        """Add a formatting issue to the list."""
        self.issues.append(FormatIssue(
            line_number=line_num + 1,
            issue_type=issue_type,
            description=description,
            original_line=line
        ))


def format_markdown_file(file_path: str, in_place: bool = False, check: bool = False) -> bool:
    """
    Format a single markdown file.

    Args:
        file_path: Path to the markdown file
        in_place: If True, modify the file in place
        check: If True, only check for issues without modifying

    Returns:
        True if changes were made (or would be made in check mode)
    """
    # Read file
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except IOError as e:
        print(f"Error reading {file_path}: {e}", file=sys.stderr)
        return False

    # Format content
    formatter = WikiFormatter()
    result = formatter.format_file(content)

    # Report issues
    if result.issues:
        print(f"\n{file_path}:")
        for issue in result.issues:
            print(f"  Line {issue.line_number}: {issue.description}")
            if issue.fixed_line:
                print(f"    Original: {repr(issue.original_line)}")
                print(f"    Fixed:    {repr(issue.fixed_line)}")

    # Take action based on mode
    if check:
        if result.changes_made:
            print(f"\n{file_path} needs formatting")
            return True
        else:
            print(f"\n{file_path} is correctly formatted")
            return False
    elif in_place:
        if result.changes_made:
            try:
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write('\n'.join(result.formatted_lines))
                print(f"\n{file_path} formatted successfully ({len(result.issues)} issues fixed)")
                return True
            except IOError as e:
                print(f"Error writing {file_path}: {e}", file=sys.stderr)
                return False
        else:
            print(f"\n{file_path} already correctly formatted")
            return False
    else:
        # Output to stdout
        print('\n'.join(result.formatted_lines))
        return result.changes_made


def expand_file_patterns(patterns):
    """
    Expand file patterns (including wildcards) to actual file paths.

    Args:
        patterns: List of file paths or wildcard patterns

    Returns:
        List of expanded file paths
    """
    expanded_files = []

    for pattern in patterns:
        # Check if pattern contains wildcards
        if '*' in pattern or '?' in pattern:
            # Expand the glob pattern
            matches = glob_module.glob(pattern)
            if not matches:
                print(f"Warning: No files match pattern '{pattern}'", file=sys.stderr)
            else:
                expanded_files.extend(matches)
        else:
            # Regular file path (may or may not exist - error will be caught later)
            expanded_files.append(pattern)

    return expanded_files


def main():
    """Main entry point for the script."""
    parser = argparse.ArgumentParser(
        description='Apply Bitbucket wiki formatting guidelines to markdown files',
        epilog='See CLAUDE_WIKI_INSTRUCTIONS.md for formatting guidelines'
    )
    parser.add_argument(
        'files',
        nargs='+',
        help='Markdown file(s) or patterns to format (e.g., *.md, CLAUDE_*.md)'
    )
    parser.add_argument(
        '--in-place',
        action='store_true',
        help='Modify files in place'
    )
    parser.add_argument(
        '--check',
        action='store_true',
        help='Check if files need formatting (exit 1 if changes needed)'
    )

    args = parser.parse_args()

    if args.in_place and args.check:
        print("Error: --in-place and --check cannot be used together", file=sys.stderr)
        sys.exit(1)

    # Expand file patterns (wildcards)
    file_paths = expand_file_patterns(args.files)

    if not file_paths:
        print("Error: No files to process", file=sys.stderr)
        sys.exit(1)

    # Process files
    needs_formatting = False
    for file_path in file_paths:
        if format_markdown_file(file_path, in_place=args.in_place, check=args.check):
            needs_formatting = True

    # Exit with error code if check mode and files need formatting
    if args.check and needs_formatting:
        sys.exit(1)


if __name__ == '__main__':
    main()
