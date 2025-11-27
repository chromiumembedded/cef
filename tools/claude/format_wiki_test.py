#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""
Unit tests for format_wiki.py

Running the tests:
  # Run all tests with verbose output
  python3 -m unittest format_wiki_test.py -v

  # Run all tests (simple)
  python3 format_wiki_test.py

  # Run a specific test class
  python3 -m unittest format_wiki_test.TestBlankLineBeforeLists -v

  # Run a specific test
  python3 -m unittest format_wiki_test.TestBlankLineBeforeLists.test_subsection_header_to_list -v
"""

import os
import tempfile
import unittest
from format_wiki import WikiFormatter, FormatResult, expand_file_patterns


class TestBlankLineBeforeLists(unittest.TestCase):
    """Test cases for adding blank lines before lists."""

    def setUp(self):
        """Set up test fixtures."""
        self.formatter = WikiFormatter()

    def test_subsection_header_to_list(self):
        """Test adding blank line after **Header:** before list"""
        content = """**Common options:**
- Option 1
- Option 2"""
        result = self.formatter.format_file(content)

        expected = """**Common options:**

- Option 1
- Option 2"""
        self.assertEqual('\n'.join(result.formatted_lines), expected)
        self.assertEqual(len(result.issues), 1)
        self.assertEqual(result.issues[0].issue_type, 'missing_blank_line')
        self.assertTrue(result.changes_made)

    def test_paragraph_to_list(self):
        """Test adding blank line after paragraph text before list"""
        content = """Some paragraph text.
- List item
- Another item"""
        result = self.formatter.format_file(content)

        expected = """Some paragraph text.

- List item
- Another item"""
        self.assertEqual('\n'.join(result.formatted_lines), expected)
        self.assertTrue(result.changes_made)

    def test_already_has_blank_line(self):
        """Test that no blank line is added when one already exists"""
        content = """Some paragraph text.

- List item
- Another item"""
        result = self.formatter.format_file(content)

        # Should remain unchanged
        self.assertEqual('\n'.join(result.formatted_lines), content)
        self.assertFalse(result.changes_made)

    def test_section_header_no_blank_line(self):
        """Test that section headers don't get blank line before list"""
        content = """## Section Header
- List item
- Another item"""
        result = self.formatter.format_file(content)

        # Should remain unchanged - section headers don't need blank lines
        self.assertEqual('\n'.join(result.formatted_lines), content)
        self.assertFalse(result.changes_made)

    def test_direct_subitem_no_blank_line(self):
        """Test that direct sub-items under parent don't get blank line"""
        content = """1. Parent item
    - Sub-item
    - Another sub-item"""
        result = self.formatter.format_file(content)

        # Should remain unchanged - sub-items directly under parent
        self.assertEqual('\n'.join(result.formatted_lines), content)
        self.assertFalse(result.changes_made)

    def test_nested_list_with_paragraph(self):
        """Test nested list after indented paragraph needs blank line"""
        content = """1. Parent item
    Some indented paragraph.
    - Nested list item
    - Another item"""
        result = self.formatter.format_file(content)

        expected = """1. Parent item
    Some indented paragraph.

    - Nested list item
    - Another item"""
        self.assertEqual('\n'.join(result.formatted_lines), expected)
        self.assertTrue(result.changes_made)

    def test_numbered_list_after_subsection(self):
        """Test numbered list after subsection header"""
        content = """**Steps:**
1. First step
2. Second step"""
        result = self.formatter.format_file(content)

        expected = """**Steps:**

1. First step
2. Second step"""
        self.assertEqual('\n'.join(result.formatted_lines), expected)
        self.assertTrue(result.changes_made)


