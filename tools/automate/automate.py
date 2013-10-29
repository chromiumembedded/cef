# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from optparse import OptionParser
import httplib
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import urllib
import xml.etree.ElementTree as ET
import zipfile

# default URL values
cef_url = 'http://chromiumembedded.googlecode.com/svn/trunk/cef3'
depot_tools_url = 'http://src.chromium.org/svn/trunk/tools/depot_tools'

def run(command_line, working_dir, depot_tools_dir=None, output_file=None):
  # add depot_tools to the path
  env = os.environ
  if not depot_tools_dir is None:
    env['PATH'] = depot_tools_dir+os.pathsep+env['PATH']

  sys.stdout.write('-------- Running "'+command_line+'" in "'+\
                   working_dir+'"...'+"\n")
  if not options.dryrun:
    args = shlex.split(command_line.replace('\\', '\\\\'))

    if not output_file:
      return subprocess.check_call(args, cwd=working_dir, env=env,
                                 shell=(sys.platform == 'win32'))
    with open(output_file, "w") as f:
      return subprocess.check_call(args, cwd=working_dir, env=env,
                                 shell=(sys.platform == 'win32'),
                                 stderr=subprocess.STDOUT, stdout=f)

def check_url(url):
  """ Check the URL and raise an exception if invalid. """
  if ':' in url[:7]:
    parts = url.split(':', 1)
    if (parts[0] == 'http' or parts[0] == 'https') and \
        parts[1] == urllib.quote(parts[1]):
      return url
  sys.stderr.write('Invalid URL: '+url+"\n")
  raise Exception('Invalid URL: '+url)

