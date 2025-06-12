# Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from __future__ import print_function
from datetime import datetime
import hashlib
from io import open
from optparse import OptionParser
import os
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

if sys.version_info.major != 3:
  sys.stderr.write('Python3 is required!')
  sys.exit(1)

from urllib.request import FancyURLopener, urlopen

##
# Default URLs.
##

depot_tools_url = 'https://chromium.googlesource.com/chromium/tools/depot_tools.git'
depot_tools_archive_url = 'https://storage.googleapis.com/chrome-infra/depot_tools.zip'

cef_git_url = 'https://bitbucket.org/chromiumembedded/cef.git'

##
# Global system variables.
##

# Script directory.
script_dir = os.path.dirname(__file__)

##
# Helper functions.
##


def msg(message):
  """ Output a message. """
  sys.stdout.write('--> ' + message + "\n")


def run(command_line, working_dir, depot_tools_dir=None, output_file=None):
  """ Runs the specified command. """
  # add depot_tools to the path
  env = os.environ
  if not depot_tools_dir is None:
    env['PATH'] = depot_tools_dir + os.pathsep + env['PATH']

  sys.stdout.write('-------- Running "'+command_line+'" in "'+\
                   working_dir+'"...'+"\n")
  if not options.dryrun:
    args = shlex.split(command_line.replace('\\', '\\\\'))

    if not output_file:
      return subprocess.check_call(
          args, cwd=working_dir, env=env, shell=(sys.platform == 'win32'))
    try:
      msg('Writing %s' % output_file)
      with open(output_file, 'w', encoding='utf-8') as fp:
        return subprocess.check_call(
            args,
            cwd=working_dir,
            env=env,
            shell=(sys.platform == 'win32'),
            stderr=subprocess.STDOUT,
            stdout=fp)
    except subprocess.CalledProcessError:
      msg('ERROR Run failed. See %s for output.' % output_file)
      raise


def create_directory(path):
  """ Creates a directory if it doesn't already exist. """
  if not os.path.exists(path):
    msg("Creating directory %s" % (path))
    if not options.dryrun:
      os.makedirs(path)


def delete_directory(path):
  """ Removes an existing directory. """
  if os.path.exists(path):
    msg("Removing directory %s" % (path))
    if not options.dryrun:
      shutil.rmtree(path, onerror=onerror)


def copy_directory(source, target, allow_overwrite=False):
  """ Copies a directory from source to target. """
  if not options.dryrun and os.path.exists(target):
    if not allow_overwrite:
      raise Exception("Directory %s already exists" % (target))
    remove_directory(target)
  if os.path.exists(source):
    msg("Copying directory %s to %s" % (source, target))
    if not options.dryrun:
      shutil.copytree(source, target)


def move_directory(source, target, allow_overwrite=False):
  """ Copies a directory from source to target. """
  if not options.dryrun and os.path.exists(target):
    if not allow_overwrite:
      raise Exception("Directory %s already exists" % (target))
    remove_directory(target)
  if os.path.exists(source):
    msg("Moving directory %s to %s" % (source, target))
    if not options.dryrun:
      try:
        # This will fail if |source| and |target| are on different filesystems,
        # or if files in |source| are currently locked.
        os.rename(source, target)
      except OSError:
        msg('ERROR Failed to move directory %s to %s' % (source, target))
        raise


def is_git_checkout(path):
  """ Returns true if the path represents a git checkout. """
  return os.path.exists(os.path.join(path, '.git'))


def exec_cmd(cmd, path):
  """ Execute the specified command and return the result. """
  out = ''
  err = ''
  sys.stdout.write("-------- Running \"%s\" in \"%s\"...\n" % (cmd, path))
  parts = cmd.split()
  try:
    process = subprocess.Popen(
        parts,
        cwd=path,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        shell=(sys.platform == 'win32'))
    out, err = process.communicate()
  except IOError as e:
    (errno, strerror) = e.args
    raise
  except:
    raise
  return {'out': out.decode('utf-8'), 'err': err.decode('utf-8')}


def get_git_hash(path, branch):
  """ Returns the git hash for the specified branch/tag/hash. """
  cmd = "%s rev-parse %s" % (git_exe, branch)
  result = exec_cmd(cmd, path)
  if result['out'] != '':
    return result['out'].strip()
  return 'Unknown'


def get_git_url(path):
  """ Returns the origin url for the specified path. """
  cmd = "%s config --get remote.origin.url" % (git_exe)
  result = exec_cmd(cmd, path)
  if result['out'] != '':
    return result['out'].strip()
  return 'Unknown'


def is_valid_sha256(hash_string):
  """ Returns true if |value| is a valid hex-encoded SHA256 hash. """
  if len(hash_string) != 64:
    return False
  if not re.fullmatch(r'[0-9a-fA-F]+', hash_string):
    return False
  return True


def sha256_of_file(file_path):
  """ Returns the lower-case hex-encoded SHA256 hash for |file_path| contents. """
  sha256_hash = hashlib.sha256()
  with open(file_path, "rb") as f:
    for chunk in iter(lambda: f.read(4096), b""):
      sha256_hash.update(chunk)
  return sha256_hash.hexdigest()


def download_and_extract(src, target, strip_components=0, sha256_hash=''):
  """ Extracts the contents of src, which may be a URL or local file, to the
      target directory. """
  temporary = False

  extension = None
  for ext in ('.zip', '.tar.bz2', '.tar.xz'):
    if src.lower().endswith(ext):
      extension = ext
      break
  if extension is None:
    raise Exception('Unsupported file extension for: ' + src)

  if src[:4] == 'http':
    # Attempt to download a URL.
    msg('Downloading %s' % src)
    opener = FancyURLopener({})
    response = opener.open(src)

    temporary = True
    handle, archive_path = tempfile.mkstemp(suffix=extension)
    os.write(handle, response.read())
    os.close(handle)
  elif os.path.exists(src):
    # Use a local file.
    archive_path = src
  else:
    raise Exception('Path type is unsupported or does not exist: ' + src)

  if len(sha256_hash) == 64:
    hash = sha256_of_file(archive_path)
    if hash != sha256_hash.lower():
      raise Exception('SHA256 hash check failed for: ' + archive_path)
    msg('SHA56 hash %s verified for %s' % (sha256_hash, archive_path))

  msg('Extracting ' + archive_path)

  if extension == '.zip':
    if not zipfile.is_zipfile(archive_path):
      raise Exception('Not a valid zip archive: ' + archive_path)

    # Attempt to extract the archive file.
    try:
      os.makedirs(target)
      zf = zipfile.ZipFile(archive_path, 'r')
      zf.extractall(target)
    except:
      shutil.rmtree(target, onerror=onerror)
      raise
    zf.close()
  elif extension.startswith('.tar'):
    if sys.platform == 'win32' and sys.getwindowsversion().build < 22000:
      # Windows versions prior to Windows 11 do not have a reliable tar command.
      # Use the builtin Python tar support instead.
      assert strip_components <= 1, strip_components

      def get_first_directory(path):
        """ Returns the full path of the first directory found within |path|. """
        for entry in os.scandir(path):
          if entry.is_dir():
            return os.path.join(path, entry.name)
        return None

      mode = ''
      if extension.endswith('.bz2'):
        mode = 'bz2'
      elif extension.endswith('.xz'):
        mode = 'xz'
      # Use a temp directory on the same filesystem to support atomic move.
      tmp_path = target + '.tmp'
      try:
        with tarfile.open(archive_path, 'r:' + mode) as tar:
          if strip_components == 0:
            # Extract directly to the target directory.
            os.makedirs(target)
            tar.extractall(path=target)
          elif strip_components == 1:
            # Extract to a temp directory and then move the first sub-folder
            # to the target directory.
            tar.extractall(path=tmp_path)
            tmp_subdir_path = get_first_directory(tmp_path)
            if not tmp_subdir_path is None:
              shutil.move(tmp_subdir_path, target)
      except:
        shutil.rmtree(target, onerror=onerror)
        raise
      if os.path.isdir(tmp_path):
        shutil.rmtree(tmp_path, onerror=onerror)
    else:
      mode = ''
      if extension.endswith('.bz2'):
        mode = 'j'
      elif extension.endswith('.xz'):
        mode = 'J'
      # TODO: Add support for utilizing multiple cores.
      cmd = 'tar x%sf "%s" -C "%s"' % (mode, archive_path, target)
      if strip_components > 0:
        cmd += ' --strip-components=%d' % strip_components
      try:
        os.makedirs(target)
        run(cmd, target)
      except:
        shutil.rmtree(target, onerror=onerror)
        raise

  # Delete the archive file if temporary.
  if temporary and os.path.exists(archive_path):
    os.remove(archive_path)