class TestListIndentation(unittest.TestCase):
    """Test cases for fixing list indentation to 4 spaces."""

    def setUp(self):
        """Set up test fixtures."""
        self.formatter = WikiFormatter()

    def test_fix_two_space_indent(self):
        """Test fixing 2-space indentation to 4 spaces"""
        content = """1. Main item
  - Sub-item
  - Another sub-item"""
        result = self.formatter.format_file(content)

        expected = """1. Main item
    - Sub-item
    - Another sub-item"""
        self.assertEqual('\n'.join(result.formatted_lines), expected)
        self.assertEqual(len(result.issues), 2)
        self.assertTrue(all(i.issue_type == 'incorrect_list_indent' for i in result.issues))
        self.assertTrue(result.changes_made)

    def test_fix_three_space_indent(self):
        """Test fixing 3-space indentation to 4 spaces"""
        content = """1. Main item
   - Sub-item"""
        result = self.formatter.format_file(content)

        expected = """1. Main item
    - Sub-item"""
        self.assertEqual('\n'.join(result.formatted_lines), expected)
        self.assertTrue(result.changes_made)

    def test_correct_four_space_indent(self):
        """Test that 4-space indentation is not changed"""
        content = """1. Main item
    - Sub-item (4 spaces)
    - Another sub-item"""
        result = self.formatter.format_file(content)

        # Should remain unchanged
        self.assertEqual('\n'.join(result.formatted_lines), content)
        self.assertFalse(result.changes_made)

    def test_fix_nested_indentation(self):
        """Test fixing nested list items at 8 spaces"""
        content = """1. Main item
    - Sub-item (4 spaces)
      - Nested sub-item (6 spaces - should be 8)"""
        result = self.formatter.format_file(content)

        expected = """1. Main item
    - Sub-item (4 spaces)
        - Nested sub-item (6 spaces - should be 8)"""
        self.assertEqual('\n'.join(result.formatted_lines), expected)
        self.assertTrue(result.changes_made)

    def test_top_level_list_unchanged(self):
        """Test that top-level (0 indent) lists are unchanged"""
        content = """- Top level item
- Another top level item"""
        result = self.formatter.format_file(content)

        self.assertEqual('\n'.join(result.formatted_lines), content)
        self.assertFalse(result.changes_made)


class TestCodeFenceIndentation(unittest.TestCase):
    """Test cases for fixing code fence indentation in lists."""

    def setUp(self):
        """Set up test fixtures."""
        self.formatter = WikiFormatter()

    def test_fix_code_fence_three_spaces(self):
        """Test fixing code fence with 3-space indentation"""
        content = """1. Step one
   ```cpp
   code_here();
   ```"""
        result = self.formatter.format_file(content)

        expected = """1. Step one
    ```cpp
   code_here();
    ```"""
        self.assertEqual('\n'.join(result.formatted_lines), expected)
        self.assertEqual(len(result.issues), 2)  # Opening and closing fence
        self.assertTrue(all(i.issue_type == 'incorrect_code_fence_indent' for i in result.issues))
        self.assertTrue(result.changes_made)

    def test_correct_code_fence_four_spaces(self):
        """Test that code fence with 4 spaces is unchanged"""
        content = """1. Step one
    ```cpp
    code_here();
    ```"""
        result = self.formatter.format_file(content)

        self.assertEqual('\n'.join(result.formatted_lines), content)
        self.assertFalse(result.changes_made)

    def test_top_level_code_fence_unchanged(self):
        """Test that top-level code fences (no indent) are unchanged"""
        content = """```cpp
void Function() {
  code();
}
```"""
        result = self.formatter.format_file(content)

        self.assertEqual('\n'.join(result.formatted_lines), content)
        self.assertFalse(result.changes_made)


class TestComplexScenarios(unittest.TestCase):
    """Test cases for complex formatting scenarios."""

    def setUp(self):
        """Set up test fixtures."""
        self.formatter = WikiFormatter()

    def test_multiple_subsections(self):
        """Test multiple subsections with lists"""
        content = """**First subsection:**
- Item 1
- Item 2

**Second subsection:**
- Item A
- Item B"""
        result = self.formatter.format_file(content)

        expected = """**First subsection:**

- Item 1
- Item 2

**Second subsection:**

- Item A
- Item B"""
        self.assertEqual('\n'.join(result.formatted_lines), expected)
        self.assertEqual(len(result.issues), 2)
        self.assertTrue(result.changes_made)

    def test_combined_indentation_and_blank_line(self):
        """Test fixing both indentation and adding blank lines"""
        content = """**Steps:**
1. First step
  - Sub-item (2 spaces)
  - Another sub-item"""
        result = self.formatter.format_file(content)

        expected = """**Steps:**

1. First step
    - Sub-item (2 spaces)
    - Another sub-item"""
        self.assertEqual('\n'.join(result.formatted_lines), expected)
        # Should have 1 blank line issue + 2 indentation issues
        self.assertGreaterEqual(len(result.issues), 3)
        self.assertTrue(result.changes_made)

    def test_realistic_documentation(self):
        """Test realistic documentation snippet"""
        content = """## Running Tests

**Platform-specific paths:**
- Windows: `ceftests.exe`
- Linux: `ceftests`

**Common options:**
- Run specific test: `--gtest_filter="MyTest.*"`
- List tests: `--gtest_list_tests`"""

        result = self.formatter.format_file(content)

        expected = """## Running Tests

**Platform-specific paths:**

- Windows: `ceftests.exe`
- Linux: `ceftests`

**Common options:**

- Run specific test: `--gtest_filter="MyTest.*"`
- List tests: `--gtest_list_tests`"""
        self.assertEqual('\n'.join(result.formatted_lines), expected)
        self.assertTrue(result.changes_made)


