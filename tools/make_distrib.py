# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from date_util import *
from file_util import *
from make_cmake import process_cmake_template
from optparse import OptionParser
import os
import re
import shlex
import subprocess
import git_util as git
import sys
import tarfile
import zipfile

def create_zip_archive(input_dir):
  """ Creates a zip archive of the specified input directory. """
  zip_file = input_dir + '.zip'
  zf = zipfile.ZipFile(zip_file, 'w', zipfile.ZIP_DEFLATED, True)
  def addDir(dir):
    for f in os.listdir(dir):
      full_path = os.path.join(dir, f)
      if os.path.isdir(full_path):
        addDir(full_path)
      else:
        zf.write(full_path, os.path.relpath(full_path, \
                 os.path.join(input_dir, os.pardir)))
  addDir(input_dir)
  zf.close()

def create_tar_archive(input_dir, format):
  """ Creates a tar archive of the specified input directory. """
  # Supported formats include "gz" and "bz2".
  tar_file = input_dir + '.tar.' + format
  tf = tarfile.open(tar_file, "w:" + format)
  tf.add(input_dir, arcname=os.path.basename(input_dir))
  tf.close()

def create_7z_archive(input_dir, format):
  """ Creates a 7z archive of the specified input directory. """
  # CEF_COMMAND_7ZIP might be "c:\Program Files (x86)\7Zip\7z.exe" or /usr/bin/7za
  # or simply 7z if the user knows that it's in the PATH var. Supported formats
  # depend on the 7za version -- check the 7-zip documentation for details.
  command = os.environ['CEF_COMMAND_7ZIP']
  working_dir = os.path.abspath(os.path.join(input_dir, os.pardir))

  tar_file = None
  if format in ('xz', 'gzip', 'bzip2'):
    # These formats only support one file per archive. Create a tar file first.
    tar_file = input_dir + '.tar'
    run('"%s" a -ttar -y %s %s' % (command, tar_file, input_dir), working_dir)
    zip_file = tar_file + '.' + format
    zip_input = tar_file
  else:
    zip_file = input_dir + '.' + format
    zip_input = input_dir

  # Create the compressed archive.
  run('"%s" a -t%s -y %s %s' % (command, format, zip_file, zip_input), working_dir)

  if not tar_file is None:
    remove_file(tar_file)

def create_output_dir(name, parent_dir):
  """ Creates an output directory and adds the path to the archive list. """
  output_dir = os.path.abspath(os.path.join(parent_dir, name))
  remove_dir(output_dir, options.quiet)
  make_dir(output_dir, options.quiet)
  archive_dirs.append(output_dir)
  return output_dir

def get_readme_component(name):
  """ Loads a README file component. """
  paths = []
  # platform directory
  if platform == 'windows':
    platform_cmp = 'win'
  elif platform == 'macosx':
    platform_cmp = 'mac'
  elif platform == 'linux':
    platform_cmp = 'linux'
  paths.append(os.path.join(script_dir, 'distrib', platform_cmp))

  # shared directory
  paths.append(os.path.join(script_dir, 'distrib'))

  # load the file if it exists
  for path in paths:
    file = os.path.join(path, 'README.' +name + '.txt')
    if path_exists(file):
      return read_file(file)

  raise Exception('Readme component not found: ' + name)

def create_readme():
  """ Creates the README.TXT file. """
  # gather the components
  header_data = get_readme_component('header')
  mode_data = get_readme_component(mode)
  redistrib_data = get_readme_component('redistrib')
  footer_data = get_readme_component('footer')

  # format the file
  data = header_data + '\n\n' + mode_data + '\n\n' + redistrib_data + '\n\n' + footer_data
  data = data.replace('$CEF_URL$', cef_url)
  data = data.replace('$CEF_REV$', cef_rev)
  data = data.replace('$CEF_VER$', cef_ver)
  data = data.replace('$CHROMIUM_URL$', chromium_url)
  data = data.replace('$CHROMIUM_REV$', chromium_rev)
  data = data.replace('$CHROMIUM_VER$', chromium_ver)
  data = data.replace('$DATE$', date)

  if platform == 'windows':
    platform_str = 'Windows'
  elif platform == 'macosx':
    platform_str = 'Mac OS-X'
  elif platform == 'linux':
    platform_str = 'Linux'

  data = data.replace('$PLATFORM$', platform_str)

  if mode == 'standard':
    distrib_type = 'Standard'
    distrib_desc = 'This distribution contains all components necessary to build and distribute an\n' \
                   'application using CEF on the ' + platform_str + ' platform. Please see the LICENSING\n' \
                   'section of this document for licensing terms and conditions.'
  elif mode == 'minimal':
    distrib_type = 'Minimal'
    distrib_desc = 'This distribution contains the minimial components necessary to build and\n' \
                   'distribute an application using CEF on the ' + platform_str + ' platform. Please see\n' \
                   'the LICENSING section of this document for licensing terms and conditions.'
  elif mode == 'client':
    distrib_type = 'Client'
    if platform == 'linux':
      client_app = 'cefsimple'
    else:
      client_app = 'cefclient'
    distrib_desc = 'This distribution contains a release build of the ' + client_app + ' sample application\n' \
                   'for the ' + platform_str + ' platform. Please see the LICENSING section of this document for\n' \
                   'licensing terms and conditions.'

  data = data.replace('$DISTRIB_TYPE$', distrib_type)
  data = data.replace('$DISTRIB_DESC$', distrib_desc)

  write_file(os.path.join(output_dir, 'README.txt'), data)
  if not options.quiet:
    sys.stdout.write('Creating README.TXT file.\n')

