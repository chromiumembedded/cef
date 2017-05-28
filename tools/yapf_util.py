# Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file

from exec_util import exec_cmd
import os
import sys

# Script directory.
script_dir = os.path.dirname(__file__)
root_dir = os.path.join(script_dir, os.pardir)


def yapf_format(file_name, file_contents):
  # Reads .style.yapf in the root_dir when specifying contents via stdin.
  result = exec_cmd("%s %s/yapf" % (sys.executable, script_dir), root_dir,
                    file_contents)
  if result['err'] != '':
    print "yapf error: %s" % result['err']
  if result['out'] != '':
    output = result['out']
    if sys.platform == 'win32':
      # Convert to Unix line endings.
      output = output.replace("\r", "")
    return output
  return None
