# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from __future__ import print_function
from cef_version import VersionFormatter
from date_util import *
from exec_util import exec_cmd
from file_util import *
import git_util as git
from io import open
from make_cmake import process_cmake_template
from optparse import OptionParser
import os
import re
import shlex
import subprocess
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
  # The default tar format changed from GNU_FORMAT to PAX_FORMAT in Python 3.8.
  # However, PAX_FORMAT generates additional @PaxHeader entries and truncates file
  # names on Windows, so we'll stick with the previous default.
  tf.format = tarfile.GNU_FORMAT
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
  run('"%s" a -t%s -y %s %s' % (command, format, zip_file, zip_input),
      working_dir)

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
  elif platform == 'mac':
    platform_cmp = 'mac'
  elif platform == 'linux':
    platform_cmp = 'linux'
  paths.append(os.path.join(script_dir, 'distrib', platform_cmp))

  # shared directory
  paths.append(os.path.join(script_dir, 'distrib'))

  # load the file if it exists
  for path in paths:
    file = os.path.join(path, 'README.' + name + '.txt')
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
  data = header_data + '\n\n' + mode_data
  if mode != 'sandbox':
    data += '\n\n' + redistrib_data
  data += '\n\n' + footer_data
  data = data.replace('$CEF_URL$', cef_url)
  data = data.replace('$CEF_REV$', cef_rev)
  data = data.replace('$CEF_VER$', cef_ver)
  data = data.replace('$CHROMIUM_URL$', chromium_url)
  data = data.replace('$CHROMIUM_REV$', chromium_rev)
  data = data.replace('$CHROMIUM_VER$', chromium_ver)
  data = data.replace('$DATE$', date)

  if platform == 'windows':
    platform_str = 'Windows'
  elif platform == 'mac':
    platform_str = 'MacOS'
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
    distrib_desc = 'This distribution contains the minimal components necessary to build and\n' \
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
  elif mode == 'sandbox':
    distrib_type = 'Sandbox'
    distrib_desc = 'This distribution contains only the cef_sandbox static library. Please see\n' \
                   'the LICENSING section of this document for licensing terms and conditions.'

  data = data.replace('$DISTRIB_TYPE$', distrib_type)
  data = data.replace('$DISTRIB_DESC$', distrib_desc)

  write_file(os.path.join(output_dir, 'README.txt'), data)
  if not options.quiet:
    sys.stdout.write('Creating README.TXT file.\n')


def copy_gtest(tests_dir):
  """ Copy GTest files to the expected directory structure. """
  if not options.quiet:
    sys.stdout.write('Building gtest directory structure.\n')

  src_gtest_dir = os.path.join(cef_dir, 'tools', 'distrib', 'gtest')
  target_gtest_dir = os.path.join(tests_dir, 'gtest')

  # gtest header file at tests/gtest/include/gtest/gtest.h
  target_gtest_header_dir = os.path.join(target_gtest_dir, 'include', 'gtest')
  make_dir(target_gtest_header_dir, options.quiet)
  copy_file(
      os.path.join(src_gtest_dir, 'gtest.h'), target_gtest_header_dir,
      options.quiet)

  # gtest source file at tests/gtest/src/gtest-all.cc
  target_gtest_cpp_dir = os.path.join(target_gtest_dir, 'src')
  make_dir(target_gtest_cpp_dir, options.quiet)
  copy_file(
      os.path.join(src_gtest_dir, 'gtest-all.cc'), target_gtest_cpp_dir,
      options.quiet)

  # gtest LICENSE file at tests/gtest/LICENSE
  copy_file(
      os.path.join(src_gtest_dir, 'LICENSE'), target_gtest_dir, options.quiet)

  # CEF README file at tests/gtest/README.cef
  copy_file(
      os.path.join(src_gtest_dir, 'README.cef'),
      os.path.join(target_gtest_dir, 'README.cef'), options.quiet)

  # Copy tests/gtest/teamcity files
  copy_dir(
      os.path.join(cef_dir, 'tests', 'gtest', 'teamcity'),
      os.path.join(target_gtest_dir, 'teamcity'), options.quiet)


