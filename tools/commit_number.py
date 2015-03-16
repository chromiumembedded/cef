# Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import git_util as git
import os
import sys

# cannot be loaded as a module
if __name__ != "__main__":
    sys.stderr.write('This file cannot be loaded as a module!')
    sys.exit()

if git.is_checkout('.'):
  sys.stdout.write(git.get_commit_number())
else:
  raise Exception('Not a valid checkout')