def read_file(path):
  """ Read a file. """
  if os.path.exists(path):
    with open(path, 'r', encoding='utf-8') as fp:
      return fp.read()
  else:
    raise Exception("Path does not exist: %s" % (path))


def write_file(path, data):
  """ Write a file. """
  msg('Writing %s' % path)
  if not options.dryrun:
    with open(path, 'w', encoding='utf-8') as fp:
      fp.write(data)


def read_config_file(path):
  """ Read a configuration file. """
  # Parse the contents.
  return eval(read_file(path), {'__builtins__': None}, None)


def write_config_file(path, contents):
  """ Write a configuration file. """
  data = "{\n"
  for key in sorted(contents.keys()):
    data += "  '%s': '%s',\n" % (key, contents[key])
  data += "}\n"
  write_file(path, data)


def read_branch_config_file(path):
  """ Read the CEF branch from the specified path. """
  config_file = os.path.join(path, 'cef.branch')
  if os.path.isfile(config_file):
    contents = read_config_file(config_file)
    if 'branch' in contents:
      return contents['branch']
  return ''


def write_branch_config_file(path, branch):
  """ Write the CEF branch to the specified path. """
  config_file = os.path.join(path, 'cef.branch')
  if not os.path.isfile(config_file):
    write_config_file(config_file, {'branch': branch})


def read_version_file(path):
  """ Read and parse a version file (key=value pairs, one per line). """
  contents = read_file(path)
  if contents is None:
    return None
  result = {}
  lines = contents.split("\n")
  for line in lines:
    parts = line.split('=', 1)
    if len(parts) == 2:
      result[parts[0]] = parts[1]
  return result


def read_chrome_version_file(src_path):
  path = os.path.join(src_path, 'chrome', 'VERSION')
  parts = read_version_file(path)
  if parts is None:
    raise Exception('Failed to read %s' % path)
  return '%s.%s.%s.%s' % (parts['MAJOR'], parts['MINOR'], parts['BUILD'],
                          parts['PATCH'])


def apply_patch(name, patch_dir=None, required=False):
  patch_file = os.path.join(cef_dir, 'patch', 'patches', name)
  if patch_dir is None:
    patch_dir = chromium_src_dir
  patch_path = patch_file + ".patch"
  if os.path.exists(patch_path):
    # Attempt to apply the patch file.
    patch_tool = os.path.join(cef_dir, 'tools', 'patcher.py')
    run('%s %s --patch-file "%s" --patch-dir "%s"' % (python_exe, patch_tool,
                                                      patch_file, patch_dir),
        chromium_src_dir, depot_tools_dir)
  elif required:
    raise Exception('Required patch file does not exist: ' + patch_path)


def apply_deps_patch():
  """ Patch the Chromium DEPS file before `gclient sync` if necessary. """
  deps_path = os.path.join(chromium_src_dir, deps_file)
  if os.path.isfile(deps_path):
    msg("Chromium DEPS file: %s" % (deps_path))
    apply_patch(deps_file)
  else:
    raise Exception("Path does not exist: %s" % (deps_path))


def apply_runhooks_patch():
  """ Patch the Chromium runhooks files before `gclient runhooks` if necessary. """
  apply_patch('runhooks')


def run_patch_updater(args='', output_file=None):
  """ Run the patch updater script. """
  tool = os.path.join(cef_src_dir, 'tools', 'patch_updater.py')
  if len(args) > 0:
    args = ' ' + args
  run('%s %s%s' % (python_exe, tool, args), cef_src_dir, depot_tools_dir,
      output_file)


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


def get_chromium_main_position(commit):
  """ Returns the closest main position for the specified Chromium commit. """
  # Using -2 because a "Publish DEPS" commit which does not have a master
  # position may be first.
  cmd = "%s log -2 %s" % (git_exe, commit)
  result = exec_cmd(cmd, chromium_src_dir)
  if result['out'] != '':
    match = re.search(r'refs/heads/(?:master|main)@{#([\d]+)}', result['out'])
    assert match != None, 'Failed to find position'
    return int(match.groups()[0])
  return None


def get_chromium_main_commit(position):
  """ Returns the main commit for the specified Chromium commit position. """
  cmd = '%s log -1 --grep=refs/heads/master@{#%s} --grep=refs/heads/main@{#%s} origin/main' % (
      git_exe, str(position), str(position))
  result = exec_cmd(cmd, chromium_src_dir)
  if result['out'] != '':
    match = re.search(r'^commit ([a-f0-9]+)', result['out'])
    assert match != None, 'Failed to find commit'
    return match.groups()[0]
  return None


def get_build_compat_versions():
  """ Returns the compatible Chromium and (optionally) depot_tools versions
      specified by the CEF checkout. """
  compat_path = os.path.join(cef_dir, 'CHROMIUM_BUILD_COMPATIBILITY.txt')
  msg("Reading %s" % compat_path)
  config = read_config_file(compat_path)

  if not 'chromium_checkout' in config:
    raise Exception("Missing chromium_checkout value in %s" % (compat_path))
  return config


def get_build_directory_name(is_debug):
  build_dir = ('Debug' if is_debug else 'Release') + '_'

  # CEF uses a consistent directory naming scheme for GN via
  # GetAllPlatformConfigs in tools/gn_args.py.
  if options.x64build:
    build_dir += 'GN_x64'
  elif options.armbuild:
    build_dir += 'GN_arm'
  elif options.arm64build:
    build_dir += 'GN_arm64'
  else:
    build_dir += 'GN_x86'
  return build_dir