class TestEdgeCases(unittest.TestCase):
    """Test cases for edge cases and special scenarios."""

    def setUp(self):
        """Set up test fixtures."""
        self.formatter = WikiFormatter()

    def test_empty_content(self):
        """Test formatting empty content"""
        content = ""
        result = self.formatter.format_file(content)

        self.assertEqual('\n'.join(result.formatted_lines), content)
        self.assertFalse(result.changes_made)

    def test_skip_content_in_code_blocks(self):
        """Test that content inside code blocks is not formatted"""
        content = """**When creating tests:**

```
Tests:
- MyFeatureTest.BasicFunctionality
- MyFeatureTest.EdgeCase
```"""
        result = self.formatter.format_file(content)

        # Content inside code block should not be formatted
        self.assertEqual('\n'.join(result.formatted_lines), content)
        self.assertFalse(result.changes_made)

    def test_skip_indented_code_in_code_blocks(self):
        """Test that indentation inside code blocks is preserved"""
        content = """1. Step one
    ```cpp
  bad_indent();
    ```"""
        result = self.formatter.format_file(content)

        # Code fence should be fixed, but content inside preserved
        expected = """1. Step one
    ```cpp
  bad_indent();
    ```"""
        self.assertEqual('\n'.join(result.formatted_lines), expected)
        # No changes because code fence is already at 4 spaces
        self.assertFalse(result.changes_made)

    def test_nested_code_fences_in_markdown_block(self):
        """Test that code fences shown as examples inside markdown blocks are preserved"""
        content = """```markdown
1. **Item:**
    - Description
    - Code example:
      ```cpp
      code_here();
      ```
```"""
        result = self.formatter.format_file(content)

        # Everything inside the markdown block should be preserved as-is
        # The inner ```cpp and ``` are just text content, not real fences
        self.assertEqual('\n'.join(result.formatted_lines), content)
        self.assertFalse(result.changes_made)

    def test_no_lists(self):
        """Test content with no lists"""
        content = """# Title

Some paragraph text.

Another paragraph."""
        result = self.formatter.format_file(content)

        self.assertEqual('\n'.join(result.formatted_lines), content)
        self.assertFalse(result.changes_made)

    def test_list_at_start(self):
        """Test list at the start of content"""
        content = """- First item
- Second item"""
        result = self.formatter.format_file(content)

        self.assertEqual('\n'.join(result.formatted_lines), content)
        self.assertFalse(result.changes_made)

    def test_asterisk_bullet_points(self):
        """Test lists using asterisk (*) instead of dash (-)"""
        content = """**Options:**
* Option 1
* Option 2"""
        result = self.formatter.format_file(content)

        expected = """**Options:**

* Option 1
* Option 2"""
        self.assertEqual('\n'.join(result.formatted_lines), expected)
        self.assertTrue(result.changes_made)

    def test_mixed_bullet_types(self):
        """Test mixed bullet types (- and *)"""
        content = """1. Parent
  - Sub with dash
  * Sub with asterisk"""
        result = self.formatter.format_file(content)

        expected = """1. Parent
    - Sub with dash
    * Sub with asterisk"""
        self.assertEqual('\n'.join(result.formatted_lines), expected)
        self.assertTrue(result.changes_made)


