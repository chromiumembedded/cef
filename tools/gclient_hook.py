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
            '--patch-config', 'patch/patch.cfg' ];
RunAction(cef_dir, patcher)

# The DEPS file contains hooks that perform necessary actions for various
# platforms. Due to broken GN binaries on Debian 7 (https://crbug.com/329649)
# we don't want to run hooks when executing `gclient sync`. So instead we've
# added the `-n` (no-hooks) switch to `gclient sync` in automate.py and moved
# the few necessary actions here.

print "\nPulling clang (Mac only)..."
command = [ 'python', '../tools/clang/scripts/update.py', '--mac-only' ];
RunAction(cef_dir, command)

print "\nUpdating cygwin mount (Windows only)..."
command = [ 'python', '../build/win/setup_cygwin_mount.py',
            '--win-only' ];
RunAction(cef_dir, command)

print "\nUpdating LASTCHANGE..."
command = [ 'python', '../build/util/lastchange.py',
            '-o', '../build/util/LASTCHANGE' ];
RunAction(cef_dir, command)

print "\nUpdating LASTCHANGE.blink..."
command = [ 'python', '../build/util/lastchange.py',
            '-s', '../third_party/WebKit',
            '-o', '../build/util/LASTCHANGE.blink' ];
RunAction(cef_dir, command)

print "\nGenerating CEF project files..."
os.environ['CEF_DIRECTORY'] = os.path.basename(cef_dir);
gyper = [ 'python', 'tools/gyp_cef', 'cef.gyp', '-I', 'cef.gypi' ]
RunAction(cef_dir, gyper)
