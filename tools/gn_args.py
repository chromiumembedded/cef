# Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
# 2012 Google Inc. All rights reserved. Use of this source code is governed by
# a BSD-style license that can be found in the LICENSE file.

# This script determines the contents of the per-configuration `args.gn` files
# that are used to build CEF/Chromium with GN. See comments in CEF's top-level
# BUILD.gn file for general GN usage instructions.
#
# This script performs the following tasks:
#
# - Defines CEF's default and required arg values in cases where they differ
#   from Chromium's.
# - Accepts user-defined arg values via the GN_DEFINES environment variable.
# - Verifies that user-defined arg values do not conflict with CEF's
#   requirements.
# - Generates multiple configurations by combining user-defined arg values with
#   CEF's default and required values.
#
# Before adding a new arg value in this script determine the following:
#
# - Chromium's default value. Default values are defined in the declare_args()
#   sections of *.gni files.
# - Chromium's value requirements. Check for assert()s related to the value in
#   Chromium code.
# - Whether a particular value is optional or required for CEF.
# - Under what conditions a particular value is required for CEF (platform,
#   build type, CPU architecture, etc).
#
# If CEF can use Chromium's default value and has no additional validation
# requirements then do nothing.
#
# If CEF can use Chromium's default value but would like to enforce additional
# validation requirements then go to 3B.
#
# If CEF cannot or should not use Chromium's default value then choose one of
# the following:
#
# 1. If CEF requires a different value either globally or based on the platform:
#  - Add an assert() for the value in CEF's top-level BUILD.gn file.
#  - Add the required value in GetRequiredArgs().
#  - Result: CEF's required value will be used. The user cannot override the
#    value via GN_DEFINES.
#
# 2. If CEF requires a different value based on the build type or CPU
#    architecture:
#  - Add an assert() for the value in CEF's top-level BUILD.gn file.
#  - Add the required value in GetConfigArgs().
#  - Result: CEF's required value will be used. The user cannot override the
#    value via GN_DEFINES.
#
# 3. If CEF recommends (but does not require) a different value either globally
#    or based on the platform:
#    A. Set the default value:
#     - Add the recommended value in GetRecommendedDefaultArgs().
#     - Result: CEF's recommended value will be used by default. The user can
#       override the value via GN_DEFINES.
#
#    B. If CEF has additional validation requirements:
#     - Add the default Chromium value in GetChromiumDefaultArgs().
#     - Perform validation in ValidateArgs().
#     - Result: An AssertionError will be thrown if validation fails.

from __future__ import absolute_import
from __future__ import print_function
import os
import platform as python_platform
import re
import shlex
import sys

# The CEF directory is the parent directory of _this_ script.
cef_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
# The src directory is the parent directory of the CEF directory.
src_dir = os.path.abspath(os.path.join(cef_dir, os.pardir))

# Determine the platform.
if sys.platform == 'win32':
  platform = 'windows'
  # Windows machines report 'ARM64' or 'AMD64'.
  machine = 'arm64' if python_platform.machine() == 'ARM64' else 'amd64'
elif sys.platform == 'darwin':
  platform = 'mac'
  # Mac machines report 'arm64' or 'x86_64'.
  machine = 'arm64' if python_platform.machine() == 'arm64' else 'amd64'
elif sys.platform.startswith('linux'):
  platform = 'linux'
else:
  print('Unknown operating system platform')
  sys.exit()

_QUIET = False


def msg(msg):
  if not _QUIET:
    print('NOTE: ' + msg)


def ParseValue(value_str):
  """
  Parse a GN value string and return the appropriate Python type.
  """
  value_str = value_str.strip()

  # Handle quoted strings.
  if (value_str.startswith('"') and value_str.endswith('"')) or \
     (value_str.startswith("'") and value_str.endswith("'")):
    # Remove quotes.
    return value_str[1:-1]

  # Handle boolean values.
  if value_str.lower() == 'true':
    return True
  elif value_str.lower() == 'false':
    return False

  # Handle numeric values.
  try:
    if '.' in value_str:
      return float(value_str)
    else:
      return int(value_str)
  except ValueError:
    pass

  # Handle arrays (basic support).
  if value_str.startswith('[') and value_str.endswith(']'):
    # Simple array parsing - assumes simple comma-separated values.
    inner = value_str[1:-1].strip()
    if not inner:
      return []

    items = []
    for item in inner.split(','):
      item = item.strip()
      if item:
        items.append(ParseValue(item))
    return items

  # Return as string if no other type matches.
  return value_str