def create_fuzed_gtest(tests_dir):
  """ Generate a fuzed version of gtest and build the expected directory structure. """
  src_gtest_dir = os.path.join(src_dir, 'testing', 'gtest')
  run('%s fuse_gtest_files.py \"%s\"' % (sys.executable, tests_dir),
      os.path.join(src_gtest_dir, 'scripts'))

  if not options.quiet:
    sys.stdout.write('Building gtest directory structure.\n')

  target_gtest_dir = os.path.join(tests_dir, 'gtest')
  gtest_header = os.path.join(target_gtest_dir, 'gtest.h')
  gtest_cpp = os.path.join(target_gtest_dir, 'gtest-all.cc')

  if not os.path.exists(gtest_header):
    raise Exception('Generated file not found: %s' % gtest_header)
  if not os.path.exists(gtest_cpp):
    raise Exception('Generated file not found: %s' % gtest_cpp)

  # gtest header file at tests/gtest/include/gtest/gtest.h
  target_gtest_header_dir = os.path.join(target_gtest_dir, 'include', 'gtest')
  make_dir(target_gtest_header_dir, options.quiet)
  move_file(gtest_header, target_gtest_header_dir, options.quiet)

  # gtest source file at tests/gtest/src/gtest-all.cc
  target_gtest_cpp_dir = os.path.join(target_gtest_dir, 'src')
  make_dir(target_gtest_cpp_dir, options.quiet)
  move_file(gtest_cpp, target_gtest_cpp_dir, options.quiet)

  # gtest LICENSE file at tests/gtest/LICENSE
  copy_file(os.path.join(src_gtest_dir, 'LICENSE'), target_gtest_dir, options.quiet)

  # CEF README file at tests/gtest/README.cef
  copy_file(os.path.join(cef_dir, 'tests', 'gtest', 'README.cef.in'),
            os.path.join(target_gtest_dir, 'README.cef'), options.quiet)

def transfer_gypi_files(src_dir, gypi_paths, gypi_path_prefix, dst_dir, quiet):
  """ Transfer files from one location to another. """
  for path in gypi_paths:
    src = os.path.join(src_dir, path)
    dst = os.path.join(dst_dir, path.replace(gypi_path_prefix, ''))
    dst_path = os.path.dirname(dst)
    make_dir(dst_path, quiet)
    copy_file(src, dst, quiet)

def normalize_headers(file, new_path = ''):
  """ Normalize headers post-processing. Remove the path component from any
      project include directives. """
  data = read_file(file)
  data = re.sub(r'''#include \"(?!include\/)[a-zA-Z0-9_\/]+\/+([a-zA-Z0-9_\.]+)\"''', \
                "// Include path modified for CEF Binary Distribution.\n#include \""+new_path+"\\1\"", data)
  write_file(file, data)

def eval_transfer_file(cef_dir, script_dir, transfer_cfg, output_dir, quiet):
  """ Transfer files based on the specified configuration. """
  if not path_exists(transfer_cfg):
    return

  configs = eval_file(transfer_cfg)
  for cfg in configs:
    dst = os.path.join(output_dir, cfg['target'])

    # perform a copy if source is specified
    if not cfg['source'] is None:
      src = os.path.join(cef_dir, cfg['source'])
      dst_path = os.path.dirname(dst)
      make_dir(dst_path, quiet)
      copy_file(src, dst, quiet)

      # place a readme file in the destination directory
      readme = os.path.join(dst_path, 'README-TRANSFER.txt')
      if not path_exists(readme):
        copy_file(os.path.join(script_dir, 'distrib/README-TRANSFER.txt'), readme)
      open(readme, 'ab').write(cfg['source']+"\n")

    # perform any required post-processing
    if 'post-process' in cfg:
      post = cfg['post-process']
      if post == 'normalize_headers':
        new_path = ''
        if cfg.has_key('new_header_path'):
          new_path = cfg['new_header_path']
        normalize_headers(dst, new_path)

