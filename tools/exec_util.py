# Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file

from __future__ import absolute_import
from subprocess import Popen, PIPE
import sys


def exec_cmd(cmd, path, input_string=None):
  """ Execute the specified command and return the result. """
  out = ''
  err = ''
  ret = -1
  parts = cmd.split()
  try:
    if input_string is None:
      process = Popen(
          parts,
          cwd=path,
          stdout=PIPE,
          stderr=PIPE,
          shell=(sys.platform == 'win32'))
      out, err = process.communicate()
      ret = process.returncode
    else:
      process = Popen(
          parts,
          cwd=path,
          stdin=PIPE,
          stdout=PIPE,
          stderr=PIPE,
          shell=(sys.platform == 'win32'))
      out, err = process.communicate(input=input_string)
      ret = process.returncode
  except IOError as e:
    (errno, strerror) = e.args
    raise
  except:
    raise
  return {'out': out.decode('utf-8'), 'err': err.decode('utf-8'), 'ret': ret}