def transfer_doxyfile(dst_dir, quiet):
  """ Transfer and post-process the Doxyfile. """
  src_file = os.path.join(cef_dir, 'Doxyfile')
  if os.path.isfile(src_file):
    data = read_file(src_file)
    data = data.replace("$(PROJECT_NUMBER)", cef_ver)
    write_file(os.path.join(dst_dir, 'Doxyfile'), data)
    if not quiet:
      sys.stdout.write('Creating Doxyfile file.\n')


def transfer_gypi_files(src_dir, gypi_paths, gypi_path_prefix, dst_dir, quiet):
  """ Transfer files from one location to another. """
  for path in gypi_paths:
    src = os.path.join(src_dir, path)
    dst = os.path.join(dst_dir, path.replace(gypi_path_prefix, ''))
    dst_path = os.path.dirname(dst)
    make_dir(dst_path, quiet)
    copy_file(src, dst, quiet)


def normalize_headers(file, new_path=''):
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
        copy_file(
            os.path.join(script_dir, 'distrib/README-TRANSFER.txt'), readme)

      str = cfg['source'] + "\n"
      with open(readme, 'a', encoding='utf-8') as fp:
        if sys.version_info.major == 2:
          fp.write(str.decode('utf-8'))
        else:
          fp.write(str)

    # perform any required post-processing
    if 'post-process' in cfg:
      post = cfg['post-process']
      if post == 'normalize_headers':
        new_path = ''
        if 'new_header_path' in cfg:
          new_path = cfg['new_header_path']
        normalize_headers(dst, new_path)


def transfer_files(cef_dir, script_dir, transfer_cfg_dir, mode, output_dir,
                   quiet):
  # Non-mode-specific transfers.
  transfer_cfg = os.path.join(transfer_cfg_dir, 'transfer.cfg')
  eval_transfer_file(cef_dir, script_dir, transfer_cfg, output_dir, quiet)
  # Mode-specific transfers.
  transfer_cfg = os.path.join(transfer_cfg_dir, 'transfer_%s.cfg' % mode)
  eval_transfer_file(cef_dir, script_dir, transfer_cfg, output_dir, quiet)


# |paths| is a list of dictionary values with the following keys:
# path        [required]  Input file or directory path relative to |build_dir|.
#                         By default this will also be the output path relative
#                         to |dst_dir|.
# out_path    [optional]  Override the output path relative to |dst_dir|.
# conditional [optional]  Set to True if the path is conditional on build
#                         settings. Missing conditional paths will not be
#                         treated as an error.
# delete      [optional]  Glob pattern of files to delete after the copy.
def copy_files_list(build_dir, dst_dir, paths):
  ''' Copy the files listed in |paths| from |build_dir| to |dst_dir|. '''
  for entry in paths:
    source_path = os.path.join(build_dir, entry['path'])
    if os.path.exists(source_path):
      target_path = os.path.join(dst_dir, entry['out_path']
                                 if 'out_path' in entry else entry['path'])
      make_dir(os.path.dirname(target_path), options.quiet)
      if os.path.isdir(source_path):
        copy_dir(source_path, target_path, options.quiet)
        if 'delete' in entry:
          for delete_path in get_files(
              os.path.join(target_path, entry['delete'])):
            if not os.path.isdir(delete_path):
              remove_file(delete_path, options.quiet)
            else:
              raise Exception('Refusing to delete directory: %s' % delete_path)
      else:
        copy_file(source_path, target_path, options.quiet)
    else:
      if 'conditional' in entry and entry['conditional']:
        sys.stdout.write('Missing conditional path: %s.\n' % source_path)
      else:
        raise Exception('Missing required path: %s' % source_path)


def get_exported_symbols(file):
  """ Returns the global symbols exported by |file|. """
  symbols = []

  # Each symbol line has a value like:
  # 0000000000000000 T _cef_sandbox_initialize
  cmdline = 'nm -g -U %s' % file
  result = exec_cmd(cmdline, os.path.join(cef_dir, 'tools'))
  if len(result['err']) > 0:
    raise Exception('ERROR: nm failed: %s' % result['err'])
  for line in result['out'].split('\n'):
    if line.find(' T ') < 0:
      continue
    symbol = line[line.rfind(' ') + 1:]
    symbols.append(symbol)

  return symbols