def transfer_files(cef_dir, script_dir, transfer_cfg_dir, mode, output_dir, quiet):
  # Non-mode-specific transfers.
  transfer_cfg = os.path.join(transfer_cfg_dir, 'transfer.cfg')
  eval_transfer_file(cef_dir, script_dir, transfer_cfg, output_dir, quiet)
  # Mode-specific transfers.
  transfer_cfg = os.path.join(transfer_cfg_dir, 'transfer_%s.cfg' % mode)
  eval_transfer_file(cef_dir, script_dir, transfer_cfg, output_dir, quiet)

def combine_libs(build_dir, libs, dest_lib):
  """ Combine multiple static libraries into a single static library. """
  cmdline = 'msvs_env.bat win%s python combine_libs.py -o "%s"' % (platform_arch, dest_lib)
  for lib in libs:
    lib_path = os.path.join(build_dir, lib)
    for path in get_files(lib_path):  # Expand wildcards in |lib_path|.
      if not path_exists(path):
        raise Exception('File not found: ' + path)
      cmdline = cmdline + ' "%s"' % path
  run(cmdline, os.path.join(cef_dir, 'tools'))

def run(command_line, working_dir):
  """ Run a command. """
  sys.stdout.write('-------- Running "'+command_line+'" in "'+\
                   working_dir+'"...'+"\n")
  args = shlex.split(command_line.replace('\\', '\\\\'))
  return subprocess.check_call(args, cwd=working_dir, env=os.environ,
                               shell=(sys.platform == 'win32'))

# cannot be loaded as a module
if __name__ != "__main__":
  sys.stderr.write('This file cannot be loaded as a module!')
  sys.exit()

# parse command-line options
disc = """
This utility builds the CEF Binary Distribution.
"""

parser = OptionParser(description=disc)
parser.add_option('--output-dir', dest='outputdir', metavar='DIR',
                  help='output directory [required]')
parser.add_option('--distrib-subdir', dest='distribsubdir',
                  help='name of the subdirectory for the distribution',
                  default='')
parser.add_option('--allow-partial',
                  action='store_true', dest='allowpartial', default=False,
                  help='allow creation of partial distributions')
parser.add_option('--no-symbols',
                  action='store_true', dest='nosymbols', default=False,
                  help='don\'t create symbol files')
parser.add_option('--no-docs',
                  action='store_true', dest='nodocs', default=False,
                  help='don\'t create documentation')
parser.add_option('--no-archive',
                  action='store_true', dest='noarchive', default=False,
                  help='don\'t create archives for output directories')
parser.add_option('--ninja-build',
                  action='store_true', dest='ninjabuild', default=False,
                  help='build was created using ninja')
parser.add_option('--x64-build',
                  action='store_true', dest='x64build', default=False,
                  help='create a 64-bit binary distribution')
parser.add_option('--arm-build',
                  action='store_true', dest='armbuild', default=False,
                  help='create an ARM binary distribution')
parser.add_option('--minimal',
                  action='store_true', dest='minimal', default=False,
                  help='include only release build binary files')
parser.add_option('--client',
                  action='store_true', dest='client', default=False,
                  help='include only the sample application')
parser.add_option('-q', '--quiet',
                  action='store_true', dest='quiet', default=False,
                  help='do not output detailed status information')
(options, args) = parser.parse_args()

# Test the operating system.
platform = '';
if sys.platform == 'win32':
  platform = 'windows'
elif sys.platform == 'darwin':
  platform = 'macosx'
elif sys.platform.startswith('linux'):
  platform = 'linux'

# the outputdir option is required
if options.outputdir is None:
  parser.print_help(sys.stderr)
  sys.exit()

if options.minimal and options.client:
  print 'Cannot specify both --minimal and --client'
  parser.print_help(sys.stderr)
  sys.exit()

if options.x64build and options.armbuild:
  print 'Cannot specify both --x64-build and --arm-build'
  parser.print_help(sys.stderr)
  sys.exit()

if options.armbuild and platform != 'linux':
  print '--arm-build is only supported on Linux.'
  sys.exit()

if not options.ninjabuild:
  print 'Ninja build is required on all platforms'
  sys.exit()

# script directory
script_dir = os.path.dirname(__file__)

# CEF root directory
cef_dir = os.path.abspath(os.path.join(script_dir, os.pardir))

# src directory
src_dir = os.path.abspath(os.path.join(cef_dir, os.pardir))

if not git.is_checkout(cef_dir):
  raise Exception('Not a valid checkout: %s' % (cef_dir))

# retrieve information for CEF
cef_url = git.get_url(cef_dir)
cef_rev = git.get_hash(cef_dir)
cef_commit_number = git.get_commit_number(cef_dir)

if not git.is_checkout(src_dir):
  raise Exception('Not a valid checkout: %s' % (src_dir))

# retrieve information for Chromium
chromium_url = git.get_url(src_dir)
chromium_rev = git.get_hash(src_dir)