class TestIssueReporting(unittest.TestCase):
    """Test cases for issue detection and reporting."""

    def setUp(self):
        """Set up test fixtures."""
        self.formatter = WikiFormatter()

    def test_issue_line_numbers(self):
        """Test that issues report correct line numbers"""
        content = """Line 1
**Header:**
- Item
"""
        result = self.formatter.format_file(content)

        # Blank line issue should be at line 3 (where the list starts)
        self.assertEqual(len(result.issues), 1)
        self.assertEqual(result.issues[0].line_number, 3)

    def test_issue_types(self):
        """Test that different issue types are correctly identified"""
        content = """**Header:**
- Item
  - Wrong indent"""
        result = self.formatter.format_file(content)

        issue_types = {issue.issue_type for issue in result.issues}
        self.assertIn('missing_blank_line', issue_types)
        self.assertIn('incorrect_list_indent', issue_types)

    def test_original_and_fixed_lines(self):
        """Test that issues include original and fixed lines"""
        content = """1. Parent
  - Wrong indent"""
        result = self.formatter.format_file(content)

        indent_issue = next(i for i in result.issues if i.issue_type == 'incorrect_list_indent')
        self.assertEqual(indent_issue.original_line, "  - Wrong indent")
        self.assertEqual(indent_issue.fixed_line, "    - Wrong indent")


class TestWildcardPatterns(unittest.TestCase):
    """Test cases for wildcard pattern expansion."""

    def setUp(self):
        """Create temporary directory with test files."""
        self.temp_dir = tempfile.mkdtemp()
        # Create test files
        self.test_files = [
            'file1.md',
            'file2.md',
            'CLAUDE_TEST.md',
            'CLAUDE_BUILD.md',
            'README.md',
            'test.txt'  # Non-markdown file
        ]
        for filename in self.test_files:
            filepath = os.path.join(self.temp_dir, filename)
            with open(filepath, 'w') as f:
                f.write('# Test\n')

    def tearDown(self):
        """Clean up temporary directory."""
        import shutil
        shutil.rmtree(self.temp_dir)

    def test_wildcard_all_md(self):
        """Test *.md pattern matches all markdown files"""
        pattern = os.path.join(self.temp_dir, '*.md')
        result = expand_file_patterns([pattern])

        # Should match 5 .md files
        self.assertEqual(len(result), 5)
        # Verify all results end with .md
        for path in result:
            self.assertTrue(path.endswith('.md'))

    def test_wildcard_prefix(self):
        """Test CLAUDE_* pattern"""
        pattern = os.path.join(self.temp_dir, 'CLAUDE_*')
        result = expand_file_patterns([pattern])

        # Should match 2 CLAUDE_ files
        self.assertEqual(len(result), 2)
        for path in result:
            basename = os.path.basename(path)
            self.assertTrue(basename.startswith('CLAUDE_'))

    def test_wildcard_specific(self):
        """Test file?.md pattern"""
        pattern = os.path.join(self.temp_dir, 'file?.md')
        result = expand_file_patterns([pattern])

        # Should match file1.md and file2.md
        self.assertEqual(len(result), 2)
        basenames = sorted([os.path.basename(p) for p in result])
        self.assertEqual(basenames, ['file1.md', 'file2.md'])

    def test_no_wildcard(self):
        """Test regular file path without wildcard"""
        filepath = os.path.join(self.temp_dir, 'README.md')
        result = expand_file_patterns([filepath])

        # Should return the single file
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0], filepath)

    def test_mixed_patterns(self):
        """Test combining wildcards and regular files"""
        pattern1 = os.path.join(self.temp_dir, 'CLAUDE_*')
        file1 = os.path.join(self.temp_dir, 'README.md')
        result = expand_file_patterns([pattern1, file1])

        # Should return 2 CLAUDE_ files + 1 README
        self.assertEqual(len(result), 3)

    def test_no_matches(self):
        """Test pattern that matches nothing"""
        pattern = os.path.join(self.temp_dir, 'NONEXISTENT_*')
        result = expand_file_patterns([pattern])

        # Should return empty list
        self.assertEqual(len(result), 0)

    def test_nonexistent_regular_file(self):
        """Test non-wildcard path for nonexistent file"""
        filepath = os.path.join(self.temp_dir, 'nonexistent.md')
        result = expand_file_patterns([filepath])

        # Should still return the path (error will be caught during file read)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0], filepath)


if __name__ == '__main__':
    unittest.main()