def FormatValue(val):
  """
  Return the GN value string for a Python value.
  """
  if isinstance(val, bool):
    if val:
      return 'true'
    else:
      return 'false'
  elif isinstance(val, int) or isinstance(val, float):
    return val
  elif isinstance(val, list):
    return '[' + ', '.join([FormatValue(v) for v in list]) + ']'
  else:
    return '"%s"' % val
  return val


def ParseArgsFile(filepath):
  """
  Parse a GN args file and return all name/value pairs as a dictionary.
  """
  result = {}

  if not os.path.exists(filepath):
    raise FileNotFoundError(f"File not found: {filepath}")

  with open(filepath, 'r') as f:
    lines = f.readlines()

  for line in lines:
    line = line.strip()

    # Skip empty lines and comments.
    if not line or line.startswith('#'):
      continue

    # Look for variable assignments (name = value).
    match = re.match(r'^([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*(.+)$', line)
    if not match:
      continue

    var_name = match.group(1)
    var_value = match.group(2).strip()

    # Parse the value based on its format.
    result[var_name] = ParseValue(var_value)

  return result


def ParseNameValueList(name_value_list):
  """
  Takes an array of strings of the form 'NAME=VALUE' and creates a dictionary
  of the pairs. If a string is simply NAME, then the value in the dictionary
  is set to True. If VALUE can be converted to a boolean or integer, it is.
  """
  result = {}
  for item in name_value_list:
    tokens = item.split('=', 1)
    key = tokens[0].strip()
    if len(tokens) == 2:
      # Parse the value based on its format.
      result[key] = ParseValue(tokens[1])
    else:
      # No value supplied, treat it as a boolean and set it.
      result[key] = True
  return result


def ShlexEnv(env_name):
  """
  Split an environment variable using shell-like syntax.
  """
  flags = os.environ.get(env_name, [])
  if flags:
    flags = shlex.split(flags)
  return flags


def MergeDicts(*dict_args):
  """
  Given any number of dicts, shallow copy and merge into a new dict.
  Precedence goes to key value pairs in latter dicts.
  """
  result = {}
  for dictionary in dict_args:
    result.update(dictionary)
  return result


def GetChromiumDefaultArgs(is_debug):
  """
  Return default GN args. These must match the Chromium defaults.
  Only args that may be retrieved via GetArgValue() need to be specified here.
  """
  # Search for these values in declare_args() sections of *.gni files to find
  # the defaults.

  defaults = {
      'dcheck_always_on': False,
      'is_asan': False,
      'is_component_build': is_debug,
      'is_debug': True,
      'is_official_build': False,
      'target_cpu': 'x64',
  }

  if platform == 'linux':
    defaults['use_sysroot'] = True

  if platform == 'windows':
    defaults['is_win_fastlink'] = False
    defaults['visual_studio_path'] = ''
    defaults['visual_studio_version'] = ''
    defaults['visual_studio_runtime_dirs'] = ''
    defaults['windows_sdk_path'] = ''
    defaults['windows_sdk_version'] = ''

  return defaults


def GetArgValue(args, key, is_debug=None):
  """
  Return an existing GN arg value or the Chromium default.
  The |is_debug| argument is required in cases where |key| references it in
  GetChromiumDefaultArgs.
  """
  defaults = GetChromiumDefaultArgs(is_debug)
  assert not defaults.get(key, None) is None, \
      "No default Chromium value specified for %s" % key
  return args.get(key, defaults[key])