def get_undefined_symbols(file):
  """ Returns the undefined symbols imported by |file|. """
  symbols = []

  # Each symbol line has a value like:
  # cef_sandbox.a:cef_sandbox.o: _memcpy
  cmdline = 'nm -u -A %s' % file
  result = exec_cmd(cmdline, os.path.join(cef_dir, 'tools'))
  if len(result['err']) > 0:
    raise Exception('ERROR: nm failed: %s' % result['err'])
  for line in result['out'].split('\n'):
    if line.find(': ') < 0:
      continue
    symbol = line[line.rfind(': ') + 2:]
    symbols.append(symbol)

  return symbols


def combine_libs(platform, build_dir, libs, dest_lib):
  """ Combine multiple static libraries into a single static library. """
  intermediate_obj = None
  if platform == 'windows':
    cmdline = 'msvs_env.bat win%s "%s" combine_libs.py -b "%s" -o "%s"' % (
        platform_arch, sys.executable, build_dir, dest_lib)
  elif platform == 'mac':
    # Find CEF_EXPORT symbols from libcef_sandbox.a (include/cef_sandbox_mac.h)
    # Export only symbols that include these strings.
    symbol_match = [
        '_cef_',  # C symbols
        'Cef',  # C++ symbols
    ]

    print('Finding exported symbols...')
    assert 'libcef_sandbox.a' in libs[0], libs[0]
    symbols = []
    for symbol in get_exported_symbols(os.path.join(build_dir, libs[0])):
      for match in symbol_match:
        if symbol.find(match) >= 0:
          symbols.append(symbol)
          break
    assert len(symbols) > 0

    # Create an intermediate object file that combines all other object files.
    # Symbols not identified above will be made private (local).
    intermediate_obj = os.path.splitext(dest_lib)[0] + '.o'
    arch = 'arm64' if options.arm64build else 'x86_64'
    cmdline = 'ld -arch %s -r -o "%s"' % (arch, intermediate_obj)
    for symbol in symbols:
      cmdline += ' -exported_symbol %s' % symbol

  for lib in libs:
    lib_path = os.path.join(build_dir, lib)
    for path in get_files(lib_path):  # Expand wildcards in |lib_path|.
      if not path_exists(path):
        raise Exception('File not found: ' + path)
      if platform == 'windows':
        path = os.path.relpath(path, build_dir)
      cmdline += ' "%s"' % path
  run(cmdline, os.path.join(cef_dir, 'tools'))

  if not intermediate_obj is None:
    # Create an archive file containing the new object file.
    cmdline = 'libtool -static -o "%s" "%s"' % (dest_lib, intermediate_obj)
    run(cmdline, os.path.join(cef_dir, 'tools'))
    remove_file(intermediate_obj)

    # Verify that only the expected symbols are exported from the archive file.
    print('Verifying exported symbols...')
    result_symbols = get_exported_symbols(dest_lib)
    if set(symbols) != set(result_symbols):
      print('Expected', symbols)
      print('Got', result_symbols)
      raise Exception('Failure verifying exported symbols')

    # Verify that no C++ symbols are imported by the archive file. If the
    # archive imports C++ symbols and the client app links an incompatible C++
    # library, the result will be undefined behavior.
    # For example, to avoid importing libc++ symbols the cef_sandbox target
    # should have a dependency on libc++abi. This dependency can be verified
    # with the following command:
    # gn path out/[config] //cef:cef_sandbox //buildtools/third_party/libc++abi
    print('Verifying imported (undefined) symbols...')
    undefined_symbols = get_undefined_symbols(dest_lib)
    cpp_symbols = list(
        filter(lambda symbol: symbol.startswith('__Z'), undefined_symbols))
    if cpp_symbols:
      print('Found C++ symbols:', cpp_symbols)
      raise Exception('Failure verifying imported (undefined) symbols')


def run(command_line, working_dir):
  """ Run a command. """
  sys.stdout.write('-------- Running "'+command_line+'" in "'+\
                   working_dir+'"...'+"\n")
  args = shlex.split(command_line.replace('\\', '\\\\'))
  return subprocess.check_call(
      args, cwd=working_dir, env=os.environ, shell=(sys.platform == 'win32'))


def print_error(msg):
  print('Error: %s\nSee --help for usage.' % msg)


# cannot be loaded as a module
if __name__ != "__main__":
  sys.stderr.write('This file cannot be loaded as a module!')
  sys.exit()

# parse command-line options
disc = """
This utility builds the CEF Binary Distribution.
"""

