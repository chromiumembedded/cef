# Copyright (c) 2011 The Chromium Embedded Framework Authors.
# Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gclient_util import *
from gn_args import GetAllPlatformConfigs, GetConfigFileContents
from file_util import make_dir, write_file
import os, sys

# The CEF directory is the parent directory of _this_ script.
cef_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
# The src directory is the parent directory of the CEF directory.
src_dir = os.path.abspath(os.path.join(cef_dir, os.pardir))

# Determine the platform.
if sys.platform == 'win32':
  platform = 'windows'
elif sys.platform == 'darwin':
  platform = 'macosx'
elif sys.platform.startswith('linux'):
  platform = 'linux'
else:
  print 'Unknown operating system platform'
  sys.exit()


print "\nGenerating CEF version header file..."
cmd = [ 'python', 'tools/make_version_header.py',
        '--header', 'include/cef_version.h',
        '--cef_version', 'VERSION',
        '--chrome_version', '../chrome/VERSION',
        '--cpp_header_dir', 'include' ]
RunAction(cef_dir, cmd)

print "\nPatching build configuration and source files for CEF..."
cmd = [ 'python', 'tools/patcher.py',
        '--patch-config', 'patch/patch.cfg' ]
RunAction(cef_dir, cmd)

print "\nGenerating CEF project files..."

gn_args = {}

if platform == 'windows':
  # Force use of the locally installed version of Visual Studio.
  if not 'DEPOT_TOOLS_WIN_TOOLCHAIN' in os.environ:
    os.environ['DEPOT_TOOLS_WIN_TOOLCHAIN'] = '0'

  # By default GN+Ninja on Windows expects Visual Studio to be installed on
  # the local machine. To build when Visual Studio is extracted to a directory
  # but not installed (e.g. via a custom toolchain) set the following
  # environment variables:
  #
  #   set WIN_CUSTOM_TOOLCHAIN=1
  #   set VS_VERSION=<VS version>
  #   set VS_CRT_ROOT=<VS CRT root directory>
  #   set VS_ROOT=<VS root directory>
  #   set SDK_ROOT=<Platform SDK root directory>
  #   set INCLUDE=<VS include paths>
  #   set LIB=<VS library paths>
  #   set PATH=<VS executable paths>
  #
  if bool(int(os.environ.get('WIN_CUSTOM_TOOLCHAIN', '0'))):
    required_vars = [
      'VS_VERSION',
      'VS_CRT_ROOT',
      'VS_ROOT',
      'SDK_ROOT',
      'INCLUDE',
      'LIB',
      'PATH',
    ]
    for var in required_vars:
      if not var in os.environ.keys():
        raise Exception('%s environment variable must be set' % var)

    # VC variables for CEF will be set via INCLUDE/LIB/PATH.
    # TODO(cef): Make INCLUDE/PATH/LIB values optional when
    # "%VS_ROOT%\VC\vcvarsall.bat" exists (use those values instead).
    os.environ['CEF_VCVARS'] = 'none'

    # Checked in build/toolchain/win/setup_toolchain.py _LoadToolchainEnv.
    # If "%VS_ROOT%\VC\vcvarsall.bat" exists then environment variables will
    # be derived from there and the specified INCLUDE/LIB/PATH values, if any,
    # will be ignored by Chromium. If this file does not exist then the
    # INCLUDE/LIB/PATH values are also required by Chromium.
    os.environ['GYP_MSVS_OVERRIDE_PATH'] = os.environ['VS_ROOT']

    # Windows custom toolchain requirements. See comments in gn_args.py.
    gn_args['visual_studio_path'] = os.environ['VS_ROOT']
    gn_args['visual_studio_version'] = os.environ['VS_VERSION']
    gn_args['visual_studio_runtime_dirs'] = os.environ['VS_CRT_ROOT']
    gn_args['windows_sdk_path'] = os.environ['SDK_ROOT']

configs = GetAllPlatformConfigs(gn_args)
for dir, config in configs.items():
  # Create out directories and write the args.gn file.
  out_path = os.path.join(src_dir, 'out', dir)
  make_dir(out_path, False)
  args_gn_path = os.path.join(out_path, 'args.gn')
  args_gn_contents = GetConfigFileContents(config)
  write_file(args_gn_path, args_gn_contents)

  # Generate the Ninja config.
  cmd = [ 'gn', 'gen', os.path.join('out', dir) ]
  if 'GN_ARGUMENTS' in os.environ.keys():
    cmd.extend(os.environ['GN_ARGUMENTS'].split(' '))
  RunAction(src_dir, cmd)