def GetRecommendedDefaultArgs():
  """
  Return recommended default GN args that differ from Chromium defaults.
  """
  # Search for these values in declare_args() sections of *.gni files to find
  # the defaults.

  result = {
      # Enable NaCL. Default is true. False is recommended for faster builds.
      'enable_nacl': False,

      # Disable component builds. Default depends on the platform. True results
      # in faster local builds but False is required to create a CEF binary
      # distribution.
      'is_component_build': False,

      # Disable support for background apps, which don't make sense with CEF.
      # Default is enabled on desktop platforms. This feature was also causing
      # strange shutdown crashes when using the Chrome runtime with a Debug
      # component build on Windows.
      'enable_background_mode': False,

      # Disable support for resource allowlist generation. When enabled this
      # introduces a Windows official build dependency on the
      # "//chrome:chrome_dll" target, which will fail to build with CEF.
      'enable_resource_allowlist_generation': False,

      # Disable downgrade processing/restart with the Chrome runtime.
      # https://github.com/chromiumembedded/cef/issues/3608
      'enable_downgrade_processing': False,
  }

  if platform == 'windows' or platform == 'mac':
    # A browser specific value of at least 32 characters that will be used in
    # the computation of the CDM storage ID.
    result['alternate_cdm_storage_id_key'] = '968b476909da4373b08903c28e859454'

  if platform != 'windows':
    # Only allow non-component Debug builds on non-Windows platforms. These
    # builds will fail on Windows due to linker issues (running out of memory,
    # etc). See https://github.com/chromiumembedded/cef/issues/2679.
    result['forbid_non_component_debug_builds'] = False

  if platform == 'linux':
    # Use a sysroot environment. Default is true. False is recommended for local
    # builds.
    # Run the following commands to download the sysroot environment:
    # x86 build only:   $ export GYP_DEFINES='target_arch=ia32'
    # x86 or x64 build: $ gclient runhooks
    result['use_sysroot'] = False

    # Disable QT by default because we don't want to introduce the build
    # dependencies at this time. For background see
    # https://groups.google.com/a/chromium.org/g/chromium-packagers/c/-2VGexQAK6w/m/5K5ppK9WBAAJ
    result['use_qt5'] = False
    result['use_qt6'] = False

    # Set the blink TLS model to local-dynamic.
    # https://github.com/chromiumembedded/cef/issues/3803#issuecomment-2980423520
    result['blink_heap_inside_shared_library'] = True

  # This file may exist when building using a source tarball.
  tarball_args_file = os.path.join(src_dir, 'tarball_args.gn')
  if os.path.isfile(tarball_args_file):
    result.update(ParseArgsFile(tarball_args_file))

  return result


def GetGNEnvArgs():
  """
  Return GN args specified via the GN_DEFINES env variable.
  """
  return ParseNameValueList(ShlexEnv('GN_DEFINES'))


def GetRequiredArgs():
  """
  Return required GN args. Also enforced by assert() in //cef/BUILD.gn.
  """
  result = {
      'optimize_webui': True,
      # Enable support for Widevine CDM.
      'enable_widevine': True,

      # Don't use the chrome style plugin.
      'clang_use_chrome_plugins': False,
  }

  if platform == 'windows' or platform == 'mac':
    # Enable Widevine CDM host verification and storage ID.
    result['enable_cdm_host_verification'] = True
    result['enable_cdm_storage_id'] = True

    # Enable use of the RLZ library as required by CDM storage ID.
    result['enable_rlz'] = True

  if platform == 'linux':
    # Don't generate Chromium installer packages. This avoids GN dependency
    # errors with CEF (see issue #2301).
    # Due to the way this variable is declared in chrome/installer/BUILD.gn it
    # can't be enforced by assert().
    result['enable_linux_installer'] = False

  return result


def GetMergedArgs(build_args):
  """
  Return merged GN args.
  """
  dict = MergeDicts(GetRecommendedDefaultArgs(), GetGNEnvArgs(), build_args)

  # Verify that the user is not trying to override required args.
  required = GetRequiredArgs()
  for key in required.keys():
    if key in dict:
      assert dict[key] == required[key], \
          "%s=%s is required" % (key, FormatValue(required[key]))

  return MergeDicts(dict, required)