parser = OptionParser(description=disc)
parser.add_option(
    '--output-dir',
    dest='outputdir',
    metavar='DIR',
    help='output directory [required]')
parser.add_option(
    '--distrib-subdir',
    dest='distribsubdir',
    help='name of the subdirectory for the distribution',
    default='')
parser.add_option(
    '--distrib-subdir-suffix',
    dest='distribsubdirsuffix',
    help='suffix added to name of the subdirectory for the distribution',
    default='')
parser.add_option(
    '--allow-partial',
    action='store_true',
    dest='allowpartial',
    default=False,
    help='allow creation of partial distributions')
parser.add_option(
    '--no-symbols',
    action='store_true',
    dest='nosymbols',
    default=False,
    help='don\'t create symbol files')
parser.add_option(
    '--no-docs',
    action='store_true',
    dest='nodocs',
    default=False,
    help='don\'t create documentation')
parser.add_option(
    '--no-archive',
    action='store_true',
    dest='noarchive',
    default=False,
    help='don\'t create archives for output directories')
parser.add_option(
    '--ninja-build',
    action='store_true',
    dest='ninjabuild',
    default=False,
    help='build was created using ninja')
parser.add_option(
    '--x64-build',
    action='store_true',
    dest='x64build',
    default=False,
    help='create a 64-bit binary distribution')
parser.add_option(
    '--arm-build',
    action='store_true',
    dest='armbuild',
    default=False,
    help='create an ARM binary distribution (Linux only)')
parser.add_option(
    '--arm64-build',
    action='store_true',
    dest='arm64build',
    default=False,
    help='create an ARM64 binary distribution (Linux only)')
parser.add_option(
    '--minimal',
    action='store_true',
    dest='minimal',
    default=False,
    help='include only release build binary files')
parser.add_option(
    '--client',
    action='store_true',
    dest='client',
    default=False,
    help='include only the sample application')
parser.add_option(
    '--sandbox',
    action='store_true',
    dest='sandbox',
    default=False,
    help='include only the cef_sandbox static library (macOS and Windows only)')
parser.add_option(
    '--ozone',
    action='store_true',
    dest='ozone',
    default=False,
    help='include ozone build related files (Linux only)')
parser.add_option(
    '-q',
    '--quiet',
    action='store_true',
    dest='quiet',
    default=False,
    help='do not output detailed status information')
(options, args) = parser.parse_args()

# Test the operating system.
platform = ''
if sys.platform == 'win32':
  platform = 'windows'
elif sys.platform == 'darwin':
  platform = 'mac'
elif sys.platform.startswith('linux'):
  platform = 'linux'

# the outputdir option is required
if options.outputdir is None:
  print_error('--output-dir is required.')
  sys.exit()

if options.minimal and options.client:
  print_error('Cannot specify both --minimal and --client.')
  sys.exit()

if options.x64build + options.armbuild + options.arm64build > 1:
  print_error('Invalid combination of build options.')
  sys.exit()

if options.armbuild and platform != 'linux':
  print_error('--arm-build is only supported on Linux.')
  sys.exit()

if options.sandbox and not platform in ('mac', 'windows'):
  print_error('--sandbox is only supported on macOS and Windows.')
  sys.exit()

if not options.ninjabuild:
  print_error('--ninja-build is required.')
  sys.exit()

if options.ozone and platform != 'linux':
  print_error('--ozone is only supported on Linux.')
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

# format version strings
formatter = VersionFormatter()
cef_ver = formatter.get_version_string()
chromium_ver = formatter.get_chromium_version_string()

# list of output directories to be archived
archive_dirs = []

if options.x64build:
  platform_arch = '64'
  binary_arch = 'x64'
elif options.armbuild:
  platform_arch = 'arm'
  binary_arch = 'arm'
elif options.arm64build:
  platform_arch = 'arm64'
  binary_arch = 'arm64'
else:
  platform_arch = '32'
  binary_arch = 'x86'

# output directory
output_dir_base = 'cef_binary_' + cef_ver

if options.distribsubdir == '':
  if platform == 'mac':
    # For backwards compatibility keep the old default directory name on mac.
    platform_name = 'macos' + ('x' if platform_arch == '64' else '')
  else:
    platform_name = platform

  output_dir_name = output_dir_base + '_' + platform_name + platform_arch
  if options.distribsubdirsuffix != '':
    output_dir_name += '_' + options.distribsubdirsuffix
