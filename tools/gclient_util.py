# Copyright (c) 2011 The Chromium Embedded Framework Authors.
# Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import
from __future__ import print_function
import os, sys

try:
  # depot_tools may already be in the import path.
  import gclient_utils
except ImportError as e:
  # Search the PATH environment variable to find the depot_tools folder.
  depot_tools = None
  paths = os.environ.get('PATH').split(os.pathsep)
  for path in paths:
    if os.path.exists(os.path.join(path, 'gclient_utils.py')):
      depot_tools = path
      break

  if depot_tools is None:
    print('Error: could not find depot_tools in PATH.', file=sys.stderr)
    sys.exit(2)

  # Add depot_tools to import path.
  sys.path.append(depot_tools)
  import gclient_utils


# Copied from gclient.py python code.
def RunAction(dir, command):
  """Runs the action."""
  try:
    gclient_utils.CheckCallAndFilter(
        command, cwd=dir, always_show_header=True, print_stdout=True)
  except gclient_utils.Error as e:
    # Use a discrete exit status code of 2 to indicate that a hook action
    # failed.  Users of this script may wish to treat hook action failures
    # differently from VC failures.
    print('Error: %s' % str(e), file=sys.stderr)
    sys.exit(2)