def read_update_file():
  update_path = os.path.join(cef_src_dir, 'CHROMIUM_UPDATE.txt')
  if not os.path.exists(update_path):
    msg("Missing file: %s" % update_path)
    return None

  msg("Reading %s" % update_path)
  return read_config_file(update_path)


def log_chromium_changes():
  """ Evaluate the Chromium checkout for changes. """
  config = read_update_file()
  if config is None:
    msg("Skipping Chromium changes log.")
    return

  if 'files' in config:
    out_file = os.path.join(download_dir, 'chromium_update_changes.diff')
    if os.path.exists(out_file):
      os.remove(out_file)

    old_commit = get_chromium_main_commit(
        get_chromium_main_position(chromium_compat_version))
    new_commit = get_chromium_main_commit(
        get_chromium_main_position(chromium_checkout))

    cmd = '%s diff --relative --no-prefix %s..%s -- %s' % (
        git_exe, old_commit, new_commit, ' '.join(config['files']))
    result = exec_cmd(cmd, chromium_src_dir)
    if result['out'] != '':
      write_file(out_file, result['out'])


def check_pattern_matches(output_file=None):
  """ Evaluate the Chromium checkout for pattern matches. """
  config = read_update_file()
  if config is None:
    msg("Skipping Chromium pattern matching.")
    return

  if 'patterns' in config:
    if output_file is None:
      fp = sys.stdout
    else:
      msg('Writing %s' % output_file)
      fp = open(output_file, 'w', encoding='utf-8')

    has_output = False
    for entry in config['patterns']:
      msg("Evaluating pattern: %s" % entry['pattern'])

      # Read patterns from a file to avoid formatting problems.
      pattern_handle, pattern_file = tempfile.mkstemp()
      os.write(pattern_handle, entry['pattern'])
      os.close(pattern_handle)

      cmd = '%s grep -n -f %s' % (git_exe, pattern_file)
      result = exec_cmd(cmd, chromium_src_dir)
      os.remove(pattern_file)

      if result['out'] != '':
        write_msg = True
        re_exclude = re.compile(
            entry['exclude_matches']) if 'exclude_matches' in entry else None

        for line in result['out'].split('\n'):
          line = line.strip()
          if len(line) == 0:
            continue
          skip = not re_exclude is None and re_exclude.match(line) != None
          if not skip:
            if write_msg:
              if has_output:
                fp.write('\n')
              fp.write('!!!! WARNING: FOUND PATTERN: %s\n' % entry['pattern'])
              if 'message' in entry:
                fp.write(entry['message'] + '\n')
              fp.write('\n')
              write_msg = False
            fp.write(line + '\n')
            has_output = True

    if not output_file is None:
      if has_output:
        msg('ERROR Matches found. See %s for output.' % out_file)
      else:
        fp.write('Good news! No matches.\n')
      fp.close()

    if has_output:
      # Don't continue when we know the build will be wrong.
      sys.exit(1)


def invalid_options_combination(a, b):
  print("Invalid combination of options: '%s' and '%s'" % (a, b))
  parser.print_help(sys.stderr)
  sys.exit(1)


##
# Program entry point.
##

# Cannot be loaded as a module.
if __name__ != "__main__":
  sys.stderr.write('This file cannot be loaded as a module!')
  sys.exit(1)

# Parse command-line options.
disc = """
This utility implements automation for the download, update, build and
distribution of CEF.
"""

parser = OptionParser(description=disc)

# Setup options.
parser.add_option(
    '--download-dir',
    dest='downloaddir',
    metavar='DIR',
    help='Download directory with no spaces [required].')
parser.add_option(
    '--depot-tools-dir',
    dest='depottoolsdir',
    metavar='DIR',
    help='Download directory for depot_tools.',
    default='')
parser.add_option(
    '--depot-tools-archive',
    dest='depottoolsarchive',
    help='Archive file that contains a single top-level depot_tools directory.',
    default='')
parser.add_option(
    '--depot-tools-archive-sha256',
    dest='depottoolsarchivesha256',
    help='SHA256 hex-encoded hash for the file passed to --depot-tools-archive.',
    default='')
parser.add_option('--branch', dest='branch',
                  help='Branch of CEF to build (master, 3987, ...). This '+\
                       'will be used to name the CEF download directory and '+\
                       'to identify the correct URL if --url is not '+\
                       'specified. The default value is master.',
                  default='master')
parser.add_option('--url', dest='url',
                  help='CEF download URL. If not specified the default URL '+\
                       'will be used.',
                  default='')
parser.add_option('--chromium-url', dest='chromiumurl',
                  help='Chromium download URL. If not specified the default '+\
                       'URL will be used.',
                  default='')
parser.add_option('--checkout', dest='checkout',
                  help='Version of CEF to checkout. If not specified the '+\
                       'most recent remote version of the branch will be used.',
                  default='')
parser.add_option('--chromium-checkout', dest='chromiumcheckout',
                  help='Version of Chromium to checkout (Git '+\
                       'branch/hash/tag). This overrides the value specified '+\
                       'by CEF in CHROMIUM_BUILD_COMPATIBILITY.txt.',
                  default='')
parser.add_option(
    '--no-chromium-history',
    action='store_true',
    dest='nochromiumhistory',
    default=False,
    help='Checkout Chromium without history.')
parser.add_option(
    '--chromium-archive',
    dest='chromiumarchive',
    help='Archive file that contains a single top-level chromium src directory.',
    default='')
parser.add_option(
    '--chromium-archive-sha256',
    dest='chromiumarchivesha256',
    help='SHA256 hex-encoded hash for the file passed to --chromium-archive.',
    default='')

# Miscellaneous options.
parser.add_option(
    '--force-config',
    action='store_true',
    dest='forceconfig',
    default=False,
    help='Force creation of a new gclient config file.')
parser.add_option('--force-clean',
                  action='store_true', dest='forceclean', default=False,
                  help='Force a clean checkout of Chromium and CEF. This will'+\
                       ' trigger a new update, build and distribution.')
parser.add_option('--force-clean-deps',
                  action='store_true', dest='forcecleandeps', default=False,
                  help='Force a clean checkout of Chromium dependencies. Used'+\
                       ' in combination with --force-clean.')
parser.add_option(
    '--dry-run',
    action='store_true',
    dest='dryrun',
    default=False,
    help="Output commands without executing them.")
parser.add_option('--dry-run-platform', dest='dryrunplatform', default=None,
                  help='Simulate a dry run on the specified platform '+\
                       '(windows, mac, linux). Must be used in combination'+\
                       ' with the --dry-run flag.')

# Update-related options.
parser.add_option('--force-update',
                  action='store_true', dest='forceupdate', default=False,
                  help='Force a Chromium and CEF update. This will trigger a '+\
                       'new build and distribution.')
parser.add_option('--no-update',
                  action='store_true', dest='noupdate', default=False,
                  help='Do not update Chromium or CEF. Pass --force-build or '+\
                       '--force-distrib if you desire a new build or '+\
                       'distribution.')
parser.add_option('--no-cef-update',
                  action='store_true', dest='nocefupdate', default=False,
                  help='Do not update CEF. Pass --force-build or '+\
                       '--force-distrib if you desire a new build or '+\
                       'distribution.')
