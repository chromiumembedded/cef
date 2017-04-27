# Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from optparse import Option, OptionParser, OptionValueError
import os
import re
import sys
from exec_util import exec_cmd
import git_util as git

def msg(message):
  """ Output a message. """
  sys.stdout.write('--> ' + message + "\n")

def warn(message):
  """ Output a warning. """
  sys.stdout.write('-' * 80 + "\n")
  sys.stdout.write('!!!! WARNING: ' + message + "\n")
  sys.stdout.write('-' * 80 + "\n")

def extract_paths(file):
  """ Extract the list of modified paths from the patch file. """
  paths = []
  fp = open(file)
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

parser = OptionParser(option_class=MultipleOption,
                      description=disc)
parser.add_option('--resave',
                  action='store_true', dest='resave', default=False,
                  help='re-save existing patch files to pick up manual changes')
parser.add_option('--revert',
                  action='store_true', dest='revert', default=False,
                  help='revert all changes from existing patch files')
parser.add_option('--patch',
                  action='extend', dest='patch', type='string', default=[],
                  help='optional patch name to process (multiples allowed)')
(options, args) = parser.parse_args()

if options.resave and options.revert:
  print 'Invalid combination of options.'
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
execfile(patch_cfg, scope)
patches = scope["patches"]

# Read each individual patch file.
patches_dir = os.path.join(patch_dir, 'patches')
for patch in patches:
  # If specific patch names are specified only process those patches.
  if options.patch and not patch['name'] in options.patch:
    continue

  sys.stdout.write('\n')
  patch_file = os.path.join(patches_dir, patch['name']+'.patch')

  if os.path.isfile(patch_file):
    msg('Reading patch file %s' % patch_file)
    if 'path' in patch:
      patch_root_abs = os.path.abspath(os.path.join(src_dir, patch['path']))
    else:
      patch_root_abs = src_dir

    # Retrieve the list of paths modified by the patch file.
    patch_paths = extract_paths(patch_file)

    # List of paths added by the patch file.
    added_paths = []

    if not options.resave:
      # Revert any changes to existing files in the patch.
      for patch_path in patch_paths:
        patch_path_abs = os.path.abspath(os.path.join(patch_root_abs, \
                                                      patch_path))
        if os.path.exists(patch_path_abs):
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
        # Apply the patch file.
        msg('Applying patch to %s' % patch_root_abs)
        patch_string = open(patch_file, 'rb').read()
        result = exec_cmd('patch -p0', patch_root_abs, patch_string)
        if result['err'] != '':
          raise Exception('Failed to apply patch file: %s' % result['err'])
        sys.stdout.write(result['out'])
        if result['out'].find('FAILED') != -1:
          warn('Failed to apply %s, fix manually and run with --resave' % \
               patch['name'])
          continue

    if not options.revert:
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
      f = open(patch_file, 'wb')
      f.write(result['out'])
      f.close()
  else:
    raise Exception('Patch file does not exist: %s' % patch_file)