date = get_date()

# Read and parse the version file (key=value pairs, one per line)
args = {}
read_version_file(os.path.join(cef_dir, 'VERSION'), args)
read_version_file(os.path.join(cef_dir, '../chrome/VERSION'), args)

cef_ver = '%s.%s.%s.g%s' % (args['CEF_MAJOR'], args['BUILD'], cef_commit_number, cef_rev[:7])
chromium_ver = args['MAJOR']+'.'+args['MINOR']+'.'+args['BUILD']+'.'+args['PATCH']

# list of output directories to be archived
archive_dirs = []

if options.x64build:
  platform_arch = '64'
elif options.armbuild:
  platform_arch = 'arm'
else:
  platform_arch = '32'

# output directory
output_dir_base = 'cef_binary_' + cef_ver

if options.distribsubdir == '':
  output_dir_name = output_dir_base + '_' + platform + platform_arch
else:
  output_dir_name = options.distribsubdir

if options.minimal:
  mode = 'minimal'
  output_dir_name = output_dir_name + '_minimal'
elif options.client:
  mode = 'client'
  output_dir_name = output_dir_name + '_client'
else:
  mode = 'standard'

output_dir = create_output_dir(output_dir_name, options.outputdir)

# create the README.TXT file
create_readme()

# transfer the LICENSE.txt file
copy_file(os.path.join(cef_dir, 'LICENSE.txt'), output_dir, options.quiet)

# read the variables list from the autogenerated cef_paths.gypi file
cef_paths = eval_file(os.path.join(cef_dir, 'cef_paths.gypi'))
cef_paths = cef_paths['variables']

# read the variables list from the manually edited cef_paths2.gypi file
cef_paths2 = eval_file(os.path.join(cef_dir, 'cef_paths2.gypi'))
cef_paths2 = cef_paths2['variables']

# Determine the build directory suffix. CEF uses a consistent directory naming
# scheme for GN via GetAllPlatformConfigs in gn_args.py.
if options.x64build:
  build_dir_suffix = '_GN_x64'
elif options.armbuild:
  build_dir_suffix = '_GN_arm'
else:
  build_dir_suffix = '_GN_x86'

# Determine the build directory paths.
out_dir = os.path.join(src_dir, 'out')
build_dir_debug = os.path.join(out_dir, 'Debug' + build_dir_suffix)
build_dir_release = os.path.join(out_dir, 'Release' + build_dir_suffix)

if mode == 'standard' or mode == 'minimal':
  # create the include directory
  include_dir = os.path.join(output_dir, 'include')
  make_dir(include_dir, options.quiet)

  # create the cmake directory
  cmake_dir = os.path.join(output_dir, 'cmake')
  make_dir(cmake_dir, options.quiet)

  # create the libcef_dll_wrapper directory
  libcef_dll_dir = os.path.join(output_dir, 'libcef_dll')
  make_dir(libcef_dll_dir, options.quiet)

  # transfer common include files
  transfer_gypi_files(cef_dir, cef_paths2['includes_common'], \
                      'include/', include_dir, options.quiet)
  transfer_gypi_files(cef_dir, cef_paths2['includes_capi'], \
                      'include/', include_dir, options.quiet)
  transfer_gypi_files(cef_dir, cef_paths2['includes_wrapper'], \
                      'include/', include_dir, options.quiet)
  transfer_gypi_files(cef_dir, cef_paths['autogen_cpp_includes'], \
                      'include/', include_dir, options.quiet)
  transfer_gypi_files(cef_dir, cef_paths['autogen_capi_includes'], \
                      'include/', include_dir, options.quiet)

  # Transfer generated include files.
  generated_includes = [
    'cef_pack_resources.h',
    'cef_pack_strings.h',
  ]
  for include in generated_includes:
    # Debug and Release build should be the same so grab whichever exists.
    src_path = os.path.join(build_dir_release, 'includes', 'include', include)
    if not os.path.exists(src_path):
      src_path = os.path.join(build_dir_debug, 'includes', 'include', include)
      if not os.path.exists(src_path):
        raise Exception('Missing generated header file: %s' % include)
    copy_file(src_path, os.path.join(include_dir, include), options.quiet)

  # transfer common libcef_dll_wrapper files
  transfer_gypi_files(cef_dir, cef_paths2['libcef_dll_wrapper_sources_base'], \
                      'libcef_dll/', libcef_dll_dir, options.quiet)
  transfer_gypi_files(cef_dir, cef_paths2['libcef_dll_wrapper_sources_common'], \
                      'libcef_dll/', libcef_dll_dir, options.quiet)
  transfer_gypi_files(cef_dir, cef_paths['autogen_client_side'], \
                      'libcef_dll/', libcef_dll_dir, options.quiet)

  # transfer additional files
  transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib'), \
                 mode, output_dir, options.quiet)

  # process cmake templates
  variables = dict(cef_paths.items() + cef_paths2.items())
  process_cmake_template(os.path.join(cef_dir, 'CMakeLists.txt.in'), \
                         os.path.join(output_dir, 'CMakeLists.txt'), \
                         variables, options.quiet)
  process_cmake_template(os.path.join(cef_dir, 'cmake', 'cef_macros.cmake.in'), \
                         os.path.join(cmake_dir, 'cef_macros.cmake'), \
                         variables, options.quiet)
  process_cmake_template(os.path.join(cef_dir, 'cmake', 'cef_variables.cmake.in'), \
                         os.path.join(cmake_dir, 'cef_variables.cmake'), \
                         variables, options.quiet)
  process_cmake_template(os.path.join(cef_dir, 'cmake', 'FindCEF.cmake.in'), \
                         os.path.join(cmake_dir, 'FindCEF.cmake'), \
                         variables, options.quiet)
  process_cmake_template(os.path.join(cef_dir, 'libcef_dll', 'CMakeLists.txt.in'), \
                         os.path.join(libcef_dll_dir, 'CMakeLists.txt'), \
                         variables, options.quiet)