parser.add_option('--force-cef-update',
                  action='store_true', dest='forcecefupdate', default=False,
                  help='Force a CEF update. This will cause local changes in '+\
                       'the CEF checkout to be discarded and patch files to '+\
                       'be reapplied.')
parser.add_option(
    '--no-chromium-update',
    action='store_true',
    dest='nochromiumupdate',
    default=False,
    help='Do not update Chromium.')
parser.add_option(
    '--no-depot-tools-update',
    action='store_true',
    dest='nodepottoolsupdate',
    default=False,
    help='Do not update depot_tools.')
parser.add_option('--fast-update',
                  action='store_true', dest='fastupdate', default=False,
                  help='Update existing Chromium/CEF checkouts for fast incremental '+\
                       'builds by attempting to minimize the number of modified files. '+\
                       'The update will fail if there are unstaged CEF changes or if '+\
                       'Chromium changes are not included in a patch file.')
parser.add_option(
    '--force-patch-update',
    action='store_true',
    dest='forcepatchupdate',
    default=False,
    help='Force update of patch files.')
parser.add_option(
    '--resave',
    action='store_true',
    dest='resave',
    default=False,
    help='Resave patch files.')
parser.add_option(
    '--log-chromium-changes',
    action='store_true',
    dest='logchromiumchanges',
    default=False,
    help='Create a log of the Chromium changes.')

# Build-related options.
parser.add_option('--force-build',
                  action='store_true', dest='forcebuild', default=False,
                  help='Force CEF debug and release builds. This builds '+\
                       '[build-target] on all platforms and chrome_sandbox '+\
                       'on Linux.')
parser.add_option(
    '--no-build',
    action='store_true',
    dest='nobuild',
    default=False,
    help='Do not build CEF.')
parser.add_option(
    '--build-target',
    dest='buildtarget',
    default='cefclient',
    help='Target name(s) to build (defaults to "cefclient").')
parser.add_option(
    '--build-tests',
    action='store_true',
    dest='buildtests',
    default=False,
    help='Also build the test target specified via --test-target.')
parser.add_option(
    '--no-debug-build',
    action='store_true',
    dest='nodebugbuild',
    default=False,
    help="Don't perform the CEF debug build.")
parser.add_option(
    '--no-release-build',
    action='store_true',
    dest='noreleasebuild',
    default=False,
    help="Don't perform the CEF release build.")
parser.add_option(
    '--verbose-build',
    action='store_true',
    dest='verbosebuild',
    default=False,
    help='Show all command lines while building.')
parser.add_option(
    '--build-failure-limit',
    dest='buildfailurelimit',
    default=1,
    type="int",
    help='Keep going until N jobs fail.')
parser.add_option('--build-log-file',
                  action='store_true', dest='buildlogfile', default=False,
                  help='Write build logs to file. The file will be named '+\
                       '"build-[branch]-[debug|release].log" in the download '+\
                       'directory.')
parser.add_option(
    '--x64-build',
    action='store_true',
    dest='x64build',
    default=False,
    help='Create a 64-bit build.')
parser.add_option(
    '--arm-build',
    action='store_true',
    dest='armbuild',
    default=False,
    help='Create an ARM build.')
parser.add_option(
    '--arm64-build',
    action='store_true',
    dest='arm64build',
    default=False,
    help='Create an ARM64 build.')
parser.add_option(
    '--with-pgo-profiles',
    action='store_true',
    dest='withpgoprofiles',
    default=False,
    help='Download PGO profiles for the build.')

# Test-related options.
parser.add_option(
    '--run-tests',
    action='store_true',
    dest='runtests',
    default=False,
    help='Run the ceftests target.')
parser.add_option(
    '--no-debug-tests',
    action='store_true',
    dest='nodebugtests',
    default=False,
    help="Don't run debug build tests.")
parser.add_option(
    '--no-release-tests',
    action='store_true',
    dest='noreleasetests',
    default=False,
    help="Don't run release build tests.")
parser.add_option(
    '--test-target',
    dest='testtarget',
    default='ceftests',
    help='Test target name to build (defaults to "ceftests").')
parser.add_option(
    '--test-prefix',
    dest='testprefix',
    default='',
    help='Prefix for running the test executable (e.g. `xvfb-run` on Linux).')
parser.add_option(
    '--test-args',
    dest='testargs',
    default='',
    help='Arguments that will be passed to the test executable.')

# Distribution-related options.
parser.add_option(
    '--force-distrib',
    action='store_true',
    dest='forcedistrib',
    default=False,
    help='Force creation of a CEF binary distribution.')
parser.add_option(
    '--no-distrib',
    action='store_true',
    dest='nodistrib',
    default=False,
    help="Don't create a CEF binary distribution.")
parser.add_option(
    '--minimal-distrib',
    action='store_true',
    dest='minimaldistrib',
    default=False,
    help='Create a minimal CEF binary distribution.')
parser.add_option(
    '--minimal-distrib-only',
    action='store_true',
    dest='minimaldistribonly',
    default=False,
    help='Create a minimal CEF binary distribution only.')
parser.add_option(
    '--client-distrib',
    action='store_true',
    dest='clientdistrib',
    default=False,
    help='Create a client CEF binary distribution.')
parser.add_option(
    '--client-distrib-only',
    action='store_true',
    dest='clientdistribonly',
    default=False,
    help='Create a client CEF binary distribution only.')
parser.add_option(
    '--sandbox-distrib',
    action='store_true',
    dest='sandboxdistrib',
    default=False,
    help='Create a sandbox distribution.')
parser.add_option(
    '--sandbox-distrib-only',
    action='store_true',
    dest='sandboxdistribonly',
    default=False,
    help='Create a sandbox distribution only.')
parser.add_option(
    '--tools-distrib',
    action='store_true',
    dest='toolsdistrib',
    default=False,
    help='Create a tools distribution.')
parser.add_option(
    '--tools-distrib-only',
    action='store_true',
    dest='toolsdistribonly',
    default=False,
    help='Create a tools distribution only.')
parser.add_option(
    '--no-distrib-docs',
    action='store_true',
    dest='nodistribdocs',
    default=False,
    help="Don't create CEF documentation.")
parser.add_option(
    '--no-distrib-archive',
    action='store_true',
    dest='nodistribarchive',
    default=False,
    help="Don't create archives for output directories.")
parser.add_option(
    '--clean-artifacts',
    action='store_true',
    dest='cleanartifacts',
    default=False,
    help='Clean the artifacts output directory.')
parser.add_option(
    '--distrib-subdir',
    dest='distribsubdir',
    default='',
    help='CEF distrib dir name, child of chromium/src/cef/binary_distrib')
parser.add_option(
    '--distrib-subdir-suffix',
    dest='distribsubdirsuffix',
    default='',
    help='CEF distrib dir name suffix, child of chromium/src/cef/binary_distrib'
)

(options, args) = parser.parse_args()

if options.downloaddir is None:
  print("The --download-dir option is required.")
  parser.print_help(sys.stderr)
  sys.exit(1)

