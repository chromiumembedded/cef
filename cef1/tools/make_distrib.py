# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from date_util import *
from file_util import *
from gclient_util import *
from optparse import OptionParser
import os
import re
import shlex
import subprocess
from svn_util import *
import sys
import zipfile

def create_archive(input_dir, zip_file):
  """ Creates a zip archive of the specified input directory. """
  zf = zipfile.ZipFile(zip_file, 'w', zipfile.ZIP_DEFLATED)
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

def eval_file(src):
  """ Loads and evaluates the contents of the specified file. """
  return eval(read_file(src), {'__builtins__': None}, None)
    
def transfer_gypi_files(src_dir, gypi_paths, gypi_path_prefix, dst_dir, quiet):
  """ Transfer files from one location to another. """
  for path in gypi_paths:
    # skip gyp includes
    if path[:2] == '<@':
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

def generate_msvs_projects(version):
  """ Generate MSVS projects for the specified version. """
  sys.stdout.write('Generating '+version+' project files...')
  os.environ['GYP_GENERATORS'] = 'msvs'
  os.environ['GYP_MSVS_VERSION'] = version
  gyper = [ 'python', 'tools/gyp_cef', os.path.relpath(os.path.join(output_dir, 'cefclient.gyp'), cef_dir) ]
  RunAction(cef_dir, gyper);
  move_file(os.path.relpath(os.path.join(output_dir, 'cefclient.sln')), \
            os.path.relpath(os.path.join(output_dir, 'cefclient'+version+'.sln')))

def create_msvs_projects():
  """ Create MSVS project files. """
  generate_msvs_projects('2005');
  generate_msvs_projects('2008');
  generate_msvs_projects('2010');

  # Fix the output directory path in all .vcproj and .vcxproj files.
  files = []
  for file in get_files(os.path.join(output_dir, '*.vcproj')):
    files.append(file)
  for file in get_files(os.path.join(output_dir, '*.vcxproj')):
    files.append(file)
  for file in files:
    data = read_file(file)
    # fix the build directory path
    data = data.replace('../../..\\build\\', 'out\\')
    data = data.replace('..\\..\\..\\build\\', 'out\\')
    # fix xcopy arguments
    data = data.replace('xcopy \\', 'xcopy /')
    write_file(file, data)

def create_xcode_projects():
  """ Create Xcode project files. """
  sys.stdout.write('Generating Xcode project files...')
  os.environ['GYP_GENERATORS'] = 'xcode'
  gyper = [ 'python', 'tools/gyp_cef', os.path.relpath(os.path.join(output_dir, 'cefclient.gyp'), cef_dir) ]
  RunAction(cef_dir, gyper);

  # Post-process the Xcode project to fix file paths
  src_file = os.path.join(output_dir, 'cefclient.xcodeproj/project.pbxproj')
  data = read_file(src_file)
  data = data.replace('../../../build/mac/', 'tools/')
  data = data.replace('../../../build', 'build')
  data = data.replace('../../../xcodebuild', 'xcodebuild')
  write_file(src_file, data)

def create_make_projects():
  """ Create make project files. """
  makefile = os.path.join(src_dir, 'Makefile')
  makefile_tmp = ''
  if path_exists(makefile):
    # Back up the existing Makefile
    makefile_tmp = makefile + '.cef_bak'
    copy_file(makefile, makefile_tmp, options.quiet)

  # Generate make project files
  sys.stdout.write('Generating make project files...')
  os.environ['GYP_GENERATORS'] = 'make'
  gyper = [ 'python', 'tools/gyp_cef', os.path.relpath(os.path.join(output_dir, 'cefclient.gyp'), cef_dir) ]
  RunAction(cef_dir, gyper);

  # Copy the resulting Makefile to the destination directory
  copy_file(makefile, output_dir, options.quiet)

  remove_file(makefile, options.quiet)
  if makefile_tmp != '':
    # Restore the original Makefile
    move_file(makefile_tmp, makefile, options.quiet)

  # Fix the output directory path in Makefile and all .mk files.
  find = os.path.relpath(output_dir, src_dir)
  files = [os.path.join(output_dir, 'Makefile')]
  for file in get_files(os.path.join(output_dir, '*.mk')):
    files.append(file)
  for file in files:
    data = read_file(file)
    data = data.replace(find, '.')
    data = data.replace('/./', '/')
    if os.path.basename(file) == 'Makefile':
      # remove the quiet_cmd_regen_makefile section
      pos = str.find(data, 'quiet_cmd_regen_makefile')
      if pos >= 0:
        epos = str.find(data, '#', pos)
        if epos >= 0:
          data = data[0:pos] + data[epos:]
    write_file(file, data)

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

# the outputdir option is required
if options.outputdir is None:
  parser.print_help(sys.stderr)
  sys.exit()

