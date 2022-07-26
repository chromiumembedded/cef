# Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
import pickle
from optparse import OptionParser
import os
import sys
from file_util import *
from git_util import git_apply_patch_file

# Cannot be loaded as a module.
if __name__ != "__main__":
  sys.stdout.write('This file cannot be loaded as a module!')
  sys.exit()

# The CEF root directory is the parent directory of _this_ script.
cef_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
cef_patch_dir = os.path.join(cef_dir, 'patch')
src_dir = os.path.abspath(os.path.join(cef_dir, os.pardir))


def write_note(type, note):
  separator = '-' * 79 + '\n'
  sys.stdout.write(separator)
  sys.stdout.write('!!!! %s: %s\n' % (type, note))
  sys.stdout.write(separator)


def apply_patch_file(patch_file, patch_dir):
  ''' Apply a specific patch file in optional patch directory. '''
  patch_name = patch_file + '.patch'
  patch_path = os.path.join(cef_patch_dir, 'patches', patch_name)

  if patch_dir is None or len(patch_dir) == 0:
    patch_dir = src_dir
  else:
    if not os.path.isabs(patch_dir):
      # Apply patch relative to the Chromium 'src' directory.
      patch_dir = os.path.join(src_dir, patch_dir)
    patch_dir = os.path.abspath(patch_dir)
    if not os.path.isdir(patch_dir):
      sys.stdout.write('\nApply %s in %s\n' % (patch_name, patch_dir))
      sys.stdout.write('... target directory does not exist (skipping).\n')
      return 'skip'

  result = git_apply_patch_file(patch_path, patch_dir)
  if result == 'fail':
    write_note('ERROR',
               'This patch failed to apply. Your build will not be correct.')
  return result


def apply_patch_config():
  ''' Apply patch files based on a configuration file. '''
  config_file = os.path.join(cef_patch_dir, 'patch.cfg')
  if not os.path.isfile(config_file):
    raise Exception('Patch config file %s does not exist.' % config_file)

  # Parse the configuration file.
  scope = {}
  exec (compile(open(config_file, "rb").read(), config_file, 'exec'), scope)
  patches = scope["patches"]

  results = {'apply': [], 'skip': [], 'fail': []}

  for patch in patches:
    patch_file = patch['name']
    dopatch = True

    if 'condition' in patch:
      # Check that the environment variable is set.
      if patch['condition'] not in os.environ:
        sys.stdout.write('\nSkipping patch file %s\n' % patch_file)
        dopatch = False

    if dopatch:
      result = apply_patch_file(patch_file, patch['path']
                                if 'path' in patch else None)
      results[result].append(patch_file)

      if 'note' in patch:
        write_note('NOTE', patch['note'])
    else:
      results['skip'].append(patch_file)

  sys.stdout.write('\n%d patches total (%d applied, %d skipped, %d failed)\n' % \
      (len(patches), len(results['apply']), len(results['skip']), len(results['fail'])))

  if len(results['fail']) > 0:
    sys.stdout.write('\n')
    write_note('ERROR',
               '%d patches failed to apply. Your build will not be correct.' %
               len(results['fail']))
    sys.stdout.write('\nTo manually revert failed patches run:' \
                     '\n$ %s ./tools/patch_updater.py --revert --patch %s\n' %
                     (os.path.basename(sys.executable), ' --patch '.join(results['fail'])))
    sys.exit(1)


# Parse command-line options.
disc = """
This utility applies patch files.
"""

parser = OptionParser(description=disc)
parser.add_option(
    '--patch-file', dest='patchfile', metavar='FILE', help='patch source file')
parser.add_option(
    '--patch-dir',
    dest='patchdir',
    metavar='DIR',
    help='patch target directory')
(options, args) = parser.parse_args()

if not options.patchfile is None:
  apply_patch_file(options.patchfile, options.patchdir)
else:
  apply_patch_config()
