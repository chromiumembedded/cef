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
import zipfile

def create_archive(input_dir, zip_file):
  """ Creates a zip archive of the specified input directory. """
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

def create_7z_archive(input_dir, zip_file):
  """ Creates a 7z archive of the specified input directory. """
  command = os.environ['CEF_COMMAND_7ZIP']
  run('"' + command + '" a -y ' + zip_file + ' ' + input_dir, os.path.split(zip_file)[0])

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
    distrib_desc = 'This distribution contains only the components required to distribute an\n' \
                   'application using CEF on the ' + platform_str + ' platform. Please see the LICENSING\n' \
                   'section of this document for licensing terms and conditions.'
  elif mode == 'client':
    distrib_type = 'Client'
    distrib_desc = 'This distribution contains a release build of the cefclient sample application\n' \
                   'for the ' + platform_str + ' platform. Please see the LICENSING section of this document for\n' \
                   'licensing terms and conditions.'

  data = data.replace('$DISTRIB_TYPE$', distrib_type)
  data = data.replace('$DISTRIB_DESC$', distrib_desc)

  write_file(os.path.join(output_dir, 'README.txt'), data)
  if not options.quiet:
    sys.stdout.write('Creating README.TXT file.\n')

def transfer_gypi_files(src_dir, gypi_paths, gypi_path_prefix, dst_dir, quiet):
  """ Transfer files from one location to another. """
  for path in gypi_paths:
    # skip gyp includes
    if path[:2] == '<@':
        continue
    # skip test files
    if path.find('/test/') >= 0:
        continue
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

def transfer_files(cef_dir, script_dir, transfer_cfg, output_dir, quiet):
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

def combine_libs(build_dir, libs, dest_lib):
  """ Combine multiple static libraries into a single static library. """
  cmdline = 'msvs_env.bat python combine_libs.py -o "%s"' % dest_lib
  for lib in libs:
    lib_path = os.path.join(build_dir, lib)
    if not path_exists(lib_path):
      raise Exception('Library not found: ' + lib_path)
    cmdline = cmdline + ' "%s"' % lib_path
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
                  help='build was created for 64-bit systems')
parser.add_option('--minimal',
                  action='store_true', dest='minimal', default=False,
                  help='include only release build binary files')
parser.add_option('--client',
                  action='store_true', dest='client', default=False,
                  help='include only the cefclient application')
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
  print 'Invalid combination of options'
  parser.print_help(sys.stderr)
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

platform_arch = '32'
if options.x64build:
  platform_arch = '64'

if platform == 'linux':
  platform_arch = ''
  lib_dir_name = 'lib'
  release_libcef_path = os.path.join(src_dir, 'out', 'Release', lib_dir_name, 'libcef.so');
  debug_libcef_path = os.path.join(src_dir, 'out', 'Debug', lib_dir_name, 'libcef.so');
  file_desc = ''
  output = subprocess.check_output('file ' + release_libcef_path + ' ' + debug_libcef_path + '; exit 0',
                                  env=os.environ, stderr=subprocess.STDOUT, shell=True)
  if output.find('32-bit') != -1:
    platform_arch = '32'
  if output.find('64-bit') != -1:
    platform_arch = '64'

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