if options.minimal and options.client:
  print 'Invalid combination of options'
  parser.print_help(sys.stderr)
  sys.exit()

# script directory
script_dir = os.path.dirname(__file__)

# CEF root directory
cef_dir = os.path.abspath(os.path.join(script_dir, os.pardir))

# src directory
src_dir = os.path.abspath(os.path.join(cef_dir, os.pardir))

# retrieve url, revision and date information
cef_info = get_svn_info(cef_dir)
cef_url = cef_info['url']
cef_rev = cef_info['revision']
chromium_info = get_svn_info(os.path.join(cef_dir, os.pardir))
chromium_url = chromium_info['url']
chromium_rev = chromium_info['revision']
date = get_date()

# Read and parse the version file (key=value pairs, one per line)
args = {}
read_version_file(os.path.join(cef_dir, 'VERSION'), args)
read_version_file(os.path.join(cef_dir, '../chrome/VERSION'), args)

cef_ver = args['CEF_MAJOR']+'.'+args['BUILD']+'.'+cef_rev
chromium_ver = args['MAJOR']+'.'+args['MINOR']+'.'+args['BUILD']+'.'+args['PATCH']

# Test the operating system.
platform = '';
if sys.platform == 'win32':
  platform = 'windows'
elif sys.platform == 'darwin':
  platform = 'macosx'
elif sys.platform.startswith('linux'):
  platform = 'linux'

# list of output directories to be archived
archive_dirs = []

# output directory
output_dir_base = 'cef_binary_' + cef_ver
output_dir_name = output_dir_base + '_' + platform

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
  transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_common'], \
                      'tests/cefclient/', cefclient_dir, options.quiet)
  transfer_gypi_files(cef_dir, cef_paths2['cefclient_bundle_resources_common'], \
                      'tests/cefclient/', cefclient_dir, options.quiet)

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
  write_file(os.path.join(output_dir, 'cef_paths2.gypi'), data)
  copy_file(os.path.join(cef_dir, 'cef_paths.gypi'), \
            os.path.join(output_dir, 'cef_paths.gypi'), options.quiet)

  # transfer additional files
  transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib/transfer.cfg'), \
                 output_dir, options.quiet)

