# Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from __future__ import print_function
from io import open
from optparse import Option, OptionParser, OptionValueError
import os
import re
import sys
from exec_util import exec_cmd
from file_util import copy_file, move_file, read_file, remove_file
import git_util as git

backup_ext = '.cefbak'


def msg(message):
  """ Output a message. """
  sys.stdout.write('--> ' + message + "\n")


def linebreak():
  """ Output a line break. """
  sys.stdout.write('-' * 80 + "\n")


def warn(message):
  """ Output a warning. """
  linebreak()
  sys.stdout.write('!!!! WARNING: ' + message + "\n")
  linebreak()


def extract_paths(file):
  """ Extract the list of modified paths from the patch file. """
  paths = []
  with open(file, 'r', encoding='utf-8') as fp:
    for line in fp:
      if line[:4] != '+++ ':
        continue
      match = re.match('^([^\t]+)', line[4:])
      if not match:
        continue
      paths.append(match.group(1).strip())
  return paths


# Cannot be loaded as a module.
if __name__ != "__main__":
  sys.stderr.write('This file cannot be loaded as a module!')
  sys.exit()

# Parse command-line options.
disc = """
This utility updates existing patch files.
"""


# Support options with multiple arguments.
class MultipleOption(Option):
  ACTIONS = Option.ACTIONS + ("extend",)
  STORE_ACTIONS = Option.STORE_ACTIONS + ("extend",)
  TYPED_ACTIONS = Option.TYPED_ACTIONS + ("extend",)
  ALWAYS_TYPED_ACTIONS = Option.ALWAYS_TYPED_ACTIONS + ("extend",)

  def take_action(self, action, dest, opt, value, values, parser):
    if action == "extend":
      values.ensure_value(dest, []).append(value)
    else:
      Option.take_action(self, action, dest, opt, value, values, parser)


parser = OptionParser(option_class=MultipleOption, description=disc)
parser.add_option(
    '--resave',
    action='store_true',
    dest='resave',
    default=False,
    help='resave existing patch files to pick up manual changes')
parser.add_option(
    '--reapply',
    action='store_true',
    dest='reapply',
    default=False,
    help='reapply the patch without first reverting changes')
parser.add_option(
    '--revert',
    action='store_true',
    dest='revert',
    default=False,
    help='revert all changes from existing patch files')
parser.add_option(
    '--backup',
    action='store_true',
    dest='backup',
    default=False,
    help='backup patched files. Used in combination with --revert.')
parser.add_option(
    '--restore',
    action='store_true',
    dest='restore',
    default=False,
    help='restore backup of patched files that have not changed. If a backup has ' +\
         'changed the patch file will be resaved. Used in combination with --reapply.')
parser.add_option(
    '--patch',
    action='extend',
    dest='patch',
    type='string',
    default=[],
    help='optional patch name to process (multiples allowed)')
parser.add_option(
    '--add',
    action='extend',
    dest='add',
    type='string',
    default=[],
    help='optional relative file paths to add (multiples allowed). Used in ' +\
         'combination with --resave and a single --patch value.')
(options, args) = parser.parse_args()

if options.resave and options.revert:
  print('Invalid combination of options.')
  parser.print_help(sys.stderr)
  sys.exit()

if len(options.add) > 0 and (len(options.patch) != 1 or not options.resave):
  print('--add can only be used with --resave and a single --patch value.')
  parser.print_help(sys.stderr)
  sys.exit()

# The CEF root directory is the parent directory of _this_ script.
cef_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
src_dir = os.path.abspath(os.path.join(cef_dir, os.pardir))

# Determine the type of Chromium checkout.
if not git.is_checkout(src_dir):
  raise Exception('Not a valid checkout: %s' % src_dir)

patch_dir = os.path.join(cef_dir, 'patch')
patch_cfg = os.path.join(patch_dir, 'patch.cfg')
if not os.path.isfile(patch_cfg):
  raise Exception('File does not exist: %s' % patch_cfg)

# Read the patch configuration file.
msg('Reading patch config %s' % patch_cfg)
scope = {}
exec (compile(open(patch_cfg, "rb").read(), patch_cfg, 'exec'), scope)
patches = scope["patches"]

failed_patches = {}

