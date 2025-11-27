# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file

from __future__ import absolute_import
from __future__ import print_function
import os
import sys

# Script directory.
script_dir = os.path.dirname(__file__)

# Import the formatter from claude subdirectory
sys.path.insert(0, os.path.join(script_dir, 'claude'))
from format_wiki import WikiFormatter


def wiki_format(file_name, file_contents):
  """
  Format markdown file according to Bitbucket wiki guidelines.

  Args:
    file_name: Name of the file being formatted (for error messages)
    file_contents: String contents of the markdown file

  Returns:
    Formatted markdown content as string, or None on error
  """
  try:
    formatter = WikiFormatter()
    result = formatter.format_file(file_contents)
    return '\n'.join(result.formatted_lines)
  except Exception as e:
    print("wiki_format error: %s" % str(e))
    return None