if platform == 'windows':
  if options.ninjabuild:
    out_dir = os.path.join(src_dir, 'out')
    libcef_dll_file = 'libcef.dll.lib'
  else:
    out_dir = cef_dir
    libcef_dll_file = 'lib/libcef.lib'

  valid_build_dir = None

  if mode == 'standard':
    # transfer Debug files
    build_dir = os.path.join(out_dir, 'Debug');
    if not options.allowpartial or path_exists(os.path.join(build_dir, 'cefclient.exe')):
      valid_build_dir = build_dir
      dst_dir = os.path.join(output_dir, 'Debug')
      make_dir(dst_dir, options.quiet)
      copy_files(os.path.join(script_dir, 'distrib/win/*.dll'), dst_dir, options.quiet)
      copy_files(os.path.join(build_dir, '*.dll'), dst_dir, options.quiet)
      copy_file(os.path.join(build_dir, libcef_dll_file), os.path.join(dst_dir, 'libcef.lib'), \
                options.quiet)

      if not options.nosymbols:
        # create the symbol output directory
        symbol_output_dir = create_output_dir(output_dir_name + '_debug_symbols', options.outputdir)
        # transfer contents
        copy_file(os.path.join(build_dir, 'libcef.dll.pdb'), symbol_output_dir, options.quiet)
    else:
      sys.stderr.write("No Debug build files.\n")

  # transfer Release files
  build_dir = os.path.join(out_dir, 'Release');
  if not options.allowpartial or path_exists(os.path.join(build_dir, 'cefclient.exe')):
    valid_build_dir = build_dir
    dst_dir = os.path.join(output_dir, 'Release')
    make_dir(dst_dir, options.quiet)
    copy_files(os.path.join(script_dir, 'distrib/win/*.dll'), dst_dir, options.quiet)
    copy_files(os.path.join(build_dir, '*.dll'), dst_dir, options.quiet)

    if mode != 'client':
      copy_file(os.path.join(build_dir, libcef_dll_file), os.path.join(dst_dir, 'libcef.lib'), \
          options.quiet)
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
    copy_file(os.path.join(build_dir, 'devtools_resources.pak'), dst_dir, options.quiet)
    copy_dir(os.path.join(build_dir, 'locales'), os.path.join(dst_dir, 'locales'), options.quiet)

  if mode == 'standard':
    # transfer include files
    transfer_gypi_files(cef_dir, cef_paths2['includes_win'], \
                        'include/', include_dir, options.quiet)

    # transfer cefclient files
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_win'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)

    # transfer additional files, if any
    transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib/win/transfer.cfg'), \
                   output_dir, options.quiet)

    create_msvs_projects()

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
  if options.ninjabuild:
    out_dir = os.path.join(src_dir, 'out')
  else:
    out_dir = os.path.join(src_dir, 'xcodebuild')

  valid_build_dir = None

  if mode == 'standard':
    # transfer Debug files
    build_dir = os.path.join(out_dir, 'Debug')
    if not options.allowpartial or path_exists(os.path.join(build_dir, 'cefclient.app')):
      valid_build_dir = build_dir
      dst_dir = os.path.join(output_dir, 'Debug')
      make_dir(dst_dir, options.quiet)
      copy_file(os.path.join(build_dir, 'libcef.dylib'), dst_dir, options.quiet)

  # transfer Release files
  build_dir = os.path.join(out_dir, 'Release')
  if not options.allowpartial or path_exists(os.path.join(build_dir, 'cefclient.app')):
    valid_build_dir = build_dir
    dst_dir = os.path.join(output_dir, 'Release')
    make_dir(dst_dir, options.quiet)
    if mode != 'client':
      copy_file(os.path.join(build_dir, 'libcef.dylib'), dst_dir, options.quiet)
    else:
      copy_dir(os.path.join(build_dir, 'cefclient.app'), os.path.join(dst_dir, 'cefclient.app'), options.quiet)

    if not options.nosymbols:
      # create the symbol output directory
      symbol_output_dir = create_output_dir(output_dir_name + '_release_symbols', options.outputdir)

      # create the real dSYM file from the "fake" dSYM file
      sys.stdout.write("Creating the real dSYM file...\n")
      src_path = os.path.join(build_dir, 'libcef.dylib.dSYM/Contents/Resources/DWARF/libcef.dylib')
      dst_path = os.path.join(symbol_output_dir, 'libcef.dylib.dSYM')
      run('dsymutil '+src_path+' -o '+dst_path, cef_dir)

  if not valid_build_dir is None and mode != 'client':
    # transfer resource files
    build_dir = valid_build_dir
    dst_dir = os.path.join(output_dir, 'Resources')
    make_dir(dst_dir, options.quiet)
    copy_files(os.path.join(cef_dir, '../third_party/WebKit/Source/WebCore/Resources/*.*'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'cefclient.app/Contents/Resources/chrome.pak'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'cefclient.app/Contents/Resources/devtools_resources.pak'), dst_dir, options.quiet)
    copy_files(os.path.join(build_dir, 'cefclient.app/Contents/Resources/*.lproj'), dst_dir, options.quiet)
    remove_dir(os.path.join(dst_dir, 'English.lproj'))

  if mode == 'standard':
    # transfer include files
    transfer_gypi_files(cef_dir, cef_paths2['includes_mac'], \
                        'include/', include_dir, options.quiet)

    # transfer cefclient files
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_sources_mac'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)
    transfer_gypi_files(cef_dir, cef_paths2['cefclient_bundle_resources_mac'], \
                        'tests/cefclient/', cefclient_dir, options.quiet)

    # transfer cefclient/mac files
    copy_dir(os.path.join(cef_dir, 'tests/cefclient/mac/'), os.path.join(output_dir, 'cefclient/mac/'), \
             options.quiet)

    # transfer additional files, if any
    transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib/mac/transfer.cfg'), \
                  output_dir, options.quiet)

    create_xcode_projects()

elif platform == 'linux':
  out_dir = os.path.join(src_dir, 'out')
  if options.ninjabuild:
    lib_dir_name = 'lib'
  else:
    lib_dir_name = 'lib.target'

  valid_build_dir = None

  if mode == 'standard':
    # transfer Debug files
    build_dir = os.path.join(out_dir, 'Debug');
    if not options.allowpartial or path_exists(os.path.join(build_dir, 'cefclient')):
      valid_build_dir = build_dir
      dst_dir = os.path.join(output_dir, 'Debug')
      make_dir(dst_dir, options.quiet)
      copy_file(os.path.join(build_dir, lib_dir_name, 'libcef.so'), dst_dir, options.quiet)
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
    copy_file(os.path.join(build_dir, 'chrome.pak'), dst_dir, options.quiet)
    copy_file(os.path.join(build_dir, 'devtools_resources.pak'), dst_dir, options.quiet)
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

    # transfer additional files, if any
    copy_file(os.path.join(script_dir, 'distrib/linux/build.sh'), output_dir, options.quiet)
    transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib/linux/transfer.cfg'), \
                  output_dir, options.quiet)

    create_make_projects()

if not options.noarchive:
  # create an archive for each output directory
  for dir in archive_dirs:
    zip_file = os.path.split(dir)[1] + '.zip'
    if not options.quiet:
      sys.stdout.write('Creating '+zip_file+"...\n")
    create_archive(dir, os.path.join(dir, os.pardir, zip_file))