# Read each individual patch file.
patches_dir = os.path.join(patch_dir, 'patches')
for patch in patches:
  # If specific patch names are specified only process those patches.
  if options.patch and not patch['name'] in options.patch:
    continue

  sys.stdout.write('\n')
  patch_file = os.path.join(patches_dir, patch['name'] + '.patch')

  if os.path.isfile(patch_file):
    msg('Reading patch file %s' % patch_file)
    if 'path' in patch:
      patch_root_abs = os.path.abspath(os.path.join(src_dir, patch['path']))
      if not os.path.isdir(patch_root_abs):
        line = 'Target directory does not exist: %s' % patch_root_abs
        msg(line)
        if options.resave:
          # Report as a fatal error for manual resave only, as the missing
          # directory may be platform-specific.
          failed_patches[patch['name']] = [line]
        continue
    else:
      patch_root_abs = src_dir

    # Retrieve the list of paths modified by the patch file.
    patch_paths = extract_paths(patch_file)

    # List of paths added by the patch file.
    added_paths = []

    # True if any backed up files have changed.
    has_backup_changes = False

    if not options.resave:
      if not options.reapply:
        # Revert any changes to existing files in the patch.
        for patch_path in patch_paths:
          patch_path_abs = os.path.abspath(os.path.join(patch_root_abs, \
                                                        patch_path))
          if os.path.exists(patch_path_abs):
            if options.backup:
              backup_path_abs = patch_path_abs + backup_ext
              if not os.path.exists(backup_path_abs):
                msg('Creating backup of %s' % patch_path_abs)
                copy_file(patch_path_abs, backup_path_abs)
              else:
                msg('Skipping backup of %s' % patch_path_abs)

            msg('Reverting changes to %s' % patch_path_abs)
            cmd = 'git checkout -- %s' % (patch_path_abs)
            result = exec_cmd(cmd, patch_root_abs)
            if result['err'] != '':
              msg('Failed to revert file: %s' % result['err'])
              msg('Deleting file %s' % patch_path_abs)
              os.remove(patch_path_abs)
              added_paths.append(patch_path_abs)
            if result['out'] != '':
              sys.stdout.write(result['out'])
          else:
            msg('Skipping non-existing file %s' % patch_path_abs)
            added_paths.append(patch_path_abs)

      if not options.revert:
        # Chromium files are occasionally (incorrectly) checked in with Windows
        # line endings. This will cause the patch tool to fail when attempting
        # to patch those files on Posix systems. Convert any such files to Posix
        # line endings before applying the patch.
        converted_files = []
        for patch_path in patch_paths:
          patch_path_abs = os.path.abspath(os.path.join(patch_root_abs, \
                                                        patch_path))
          if os.path.exists(patch_path_abs):
            with open(patch_path_abs, 'r', encoding='utf-8') as fp:
              contents = fp.read()
            if "\r\n" in contents:
              msg('Converting to Posix line endings for %s' % patch_path_abs)
              converted_files.append(patch_path_abs)
              contents = contents.replace("\r\n", "\n")
              with open(patch_path_abs, 'wb') as fp:
                fp.write(contents)

        # Apply the patch file.
        msg('Applying patch to %s' % patch_root_abs)
        patch_string = open(patch_file, 'rb').read()
        result = exec_cmd('patch -p0', patch_root_abs, patch_string)

        if len(converted_files) > 0:
          # Restore Windows line endings in converted files so that the diff is
          # correct if/when the patch file is re-saved.
          for patch_path_abs in converted_files:
            with open(patch_path_abs, 'rb') as fp:
              contents = fp.read()
            msg('Converting to Windows line endings for %s' % patch_path_abs)
            contents = contents.replace("\n", "\r\n")
            with open(patch_path_abs, 'wb') as fp:
              fp.write(contents)

        if result['err'] != '':
          raise Exception('Failed to apply patch file: %s' % result['err'])
        sys.stdout.write(result['out'])
        if result['out'].find('FAILED') != -1:
          failed_lines = []
          for line in result['out'].split('\n'):
            if line.find('FAILED') != -1:
              failed_lines.append(line.strip())
          warn('Failed to apply %s, fix manually and run with --resave' % \
               patch['name'])
          failed_patches[patch['name']] = failed_lines
          continue

        if options.restore:
          # Restore from backup if a backup exists.
          for patch_path in patch_paths:
            patch_path_abs = os.path.abspath(os.path.join(patch_root_abs, \
                                                          patch_path))
            backup_path_abs = patch_path_abs + backup_ext
            if os.path.exists(backup_path_abs):
              if read_file(patch_path_abs) == read_file(backup_path_abs):
                msg('Restoring backup of %s' % patch_path_abs)
                remove_file(patch_path_abs)
                move_file(backup_path_abs, patch_path_abs)
              else:
                msg('Discarding backup of %s' % patch_path_abs)
                remove_file(backup_path_abs)
                has_backup_changes = True
            else:
              msg('No backup of %s' % patch_path_abs)

    if (not options.revert and not options.reapply) or has_backup_changes:
      if len(options.add) > 0:
        # Add additional requested files to the patch.
        for patch_path in options.add:
          patch_path_abs = os.path.abspath(os.path.join(patch_root_abs, \
                                                        patch_path))
          if os.path.exists(patch_path_abs):
            msg('Adding file %s' % patch_path_abs)
            patch_paths.append(patch_path)
          else:
            msg('Skipping non-existing file %s' % patch_path_abs)

      msg('Saving changes to %s' % patch_file)
      if added_paths:
        # Inform git of the added paths so they appear in the patch file.
        cmd = 'git add -N %s' % ' '.join(added_paths)
        result = exec_cmd(cmd, patch_root_abs)
        if result['err'] != '' and result['err'].find('warning:') != 0:
          raise Exception('Failed to add paths: %s' % result['err'])

      # Re-create the patch file.
      patch_paths_str = ' '.join(patch_paths)
      cmd = 'git diff --no-prefix --relative %s' % patch_paths_str
      result = exec_cmd(cmd, patch_root_abs)
      if result['err'] != '' and result['err'].find('warning:') != 0:
        raise Exception('Failed to create patch file: %s' % result['err'])

      if "\r\n" in result['out']:
        # Patch files should always be saved with Posix line endings.
        # This will avoid problems when attempting to re-apply the patch
        # file on Posix systems.
        msg('Converting to Posix line endings for %s' % patch_file)
        result['out'] = result['out'].replace("\r\n", "\n")

      f = open(patch_file, 'w', encoding='utf-8')
      f.write(result['out'])
      f.close()
  else:
    raise Exception('Patch file does not exist: %s' % patch_file)

if len(failed_patches) > 0:
  sys.stdout.write("\n")
  linebreak()
  sys.stdout.write("!!!! FAILED PATCHES, fix manually and run with --resave\n")
  for name in sorted(failed_patches.keys()):
    sys.stdout.write("%s:\n" % name)
    for line in failed_patches[name]:
      if sys.platform == 'win32' and line.find('.rej') > 0:
        # Convert paths to use Windows-style separator.
        line = line.replace('/', '\\')
      sys.stdout.write("  %s\n" % line)
  linebreak()
  sys.exit(1)