if mode == 'standard':
  # create the tests directory
  tests_dir = os.path.join(output_dir, 'tests')
  make_dir(tests_dir, options.quiet)

  # create the tests/shared directory
  shared_dir = os.path.join(tests_dir, 'shared')
  make_dir(shared_dir, options.quiet)

  # create the tests/cefclient directory
  cefclient_dir = os.path.join(tests_dir, 'cefclient')
  make_dir(cefclient_dir, options.quiet)

  # create the tests/cefsimple directory
  cefsimple_dir = os.path.join(tests_dir, 'cefsimple')
  make_dir(cefsimple_dir, options.quiet)

  # create the tests/ceftests directory
  ceftests_dir = os.path.join(tests_dir, 'ceftests')
  make_dir(ceftests_dir, options.quiet)

  # transfer common shared files
  transfer_gypi_files(cef_dir, cef_paths2['shared_sources_browser'], \
                      'tests/shared/', shared_dir, options.quiet)
  transfer_gypi_files(cef_dir, cef_paths2['shared_sources_common'], \
                      'tests/shared/', shared_dir, options.quiet)
  transfer_gypi_files(cef_dir, cef_paths2['shared_sources_renderer'], \
                      'tests/shared/', shared_dir, options.quiet)
  transfer_gypi_files(cef_dir, cef_paths2['shared_sources_resources'], \
                      'tests/shared/', shared_dir, options.quiet)

  # transfer common cefclient files
  transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_browser'], \
                      'tests/cefclient/', cefclient_dir, options.quiet)
  transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_common'], \
                      'tests/cefclient/', cefclient_dir, options.quiet)
  transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_renderer'], \
                      'tests/cefclient/', cefclient_dir, options.quiet)
  transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_resources'], \
                      'tests/cefclient/', cefclient_dir, options.quiet)

  # transfer common cefsimple files
  transfer_gypi_files(cef_dir, cef_paths2['cefsimple_sources_common'], \
                      'tests/cefsimple/', cefsimple_dir, options.quiet)

  # transfer common ceftests files
  transfer_gypi_files(cef_dir, cef_paths2['ceftests_sources_common'], \
                      'tests/ceftests/', ceftests_dir, options.quiet)

  # create the fuzed gtest version
  create_fuzed_gtest(tests_dir)

  # process cmake templates
  process_cmake_template(os.path.join(cef_dir, 'tests', 'cefclient', 'CMakeLists.txt.in'), \
                         os.path.join(cefclient_dir, 'CMakeLists.txt'), \
                         variables, options.quiet)
  process_cmake_template(os.path.join(cef_dir, 'tests', 'cefsimple', 'CMakeLists.txt.in'), \
                         os.path.join(cefsimple_dir, 'CMakeLists.txt'), \
                         variables, options.quiet)
  process_cmake_template(os.path.join(cef_dir, 'tests', 'gtest', 'CMakeLists.txt.in'), \
                         os.path.join(tests_dir, 'gtest', 'CMakeLists.txt'), \
                         variables, options.quiet)
  process_cmake_template(os.path.join(cef_dir, 'tests', 'ceftests', 'CMakeLists.txt.in'), \
                         os.path.join(ceftests_dir, 'CMakeLists.txt'), \
                         variables, options.quiet)

  # transfer gypi files
  copy_file(os.path.join(cef_dir, 'cef_paths.gypi'), \
            os.path.join(output_dir, 'cef_paths.gypi'), options.quiet)
  copy_file(os.path.join(cef_dir, 'cef_paths2.gypi'), \
            os.path.join(output_dir, 'cef_paths2.gypi'), options.quiet)