def ValidateArgs(args, is_debug):
  """
  Validate GN arg combinations that we know about. Also provide suggestions
  where appropriate.
  """
  dcheck_always_on = GetArgValue(args, 'dcheck_always_on')
  is_asan = GetArgValue(args, 'is_asan')
  is_debug = GetArgValue(args, 'is_debug')
  is_official_build = GetArgValue(args, 'is_official_build')
  target_cpu = GetArgValue(args, 'target_cpu')

  if platform == 'linux':
    use_sysroot = GetArgValue(args, 'use_sysroot')

  if platform == 'windows':
    is_win_fastlink = GetArgValue(args, 'is_win_fastlink')
    visual_studio_path = GetArgValue(args, 'visual_studio_path')
    visual_studio_version = GetArgValue(args, 'visual_studio_version')
    visual_studio_runtime_dirs = GetArgValue(args, 'visual_studio_runtime_dirs')
    windows_sdk_path = GetArgValue(args, 'windows_sdk_path')
    windows_sdk_version = GetArgValue(args, 'windows_sdk_version')

  # Target CPU architecture.
  # - Windows supports "x86", "x64" and "arm64".
  # - Mac supports "x64" and "arm64".
  # - Linux supports only "x64" unless using a sysroot environment.
  if platform == 'mac':
    assert target_cpu in ('x64', 'arm64'), 'target_cpu must be "x64" or "arm64"'
  elif platform == 'windows':
    assert target_cpu in ('x86', 'x64',
                          'arm64'), 'target_cpu must be "x86", "x64" or "arm64"'
  elif platform == 'linux':
    assert target_cpu in (
        'x86', 'x64', 'arm',
        'arm64'), 'target_cpu must be "x86", "x64", "arm" or "arm64"'

  if platform == 'linux':
    if target_cpu == 'x86':
      assert use_sysroot, 'target_cpu="x86" requires use_sysroot=true'
    elif target_cpu == 'arm':
      assert use_sysroot, 'target_cpu="arm" requires use_sysroot=true'
    elif target_cpu == 'arm64':
      assert use_sysroot, 'target_cpu="arm64" requires use_sysroot=true'

  # ASAN requires Release builds.
  if is_asan:
    assert not is_debug, "is_asan=true requires is_debug=false"
    if not dcheck_always_on:
      msg('is_asan=true recommends dcheck_always_on=true')

  # Official build requires Release builds.
  if is_official_build:
    assert not is_debug, "is_official_build=true requires is_debug=false"

  if platform == 'windows':
    # Official builds should not use /DEBUG:FASTLINK.
    if is_official_build:
      assert not is_win_fastlink, "is_official_build=true precludes is_win_fastlink=true"

    # Windows custom toolchain requirements.
    #
    # Required GN arguments:
    #   visual_studio_path="<path to VS root>"
    #     The directory that contains Visual Studio. For example, a subset of
    #     "C:\Program Files (x86)\Microsoft Visual Studio 14.0".
    #   visual_studio_version="<VS version>"
    #     The VS version. For example, "2015".
    #   visual_studio_runtime_dirs="<path to VS CRT>"
    #     The directory that contains the VS CRT. For example, the contents of
    #     "C:\Program Files (x86)\Windows Kits\10\Redist\ucrt\DLLs\x64" plus
    #     "C:\Windows\System32\ucrtbased.dll"
    #   windows_sdk_path="<path to WinSDK>"
    #     The directory that contains the Win SDK. For example, a subset of
    #     "C:\Program Files (x86)\Windows Kits\10".
    #   windows_sdk_version="<WinSDK version>"
    #     The WinSDK version. For example, "10.0.22621.0".
    #
    # Required environment variables:
    #   DEPOT_TOOLS_WIN_TOOLCHAIN=0
    #   GYP_MSVS_OVERRIDE_PATH=<path to VS root, must match visual_studio_path>
    #   GYP_MSVS_VERSION=<VS version, must match visual_studio_version>
    #   CEF_VCVARS=none
    #
    # Optional environment variables (required if vcvarsall.bat does not exist):
    #   INCLUDE=<VS include paths>
    #   LIB=<VS library paths>
    #   PATH=<VS executable paths>
    #
    # See comments in gclient_hook.py for environment variable usage.
    #
    if visual_studio_path != '':
      assert visual_studio_version != '', 'visual_studio_path requires visual_studio_version'
      assert visual_studio_runtime_dirs != '', 'visual_studio_path requires visual_studio_runtime_dirs'
      assert windows_sdk_path != '', 'visual_studio_path requires windows_sdk_path'
      assert windows_sdk_version != '', 'visual_studio_path requires windows_sdk_version'

      assert os.environ.get('DEPOT_TOOLS_WIN_TOOLCHAIN', '') == '0', \
        "visual_studio_path requires DEPOT_TOOLS_WIN_TOOLCHAIN=0 env variable"

      msvs_path = os.environ.get('GYP_MSVS_OVERRIDE_PATH', '')
      assert msvs_path == visual_studio_path and os.path.exists(msvs_path), \
        "visual_studio_path requires matching GYP_MSVS_OVERRIDE_PATH env variable"

      msvs_version = os.environ.get('GYP_MSVS_VERSION', '')
      assert msvs_version == visual_studio_version, \
        "visual_studio_version requires matching GYP_MSVS_VERSION env variable"

      assert os.environ.get('CEF_VCVARS', '') == 'none', \
        "visual_studio_path requires CEF_VCVARS=none env variable"

      # If vcvarsall.bat exists then environment variables will be derived from
      # there and any specified INCLUDE/LIB values will be ignored by Chromium
      # (PATH is retained because it might contain required VS runtime
      # libraries). If this file does not exist then the INCLUDE/LIB/PATH values
      # are also required by Chromium.
      vcvars_path = os.path.join(msvs_path, 'VC', 'vcvarsall.bat')
      if not os.path.exists(vcvars_path):
        vcvars_path = os.path.join(msvs_path, 'VC', 'Auxiliary', 'Build',
                                   'vcvarsall.bat')
      if os.path.exists(vcvars_path):
        if 'INCLUDE' in os.environ:
          del os.environ['INCLUDE']
        if 'LIB' in os.environ:
          del os.environ['LIB']
        if 'LIBPATH' in os.environ:
          del os.environ['LIBPATH']
      else:
        assert 'INCLUDE' in os.environ \
          and 'LIB' in os.environ \
          and 'PATH' in os.environ, \
          "visual_studio_path requires INCLUDE, LIB and PATH env variables"