if mode == 'standard':
  # create the include directory
  include_dir = os.path.join(output_dir, 'include')
  make_dir(include_dir, options.quiet)

  # create the cefclient directory
  cefclient_dir = os.path.join(output_dir, 'cefclient')
  make_dir(cefclient_dir, options.quiet)
  
  # create the cefsimple directory
  cefsimple_dir = os.path.join(output_dir, 'cefsimple')
  make_dir(cefsimple_dir, options.quiet)

  # create the libcef_dll_wrapper directory
  wrapper_dir = os.path.join(output_dir, 'libcef_dll')
  make_dir(wrapper_dir, options.quiet)

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

  # transfer common libcef_dll_wrapper files
  transfer_gypi_files(cef_dir, cef_paths2['libcef_dll_wrapper_sources_common'], \
                      'libcef_dll/', wrapper_dir, options.quiet)
  transfer_gypi_files(cef_dir, cef_paths['autogen_client_side'], \
                      'libcef_dll/', wrapper_dir, options.quiet)

  # transfer gyp files
  copy_file(os.path.join(script_dir, 'distrib/cefclient.gyp'), output_dir, options.quiet)
  paths_gypi = os.path.join(cef_dir, 'cef_paths2.gypi')
  data = read_file(paths_gypi)
  data = data.replace('tests/cefclient/', 'cefclient/')
  data = data.replace('tests/cefsimple/', 'cefsimple/')
  write_file(os.path.join(output_dir, 'cef_paths2.gypi'), data)
  copy_file(os.path.join(cef_dir, 'cef_paths.gypi'), \
            os.path.join(output_dir, 'cef_paths.gypi'), options.quiet)

  # transfer additional files
  transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib/transfer.cfg'), \
                 output_dir, options.quiet)

  # process cmake templates
  variables = dict(cef_paths.items() + cef_paths2.items())
  process_cmake_template(os.path.join(cef_dir, 'CMakeLists.txt.in'), \
                         os.path.join(output_dir, 'CMakeLists.txt'), \
                         variables, options.quiet)
  process_cmake_template(os.path.join(cef_dir, 'macros.cmake.in'), \
                         os.path.join(output_dir, 'macros.cmake'), \
                         variables, options.quiet)
  process_cmake_template(os.path.join(cef_dir, 'libcef_dll', 'CMakeLists.txt.in'), \
                         os.path.join(output_dir, 'libcef_dll', 'CMakeLists.txt'), \
                         variables, options.quiet)
  process_cmake_template(os.path.join(cef_dir, 'tests', 'cefclient', 'CMakeLists.txt.in'), \
                         os.path.join(output_dir, 'cefclient', 'CMakeLists.txt'), \
                         variables, options.quiet)
  process_cmake_template(os.path.join(cef_dir, 'tests', 'cefsimple', 'CMakeLists.txt.in'), \
                         os.path.join(output_dir, 'cefsimple', 'CMakeLists.txt'), \
                         variables, options.quiet)