# Opt into component-specific flags for later use.
if options.noupdate:
  options.nocefupdate = True
  options.nochromiumupdate = True
  options.nodepottoolsupdate = True

if options.chromiumarchive != '':
  options.nochromiumhistory = True

if options.depottoolsarchivesha256 != '' and \
   not is_valid_sha256(options.depottoolsarchivesha256):
  print('Invalid --depot-tools-archive-sha256 value.')
  parser.print_help(sys.stderr)
  sys.exit(1)

if options.chromiumarchivesha256 != '' and \
   not is_valid_sha256(options.chromiumarchivesha256):
  print('Invalid --chromium-archive-sha256 value.')
  parser.print_help(sys.stderr)
  sys.exit(1)

if options.runtests:
  options.buildtests = True

if (options.nochromiumupdate and options.forceupdate):
  invalid_options_combination('--no-chromium-update', '--force-update')
if (options.nocefupdate and options.forceupdate):
  invalid_options_combination('--no-cef-update', '--force-update')
if (options.nobuild and options.forcebuild):
  invalid_options_combination('--no-build', '--force-build')
if (options.nodistrib and options.forcedistrib):
  invalid_options_combination('--no-distrib', '--force-distrib')
if (options.forceclean and options.fastupdate):
  invalid_options_combination('--force-clean', '--fast-update')
if (options.forcecleandeps and options.fastupdate):
  invalid_options_combination('--force-clean-deps', '--fast-update')
if (options.nochromiumhistory and options.logchromiumchanges):
  invalid_options_combination('--no-chromium-history', '--log-chromium-changes')

if (options.noreleasebuild and \
     (options.minimaldistrib or options.minimaldistribonly or \
      options.clientdistrib or options.clientdistribonly)) or \
   (options.minimaldistribonly + options.clientdistribonly + options.sandboxdistribonly + options.toolsdistribonly > 1):
  print('Invalid combination of options.')
  parser.print_help(sys.stderr)
  sys.exit(1)

if options.x64build + options.armbuild + options.arm64build > 1:
  print('Invalid combination of options.')
  parser.print_help(sys.stderr)
  sys.exit(1)

if (options.buildtests or options.runtests) and len(options.testtarget) == 0:
  print("A test target must be specified via --test-target.")
  parser.print_help(sys.stderr)
  sys.exit(1)

# Operating system.
if options.dryrun and options.dryrunplatform is not None:
  platform = options.dryrunplatform
  if not platform in ['windows', 'mac', 'linux']:
    print('Invalid dry-run-platform value: %s' % (platform))
    sys.exit(1)
elif sys.platform == 'win32':
  platform = 'windows'
elif sys.platform == 'darwin':
  platform = 'mac'
elif sys.platform.startswith('linux'):
  platform = 'linux'
else:
  print('Unknown operating system platform')
  sys.exit(1)

if options.clientdistrib or options.clientdistribonly:
  if platform == 'linux' or (platform == 'windows' and options.arm64build):
    client_app = 'cefsimple'
  else:
    client_app = 'cefclient'
  if options.buildtarget.find(client_app) == -1:
    print('A client distribution cannot be generated if --build-target ' +
          'excludes %s.' % client_app)
    parser.print_help(sys.stderr)
    sys.exit(1)

# CEF branch.
cef_branch = options.branch

branch_is_master = (cef_branch == 'master' or cef_branch == 'trunk')
if not branch_is_master:
  # Verify that the branch value is numeric.
  if not cef_branch.isdigit():
    print('Invalid branch value: %s' % cef_branch)
    sys.exit(1)

  # Verify the minimum supported branch number.
  if int(cef_branch) < 5060:
    print('The requested branch (%s) is too old to build using this tool. ' +
          'The minimum supported branch is 5060.' % cef_branch)
    sys.exit(1)

# True if the requested branch is 7151 or older.
branch_is_7151_or_older = not branch_is_master and int(cef_branch) <= 7151

if options.armbuild:
  if platform != 'linux':
    print('The ARM build option is only supported on Linux.')
    sys.exit(1)

deps_file = 'DEPS'

if platform == 'mac' and not (options.x64build or options.arm64build):
  print('32-bit MacOS builds are not supported. ' +
        'Add --x64-build or --arm64-build flag to generate a 64-bit build.')
  sys.exit(1)

# Platforms that build a cef_sandbox static library in a separate output directory.
sandbox_static_platforms = []

# Platforms that build a cef_sandbox shared library in the same output directory.
sandbox_shared_platforms = []

# Platforms that build a bootstrap executable in the same output directory.
bootstrap_exe_platforms = []

if branch_is_7151_or_older:
  sandbox_static_platforms.extend(['windows', 'mac'])
else:
  bootstrap_exe_platforms.append('windows')
  sandbox_shared_platforms.append('mac')

if not platform in sandbox_static_platforms and \
   not platform in sandbox_shared_platforms and \
   not platform in bootstrap_exe_platforms and \
   (options.sandboxdistrib or options.sandboxdistribonly):
  print('The sandbox distribution is not supported on this platform.')
  sys.exit(1)

# Options that force the sources to change.
force_change = options.forceclean or options.forceupdate

# Options that cause local changes to be discarded.
discard_local_changes = force_change or options.forcecefupdate

if options.resave and (options.forcepatchupdate or discard_local_changes):
  print('--resave cannot be combined with options that modify or discard ' +
        'patches.')
  parser.print_help(sys.stderr)
  sys.exit(1)

if platform == 'windows':
  # Avoid errors when the "vs_toolchain.py update" Chromium hook runs.
  os.environ['DEPOT_TOOLS_WIN_TOOLCHAIN'] = '0'

download_dir = os.path.abspath(options.downloaddir)
chromium_dir = os.path.join(download_dir, 'chromium')
chromium_src_dir = os.path.join(chromium_dir, 'src')
out_src_dir = os.path.join(chromium_src_dir, 'out')
cef_src_dir = os.path.join(chromium_src_dir, 'cef')

if options.fastupdate and os.path.exists(cef_src_dir):
  cef_dir = cef_src_dir
else:
  cef_dir = os.path.join(download_dir, 'cef')

##
# Manage the download directory.
##

# Create the download directory if necessary.
create_directory(download_dir)

msg("Download Directory: %s" % (download_dir))

##
# Manage the depot_tools directory.
##

# Check if the depot_tools directory exists.
if options.depottoolsdir != '':
  depot_tools_dir = os.path.abspath(options.depottoolsdir)
else:
  depot_tools_dir = os.path.join(download_dir, 'depot_tools')

msg("Depot Tools Directory: %s" % (depot_tools_dir))

if not os.path.exists(depot_tools_dir):
  if platform == 'windows' and options.depottoolsarchive == '':
    # On Windows download depot_tools as an archive file since we can't assume
    # that git is already installed.
    options.depottoolsarchive = depot_tools_archive_url

  if options.depottoolsarchive != '':
    # Extract depot_tools from an archive file.
    msg('Extracting %s to %s' % (options.depottoolsarchive, depot_tools_dir))
    if not options.dryrun:
      download_and_extract(
          options.depottoolsarchive,
          depot_tools_dir,
          sha256_hash=options.depottoolsarchivesha256)
  else:
    # On Linux and OS X check out depot_tools using Git.
    run('git clone ' + depot_tools_url + ' ' + depot_tools_dir, download_dir)