if platform == 'windows':
  binaries = [
    'chrome_elf.dll',
    'd3dcompiler_47.dll',
    'libcef.dll',
    'libEGL.dll',
    'libGLESv2.dll',
    'natives_blob.bin',
    'snapshot_blob.bin',
    # Should match the output path from media/cdm/ppapi/cdm_paths.gni.
    'WidevineCdm\\_platform_specific\\win_%s\\widevinecdmadapter.dll' % \
      ('x64' if options.x64build else 'x86'),
  ]

  libcef_dll_file = 'libcef.dll.lib'
  sandbox_libs = [
    'obj\\base\\allocator\\unified_allocator_shim\\*.obj',
    'obj\\base\\base.lib',
    'obj\\base\\base_static.lib',
    'obj\\base\\third_party\\dynamic_annotations\\dynamic_annotations.lib',
    'obj\\cef\\cef_sandbox.lib',
    'obj\\sandbox\\win\\sandbox.lib',
  ]

  valid_build_dir = None

  if mode == 'standard':
    # transfer Debug files
    build_dir = build_dir_debug
    if not options.allowpartial or path_exists(os.path.join(build_dir, 'libcef.dll')):
      valid_build_dir = build_dir
      dst_dir = os.path.join(output_dir, 'Debug')
      make_dir(dst_dir, options.quiet)
      copy_files(os.path.join(script_dir, 'distrib/win/*.dll'), dst_dir, options.quiet)
      for binary in binaries:
        copy_file(os.path.join(build_dir, binary), os.path.join(dst_dir, os.path.basename(binary)), options.quiet)
      copy_file(os.path.join(build_dir, libcef_dll_file), os.path.join(dst_dir, 'libcef.lib'), \
                options.quiet)
      combine_libs(build_dir, sandbox_libs, os.path.join(dst_dir, 'cef_sandbox.lib'));

      if not options.nosymbols:
        # create the symbol output directory
        symbol_output_dir = create_output_dir(output_dir_name + '_debug_symbols', options.outputdir)
        # transfer contents
        copy_file(os.path.join(build_dir, 'libcef.dll.pdb'), symbol_output_dir, options.quiet)
    else:
      sys.stderr.write("No Debug build files.\n")

  # transfer Release files
  build_dir = build_dir_release
  if not options.allowpartial or path_exists(os.path.join(build_dir, 'libcef.dll')):
    valid_build_dir = build_dir
    dst_dir = os.path.join(output_dir, 'Release')
    make_dir(dst_dir, options.quiet)
    copy_files(os.path.join(script_dir, 'distrib/win/*.dll'), dst_dir, options.quiet)
    for binary in binaries:
      copy_file(os.path.join(build_dir, binary), os.path.join(dst_dir, os.path.basename(binary)), options.quiet)

    if mode != 'client':
      copy_file(os.path.join(build_dir, libcef_dll_file), os.path.join(dst_dir, 'libcef.lib'), \
          options.quiet)
      combine_libs(build_dir, sandbox_libs, os.path.join(dst_dir, 'cef_sandbox.lib'));
    else:
      copy_file(os.path.join(build_dir, 'cefclient.exe'), dst_dir, options.quiet)

    if not options.nosymbols:
      # create the symbol output directory
      symbol_output_dir = create_output_dir(output_dir_name + '_release_symbols', options.outputdir)
      # transfer contents
      copy_file(os.path.join(build_dir, 'libcef.dll.pdb'), symbol_output_dir, options.quiet)
  else:
    sys.stderr.write("No Release build files.\n")

  if not valid_build_dir is None:
    # transfer resource files
    build_dir = valid_build_dir
    if mode == 'client':
      dst_dir = os.path.join(output_dir, 'Release')
    else:
      dst_dir = os.path.join(output_dir, 'Resources')
    make_dir(dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'cef.pak'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'cef_100_percent.pak'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'cef_200_percent.pak'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'cef_extensions.pak'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'devtools_resources.pak'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'icudtl.dat'), dst_dir, options.quiet)
    copy_dir(os.path.join(build_dir, 'locales'), os.path.join(dst_dir, 'locales'), options.quiet)

  if mode == 'standard' or mode == 'minimal':
    # transfer include files
    transfer_gypi_files(cef_dir, cef_paths2['includes_win'], \
                        'include/', include_dir, options.quiet)

    # transfer additional files, if any
    transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib', 'win'), \
                   mode, output_dir, options.quiet)

  if mode == 'standard':
    # transfer shared files
    transfer_gypi_files(cef_dir, cef_paths2['shared_sources_win'], \
                        'tests/shared/', shared_dir, options.quiet)

    # transfer cefclient files
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_win'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)

    # transfer cefsimple files
    transfer_gypi_files(cef_dir, cef_paths2['cefsimple_sources_win'], \
                        'tests/cefsimple/', cefsimple_dir, options.quiet)

    # transfer ceftests files
    transfer_gypi_files(cef_dir, cef_paths2['ceftests_sources_win'], \
                        'tests/ceftests/', ceftests_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['ceftests_sources_views'], \
                        'tests/ceftests/', ceftests_dir, options.quiet)

  if not options.nodocs:
    # generate doc files
    os.popen('make_cppdocs.bat '+cef_rev)

    src_dir = os.path.join(cef_dir, 'docs')
    if path_exists(src_dir):
      # create the docs output directory
      docs_output_dir = create_output_dir(output_dir_base + '_docs', options.outputdir)
      # transfer contents
      copy_dir(src_dir, docs_output_dir, options.quiet)