def GetConfigArgs(args, is_debug, cpu):
  """
  Return merged GN args for the configuration and validate.
  """
  add_args = {}

  if GetArgValue(args, 'is_official_build'):
    # Disable Chromium field trials in official builds.
    add_args['disable_fieldtrial_testing_config'] = True

    # Cannot create is_official_build=true is_debug=true builds.
    # This restriction is enforced in //build/config/BUILDCONFIG.gn.
    # Instead, our "official Debug" build is a Release build with dchecks and
    # symbols. Symbols will be generated by default for official builds; see the
    # definition of 'symbol_level' in //build/config/compiler/compiler.gni.
    if is_debug:
      is_debug = False
      add_args['dcheck_always_on'] = True

    if platform == 'linux':
      # Use PartitionAlloc-Everywhere (PA-E) instead of the default system
      # allocator. Default is True with the allocator shim enabled. This is
      # disabled for improved client app compatibility (see issues #3061 and
      # #3095).
      #
      # Note that PartitionAlloc is still available in the binary and may be
      # used explicitly in some cases. It would be better to instead build with
      # |use_allocator_shim=false use_partition_alloc=false| to completely
      # disable the allocator shim and PartitionAlloc, however that currently
      # causes build errors in Chromium (see issue #3095).
      add_args['use_partition_alloc_as_malloc'] = False

      # BackupRefPtr support requires PA-E, so disable it.
      add_args['enable_backup_ref_ptr_support'] = False
  else:
    is_asan = GetArgValue(args, 'is_asan')

    # Enable additional BackupRefPtr debug features in non-Official build
    # configurations that support them. See //main/base/memory/raw_ptr.md
    if (is_debug or platform == 'windows' or is_asan) and \
        not GetArgValue(args, 'is_component_build', is_debug):
      if is_asan:
        # Enable additional security checks for ASAN. Note that ASAN does not
        # support PartitionAlloc-Everywhere (PA-E) and consequently provides
        # slightly different functionality. This arg is equivalent to passing
        # `--enable-features=PartitionAllocBackupRefPtr` on the command-line.
        add_args['enable_backup_ref_ptr_feature_flag'] = True
      else:
        # Use a global table to track all live raw_ptr/raw_ref instances to help
        # debug dangling pointers. This requires PA-E.
        add_args['enable_backup_ref_ptr_instance_tracer'] = True

  result = MergeDicts(args, add_args, {
      'is_debug': is_debug,
      'target_cpu': cpu,
  })

  if platform == 'linux' and not cpu.startswith('arm'):
    # Remove any arm-related values from non-arm configs.
    for key in result.keys():
      if key.startswith('arm_'):
        del result[key]

  ValidateArgs(result, is_debug)
  return result