if platform == 'windows':
  binaries = [
    'd3dcompiler_47.dll',
    'libcef.dll',
    'libEGL.dll',
    'libGLESv2.dll',
    'natives_blob.bin',
    'snapshot_blob.bin',
    'widevinecdmadapter.dll',
  ]
  if not options.x64build:
    binaries.append('wow_helper.exe')

  out_dir = os.path.join(src_dir, 'out')
  libcef_dll_file = 'libcef.dll.lib'
  sandbox_libs = [
    'obj\\base\\base.lib',
    'obj\\base\\base_static.lib',
    'obj\\cef\\cef_sandbox.lib',
    'obj\\base\\third_party\\dynamic_annotations\\dynamic_annotations.lib',
    'obj\\sandbox\\sandbox.lib',
  ]

  valid_build_dir = None

  build_dir_suffix = ''
  if options.x64build:
    build_dir_suffix = '_x64'

  if mode == 'standard':
    # transfer Debug files
    build_dir = os.path.join(out_dir, 'Debug' + build_dir_suffix);
    if not options.allowpartial or path_exists(os.path.join(build_dir, 'cefclient.exe')):
      valid_build_dir = build_dir
      dst_dir = os.path.join(output_dir, 'Debug')
      make_dir(dst_dir, options.quiet)
      copy_files(os.path.join(script_dir, 'distrib/win/*.dll'), dst_dir, options.quiet)
      for binary in binaries:
        copy_file(os.path.join(build_dir, binary), os.path.join(dst_dir, binary), options.quiet)
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
  build_dir = os.path.join(out_dir, 'Release' + build_dir_suffix);
  if not options.allowpartial or path_exists(os.path.join(build_dir, 'cefclient.exe')):
    valid_build_dir = build_dir
    dst_dir = os.path.join(output_dir, 'Release')
    make_dir(dst_dir, options.quiet)
    copy_files(os.path.join(script_dir, 'distrib/win/*.dll'), dst_dir, options.quiet)
    for binary in binaries:
      copy_file(os.path.join(build_dir, binary), os.path.join(dst_dir, binary), options.quiet)

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

  if mode == 'standard':
    # transfer include files
    transfer_gypi_files(cef_dir, cef_paths2['includes_win'], \
                        'include/', include_dir, options.quiet)

    # transfer cefclient files
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_win'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)

    # transfer cefsimple files
    transfer_gypi_files(cef_dir, cef_paths2['cefsimple_sources_win'], \
                        'tests/cefsimple/', cefsimple_dir, options.quiet)

    # transfer additional files, if any
    transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib/win/transfer.cfg'), \
                   output_dir, options.quiet)

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
  out_dir = os.path.join(src_dir, 'out')

  valid_build_dir = None
  framework_name = 'Chromium Embedded Framework'

  if mode == 'standard':
    # transfer Debug files
    build_dir = os.path.join(out_dir, 'Debug')
    if not options.allowpartial or path_exists(os.path.join(build_dir, 'cefclient.app')):
      valid_build_dir = build_dir
      dst_dir = os.path.join(output_dir, 'Debug')
      make_dir(dst_dir, options.quiet)
      copy_dir(os.path.join(build_dir, 'cefclient.app/Contents/Frameworks/%s.framework' % framework_name), \
               os.path.join(dst_dir, '%s.framework' % framework_name), options.quiet)

  # transfer Release files
  build_dir = os.path.join(out_dir, 'Release')
  if not options.allowpartial or path_exists(os.path.join(build_dir, 'cefclient.app')):
    valid_build_dir = build_dir
    dst_dir = os.path.join(output_dir, 'Release')
    make_dir(dst_dir, options.quiet)
    if mode != 'client':
      copy_dir(os.path.join(build_dir, 'cefclient.app/Contents/Frameworks/%s.framework' % framework_name), \
               os.path.join(dst_dir, '%s.framework' % framework_name), options.quiet)
    else:
      copy_dir(os.path.join(build_dir, 'cefclient.app'), os.path.join(dst_dir, 'cefclient.app'), options.quiet)

    if not options.nosymbols:
      # create the symbol output directory
      symbol_output_dir = create_output_dir(output_dir_name + '_release_symbols', options.outputdir)

      # create the real dSYM file from the "fake" dSYM file
      sys.stdout.write("Creating the real dSYM file...\n")
      src_path = os.path.join(build_dir, \
          '%s.framework.dSYM/Contents/Resources/DWARF/%s' % (framework_name, framework_name))
      dst_path = os.path.join(symbol_output_dir, '%s.dSYM' % framework_name)
      run('dsymutil "%s" -o "%s"' % (src_path, dst_path), cef_dir)

  if mode == 'standard':
    # transfer include files
    transfer_gypi_files(cef_dir, cef_paths2['includes_mac'], \
                        'include/', include_dir, options.quiet)

    # transfer cefclient files
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_mac'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_mac_helper'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_bundle_resources_mac'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)

    # transfer cefclient/resources/mac files
    copy_dir(os.path.join(cef_dir, 'tests/cefclient/resources/mac/'), \
             os.path.join(output_dir, 'cefclient/resources/mac/'), \
             options.quiet)

    # transfer cefsimple files
    transfer_gypi_files(cef_dir, cef_paths2['cefsimple_sources_mac'], \
                        'tests/cefsimple/', cefsimple_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['cefsimple_sources_mac_helper'], \
                        'tests/cefsimple/', cefsimple_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['cefsimple_bundle_resources_mac'], \
                        'tests/cefsimple/', cefsimple_dir, options.quiet)

    # transfer cefsimple/mac files
    copy_dir(os.path.join(cef_dir, 'tests/cefsimple/mac/'), os.path.join(output_dir, 'cefsimple/mac/'), \
             options.quiet)

    # transfer additional files, if any
    transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib/mac/transfer.cfg'), \
                  output_dir, options.quiet)