if not options.nodepottoolsupdate:
  # Update depot_tools.
  # On Windows this will download required python and git binaries.
  msg('Updating depot_tools')
  if platform == 'windows':
    run('update_depot_tools.bat', depot_tools_dir, depot_tools_dir)
  else:
    run('update_depot_tools', depot_tools_dir, depot_tools_dir)

# Determine the executables to use.
if platform == 'windows':
  # Force use of the system installed Git version.
  git_exe = 'git.exe'
  # Force use of the Python version bundled with depot_tools.
  python_bat = 'python3.bat'
  python_exe = os.path.join(depot_tools_dir, python_bat)
  if options.dryrun and not os.path.exists(python_exe):
    sys.stdout.write("WARNING: --dry-run assumes that depot_tools" \
                     " is already in your PATH. If it isn't\nplease" \
                     " specify a --depot-tools-dir value.\n")
    python_exe = python_bat
else:
  git_exe = 'git'
  python_exe = sys.executable

##
# Manage the cef directory.
##

# Delete the existing CEF directory if requested.
if options.forceclean and os.path.exists(cef_dir):
  delete_directory(cef_dir)

# Determine the type of CEF checkout to use.
if os.path.exists(cef_dir) and not is_git_checkout(cef_dir):
  raise Exception("Not a valid CEF Git checkout: %s" % (cef_dir))

# Determine the CEF download URL to use.
cef_url = options.url.strip()
if cef_url == '':
  cef_url = cef_git_url

# Verify that the requested CEF URL matches the existing checkout.
if not options.nocefupdate and os.path.exists(cef_dir):
  cef_existing_url = get_git_url(cef_dir)
  if cef_url != cef_existing_url:
    raise Exception(
        'Requested CEF checkout URL %s does not match existing URL %s' %
        (cef_url, cef_existing_url))

msg("CEF Branch: %s" % (cef_branch))
msg("CEF URL: %s" % (cef_url))
msg("CEF Source Directory: %s" % (cef_dir))

# Determine the CEF Git branch to use.
if options.checkout == '':
  # Target the most recent branch commit from the remote repo.
  if branch_is_master:
    cef_checkout = 'origin/master'
  else:
    cef_checkout = 'origin/' + cef_branch
else:
  cef_checkout = options.checkout

# Create the CEF checkout if necessary.
if not options.nocefupdate and not os.path.exists(cef_dir):
  cef_checkout_new = True
  run('%s clone %s %s' % (git_exe, cef_url, cef_dir), download_dir,
      depot_tools_dir)
else:
  cef_checkout_new = False

# Determine if the CEF checkout needs to change.
if not options.nocefupdate and os.path.exists(cef_dir):
  cef_current_hash = get_git_hash(cef_dir, 'HEAD')

  if not cef_checkout_new:
    # Fetch updated sources.
    run('%s fetch' % (git_exe), cef_dir, depot_tools_dir)

  cef_desired_hash = get_git_hash(cef_dir, cef_checkout)
  cef_checkout_changed = cef_checkout_new or force_change or \
                         options.forcecefupdate or \
                         cef_current_hash != cef_desired_hash

  msg("CEF Current Checkout: %s" % (cef_current_hash))
  msg("CEF Desired Checkout: %s (%s)" % (cef_desired_hash, cef_checkout))

  if cef_checkout_changed:
    if cef_dir == cef_src_dir:
      # Running in fast update mode. Backup and revert the patched files before
      # changing the CEF checkout.
      run_patch_updater("--backup --revert")

    # Update the CEF checkout.
    run('%s checkout %s%s' %
      (git_exe, '--force ' if discard_local_changes else '', cef_checkout), \
      cef_dir, depot_tools_dir)
else:
  cef_checkout_changed = False

build_compat_versions = get_build_compat_versions()

# Determine the Chromium checkout options required by CEF.
chromium_compat_version = build_compat_versions['chromium_checkout']
if len(options.chromiumcheckout) > 0:
  chromium_checkout = options.chromiumcheckout
else:
  chromium_checkout = chromium_compat_version

if not options.nodepottoolsupdate and \
    'depot_tools_checkout' in build_compat_versions:
  # Update the depot_tools checkout.
  depot_tools_compat_version = build_compat_versions['depot_tools_checkout']
  run('%s checkout %s%s' %
      (git_exe, '--force ' if discard_local_changes else '', depot_tools_compat_version), \
      depot_tools_dir, depot_tools_dir)

# Disable further depot_tools updates.
os.environ['DEPOT_TOOLS_UPDATE'] = '0'

##
# Manage the out directory.
##

out_dir = os.path.join(download_dir, 'out_' + cef_branch)

# Delete the existing out directory if requested.
if options.forceclean and os.path.exists(out_dir):
  delete_directory(out_dir)

msg("CEF Output Directory: %s" % (out_dir))

##
# Manage the chromium directory.
##

# Create the chromium directory if necessary.
create_directory(chromium_dir)

gclient_file = os.path.join(chromium_dir, '.gclient')
force_config = not os.path.exists(gclient_file) or options.forceconfig

# Extract <version> from a value like 'refs/tags/<version>'.
chromium_version = chromium_checkout.split('/')[2]

if options.nochromiumhistory and os.path.exists(chromium_src_dir):
  # When using no history the Chromium checkout must be correct, or it must be deleted/replaced.
  current_version = read_chrome_version_file(chromium_src_dir)
  if current_version != chromium_version:
    error = ''
    if options.nochromiumupdate:
      error += ' Remove --no-chromium-update.'
    if not force_change:
      error += ' Add --force-clean or --force-update.'
    if len(error) > 0:
      raise Exception(
          'Current Chromium checkout with --no-chromium-history is incorrect.' +
          error)

    remove_directory(chromium_src_dir)
    force_config = True

if options.chromiumurl != '':
  chromium_url = options.chromiumurl
else:
  chromium_url = 'https://chromium.googlesource.com/chromium/src.git'

if options.nochromiumhistory:
  chromium_url += '@' + chromium_version

# Create gclient configuration file.
if force_config:
  # yapf: disable
  gclient_spec = (
      "solutions = [{"+
        "'managed': False, "+
        "'name': 'src', "+
        "'url': '" + chromium_url + "', "+
        "'custom_vars': {"+
          "'checkout_pgo_profiles': " + ('True' if options.withpgoprofiles else 'False') + ", "+
          "'source_tarball': " + ('True' if options.chromiumarchive != '' else 'False') + ", "+
        "}, "+
        "'custom_deps': {}, "+
        "'deps_file': '" + deps_file + "', "+
        "'safesync_url': ''"+
      "}]"
  )
  # yapf: disable

  msg('Writing %s' % gclient_file)
  if not options.dryrun:
    with open(gclient_file, 'w', encoding='utf-8') as fp:
      fp.write(gclient_spec)