elif platform == 'macosx':
  valid_build_dir = None
  framework_name = 'Chromium Embedded Framework'

  if mode == 'standard':
    # transfer Debug files
    build_dir = build_dir_debug
    if not options.allowpartial or path_exists(os.path.join(build_dir, 'cefclient.app')):
      valid_build_dir = build_dir
      dst_dir = os.path.join(output_dir, 'Debug')
      make_dir(dst_dir, options.quiet)
      copy_dir(os.path.join(build_dir, 'cefclient.app/Contents/Frameworks/%s.framework' % framework_name), \
               os.path.join(dst_dir, '%s.framework' % framework_name), options.quiet)
      copy_file(os.path.join(script_dir, 'distrib/mac/widevinecdmadapter.plugin'), dst_dir, options.quiet)

      if not options.nosymbols:
        # create the symbol output directory
        symbol_output_dir = create_output_dir(output_dir_name + '_debug_symbols', options.outputdir)

        # The real dSYM already exists, just copy it to the output directory.
        # dSYMs are only generated when is_official_build=true or enable_dsyms=true.
        # See //build/config/mac/symbols.gni.
        copy_dir(os.path.join(build_dir, '%s.dSYM' % framework_name),
                 os.path.join(symbol_output_dir, '%s.dSYM' % framework_name), options.quiet)

  # transfer Release files
  build_dir = build_dir_release
  if not options.allowpartial or path_exists(os.path.join(build_dir, 'cefclient.app')):
    valid_build_dir = build_dir
    dst_dir = os.path.join(output_dir, 'Release')
    make_dir(dst_dir, options.quiet)
    if mode != 'client':
      copy_dir(os.path.join(build_dir, 'cefclient.app/Contents/Frameworks/%s.framework' % framework_name), \
               os.path.join(dst_dir, '%s.framework' % framework_name), options.quiet)
      copy_file(os.path.join(script_dir, 'distrib/mac/widevinecdmadapter.plugin'), dst_dir, options.quiet)
    else:
      copy_dir(os.path.join(build_dir, 'cefclient.app'), os.path.join(dst_dir, 'cefclient.app'), options.quiet)

    if not options.nosymbols:
      # create the symbol output directory
      symbol_output_dir = create_output_dir(output_dir_name + '_release_symbols', options.outputdir)

      # The real dSYM already exists, just copy it to the output directory.
      # dSYMs are only generated when is_official_build=true or enable_dsyms=true.
      # See //build/config/mac/symbols.gni.
      copy_dir(os.path.join(build_dir, '%s.dSYM' % framework_name),
               os.path.join(symbol_output_dir, '%s.dSYM' % framework_name), options.quiet)

  if mode == 'standard' or mode == 'minimal':
    # transfer include files
    transfer_gypi_files(cef_dir, cef_paths2['includes_mac'], \
                        'include/', include_dir, options.quiet)

    # transfer additional files, if any
    transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib', 'mac'), \
                   mode, output_dir, options.quiet)

  if mode == 'standard':
    # transfer shared files
    transfer_gypi_files(cef_dir, cef_paths2['shared_sources_mac'], \
                        'tests/shared/', shared_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['shared_sources_mac_helper'], \
                        'tests/shared/', shared_dir, options.quiet)

    # transfer cefclient files
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_mac'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)

    # transfer cefclient/resources/mac files
    copy_dir(os.path.join(cef_dir, 'tests/cefclient/resources/mac'), \
             os.path.join(cefclient_dir, 'resources/mac'), \
             options.quiet)

    # transfer cefsimple files
    transfer_gypi_files(cef_dir, cef_paths2['cefsimple_sources_mac'], \
                        'tests/cefsimple/', cefsimple_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['cefsimple_sources_mac_helper'], \
                        'tests/cefsimple/', cefsimple_dir, options.quiet)

    # transfer cefsimple/mac files
    copy_dir(os.path.join(cef_dir, 'tests/cefsimple/mac'), \
             os.path.join(cefsimple_dir, 'mac'), \
             options.quiet)

    # transfer ceftests files
    transfer_gypi_files(cef_dir, cef_paths2['ceftests_sources_mac'], \
                        'tests/ceftests/', ceftests_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['ceftests_sources_mac_helper'], \
                        'tests/ceftests/', ceftests_dir, options.quiet)

    # transfer ceftests/resources/mac files
    copy_dir(os.path.join(cef_dir, 'tests/ceftests/resources/mac'), \
             os.path.join(ceftests_dir, 'resources/mac'), \
             options.quiet)