else:
  output_dir_name = options.distribsubdir

if options.minimal:
  mode = 'minimal'
  output_dir_name = output_dir_name + '_minimal'
elif options.client:
  mode = 'client'
  output_dir_name = output_dir_name + '_client'
elif options.sandbox:
  mode = 'sandbox'
  output_dir_name = output_dir_name + '_sandbox'
else:
  mode = 'standard'

if options.ozone:
  output_dir_name = output_dir_name + '_ozone'

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
elif options.arm64build:
  build_dir_suffix = '_GN_arm64'
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
  transfer_gypi_files(cef_dir, cef_paths2['includes_common_capi'], \
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
      'cef_command_ids.h',
      'cef_config.h',
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

  if mode == 'standard' or mode == 'minimal':
    # transfer additional files
    transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib'), \
                   mode, output_dir, options.quiet)

  # process cmake templates
  variables = cef_paths.copy()
  variables.update(cef_paths2)
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

  if not options.ozone:
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

  if not options.ozone:
    # transfer common cefclient files
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_browser'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_common'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_renderer'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_resources'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_resources_extensions_set_page_color'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)

  # transfer common cefsimple files
  transfer_gypi_files(cef_dir, cef_paths2['cefsimple_sources_common'], \
                      'tests/cefsimple/', cefsimple_dir, options.quiet)

  # transfer common ceftests files
  transfer_gypi_files(cef_dir, cef_paths2['ceftests_sources_common'], \
                      'tests/ceftests/', ceftests_dir, options.quiet)

  # copy GTest files
  copy_gtest(tests_dir)

  # process cmake templates
  if not options.ozone:
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

  # transfer Doxyfile
  transfer_doxyfile(output_dir, options.quiet)

  # transfer README.md
  copy_file(os.path.join(cef_dir, 'README.md'), \
            os.path.join(output_dir, 'README.md'), options.quiet)

if not options.nodocs:
  # generate doc files
  sys.stdout.write("Generating docs...\n")
  result = exec_cmd(
      os.path.join('tools', 'make_cppdocs.%s' %
                   ('bat' if platform == 'windows' else 'sh')), cef_dir)
  if (len(result['err']) > 0):
    sys.stdout.write(result['err'])
  sys.stdout.write(result['out'])

  src_dir = os.path.join(cef_dir, 'docs')
  if path_exists(src_dir):
    # create the docs output directory
    docs_output_dir = create_output_dir(output_dir_base + '_docs',
                                        options.outputdir)
    # transfer contents
    copy_dir(src_dir, docs_output_dir, options.quiet)
  else:
    sys.stdout.write("ERROR: No docs generated.\n")

if platform == 'windows':
  libcef_dll = 'libcef.dll'
  libcef_dll_lib = '%s.lib' % libcef_dll
  libcef_dll_pdb = '%s.pdb' % libcef_dll
  # yapf: disable
  binaries = [
      {'path': 'chrome_elf.dll'},
      {'path': 'd3dcompiler_47.dll'},
      {'path': 'dxcompiler.dll', 'conditional': True},
      {'path': 'dxil.dll', 'conditional': True},
      {'path': libcef_dll},
      {'path': 'libEGL.dll'},
      {'path': 'libGLESv2.dll'},
      {'path': 'snapshot_blob.bin', 'conditional': True},
      {'path': 'v8_context_snapshot.bin', 'conditional': True},
      {'path': 'vk_swiftshader.dll'},
      {'path': 'vk_swiftshader_icd.json'},
      {'path': 'vulkan-1.dll'},
  ]
  # yapf: enable

  if mode == 'client':
    binaries.append({
        'path': 'cefsimple.exe' if platform_arch == 'arm64' else 'cefclient.exe'
    })
  else:
    binaries.append({'path': libcef_dll_lib, 'out_path': 'libcef.lib'})

  # yapf: disable
  resources = [
      {'path': 'chrome_100_percent.pak'},
      {'path': 'chrome_200_percent.pak'},
      {'path': 'resources.pak'},
      {'path': 'icudtl.dat'},
      {'path': 'locales', 'delete': '*.info'},
  ]
  # yapf: enable

  cef_sandbox_lib = 'obj\\cef\\cef_sandbox.lib'
  sandbox_libs = [
      'obj\\base\\base.lib',
      'obj\\base\\base_static.lib',
      'obj\\base\\third_party\\double_conversion\\double_conversion.lib',
      'obj\\base\\third_party\\dynamic_annotations\\dynamic_annotations.lib',
      'obj\\base\\win\\pe_image.lib',
      cef_sandbox_lib,
      'obj\\sandbox\\common\\*.obj',
      'obj\\sandbox\\win\\sandbox.lib',
      'obj\\sandbox\\win\\service_resolver\\*.obj',
      'obj\\third_party\\abseil-cpp\\absl\\base\\**\\*.obj',
      'obj\\third_party\\abseil-cpp\\absl\\debugging\\**\\*.obj',
      'obj\\third_party\\abseil-cpp\\absl\\numeric\\**\\*.obj',
      'obj\\third_party\\abseil-cpp\\absl\\synchronization\\**\\*.obj',
      'obj\\third_party\\abseil-cpp\\absl\\time\\**\\*.obj',
      'obj\\third_party\\abseil-cpp\\absl\\types\\**\\*.obj',
  ]

  # Generate the cef_sandbox.lib merged library. A separate *_sandbox build
  # should exist when GN is_official_build=true.
  if mode in ('standard', 'minimal', 'sandbox'):
    dirs = {
        'Debug': (build_dir_debug + '_sandbox', build_dir_debug),
        'Release': (build_dir_release + '_sandbox', build_dir_release)
    }
    for dir_name in dirs.keys():
      for src_dir in dirs[dir_name]:
        if path_exists(os.path.join(src_dir, cef_sandbox_lib)):
          dst_dir = os.path.join(output_dir, dir_name)
          make_dir(dst_dir, options.quiet)
          combine_libs(platform, src_dir, sandbox_libs,
                       os.path.join(dst_dir, 'cef_sandbox.lib'))
          break

  valid_build_dir = None

  if mode == 'standard':
    # transfer Debug files
    build_dir = build_dir_debug
    if not options.allowpartial or path_exists(
        os.path.join(build_dir, libcef_dll)):
      valid_build_dir = build_dir
      dst_dir = os.path.join(output_dir, 'Debug')
      copy_files_list(build_dir, dst_dir, binaries)

      if not options.nosymbols:
        # create the symbol output directory
        symbol_output_dir = create_output_dir(
            output_dir_name + '_debug_symbols', options.outputdir)
        # transfer contents
        copy_file(
            os.path.join(build_dir, libcef_dll_pdb), symbol_output_dir,
            options.quiet)
    else:
      sys.stdout.write("No Debug build files.\n")

  if mode != 'sandbox':
    # transfer Release files
    build_dir = build_dir_release
    if not options.allowpartial or path_exists(
        os.path.join(build_dir, libcef_dll)):
      valid_build_dir = build_dir
      dst_dir = os.path.join(output_dir, 'Release')
      copy_files_list(build_dir, dst_dir, binaries)

      if not options.nosymbols:
        # create the symbol output directory
        symbol_output_dir = create_output_dir(
            output_dir_name + '_release_symbols', options.outputdir)
        # transfer contents
        copy_file(
            os.path.join(build_dir, libcef_dll_pdb), symbol_output_dir,
            options.quiet)
    else:
      sys.stdout.write("No Release build files.\n")

  if not valid_build_dir is None:
    # transfer resource files
    build_dir = valid_build_dir
    if mode == 'client':
      dst_dir = os.path.join(output_dir, 'Release')
    else:
      dst_dir = os.path.join(output_dir, 'Resources')
    copy_files_list(build_dir, dst_dir, resources)

  if mode == 'standard' or mode == 'minimal':
    # transfer include files
    transfer_gypi_files(cef_dir, cef_paths2['includes_win'], \
                        'include/', include_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['includes_win_capi'], \
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
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_resources_win'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)

    # transfer cefsimple files
    transfer_gypi_files(cef_dir, cef_paths2['cefsimple_sources_win'], \
                        'tests/cefsimple/', cefsimple_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['cefsimple_sources_resources_win'], \
                        'tests/cefsimple/', cefsimple_dir, options.quiet)

    # transfer ceftests files
    transfer_gypi_files(cef_dir, cef_paths2['ceftests_sources_win'], \
                        'tests/ceftests/', ceftests_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['ceftests_sources_resources_win'], \
                        'tests/ceftests/', ceftests_dir, options.quiet)

elif platform == 'mac':
  framework_name = 'Chromium Embedded Framework'
  framework_dsym = '%s.dSYM' % framework_name
  cefclient_app = 'cefclient.app'

  cef_sandbox_lib = 'obj/cef/libcef_sandbox.a'
  sandbox_libs = [
      cef_sandbox_lib,
      'obj/sandbox/mac/libseatbelt.a',
      'obj/sandbox/mac/libseatbelt_proto.a',
      'obj/third_party/protobuf/libprotobuf_lite.a',
      'obj/buildtools/third_party/libc++/libc++/*.o',
      'obj/buildtools/third_party/libc++abi/libc++abi/*.o',
  ]

  # Generate the cef_sandbox.a merged library. A separate *_sandbox build
  # should exist when GN is_official_build=true.
  if mode in ('standard', 'minimal', 'sandbox'):
    dirs = {
        'Debug': (build_dir_debug + '_sandbox', build_dir_debug),
        'Release': (build_dir_release + '_sandbox', build_dir_release)
    }
    for dir_name in dirs.keys():
      for src_dir in dirs[dir_name]:
        if path_exists(os.path.join(src_dir, cef_sandbox_lib)):
          dst_dir = os.path.join(output_dir, dir_name)
          make_dir(dst_dir, options.quiet)
          combine_libs(platform, src_dir, sandbox_libs,
                       os.path.join(dst_dir, 'cef_sandbox.a'))
          break

  valid_build_dir = None

  if mode == 'standard':
    # transfer Debug files
    build_dir = build_dir_debug
    if not options.allowpartial or path_exists(
        os.path.join(build_dir, cefclient_app)):
      valid_build_dir = build_dir
      dst_dir = os.path.join(output_dir, 'Debug')
      make_dir(dst_dir, options.quiet)
      framework_src_dir = os.path.join(
          build_dir, '%s/Contents/Frameworks/%s.framework/Versions/A' %
          (cefclient_app, framework_name))
      framework_dst_dir = os.path.join(dst_dir, '%s.framework' % framework_name)
      copy_dir(framework_src_dir, framework_dst_dir, options.quiet)

      if not options.nosymbols:
        # create the symbol output directory
        symbol_output_dir = create_output_dir(
            output_dir_name + '_debug_symbols', options.outputdir)

        # The real dSYM already exists, just copy it to the output directory.
        # dSYMs are only generated when is_official_build=true or enable_dsyms=true.
        # See //build/config/mac/symbols.gni.
        copy_dir(
            os.path.join(build_dir, framework_dsym),
            os.path.join(symbol_output_dir, framework_dsym), options.quiet)
    else:
      sys.stdout.write("No Debug build files.\n")

  if mode != 'sandbox':
    # transfer Release files
    build_dir = build_dir_release
    if not options.allowpartial or path_exists(
        os.path.join(build_dir, cefclient_app)):
      valid_build_dir = build_dir
      dst_dir = os.path.join(output_dir, 'Release')
      make_dir(dst_dir, options.quiet)
      framework_src_dir = os.path.join(
          build_dir, '%s/Contents/Frameworks/%s.framework/Versions/A' %
          (cefclient_app, framework_name))
      if mode != 'client':
        framework_dst_dir = os.path.join(dst_dir,
                                         '%s.framework' % framework_name)
      else:
        copy_dir(
            os.path.join(build_dir, cefclient_app),
            os.path.join(dst_dir, cefclient_app), options.quiet)
        # Replace the versioned framework with an unversioned framework in the sample app.
        framework_dst_dir = os.path.join(
            dst_dir, '%s/Contents/Frameworks/%s.framework' % (cefclient_app,
                                                              framework_name))
        remove_dir(framework_dst_dir, options.quiet)
      copy_dir(framework_src_dir, framework_dst_dir, options.quiet)

      if not options.nosymbols:
        # create the symbol output directory
        symbol_output_dir = create_output_dir(
            output_dir_name + '_release_symbols', options.outputdir)

        # The real dSYM already exists, just copy it to the output directory.
        # dSYMs are only generated when is_official_build=true or enable_dsyms=true.
        # See //build/config/mac/symbols.gni.
        copy_dir(
            os.path.join(build_dir, framework_dsym),
            os.path.join(symbol_output_dir, framework_dsym), options.quiet)
    else:
      sys.stdout.write("No Release build files.\n")

  if mode == 'standard' or mode == 'minimal':
    # transfer include files
    transfer_gypi_files(cef_dir, cef_paths2['includes_mac'], \
                        'include/', include_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['includes_mac_capi'], \
                        'include/', include_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['includes_wrapper_mac'], \
                        'include/', include_dir, options.quiet)

    # transfer libcef_dll_wrapper files
    transfer_gypi_files(cef_dir, cef_paths2['libcef_dll_wrapper_sources_mac'], \
                      'libcef_dll/', libcef_dll_dir, options.quiet)

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
  libcef_so = 'libcef.so'
  # yapf: disable
  binaries = [
      {'path': 'chrome_sandbox', 'out_path': 'chrome-sandbox'},
      {'path': libcef_so},
      {'path': 'libEGL.so'},
      {'path': 'libGLESv2.so'},
      {'path': 'libvk_swiftshader.so'},
      {'path': 'libvulkan.so.1'},
      {'path': 'snapshot_blob.bin', 'conditional': True},
      {'path': 'v8_context_snapshot.bin', 'conditional': True},
      {'path': 'vk_swiftshader_icd.json'},
  ]
  # yapf: enable
  if options.ozone:
    binaries.append({'path': 'libminigbm.so', 'conditional': True})

  if mode == 'client':
    binaries.append({'path': 'cefsimple'})

  # yapf: disable
  resources = [
      {'path': 'chrome_100_percent.pak'},
      {'path': 'chrome_200_percent.pak'},
      {'path': 'resources.pak'},
      {'path': 'icudtl.dat'},
      {'path': 'locales', 'delete': '*.info'},
  ]
  # yapf: enable

  valid_build_dir = None

  if mode == 'standard':
    # transfer Debug files
    build_dir = build_dir_debug
    libcef_path = os.path.join(build_dir, libcef_so)
    if not options.allowpartial or path_exists(libcef_path):
      valid_build_dir = build_dir
      dst_dir = os.path.join(output_dir, 'Debug')
      copy_files_list(build_dir, dst_dir, binaries)
    else:
      sys.stdout.write("No Debug build files.\n")

  # transfer Release files
  build_dir = build_dir_release
  libcef_path = os.path.join(build_dir, libcef_so)
  if not options.allowpartial or path_exists(libcef_path):
    valid_build_dir = build_dir
    dst_dir = os.path.join(output_dir, 'Release')
    copy_files_list(build_dir, dst_dir, binaries)
  else:
    sys.stdout.write("No Release build files.\n")

  if not valid_build_dir is None:
    # transfer resource files
    build_dir = valid_build_dir
    if mode == 'client':
      dst_dir = os.path.join(output_dir, 'Release')
    else:
      dst_dir = os.path.join(output_dir, 'Resources')
    copy_files_list(build_dir, dst_dir, resources)

  if mode == 'standard' or mode == 'minimal':
    # transfer include files
    transfer_gypi_files(cef_dir, cef_paths2['includes_linux'], \
                        'include/', include_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['includes_linux_capi'], \
                        'include/', include_dir, options.quiet)

    # transfer additional files, if any
    transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib', 'linux'), \
                   mode, output_dir, options.quiet)

  if mode == 'standard':
    # transfer shared files
    transfer_gypi_files(cef_dir, cef_paths2['shared_sources_linux'], \
                        'tests/shared/', shared_dir, options.quiet)

    if not options.ozone:
      # transfer cefclient files
      transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_linux'], \
                          'tests/cefclient/', cefclient_dir, options.quiet)

    # transfer cefsimple files
    transfer_gypi_files(cef_dir, cef_paths2['cefsimple_sources_linux'], \
                        'tests/cefsimple/', cefsimple_dir, options.quiet)

    # transfer ceftests files
    transfer_gypi_files(cef_dir, cef_paths2['ceftests_sources_linux'], \
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
      sys.stdout.write("Creating %s archive for %s...\n" %
                       (archive_format, os.path.basename(dir)))
    if archive_format == 'zip':
      create_zip_archive(dir)
    elif archive_format == 'tar.gz':
      create_tar_archive(dir, 'gz')
    elif archive_format == 'tar.bz2':
      create_tar_archive(dir, 'bz2')
    else:
      create_7z_archive(dir, archive_format)
