# Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file

from exec_util import exec_cmd
import os

def is_checkout(path):
  """ Returns true if the path represents a git checkout. """
  return os.path.exists(os.path.join(path, '.git'))

def get_hash(path = '.', branch = 'HEAD'):
  """ Returns the git hash for the specified branch/tag/hash. """
  cmd = "git rev-parse %s" % (branch)
  result = exec_cmd(cmd, path)
  if result['out'] != '':
    return result['out'].strip()
  return 'Unknown'

def get_url(path = '.'):
  """ Returns the origin url for the specified path. """
  cmd = "git config --get remote.origin.url"
  result = exec_cmd(cmd, path)
  if result['out'] != '':
    return result['out'].strip()
  return 'Unknown'

def get_svn_revision(path = '.', branch = 'HEAD'):
  """ Returns the SVN revision associated with the specified path and git
      branch/tag/hash. """
  svn_rev = "None"
  cmd = "git log --grep=^git-svn-id: -n 1 %s" % (branch)
  result = exec_cmd(cmd, path)
  if result['err'] == '':
    for line in result['out'].split('\n'):
      if line.find("git-svn-id") > 0:
        svn_rev = line.split("@")[1].split()[0]
        break
  return svn_rev

def get_changed_files(path = '.'):
  """ Retrieves the list of changed files. """
  # not implemented
  return []
