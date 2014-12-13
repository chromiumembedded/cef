# Copyright (c) 2011 The Chromium Embedded Framework Authors.
# Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gclient_util import *
import os, sys

# The CEF directory is the parent directory of _this_ script.
cef_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
# The src directory is the parent directory of the CEF directory.
src_dir = os.path.abspath(os.path.join(cef_dir, os.pardir))

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

# By default GYP+Ninja on Windows expects Visual Studio to be installed on the
# local machine. To build when Visual Studio is extracted to a directory but not
# installed (e.g. via a custom toolchain) you have two options:
#
# 1. Set up the environment using only environment variables:
#   set WIN_CUSTOM_TOOLCHAIN=1
#   set VS_ROOT=<VS root directory>
#   set SDK_ROOT=<Platform SDK root directory>
#   set INCLUDE=<VS include paths>
#   set PATH=<VS executable paths>
#   set LIB=<VS library paths>
#
# 2. Set up the environment using a combination of environment variables and the
#    "%GYP_MSVS_OVERRIDE_PATH%\VC\vcvarsall.bat" script:
#   set GYP_MSVS_OVERRIDE_PATH=<VS root directory>
#   set GYP_DEFINES="windows_sdk_path=<Platform SDK root directory>"
#
# The following environment variables must also be set:
#   set DEPOT_TOOLS_WIN_TOOLCHAIN=0
#   set GYP_MSVS_VERSION=<VS version>
#   set CEF_VCVARS=<empty .bat file>
custom_toolchain = False
if bool(int(os.environ.get('WIN_CUSTOM_TOOLCHAIN', '0'))):
  required_vars = [
    'GYP_MSVS_VERSION',
    'VS_ROOT',
    'SDK_ROOT',
    'INCLUDE',
    'PATH',
    'LIB',
  ]
  for var in required_vars:
    if not var in os.environ.keys():
      raise Exception('%s environment variable must be set' % var)

  custom_toolchain = True

  # Set windows_sdk_path via GYP_DEFINES.
  gyp_defines = ''
  if 'GYP_DEFINES' in os.environ.keys():
    gyp_defines = os.environ['GYP_DEFINES'] + ' '
  gyp_defines = gyp_defines + \
                'windows_sdk_path=' + os.environ['SDK_ROOT'].replace('\\', '/')
  os.environ['GYP_DEFINES'] = gyp_defines

  # Necessary to return correct VS version information via GetVSVersion in
  # src/tools/gyp/pylib/gyp/msvs_emulation.py.
  os.environ['GYP_MSVS_OVERRIDE_PATH'] = os.environ['VS_ROOT']

  # Generate environment files (environment.x64, environment.x86) in each
  # build output directory.
  # When using the default toolchain this is done by GenerateEnvironmentFiles
  # in src/tools/gyp/pylib/gyp/msvs_emulation.py.
  setup_script = os.path.join(cef_dir, 'tools/setup_toolchain.py')
  win_tool_script = os.path.join(src_dir, 'tools/gyp/pylib/gyp/win_tool.py')
  out_dirs = ['Debug', 'Debug_x64', 'Release', 'Release_x64']
  for out_dir in out_dirs:
    out_dir_abs = os.path.join(src_dir, 'out', out_dir)
    if not os.path.exists(out_dir_abs):
      os.makedirs(out_dir_abs)
    cmd = ['python', setup_script,
           os.environ['VS_ROOT'], win_tool_script, os.environ['SDK_ROOT']]
    RunAction(out_dir_abs, cmd)

os.environ['CEF_DIRECTORY'] = os.path.basename(cef_dir)
gyper = [ 'python', '../build/gyp_chromium', 'cef.gyp', '-I', 'cef.gypi' ]
if custom_toolchain:
  # Disable GYP's auto-detection of the VS install.
  gyper.extend(['-G', 'ninja_use_custom_environment_files'])
if 'GYP_ARGUMENTS' in os.environ.keys():
  gyper.extend(os.environ['GYP_ARGUMENTS'].split(' '))
RunAction(cef_dir, gyper)
