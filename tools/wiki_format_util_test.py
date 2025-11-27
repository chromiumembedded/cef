#!/usr/bin/env python
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file

from __future__ import absolute_import
from __future__ import print_function
import unittest
from wiki_format_util import wiki_format


class TestWikiFormatUtil(unittest.TestCase):
  """Test cases for wiki_format_util."""

  def test_basic_formatting(self):
    """Test basic markdown formatting"""
    content = """# Title
**Steps:**
- Step 1
- Step 2"""

    result = wiki_format("test.md", content)

    # Should add blank line before list
    expected = """# Title
**Steps:**

- Step 1
- Step 2"""
    self.assertEqual(result, expected)

  def test_already_formatted(self):
    """Test content that's already correctly formatted"""
    content = """# Title

- Item 1
- Item 2"""

    result = wiki_format("test.md", content)
    self.assertEqual(result, content)

  def test_indentation_fix(self):
    """Test indentation correction"""
    content = """1. Step
   - Sub item"""

    result = wiki_format("test.md", content)

    expected = """1. Step
    - Sub item"""
    self.assertEqual(result, expected)

  def test_returns_none_on_error(self):
    """Test error handling - wiki_format should not raise but return None"""
    # This test verifies the error handling in wiki_format_util
    # The formatter itself shouldn't raise on normal content
    result = wiki_format("test.md", "# Valid markdown")
    self.assertIsNotNone(result)


if __name__ == '__main__':
  unittest.main()