def get_svn_info(path):
  """ Retrieves the URL and revision from svn info. """
  url = 'None'
  rev = 'None'
  sys.stdout.write("-------- Running \"%s info --xml %s\"...\n" % (svn_exe, path))
  if path[0:4] == 'http' or os.path.exists(path):
    try:
      p = subprocess.Popen([svn_exe, 'info', '--xml', path], \
          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
      out, err = p.communicate()
      if err == '':
        tree = ET.ElementTree(ET.fromstring(out))
        entry = tree.getroot().find('entry')
        url = entry.find('url').text
        rev = entry.attrib['revision']
      else:
        raise Exception("Failed to execute svn info:\n"+err+"\n")
    except IOError, (errno, strerror):
      sys.stderr.write('Failed to read svn info: '+strerror+"\n")
      raise
    except:
      raise
  return {'url': url, 'revision': rev}

def download_and_extract(src, target, contents_prefix):
  """ Extracts the contents of src, which may be a URL or local file, to the
      target directory. """
  sys.stdout.write('Extracting %s to %s.\n' % (src, target))
  temporary = False

  if src[:4] == 'http':
    # Attempt to download a URL.
    opener = urllib.FancyURLopener({})
    response = opener.open(src)

    temporary = True
    handle, archive_path = tempfile.mkstemp(suffix = '.zip')
    os.write(handle, response.read())
    os.close(handle)
  elif os.path.exists(src):
    # Use a local file.
    archive_path = src
  else:
    raise Exception('Path type is unsupported or does not exist: ' + src)

  if not zipfile.is_zipfile(archive_path):
    raise Exception('Not a valid zip archive: ' + src)

  def remove_prefix(zip, prefix):
    offset = len(prefix)
    for zipinfo in zip.infolist():
      name = zipinfo.filename
      if len(name) > offset and name[:offset] == prefix:
        zipinfo.filename = name[offset:]
        yield zipinfo

  # Attempt to extract the archive file.
  try:
    os.makedirs(target)
    zf = zipfile.ZipFile(archive_path, 'r')
    zf.extractall(target, remove_prefix(zf, contents_prefix))
  except:
    shutil.rmtree(target, onerror=onerror)
    raise
  zf.close()

  # Delete the archive file if temporary.
  if temporary and os.path.exists(archive_path):
    os.remove(archive_path)

def onerror(func, path, exc_info):
  """
  Error handler for ``shutil.rmtree``.

  If the error is due to an access error (read only file)
  it attempts to add write permission and then retries.

  If the error is for another reason it re-raises the error.

  Usage : ``shutil.rmtree(path, onerror=onerror)``
  """
  import stat
  if not os.access(path, os.W_OK):
    # Is the error an access error ?
    os.chmod(path, stat.S_IWUSR)
    func(path)
  else:
    raise

# cannot be loaded as a module
if __name__ != "__main__":
  sys.stderr.write('This file cannot be loaded as a module!')
  sys.exit()

# parse command-line options
disc = """
This utility implements automation for the download, update, build and
distribution of CEF.
"""

parser = OptionParser(description=disc)
parser.add_option('--download-dir', dest='downloaddir', metavar='DIR',
                  help='download directory with no spaces [required]')
parser.add_option('--revision', dest='revision', type="int",
                  help='CEF source revision')
parser.add_option('--url', dest='url',
                  help='CEF source URL')
parser.add_option('--depot-tools', dest='depottools', metavar='DIR',
                  help='download directory for depot_tools', default='')
parser.add_option('--depot-tools-archive', dest='depottoolsarchive',
                  help='zip archive file that contains a single top-level '+\
                       'depot_tools directory', default='')
parser.add_option('--force-config',
                  action='store_true', dest='forceconfig', default=False,
                  help='force Chromium configuration')
parser.add_option('--force-clean',
                  action='store_true', dest='forceclean', default=False,
                  help='force revert of all Chromium changes, deletion of '+\
                       'all unversioned files including the CEF folder and '+\
                       'trigger the force-update, force-build and '+\
                       'force-distrib options')
parser.add_option('--force-update',
                  action='store_true', dest='forceupdate', default=False,
                  help='force Chromium and CEF update')
parser.add_option('--no-update',
                  action='store_true', dest='noupdate', default=False,
                  help='do not update Chromium and CEF.' +\
                       'Cannot be used along with --force[update|config|clean]')
parser.add_option('--force-build',
                  action='store_true', dest='forcebuild', default=False,
                  help='force CEF debug and release builds')
parser.add_option('--build-tests',
                  action='store_true', dest='buildtests', default=False,
                  help='build cef_unittests target besides cefclient')
parser.add_option('--force-distrib',
                  action='store_true', dest='forcedistrib', default=False,
                  help='force creation of CEF binary distribution')
parser.add_option('--no-debug-build',
                  action='store_true', dest='nodebugbuild', default=False,
                  help="don't perform the CEF debug build")
parser.add_option('--no-release-build',
                  action='store_true', dest='noreleasebuild', default=False,
                  help="don't perform the CEF release build")
parser.add_option('--no-distrib',
                  action='store_true', dest='nodistrib', default=False,
                  help="don't create any CEF binary distribution")
parser.add_option('--minimal-distrib',
                  action='store_true', dest='minimaldistrib', default=False,
                  help='create a minimal CEF binary distribution')
parser.add_option('--minimal-distrib-only',
                  action='store_true', dest='minimaldistribonly', default=False,
                  help='create a minimal CEF binary distribution only')
parser.add_option('--client-distrib',
                  action='store_true', dest='clientdistrib', default=False,
                  help='create a client CEF binary distribution')
parser.add_option('--client-distrib-only',
                  action='store_true', dest='clientdistribonly', default=False,
                  help='create a client CEF binary distribution only')
parser.add_option('--no-distrib-docs',
                  action='store_true', dest='nodistribdocs', default=False,
                  help="don't create CEF documentation")
parser.add_option('--no-distrib-archive',
                  action='store_true', dest='nodistribarchive', default=False,
                  help="don't create archives for output directories")
parser.add_option('--ninja-build',
                  action='store_true', dest='ninjabuild', default=False,
                  help="build using ninja")
parser.add_option('--verbose',
                  action='store_true', dest='verbose', default=False,
                  help='show all command lines while building')
parser.add_option('--build-log-file',
                  action='store_true', dest='buildlogfile', default=False,
                  help='write build logs to files')
parser.add_option('--x64-build',
                  action='store_true', dest='x64build', default=False,
                  help='build for 64-bit systems (Windows and Mac OS X only)')
parser.add_option('--clean-artifacts',
                  action='store_true', dest='cleanartifacts', default=False,
                  help='clean the artifacts output directory')
parser.add_option('--dry-run',
                  action='store_true', dest='dryrun', default=False,
                  help="output commands without executing them")
(options, args) = parser.parse_args()

# Test the operating system.
platform = '';
if sys.platform == 'win32':
  platform = 'windows'
elif sys.platform == 'darwin':
  platform = 'macosx'
elif sys.platform.startswith('linux'):
  platform = 'linux'

# the downloaddir option is required
if options.downloaddir is None:
  parser.print_help(sys.stderr)
  sys.exit()

if (options.noreleasebuild and (options.minimaldistrib or options.minimaldistribonly or \
                                options.clientdistrib or options.clientdistribonly)) or \
   (options.minimaldistribonly and options.clientdistribonly):
  print 'Invalid combination of options'
  parser.print_help(sys.stderr)
  sys.exit()

if options.noupdate and (options.forceclean or options.forceupdate or options.forceconfig):
  print "Invalid combination of options."
  print "--no-update cannot be used along with --force-[update|config|clean]\n"
  sys.exit()

if options.x64build and platform != 'windows' and platform != 'macosx':
  print 'The x64 build option is only used on Windows and Mac OS X.'
  sys.exit()

if options.x64build and platform == 'windows' and not options.ninjabuild:
  print 'The x64 build option on Windows requires ninja.'
  sys.exit()

# script directory
script_dir = os.path.dirname(__file__)

download_dir = os.path.abspath(options.downloaddir)
if not options.dryrun and not os.path.exists(download_dir):
  # create the download directory
  os.makedirs(download_dir)

# set the expected script extension
if platform == 'windows':
  script_ext = '.bat'
else:
  script_ext = '.sh'

# check if the "depot_tools" directory exists
if options.depottools != '':
  depot_tools_dir = os.path.abspath(options.depottools)
else:
  depot_tools_dir = os.path.join(download_dir, 'depot_tools')
if not os.path.exists(depot_tools_dir):
  if options.depottoolsarchive != '':
    # extract depot_tools from an archive file
    download_and_extract(options.depottoolsarchive, depot_tools_dir,
        'depot_tools/')
  else:
    # checkout depot_tools
    run('svn checkout '+depot_tools_url+' '+depot_tools_dir, download_dir)

if not options.noupdate and options.depottools == '':
  # Update depot_tools. It will download required scripts (svn, python, ...)
  if platform == 'windows':
    run('update_depot_tools.bat', depot_tools_dir, depot_tools_dir);
  else:
    run('update_depot_tools', depot_tools_dir, depot_tools_dir);

if platform == 'windows':
  # Force use of the SVN version bundled with depot_tools.
  svn_exe = os.path.join(depot_tools_dir, 'svn.bat')
  if options.dryrun and not os.path.exists(svn_exe):
    sys.stdout.write("WARNING: --dry-run assumes that depot_tools" \
                     " is already in your PATH. If it isn't\nplease" \
                     " specify a --depot-tools value.\n")
    svn_exe = 'svn.bat'
else:
  svn_exe = 'svn'

if not options.url is None:
  # set the CEF URL
  cef_url = check_url(options.url)

if not options.revision is None:
  # set the CEF revision
  cef_rev = str(options.revision)
else:
  # retrieve the CEF revision from the remote repo
  info = get_svn_info(cef_url)
  cef_rev = info['revision']
  if cef_rev == 'None':
    sys.stderr.write('No SVN info for: '+cef_url+"\n")
    raise Exception('No SVN info for: '+cef_url)

# Retrieve the Chromium URL and revision from the CEF repo
compat_url = cef_url + "/CHROMIUM_BUILD_COMPATIBILITY.txt?r="+cef_rev

release_url = None
chromium_url = None
chromium_rev = None

try:
  # Read the remote URL contents
  handle = urllib.urlopen(compat_url)
  compat_value = handle.read().strip()
  handle.close()

  # Parse the contents
  config = eval(compat_value, {'__builtins__': None}, None)

  if 'release_url' in config:
    # building from a release
    release_url = check_url(config['release_url'])
  else:
    # building from chromium src
    if not 'chromium_url' in config:
      raise Exception("Missing chromium_url value")
    if not 'chromium_revision' in config:
      raise Exception("Missing chromium_revision value")

    chromium_url = check_url(config['chromium_url'])
    chromium_rev = str(int(config['chromium_revision']))
except Exception, e:
  sys.stderr.write('Failed to read URL and revision information from '+ \
                   compat_url+"\n")
  raise

# check if the "chromium" directory exists
chromium_dir = os.path.join(download_dir, 'chromium')
if not options.dryrun and not os.path.exists(chromium_dir):
  # create the "chromium" directory
  os.makedirs(chromium_dir)

chromium_src_dir = os.path.join(chromium_dir, 'src')
cef_src_dir = os.path.join(chromium_src_dir, 'cef')
cef_tools_dir = os.path.join(cef_src_dir, 'tools')

# retrieve the current CEF URL and revision
info = get_svn_info(cef_src_dir)
current_cef_url = info['url']
current_cef_rev = info['revision']

if release_url is None:
  # retrieve the current Chromium URL and revision
  info = get_svn_info(chromium_src_dir)
  current_chromium_url = info['url']
  current_chromium_rev = info['revision']

changed_to_message = '  -> CHANGED TO: '
if options.noupdate:
  changed_to_message = '  -> AVAILABLE: '

# test if the CEF URL changed
cef_url_changed = current_cef_url != cef_url
sys.stdout.write('CEF URL: '+current_cef_url+"\n")
if cef_url_changed:
  sys.stdout.write(changed_to_message+cef_url+"\n")

# test if the CEF revision changed
cef_rev_changed = current_cef_rev != cef_rev
sys.stdout.write('CEF Revision: '+current_cef_rev+"\n")
if cef_rev_changed:
  sys.stdout.write(changed_to_message+cef_rev+"\n")

release_url_changed = False
chromium_url_changed = False
chromium_rev_changed = False

if release_url is None:
  # test if the Chromium URL changed
  chromium_url_changed = current_chromium_url != chromium_url
  sys.stdout.write('Chromium URL: '+current_chromium_url+"\n")
  if chromium_url_changed:
    sys.stdout.write(changed_to_message+chromium_url+"\n")

  # test if the Chromium revision changed
  chromium_rev_changed = current_chromium_rev != chromium_rev
  sys.stdout.write('Chromium Revision: '+current_chromium_rev+"\n")
  if chromium_rev_changed:
    sys.stdout.write(changed_to_message+chromium_rev+"\n")
else:
  # test if the release URL changed
  current_release_url = 'None'

  path = os.path.join(chromium_dir, '.gclient')
  if os.path.exists(path):
    # read the .gclient file
    fp = open(path, 'r')
    data = fp.read()
    fp.close()

    # Parse the contents
    config_dict = {}
    try:
      exec(data, config_dict)
      current_release_url = config_dict['solutions'][0]['url']
    except Exception, e:
      sys.stderr.write('Failed to parse existing .glient file.\n')
      raise

  release_url_changed = current_release_url != release_url
  sys.stdout.write('Release URL: '+current_release_url+"\n")
  if release_url_changed:
    sys.stdout.write(changed_to_message+release_url+"\n")

# true if anything changed
any_changed = release_url_changed or chromium_url_changed or \
              chromium_rev_changed or cef_url_changed or cef_rev_changed
if not any_changed:
  sys.stdout.write("No changes.\n")
elif options.noupdate:
  sys.stdout.write("You have updates. Remove --no-update flag to update source code\n")
  release_url_changed = False
  chromium_url_changed = False
  chromium_rev_changed = False
  cef_url_changed = False
  cef_rev_changed = False
  any_changed = False


if release_url_changed or chromium_url_changed or options.forceconfig:
  if release_url is None:
    url = chromium_url
  else:
    url = release_url

  # run gclient config to create the .gclient file
  run('gclient config '+url, chromium_dir, depot_tools_dir)

  if not options.dryrun:
    path = os.path.join(chromium_dir, '.gclient')
    if not os.path.exists(path):
      sys.stderr.write(".gclient file was not created\n")
      raise Exception('.gclient file was not created')

    # read the resulting .gclient file
    fp = open(path, 'r')
    data = fp.read()
    fp.close()

    custom_deps = \
        "\n      "+'"src/third_party/WebKit/LayoutTests": None,'+\
        "\n      "+'"src/chrome_frame/tools/test/reference_build/chrome": None,'+\
        "\n      "+'"src/chrome/tools/test/reference_build/chrome_mac": None,'+\
        "\n      "+'"src/chrome/tools/test/reference_build/chrome_win": None,'+\
        "\n      "+'"src/chrome/tools/test/reference_build/chrome_linux": None,'

    if not release_url is None:
      # TODO: Read the DEPS file and exclude all non-src directories.
      custom_deps += \
        "\n      "+'"chromeos": None,'+\
        "\n      "+'"depot_tools": None,'

    # populate "custom_deps" section
    data = data.replace('"custom_deps" : {', '"custom_deps" : {'+custom_deps)

    # write the new .gclient file
    fp = open(path, 'w')
    fp.write(data)
    fp.close()

if options.forceclean:
  if os.path.exists(chromium_src_dir):
    # revert all Chromium changes and delete all unversioned files
    run('gclient revert -n', chromium_dir, depot_tools_dir)

    if not options.dryrun:
      # remove the build output directories
      output_dirs = []
      if platform == 'windows':
        output_dirs.append(os.path.join(chromium_src_dir, 'build\\Debug'))
        output_dirs.append(os.path.join(chromium_src_dir, 'build\\Release'))
      elif platform == 'macosx':
        output_dirs.append(os.path.join(chromium_src_dir, 'xcodebuild'))
      elif platform == 'linux':
        output_dirs.append(os.path.join(chromium_src_dir, 'out'))

      if options.ninjabuild:
        output_dirs.append(os.path.join(chromium_src_dir, 'out'))

      for output_dir in output_dirs:
        if os.path.exists(output_dir):
          shutil.rmtree(output_dir, onerror=onerror)

  # force update, build and distrib steps
  options.forceupdate = True
  options.forcebuild = True
  options.forcedistrib = True

if release_url is None:
  if chromium_url_changed or chromium_rev_changed or options.forceupdate:
    # download/update the Chromium source code
    run('gclient sync --revision src@'+chromium_rev+' --jobs 8 --force', \
        chromium_dir, depot_tools_dir)
elif release_url_changed or options.forceupdate:
  # download/update the release source code
  run('gclient sync --jobs 8 --force', chromium_dir, depot_tools_dir)

if not os.path.exists(cef_src_dir) or cef_url_changed:
  if not options.dryrun and cef_url_changed and os.path.exists(cef_src_dir):
    # delete the cef directory (it will be re-downloaded)
    shutil.rmtree(cef_src_dir)

  # download the CEF source code
  run(svn_exe+' checkout '+cef_url+' -r '+cef_rev+' '+cef_src_dir, download_dir)
elif cef_rev_changed or options.forceupdate:
  # update the CEF source code
  run(svn_exe+' update -r '+cef_rev+' '+cef_src_dir, download_dir)

if any_changed or options.forceupdate:
  # create CEF projects
  if options.ninjabuild:
    os.environ['GYP_GENERATORS'] = 'ninja'
  if options.x64build:
    if 'GYP_DEFINES' in os.environ.keys():
      os.environ['GYP_DEFINES'] = os.environ['GYP_DEFINES'] + ' ' + 'target_arch=x64'
    else:
      os.environ['GYP_DEFINES'] = 'target_arch=x64'
  path = os.path.join(cef_src_dir, 'cef_create_projects'+script_ext)
  run(path, cef_src_dir, depot_tools_dir)

if any_changed or options.forcebuild:
  if options.ninjabuild:
    command = 'ninja -C '
    if options.verbose:
      command = 'ninja -v -C'
    target = ' cefclient'
    if options.buildtests:
      target = ' cefclient cef_unittests'
    build_dir_suffix = ''
    if platform == 'windows' and options.x64build:
      build_dir_suffix = '_x64'

    if not options.nodebugbuild:
      # make CEF Debug build
      run(command + os.path.join('out', 'Debug' + build_dir_suffix) + target, \
          chromium_src_dir, depot_tools_dir,
          os.path.join(chromium_src_dir, 'ninja-build-debug.log') if options.buildlogfile else None)

    if not options.noreleasebuild:
      # make CEF Release build
      run(command + os.path.join('out', 'Release' + build_dir_suffix) + target, \
          chromium_src_dir, depot_tools_dir,
          os.path.join(chromium_src_dir, 'ninja-build-release.log') if options.buildlogfile else None)
  else:
    path = os.path.join(cef_tools_dir, 'build_projects'+script_ext)

    if not options.nodebugbuild:
      # make CEF Debug build
      run(path+' Debug', cef_tools_dir, depot_tools_dir,
        os.path.join(chromium_src_dir, 'build-debug.log') if options.buildlogfile else None)

    if not options.noreleasebuild:
      # make CEF Release build
      run(path+' Release', cef_tools_dir, depot_tools_dir,
        os.path.join(chromium_src_dir, 'build-release.log') if options.buildlogfile else None)

if (any_changed or options.forcedistrib) and not options.nodistrib:
  if not options.forceclean and options.cleanartifacts:
    # clean the artifacts output directory
    artifacts_path = os.path.join(cef_src_dir, 'binary_distrib')
    if os.path.exists(artifacts_path):
      shutil.rmtree(artifacts_path, onerror=onerror)

  # determine the requested distribution types
  distrib_types = []
  if options.minimaldistribonly:
    distrib_types.append('minimal')
  elif options.clientdistribonly:
    distrib_types.append('client')
  else:
    distrib_types.append('standard')
    if options.minimaldistrib:
      distrib_types.append('minimal')
    if options.clientdistrib:
      distrib_types.append('client')

  first_type = True

  # create the requested distribution types
  for type in distrib_types:
    path = os.path.join(cef_tools_dir, 'make_distrib'+script_ext)
    if options.nodebugbuild or options.noreleasebuild or type != 'standard':
      path = path + ' --allow-partial'
    if options.ninjabuild:
      path = path + ' --ninja-build'
    if options.x64build:
      path = path + ' --x64-build'

    if type == 'minimal':
      path = path + ' --minimal'
    elif type == 'client':
      path = path + ' --client'

    if first_type:
      if options.nodistribdocs:
        path = path + ' --no-docs'
      if options.nodistribarchive:
        path = path + ' --no-archive'
      first_type = False
    else:
      # don't create the symbol archives or documentation more than once
      path = path + ' --no-symbols --no-docs'

    # create the distribution
    run(path, cef_tools_dir, depot_tools_dir)
