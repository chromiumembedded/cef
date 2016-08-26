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

# Whether to use GN or GYP. GYP is currently the default.
use_gn = bool(int(os.environ.get('CEF_USE_GN', '0')))
gn_args = {}

custom_toolchain = False

if platform == 'windows':
  # Force use of the locally installed version of Visual Studio.
  if not 'DEPOT_TOOLS_WIN_TOOLCHAIN' in os.environ:
    os.environ['DEPOT_TOOLS_WIN_TOOLCHAIN'] = '0'

  # By default GN/GYP+Ninja on Windows expects Visual Studio to be installed on
  # the local machine. To build when Visual Studio is extracted to a directory
  # but not installed (e.g. via a custom toolchain):
  #
  # o Enable use of a custom toolchain on Windows.
  #
  #   set WIN_CUSTOM_TOOLCHAIN=1
  #
  # o Used by tools/msvs_env.bat to configure the MSVS tools environment.
  #   Should be set to "none" because VC variables for CEF will be set via
  #   INCLUDE/LIB/PATH.
  #   TODO(cef): Make INCLUDE/LIB/PATH values optional when
  #   "%VS_ROOT%\VC\vcvarsall.bat" exists (use those values instead).
  #
  #   set CEF_VCVARS=none
  #
  # o Used by the following scripts:
  #   (a) build/vs_toolchain.py SetEnvironmentAndGetRuntimeDllDirs when
  #   determining whether to copy VS runtime binaries to the output directory.
  #   If GYP_MSVS_OVERRIDE_PATH exists then binaries will not be copied and
  #   should instead be discoverable via the PATH env variable.
  #   (b) build/toolchain/win/setup_toolchain.py _LoadToolchainEnv when
  #   writing environment.* files that specify INCLUDE/LIB/PATH values. If
  #   "%GYP_MSVS_OVERRIDE_PATH%\VC\vcvarsall.bat" exists then environment
  #   variables will be derived from there and the specified INCLUDE/LIB/PATH
  #   values, if any, will be ignored by Chromium. If this file does not exist
  #   then the INCLUDE/LIB/PATH values are also required by Chromium.
  #   TODO(cef): Rename to VS_PATH and VS_VERSION after Chromium cleans up GYP
  #   dependencies.
  #
  #   set GYP_MSVS_OVERRIDE_PATH=<VS root directory>
  #   set GYP_MSVS_VERSION=<VS version>
  #
  # o Used to configure GN and GYP arguments in this script.
  #
  #   set VS_CRT_ROOT=<VS CRT root directory> (GN only)
  #   set SDK_ROOT=<Platform SDK root directory>
  #
  # o Used by various scripts as described above.
  #
  #   set INCLUDE=<VS include paths>
  #   set LIB=<VS library paths>
  #   set PATH=<VS executable paths>
  #
  # See tools/depot_tools/win_toolchain/package_from_installed.py for an example
  # packaging script along with required directory contents and INCLUDE/LIB/PATH
  # values.
  #
  if bool(int(os.environ.get('WIN_CUSTOM_TOOLCHAIN', '0'))):
    required_vars = [
      'CEF_VCVARS',
      'GYP_MSVS_OVERRIDE_PATH',
      'GYP_MSVS_VERSION',
      'SDK_ROOT',
      'INCLUDE',
      'LIB',
      'PATH',
    ]
    if use_gn:
      required_vars.append('VS_CRT_ROOT')
    for var in required_vars:
      if not var in os.environ.keys():
        raise Exception('%s environment variable must be set' % var)

    custom_toolchain = True

    if use_gn:
      # Windows custom toolchain requirements. See comments in gn_args.py.
      gn_args['visual_studio_path'] = os.environ['GYP_MSVS_OVERRIDE_PATH']
      gn_args['visual_studio_version'] = os.environ['GYP_MSVS_VERSION']
      gn_args['visual_studio_runtime_dirs'] = os.environ['VS_CRT_ROOT']
      gn_args['windows_sdk_path'] = os.environ['SDK_ROOT']
    else:
      # Set windows_sdk_path via GYP_DEFINES.
      gyp_defines = ''
      if 'GYP_DEFINES' in os.environ:
        gyp_defines = os.environ['GYP_DEFINES'] + ' '
      gyp_defines = \
        gyp_defines + \
        'windows_sdk_path=' + os.environ['SDK_ROOT'].replace('\\', '/')
      os.environ['GYP_DEFINES'] = gyp_defines

      # Generate environment files (environment.x64, environment.x86) in each
      # build output directory.
      # When using the default toolchain this is done by
      # GenerateEnvironmentFiles in src/tools/gyp/pylib/gyp/msvs_emulation.py.
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

if use_gn:
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
else:
  os.environ['CEF_DIRECTORY'] = os.path.basename(cef_dir)
  gyper = [ 'python', '../build/gyp_chromium', 'cef.gyp', '-I', 'cef.gypi' ]
  if custom_toolchain:
    # Disable GYP's auto-detection of the VS install.
    gyper.extend(['-G', 'ninja_use_custom_environment_files'])
  if 'GYP_ARGUMENTS' in os.environ.keys():
    gyper.extend(os.environ['GYP_ARGUMENTS'].split(' '))
  RunAction(cef_dir, gyper)