def LinuxSysrootExists(cpu):
  """
  Returns true if the sysroot for the specified |cpu| architecture exists.
  """
  # Directory that contains sysroots.
  sysroot_root = os.path.join(src_dir, 'build', 'linux')
  # CPU-specific sysroot directory names.
  # Should match the values in build/config/sysroot.gni.
  release = 'bullseye'
  if cpu == 'x64':
    sysroot_name = 'debian_%s_amd64-sysroot' % release
  elif cpu == 'arm':
    sysroot_name = 'debian_%s_armhf-sysroot' % release
  elif cpu == 'arm64':
    sysroot_name = 'debian_%s_arm64-sysroot' % release
  else:
    raise Exception('Unrecognized sysroot CPU: %s' % cpu)

  return os.path.isdir(os.path.join(sysroot_root, sysroot_name))


def GetAllPlatformConfigs(build_args, quiet=False):
  """
  Return a map of directory name to GN args for the current platform.
  """
  result = {}

  if quiet:
    global _QUIET
    _QUIET = True

  # Merged args without validation.
  args = GetMergedArgs(build_args)

  create_debug = True

  # Don't create debug directories for asan builds.
  if GetArgValue(args, 'is_asan'):
    create_debug = False
    msg('Not generating Debug configuration due to is_asan=true')

  supported_cpus = []

  if platform == 'linux':
    use_sysroot = GetArgValue(args, 'use_sysroot')
    if use_sysroot:
      # Only generate configurations for sysroots that have been installed.
      for cpu in ('x64', 'arm', 'arm64'):
        if LinuxSysrootExists(cpu):
          supported_cpus.append(cpu)
        else:
          msg('Not generating %s configuration due to missing sysroot directory'
              % cpu)
    else:
      supported_cpus = ['x64']
  elif platform in ('windows', 'mac'):
    if machine == 'amd64' or os.environ.get('CEF_ENABLE_AMD64', '') == '1':
      supported_cpus.append('x64')
      if platform == 'windows':
        supported_cpus.append('x86')
    if machine == 'arm64' or os.environ.get('CEF_ENABLE_ARM64', '') == '1':
      supported_cpus.append('arm64')
  else:
    raise Exception('Unsupported platform')

  if len(supported_cpus) == 0:
    raise Exception('No supported architectures')

  for cpu in supported_cpus:
    if create_debug:
      result['Debug_GN_' + cpu] = GetConfigArgs(args, True, cpu)
    result['Release_GN_' + cpu] = GetConfigArgs(args, False, cpu)

  out_configs = os.environ.get('GN_OUT_CONFIGS', None)
  if not out_configs is None:
    # Only generate the specified configurations.
    out_configs = [c.strip() for c in out_configs.split(',')]
    for key in list(result.keys()):
      if not key in out_configs:
        msg('Not generating %s configuration due to GN_OUT_CONFIGS' % key)
        del result[key]
    if not bool(result):
      raise Exception('No supported configurations in GN_OUT_CONFIGS ("%s")' %
                      ','.join(out_configs))

  return result


def GetConfigFileContents(args):
  """
  Generate config file contents for the arguments.
  """
  pairs = []
  for k in sorted(args.keys()):
    pairs.append("%s=%s" % (k, FormatValue(args[k])))
  return "\n".join(pairs)


# Program entry point.
if __name__ == '__main__':
  import sys

  # Allow override of the platform via the command-line for testing.
  if len(sys.argv) > 1:
    platform = sys.argv[1]
    if not platform in ('linux', 'mac', 'windows'):
      sys.stderr.write('Usage: %s <platform>\n' % sys.argv[0])
      sys.exit()

  print('Platform: %s' % platform)

  # Dump the configuration based on platform and environment.
  configs = GetAllPlatformConfigs({})
  for dir, config in configs.items():
    print('\n\nout/%s:\n' % dir)
    print(GetConfigFileContents(config))