elif platform == 'linux':
  valid_build_dir = None

  if mode == 'standard':
    # transfer Debug files
    build_dir = build_dir_debug
    libcef_path = os.path.join(build_dir, 'libcef.so')
    if not options.allowpartial or path_exists(libcef_path):
      valid_build_dir = build_dir
      dst_dir = os.path.join(output_dir, 'Debug')
      make_dir(dst_dir, options.quiet)
      copy_file(os.path.join(build_dir, 'chrome_sandbox'), os.path.join(dst_dir, 'chrome-sandbox'), options.quiet)
      copy_file(libcef_path, dst_dir, options.quiet)
      copy_file(os.path.join(build_dir, 'libwidevinecdmadapter.so'), dst_dir, options.quiet)
      copy_file(os.path.join(build_dir, 'natives_blob.bin'), dst_dir, options.quiet)
      copy_file(os.path.join(build_dir, 'snapshot_blob.bin'), dst_dir, options.quiet)
    else:
      sys.stderr.write("No Debug build files.\n")

  # transfer Release files
  build_dir = build_dir_release
  libcef_path = os.path.join(build_dir, 'libcef.so')
  if not options.allowpartial or path_exists(libcef_path):
    valid_build_dir = build_dir
    dst_dir = os.path.join(output_dir, 'Release')
    make_dir(dst_dir, options.quiet)

    if mode == 'client':
      copy_file(os.path.join(build_dir, 'cefsimple'), dst_dir, options.quiet)
    copy_file(libcef_path, dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'chrome_sandbox'), os.path.join(dst_dir, 'chrome-sandbox'), options.quiet)
    copy_file(os.path.join(build_dir, 'libwidevinecdmadapter.so'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'natives_blob.bin'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'snapshot_blob.bin'), dst_dir, options.quiet)
  else:
    sys.stderr.write("No Release build files.\n")

  if not valid_build_dir is None:
    # transfer resource files
    build_dir = valid_build_dir
    if mode == 'client':
      dst_dir = os.path.join(output_dir, 'Release')
    else:
      dst_dir = os.path.join(output_dir, 'Resources')
    make_dir(dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'cef.pak'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'cef_100_percent.pak'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'cef_200_percent.pak'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'cef_extensions.pak'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'devtools_resources.pak'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'icudtl.dat'), dst_dir, options.quiet)
    copy_dir(os.path.join(build_dir, 'locales'), os.path.join(dst_dir, 'locales'), options.quiet)

  if mode == 'standard' or mode == 'minimal':
    # transfer include files
    transfer_gypi_files(cef_dir, cef_paths2['includes_linux'], \
                        'include/', include_dir, options.quiet)

    # transfer additional files, if any
    transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib', 'linux'), \
                   mode, output_dir, options.quiet)

  if mode == 'standard':
    # transfer shared files
    transfer_gypi_files(cef_dir, cef_paths2['shared_sources_linux'], \
                        'tests/shared/', shared_dir, options.quiet)

    # transfer cefclient files
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_linux'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)

    # transfer cefsimple files
    transfer_gypi_files(cef_dir, cef_paths2['cefsimple_sources_linux'], \
                        'tests/cefsimple/', cefsimple_dir, options.quiet)

    # transfer ceftests files
    transfer_gypi_files(cef_dir, cef_paths2['ceftests_sources_linux'], \
                        'tests/ceftests/', ceftests_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['ceftests_sources_views'], \
                        'tests/ceftests/', ceftests_dir, options.quiet)

if not options.noarchive:
  # create an archive for each output directory
  archive_format = os.getenv('CEF_ARCHIVE_FORMAT', 'zip')
  if archive_format not in ('zip', 'tar.gz', 'tar.bz2'):
    raise Exception('Unsupported archive format: %s' % archive_format)

  if os.getenv('CEF_COMMAND_7ZIP', '') != '':
    archive_format = os.getenv('CEF_COMMAND_7ZIP_FORMAT', '7z')

  for dir in archive_dirs:
    if not options.quiet:
      sys.stdout.write("Creating %s archive for %s...\n" % (archive_format, os.path.basename(dir)))
    if archive_format == 'zip':
      create_zip_archive(dir)
    elif archive_format == 'tar.gz':
      create_tar_archive(dir, 'gz')
    elif archive_format == 'tar.bz2':
      create_tar_archive(dir, 'bz2')
    else:
      create_7z_archive(dir, archive_format)