# Initial Chromium checkout.
if not options.nochromiumupdate and not os.path.exists(chromium_src_dir):
  chromium_checkout_new = True
  sync_args = '--nohooks '

  if options.chromiumarchive != '':
    msg('Extracting %s to %s' % (options.chromiumarchive, chromium_src_dir))
    if not options.dryrun:
      # Extract without the top-level directory, which has the same name as the archive file.
      download_and_extract(options.chromiumarchive, chromium_src_dir, strip_components=1,
                           sha256_hash=options.chromiumarchivesha256)

    # Apply patches required for source tarball support.
    apply_patch('tarball_deps', chromium_src_dir, required=True)
    apply_patch('tarball_gclient', depot_tools_dir, required=True)
  elif options.nochromiumhistory:
    sync_args += '--no-history'
  else:
    sync_args += '--with_branch_heads'

  run("gclient sync %s" % sync_args, chromium_dir, depot_tools_dir)
else:
  chromium_checkout_new = False

# Verify the Chromium checkout.
if options.chromiumarchive:
  current_version = read_chrome_version_file(chromium_src_dir)
  if current_version != chromium_version:
    raise Exception('Found Chromium version %s, expected %s at: %s' %
                    (current_version, chromium_version, chromium_src_dir))
else:
  if not options.dryrun and not is_git_checkout(chromium_src_dir):
    raise Exception('Not a valid git checkout: %s' % (chromium_src_dir))

  if os.path.exists(chromium_src_dir):
    msg("Chromium URL: %s" % (get_git_url(chromium_src_dir)))

if options.nochromiumhistory:
  chromium_checkout_changed = chromium_checkout_new
  msg("Chromium Checkout (no history): %s" % chromium_checkout)
else:
  # Fetch Chromium changes so that we can perform the necessary calculations using
  # local history.
  if not options.nochromiumupdate and os.path.exists(chromium_src_dir):
    # Fetch updated sources.
    run("%s fetch" % (git_exe), chromium_src_dir, depot_tools_dir)
    # Also fetch tags, which are required for release branch builds.
    run("%s fetch --tags" % (git_exe), chromium_src_dir, depot_tools_dir)

  # Determine if the Chromium checkout needs to change.
  if not options.nochromiumupdate and os.path.exists(chromium_src_dir):
    chromium_current_hash = get_git_hash(chromium_src_dir, 'HEAD')
    chromium_desired_hash = get_git_hash(chromium_src_dir, chromium_checkout)
    chromium_checkout_changed = chromium_checkout_new or force_change or \
                                chromium_current_hash != chromium_desired_hash

    msg("Chromium Current Checkout: %s" % (chromium_current_hash))
    msg("Chromium Desired Checkout: %s (%s)" % \
        (chromium_desired_hash, chromium_checkout))
  else:
    chromium_checkout_changed = options.dryrun

if cef_checkout_changed:
  if cef_dir != cef_src_dir and os.path.exists(cef_src_dir):
    # Delete the existing src/cef directory. It will be re-copied from the
    # download directory later.
    delete_directory(cef_src_dir)
elif chromium_checkout_changed and cef_dir == cef_src_dir:
  # Running in fast update mode. Backup and revert the patched files before
  # changing the Chromium checkout.
  run_patch_updater("--backup --revert")

# Delete the existing src/out directory if requested.
if options.forceclean and os.path.exists(out_src_dir):
  delete_directory(out_src_dir)

# Move the existing src/out directory to the correct location in the download
# directory. It will be moved back from the download directory later.
if os.path.exists(out_src_dir):
  old_branch = read_branch_config_file(out_src_dir)
  if old_branch != '' and (chromium_checkout_changed or
                           old_branch != cef_branch):
    old_out_dir = os.path.join(download_dir, 'out_' + old_branch)
    move_directory(out_src_dir, old_out_dir)

# Update the Chromium checkout.
if chromium_checkout_changed:
  if not options.nochromiumhistory:
    if not chromium_checkout_new and not options.fastupdate:
      if options.forceclean and options.forcecleandeps:
        # Remove all local changes including third-party git checkouts managed by
        # gclient.
        run("%s clean -dffx" % (git_exe), chromium_src_dir, depot_tools_dir)
      else:
        # Revert all changes in the Chromium checkout.
        run("gclient revert --nohooks", chromium_dir, depot_tools_dir)

    # Checkout the requested branch.
    run("%s checkout %s%s" % \
      (git_exe, '--force ' if discard_local_changes else '', chromium_checkout), \
      chromium_src_dir, depot_tools_dir)

  # Patch the Chromium DEPS file if necessary.
  apply_deps_patch()

  if not options.nochromiumhistory:
    # Update third-party dependencies including branch/tag information.
    run("gclient sync %s--nohooks --with_branch_heads" % \
        ('--reset ' if discard_local_changes else ''), chromium_dir, depot_tools_dir)

  # Patch the Chromium runhooks scripts if necessary.
  apply_runhooks_patch()

  # Runs hooks for files that have been modified in the local working copy.
  run("gclient runhooks", chromium_dir, depot_tools_dir)

  # Delete the src/out directory created by `gclient sync`.
  delete_directory(out_src_dir)

if cef_dir == cef_src_dir:
  # Running in fast update mode.
  if cef_checkout_changed or chromium_checkout_changed:
    # Check and restore the patched files.
    run_patch_updater("--reapply --restore")
elif os.path.exists(cef_dir) and not os.path.exists(cef_src_dir):
  # Restore the src/cef directory.
  copy_directory(cef_dir, cef_src_dir)

# Restore the src/out directory.
out_src_dir_exists = os.path.exists(out_src_dir)
if os.path.exists(out_dir) and not out_src_dir_exists:
  move_directory(out_dir, out_src_dir)
  out_src_dir_exists = True
elif not out_src_dir_exists:
  create_directory(out_src_dir)

# Write the config file for identifying the branch.
write_branch_config_file(out_src_dir, cef_branch)

if options.logchromiumchanges and chromium_checkout != chromium_compat_version:
  log_chromium_changes()

if options.forcepatchupdate or ((chromium_checkout_new or not options.fastupdate) and \
                                chromium_checkout_changed and \
                                chromium_checkout != chromium_compat_version):
  # Not using the known-compatible Chromium version. Try to update patch files.
  if options.logchromiumchanges:
    out_file = os.path.join(download_dir, 'chromium_update_patches.txt')
    if os.path.exists(out_file):
      os.remove(out_file)
  else:
    out_file = None
  run_patch_updater(output_file=out_file)
elif options.resave:
  # Resave patch files.
  run_patch_updater("--resave")

if chromium_checkout != chromium_compat_version:
  if options.logchromiumchanges:
    out_file = os.path.join(download_dir, 'chromium_update_patterns.txt')
    if os.path.exists(out_file):
      os.remove(out_file)
  else:
    out_file = None
  check_pattern_matches(output_file=out_file)

##
# Build CEF.
##

