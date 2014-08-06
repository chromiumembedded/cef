# Copyright (c) 2011 The Chromium Embedded Framework Authors.
# Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gclient_util import *
import os, sys

# The CEF root directory is the parent directory of _this_ script.
cef_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))

print "\nChecking CEF and Chromium revisions..."
gyper = [ 'python', 'tools/check_revision.py' ]
RunAction(cef_dir, gyper)

print "\nGenerating CEF version header file..."
gyper = [ 'python', 'tools/make_version_header.py',
          '--header', 'include/cef_version.h',
          '--cef_version', 'VERSION',
          '--chrome_version', '../chrome/VERSION',
          '--cpp_header_dir', 'include' ]
RunAction(cef_dir, gyper)

print "\nPatching build configuration and source files for CEF..."
patcher = [ 'python', 'tools/patcher.py',
            '--patch-config', 'patch/patch.cfg' ]
RunAction(cef_dir, patcher)

print "\nGenerating CEF project files..."

# depot_tools currently bundles VS2013 Express Update 1 which causes linker
# errors with Debug builds (see issue #1304). Don't use the bundled version
# unless explicitly requested.
if not 'DEPOT_TOOLS_WIN_TOOLCHAIN' in os.environ.keys():
  os.environ['DEPOT_TOOLS_WIN_TOOLCHAIN'] = '0'

os.environ['CEF_DIRECTORY'] = os.path.basename(cef_dir)
gyper = [ 'python', '../build/gyp_chromium', 'cef.gyp', '-I', 'cef.gypi' ]
RunAction(cef_dir, gyper)
