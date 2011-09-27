# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from date_util import *
from file_util import *
from gclient_util import *
from optparse import OptionParser
import os
import re
from svn_util import *
import sys

def eval_file(src):
  """ Loads and evaluates the contents of the specified file. """
  return eval(read_file(src), {'__builtins__': None}, None)
    
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
  data = re.sub(r'''#include \"[a-zA-Z0-9_\/]+\/+([a-zA-Z0-9_\.]+)\"''', \
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
      open(readme, 'a').write(cfg['source']+"\n")
    
    # perform any required post-processing
    post = cfg['post-process']
    if post == 'normalize_headers':
      new_path = ''
      if cfg.has_key('new_header_path'):
        new_path = cfg['new_header_path']
      normalize_headers(dst, new_path)

def generate_msvs_projects(version):
  """ Generate MSVS projects for the specified version. """
  sys.stdout.write('Generating '+version+' project files...')
  os.environ['GYP_MSVS_VERSION'] = version
  gyper = [ 'python', 'tools/gyp_cef', os.path.relpath(os.path.join(output_dir, 'cefclient.gyp'), cef_dir) ]
  RunAction(cef_dir, gyper);
  move_file(os.path.relpath(os.path.join(output_dir, 'cefclient.sln')), \
            os.path.relpath(os.path.join(output_dir, 'cefclient'+version+'.sln')))
      
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
parser.add_option('-q', '--quiet',
                  action='store_true', dest='quiet', default=False,
                  help='do not output detailed status information')
(options, args) = parser.parse_args()

# the outputdir option is required
if options.outputdir is None:
  parser.print_help(sys.stdout)
  sys.exit()

# retrieve revision and date information
cef_rev = get_revision()
chromium_rev = get_revision('../../')
date = get_date()

# script directory
script_dir = os.path.dirname(__file__)

# CEF root directory
cef_dir = os.path.abspath(os.path.join(script_dir, os.pardir))

# output directory
output_dir = os.path.abspath(os.path.join(options.outputdir, 'cef_binary_r'+cef_rev))
remove_dir(output_dir, options.quiet)
make_dir(output_dir, options.quiet)

# write the README.TXT file
data = read_file(os.path.join(script_dir, 'distrib/README.TXT'))
data = data.replace('$CEF_REV$', cef_rev)
data = data.replace('$CHROMIUM_REV$', chromium_rev)
data = data.replace('$DATE$', date)
write_file(os.path.join(output_dir, 'README.TXT'), data)
if not options.quiet:
  sys.stdout.write('Creating README.TXT file.\n')

# transfer the LICENSE.TXT file
copy_file(os.path.join(cef_dir, 'LICENSE.TXT'), output_dir, options.quiet)

# read the variables list from cef_paths.gypi
cef_paths = eval_file(os.path.join(cef_dir, 'cef_paths.gypi'))
cef_paths = cef_paths['variables']

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
transfer_gypi_files(cef_dir, cef_paths['includes_common'], \
                    'include/', include_dir, options.quiet)

# transfer common cefclient files
transfer_gypi_files(cef_dir, cef_paths['cefclient_sources_common'], \
                    'tests/cefclient/', cefclient_dir, options.quiet)

# transfer common libcef_dll_wrapper files
transfer_gypi_files(cef_dir, cef_paths['libcef_dll_wrapper_sources_common'], \
                    'libcef_dll/', wrapper_dir, options.quiet)

# transfer gyp files
copy_file(os.path.join(script_dir, 'distrib/cefclient.gyp'), output_dir, options.quiet)
paths_gypi = os.path.join(cef_dir, 'cef_paths.gypi')
data = read_file(paths_gypi)
data = data.replace('tests/cefclient/', 'cefclient/')
write_file(os.path.join(output_dir, 'cef_paths.gypi'), data)

# transfer additional files
transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib/transfer.cfg'), \
               output_dir, options.quiet)

if sys.platform == 'win32':
   # transfer include files
  transfer_gypi_files(cef_dir, cef_paths['includes_win'], \
                      'include/', include_dir, options.quiet)

  # transfer cefclient files
  transfer_gypi_files(cef_dir, cef_paths['cefclient_sources_win'], \
                      'tests/cefclient/', cefclient_dir, options.quiet)
                      
  # transfer build/Debug files
  dst_dir = os.path.join(output_dir, 'Debug')
  make_dir(dst_dir, options.quiet)
  copy_files(os.path.join(cef_dir, 'Debug/*.dll'), dst_dir, options.quiet)
  copy_files(os.path.join(script_dir, 'distrib/win/*.dll'), dst_dir, options.quiet)
  
  # transfer build/Release files
  dst_dir = os.path.join(output_dir, 'Release')
  make_dir(dst_dir, options.quiet)
  copy_files(os.path.join(cef_dir, 'Release/*.dll'), dst_dir, options.quiet)
  copy_files(os.path.join(script_dir, 'distrib/win/*.dll'), dst_dir, options.quiet)
  
  # transfer lib/Debug files
  dst_dir = os.path.join(output_dir, 'lib/Debug')
  make_dir(dst_dir, options.quiet)
  copy_file(os.path.join(cef_dir, 'Debug/lib/libcef.lib'), dst_dir, options.quiet)
  
  # transfer lib/Release files
  dst_dir = os.path.join(output_dir, 'lib/Release')
  make_dir(dst_dir, options.quiet)
  copy_file(os.path.join(cef_dir, 'Release/lib/libcef.lib'), dst_dir, options.quiet)
  
  # generate doc files
  os.popen('make_cppdocs.bat '+cef_rev)
  
  # transfer docs files
  dst_dir = os.path.join(output_dir, 'docs')
  src_dir = os.path.join(cef_dir, 'docs')
  if path_exists(dst_dir):
    copy_dir(src_dir, dst_dir, options.quiet)
  
  # transfer additional files, if any
  transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib/win/transfer.cfg'), \
                output_dir, options.quiet)
  
  # generate the project files
  generate_msvs_projects('2005');
  generate_msvs_projects('2008');
  generate_msvs_projects('2010');

elif sys.platform == 'darwin':
   # transfer include files
  transfer_gypi_files(cef_dir, cef_paths['includes_mac'], \
                      'include/', include_dir, options.quiet)

  # transfer cefclient files
  transfer_gypi_files(cef_dir, cef_paths['cefclient_sources_mac'], \
                      'tests/cefclient/', cefclient_dir, options.quiet)

  # transfer additional files, if any
  transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib/mac/transfer.cfg'), \
                output_dir, options.quiet)

elif sys.platform == 'linux2':
   # transfer include files
  transfer_gypi_files(cef_dir, cef_paths['includes_linux'], \
                      'include/', include_dir, options.quiet)

  # transfer cefclient files
  transfer_gypi_files(cef_dir, cef_paths['cefclient_sources_linux'], \
                      'tests/cefclient/', cefclient_dir, options.quiet)

  # transfer additional files, if any
  transfer_files(cef_dir, script_dir, os.path.join(script_dir, 'distrib/linux/transfer.cfg'), \
                output_dir, options.quiet)