if not options.nobuild and (chromium_checkout_changed or \
                            cef_checkout_changed or options.forcebuild or \
                            not out_src_dir_exists):
  # Building should also force a distribution.
  options.forcedistrib = True

  # Make sure the GN configuration exists.
  if not options.dryrun and \
    not os.path.exists(os.path.join(cef_src_dir, 'BUILD.gn')):
    raise Exception('GN configuration does not exist.')

  # Print all build-related environment variables including any that were set
  # previously.
  for key in os.environ.keys():
    if key.startswith('CEF_') or key.startswith('GCLIENT_') or \
       key.startswith('GN_') or key.startswith('GYP_') or \
       key.startswith('DEPOT_TOOLS_'):
      msg('%s=%s' % (key, os.environ[key]))

  # Generate project files.
  tool = os.path.join(cef_src_dir, 'tools', 'gclient_hook.py')
  run('%s %s' % (python_exe, tool), cef_src_dir, depot_tools_dir)

  # Build using autoninja for automatic `-j (#cores)` configuration.
  command = 'autoninja '
  if options.verbosebuild:
    command += '-v '
  if options.buildfailurelimit != 1:
    command += '-k %d ' % options.buildfailurelimit
  command += '-C '
  target = ' ' + options.buildtarget
  if options.buildtests:
    target += ' ' + options.testtarget
  if platform == 'linux':
    target += ' chrome_sandbox'
  if platform in bootstrap_exe_platforms:
    target += ' bootstrap bootstrapc'

  # Make a CEF Debug build.
  if not options.nodebugbuild:
    build_path = os.path.join('out', get_build_directory_name(True))
    args_path = os.path.join(chromium_src_dir, build_path, 'args.gn')
    msg(args_path + ' contents:\n' + read_file(args_path))

    run(command + build_path + target, chromium_src_dir, depot_tools_dir,
        os.path.join(download_dir, 'build-%s-debug.log' % (cef_branch)) \
          if options.buildlogfile else None)

    if platform in sandbox_static_platforms:
      # Make the separate cef_sandbox build when GN is_official_build=true.
      build_path += '_sandbox'
      if os.path.exists(os.path.join(chromium_src_dir, build_path)):
        args_path = os.path.join(chromium_src_dir, build_path, 'args.gn')
        msg(args_path + ' contents:\n' + read_file(args_path))

        run(command + build_path + ' cef_sandbox', chromium_src_dir, depot_tools_dir,
            os.path.join(download_dir, 'build-%s-debug-sandbox.log' % (cef_branch)) \
              if options.buildlogfile else None)

  # Make a CEF Release build.
  if not options.noreleasebuild:
    build_path = os.path.join('out', get_build_directory_name(False))
    args_path = os.path.join(chromium_src_dir, build_path, 'args.gn')
    msg(args_path + ' contents:\n' + read_file(args_path))

    run(command + build_path + target, chromium_src_dir, depot_tools_dir,
        os.path.join(download_dir, 'build-%s-release.log' % (cef_branch)) \
          if options.buildlogfile else None)

    if platform in sandbox_static_platforms:
      # Make the separate cef_sandbox build when GN is_official_build=true.
      build_path += '_sandbox'
      if os.path.exists(os.path.join(chromium_src_dir, build_path)):
        args_path = os.path.join(chromium_src_dir, build_path, 'args.gn')
        msg(args_path + ' contents:\n' + read_file(args_path))

        run(command + build_path + ' cef_sandbox', chromium_src_dir, depot_tools_dir,
            os.path.join(download_dir, 'build-%s-release-sandbox.log' % (cef_branch)) \
              if options.buildlogfile else None)

elif not options.nobuild:
  msg('Not building. The source hashes have not changed and ' +
      'the output folder "%s" already exists' % (out_src_dir))

##
# Run CEF tests.
##

if options.runtests:
  if platform == 'windows':
    test_exe = '%s.exe' % options.testtarget
  elif platform == 'mac':
    test_exe = '%s.app/Contents/MacOS/%s' % (options.testtarget,
                                             options.testtarget)
  elif platform == 'linux':
    test_exe = options.testtarget

  test_prefix = options.testprefix
  if len(test_prefix) > 0:
    test_prefix += ' '

  test_args = options.testargs
  if len(test_args) > 0:
    test_args = ' ' + test_args

  if not options.nodebugtests:
    build_path = os.path.join(out_src_dir, get_build_directory_name(True))
    test_path = os.path.join(build_path, test_exe)
    if os.path.exists(test_path):
      run(test_prefix + test_path + test_args, build_path, depot_tools_dir)
    else:
      msg('Not running debug tests. Missing executable: %s' % test_path)

  if not options.noreleasetests:
    build_path = os.path.join(out_src_dir, get_build_directory_name(False))
    test_path = os.path.join(build_path, test_exe)
    if os.path.exists(test_path):
      run(test_prefix + test_path + test_args, build_path, depot_tools_dir)
    else:
      msg('Not running release tests. Missing executable: %s' % test_path)

##
# Create the CEF binary distribution.
##

if not options.nodistrib and (chromium_checkout_changed or \
                              cef_checkout_changed or options.forcedistrib):
  if not options.forceclean and options.cleanartifacts:
    # Clean the artifacts output directory.
    artifacts_path = os.path.join(cef_src_dir, 'binary_distrib')
    delete_directory(artifacts_path)

  # Determine the requested distribution types.
  distrib_types = []
  if options.minimaldistribonly:
    distrib_types.append('minimal')
  elif options.clientdistribonly:
    distrib_types.append('client')
  elif options.sandboxdistribonly:
    distrib_types.append('sandbox')
  elif options.toolsdistribonly:
    distrib_types.append('tools')
  else:
    distrib_types.append('standard')
    if options.minimaldistrib:
      distrib_types.append('minimal')
    if options.clientdistrib:
      distrib_types.append('client')
    if options.sandboxdistrib:
      distrib_types.append('sandbox')
    if options.toolsdistrib:
      distrib_types.append('tools')

  cef_tools_dir = os.path.join(cef_src_dir, 'tools')

  # Create the requested distribution types.
  first_type = True
  for type in distrib_types:
    path = '%s make_distrib.py --output-dir=../binary_distrib/' % python_exe

    if options.nodebugbuild or options.noreleasebuild or type != 'standard':
      path += ' --allow-partial'
    path = path + ' --ninja-build'
    if options.x64build:
      path += ' --x64-build'
    elif options.armbuild:
      path += ' --arm-build'
    elif options.arm64build:
      path += ' --arm64-build'

    if type == 'minimal':
      path += ' --minimal'
    elif type == 'client':
      path += ' --client'
    elif type == 'sandbox':
      path += ' --sandbox'
    elif type == 'tools':
      path += ' --tools'

    if first_type:
      if options.nodistribdocs:
        path += ' --no-docs'
      if options.nodistribarchive:
        path += ' --no-archive'
      first_type = False
    else:
      # Don't create the symbol archives or documentation more than once.
      path += ' --no-symbols --no-docs'

    # Override the subdirectory name of binary_distrib if the caller requested.
    if options.distribsubdir != '':
      path += ' --distrib-subdir=' + options.distribsubdir
    if options.distribsubdirsuffix != '':
      path += ' --distrib-subdir-suffix=' + options.distribsubdirsuffix

    # Create the distribution.
    run(path, cef_tools_dir, depot_tools_dir)