elif platform == 'linux':
  out_dir = os.path.join(src_dir, 'out')
  lib_dir_name = 'lib'

  valid_build_dir = None

  if mode == 'standard':
    # transfer Debug files
    build_dir = os.path.join(out_dir, 'Debug');
    if not options.allowpartial or path_exists(os.path.join(build_dir, 'cefclient')):
      valid_build_dir = build_dir
      dst_dir = os.path.join(output_dir, 'Debug')
      make_dir(dst_dir, options.quiet)
      copy_file(os.path.join(build_dir, 'chrome_sandbox'), os.path.join(dst_dir, 'chrome-sandbox'), options.quiet)
      copy_file(os.path.join(build_dir, lib_dir_name, 'libcef.so'), dst_dir, options.quiet)
      copy_file(os.path.join(build_dir, 'natives_blob.bin'), dst_dir, options.quiet)
      copy_file(os.path.join(build_dir, 'snapshot_blob.bin'), dst_dir, options.quiet)
    else:
      sys.stderr.write("No Debug build files.\n")

  # transfer Release files
  build_dir = os.path.join(out_dir, 'Release');
  if not options.allowpartial or path_exists(os.path.join(build_dir, 'cefclient')):
    valid_build_dir = build_dir
    dst_dir = os.path.join(output_dir, 'Release')
    make_dir(dst_dir, options.quiet)

    if mode == 'client':
      lib_dst_dir = os.path.join(dst_dir, lib_dir_name)
      make_dir(lib_dst_dir, options.quiet)
      copy_file(os.path.join(build_dir, lib_dir_name, 'libcef.so'), lib_dst_dir, options.quiet)
      copy_file(os.path.join(build_dir, 'cefclient'), dst_dir, options.quiet)
    else:
      copy_file(os.path.join(build_dir, lib_dir_name, 'libcef.so'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'chrome_sandbox'), os.path.join(dst_dir, 'chrome-sandbox'), options.quiet)
    copy_file(os.path.join(build_dir, 'natives_blob.bin'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'snapshot_blob.bin'), dst_dir, options.quiet)
  else:
    sys.stderr.write("No Release build files.\n")

  if not valid_build_dir is None:
    # transfer resource files
    build_dir = valid_build_dir
    if mode == 'client':
      dst_dir = os.path.join(output_dir, 'Release')
      copy_dir(os.path.join(build_dir, 'files'), os.path.join(dst_dir, 'files'), options.quiet)
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

  if mode == 'standard':
    # transfer include files
    transfer_gypi_files(cef_dir, cef_paths2['includes_linux'], \
                        'include/', include_dir, options.quiet)

    # transfer cefclient files
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_linux'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_bundle_resources_linux'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)

    # transfer cefsimple files
    transfer_gypi_files(cef_dir, cef_paths2['cefsimple_sources_linux'], \
                        'tests/cefsimple/', cefsimple_dir, options.quiet)

    # transfer additional files, if any
    transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib/linux/transfer.cfg'), \
                  output_dir, options.quiet)

if not options.noarchive:
  # create an archive for each output directory
  archive_extenstion = '.zip'
  if os.getenv('CEF_COMMAND_7ZIP', '') != '':
    archive_extenstion = '.7z'
  for dir in archive_dirs:
    zip_file = os.path.split(dir)[1] + archive_extenstion
    if not options.quiet:
      sys.stdout.write('Creating '+zip_file+"...\n")
    if archive_extenstion == '.zip':
      create_archive(dir, os.path.join(dir, os.pardir, zip_file))
    else:
      create_7z_archive(dir, os.path.join(dir, os.pardir, zip_file))
