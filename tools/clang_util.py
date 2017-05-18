# Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file

from exec_util import exec_cmd
import sys

if sys.platform == 'win32':
  # Force use of the clang-format version bundled with depot_tools.
  clang_format_exe = 'clang-format.bat'
else:
  clang_format_exe = 'clang-format'

def clang_format(file_contents):
  result = exec_cmd(clang_format_exe, ".", file_contents)
  if result['out'] != '':
    output = result['out']
    if sys.platform == 'win32':
      # Convert to Unix line endings.
      output = output.replace("\r", "")
    return output
  return None
