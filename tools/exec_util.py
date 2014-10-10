# Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file

from subprocess import Popen, PIPE
import sys

def exec_cmd(cmd, path, input_file=None):
  """ Execute the specified command and return the result. """
  out = ''
  err = ''
  parts = cmd.split()
  try:
    if not input_file:
      process = Popen(parts, cwd=path, stdout=PIPE, stderr=PIPE,
                      shell=(sys.platform == 'win32'))
    else:
      with open(input_file, 'rb') as f:
        process = Popen(parts, cwd=path, stdout=PIPE, stderr=PIPE,
                        stdin=f,
                        shell=(sys.platform == 'win32'))
    out, err = process.communicate()
  except IOError, (errno, strerror):
    raise
  except:
    raise
  return {'out': out, 'err': err}
