# Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file

from __future__ import absolute_import
from exec_util import exec_cmd
import os
import sys

if sys.platform == 'win32':
  # Force use of the system installed Git version.
  git_exe = 'git.exe'
  patch_exe = 'patch.exe'
else:
  git_exe = 'git'
  patch_exe = 'patch'


def is_checkout(path):
  """ Returns true if the path represents a git checkout. """
  return os.path.exists(os.path.join(path, '.git'))


def is_ancestor(path='.', commit1='HEAD', commit2='master'):
  """ Returns whether |commit1| is an ancestor of |commit2|. """
  cmd = "%s merge-base --is-ancestor %s %s" % (git_exe, commit1, commit2)
  result = exec_cmd(cmd, path)
  return result['ret'] == 0


def exec_git_cmd(args, path='.', quiet=False):
  """ Executes a git command with the specified |args|. """
  cmd = "%s %s" % (git_exe, args)
  result = exec_cmd(cmd, path)
  if result['ret'] != 0 and not quiet:
    sys.stderr.write('Command \"%s\" exited with retval %d\n' % (cmd,
                                                                 result['ret']))
    if result['err'] != '':
      err = result['err'].strip()
      if sys.platform == 'win32':
        # Convert to Unix line endings.
        err = err.replace('\r\n', '\n')
      sys.stderr.write(err + '\n')
  if result['out'] != '':
    out = result['out'].strip()
    if sys.platform == 'win32':
      # Convert to Unix line endings.
      out = out.replace('\r\n', '\n')
    return out
  return None


def get_hash(path='.', branch='HEAD'):
  """ Returns the git hash for the specified branch/tag/hash. """
  cmd = "rev-parse %s" % branch
  result = exec_git_cmd(cmd, path)
  return 'Unknown' if result is None else result


def get_branch_name(path='.', branch='HEAD'):
  """ Returns the branch name for the specified branch/tag/hash. """
  # Returns the branch name if not in detached HEAD state, else an empty string
  # or "HEAD".
  cmd = "rev-parse --abbrev-ref %s" % branch
  result = exec_git_cmd(cmd, path)
  if result is None:
    return 'Unknown'
  if result != 'HEAD':
    return result

  # Returns a value like "(HEAD, origin/3729, 3729)".
  # Ubuntu 14.04 uses Git version 1.9.1 which does not support %D (which
  # provides the same output but without the parentheses).
  cmd = "log -n 1 --pretty=%%d %s" % branch
  result = exec_git_cmd(cmd, path)
  return 'Unknown' if result is None else result[1:-1].split(', ')[-1]


def get_url(path='.'):
  """ Returns the origin url for the specified path. """
  cmd = "config --get remote.origin.url"
  result = exec_git_cmd(cmd, path)
  return 'Unknown' if result is None else result


def get_commit_number(path='.', branch='HEAD'):
  """ Returns the number of commits in the specified branch/tag/hash. """
  cmd = "rev-list --count %s" % (branch)
  result = exec_git_cmd(cmd, path)
  return '0' if result is None else result


def get_changed_files(path, hash):
  """ Retrieves the list of changed files. """
  if hash == 'unstaged':
    cmd = "diff --name-only"
  elif hash == 'staged':
    cmd = "diff --name-only --cached"
  else:
    cmd = "diff-tree --no-commit-id --name-only -r %s" % hash
  result = exec_git_cmd(cmd, path, quiet=True)
  return [] if result is None else result.split("\n")


def get_branch_hashes(path='.', branch='HEAD', ref='origin/master'):
  """ Returns an ordered list of hashes for commits that have been applied since
      branching from ref. """
  cmd = "cherry %s %s" % (ref, branch)
  result = exec_git_cmd(cmd, path)
  if result is None:
    return []
  # Remove the "+ " or "- " prefix.
  return [line[2:] for line in result.split('\n')]


def write_indented_output(output):
  """ Apply a fixed amount of intent to lines before printing. """
  if output == '':
    return
  for line in output.split('\n'):
    line = line.strip()
    if len(line) == 0:
      continue
    sys.stdout.write('\t%s\n' % line)


def _patch_apply_patch_string(patch_dir, patch_string):
  """ Fallback to using the patch tool. """
  config = '-p0 --ignore-whitespace --force'

  # Reverse check to see if the patch has already been applied.
  cmd = '%s %s --reverse --dry-run' % (patch_exe, config)
  result = exec_cmd(cmd, patch_dir, patch_string)
  if result['ret'] == 0:
    sys.stdout.write('... already applied (skipping).\n')
    return 'skip'

  # Apply the patch file.
  cmd = '%s %s' % (patch_exe, config)
  result = exec_cmd(cmd, patch_dir, patch_string)
  write_indented_output(result['out'])
  if result['ret'] == 0:
    sys.stdout.write('... successfully applied.\n')
    return 'apply'
  sys.stdout.write('... failed to apply:\n')
  write_indented_output(result['err'])
  return 'fail'


def git_apply_patch_file(patch_path, patch_dir):
  """ Apply |patch_path| to files in |patch_dir|. """
  patch_name = os.path.basename(patch_path)
  sys.stdout.write('\nApply %s in %s\n' % (patch_name, patch_dir))

  if not os.path.isfile(patch_path):
    sys.stdout.write('... patch file does not exist.\n')
    return 'fail'

  patch_string = open(patch_path, 'rb').read()
  if sys.platform == 'win32':
    # Convert the patch to Unix line endings. This is necessary to avoid
    # whitespace errors with git apply.
    patch_string = patch_string.replace(b'\r\n', b'\n')

  # Git apply fails silently if not run relative to a respository root.
  # Fallback to using the patch tool in that case.
  if not is_checkout(patch_dir):
    return _patch_apply_patch_string(patch_dir, patch_string)

  config = '-p0 --ignore-whitespace'

  # Output patch contents.
  cmd = '%s apply %s --numstat' % (git_exe, config)
  result = exec_cmd(cmd, patch_dir, patch_string)
  write_indented_output(result['out'].replace('<stdin>', patch_name))

  # Reverse check to see if the patch has already been applied.
  cmd = '%s apply %s --reverse --check' % (git_exe, config)
  result = exec_cmd(cmd, patch_dir, patch_string)
  if result['err'].find('error:') < 0:
    sys.stdout.write('... already applied (skipping).\n')
    return 'skip'

  # Normal check to see if the patch can be applied cleanly.
  cmd = '%s apply %s --check' % (git_exe, config)
  result = exec_cmd(cmd, patch_dir, patch_string)
  if result['err'].find('error:') >= 0:
    sys.stdout.write('... failed to apply:\n')
    write_indented_output(result['err'].replace('<stdin>', patch_name))
    return 'fail'

  # Apply the patch file. This should always succeed because the previous
  # command succeeded.
  cmd = '%s apply %s' % (git_exe, config)
  result = exec_cmd(cmd, patch_dir, patch_string)
  if result['err'] == '':
    sys.stdout.write('... successfully applied.\n')
  else:
    sys.stdout.write('... successfully applied (with warnings):\n')
    write_indented_output(result['err'].replace('<stdin>', patch_name))
  return 'apply'
