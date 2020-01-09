# Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from __future__ import print_function
from file_util import *
import git_util as git
import os


class VersionFormatter:
  """ Formats CEF version information. """

  def __init__(self, chromium_src_path=None):
    if chromium_src_path is None:
      # Relative to the current directory.
      script_path = os.path.abspath(os.path.dirname(__file__))
      chromium_src_path = os.path.abspath(
          os.path.join(script_path, os.pardir, os.pardir))

    self.src_path = chromium_src_path
    assert os.path.isdir(self.src_path), self.src_path
    self.cef_path = os.path.join(self.src_path, 'cef')
    assert os.path.isdir(self.cef_path), self.cef_path

    # Whether to use the old version format by default.
    self._old_format_default = \
        bool(int(os.environ.get('CEF_OLD_VERSION_FORMAT', '0')))

    self.reset()

  def reset(self):
    """ Reset internal state. """
    self._chrome_version = {}
    self._cef_version = {}
    self._cef_commit = {}
    self._branch_version = {}
    self._old_version_string = None
    self._old_version_parts = {}
    self._version_string = None
    self._version_parts = {}

  def get_chrome_version_components(self):
    """ Returns Chrome version components. """
    if not bool(self._chrome_version):
      file_path = os.path.join(self.src_path, 'chrome', 'VERSION')
      assert os.path.isfile(file_path), file_path
      read_version_file(file_path, self._chrome_version)
    return self._chrome_version

  def get_cef_version_components(self):
    """ Returns CEF version components. """
    if not bool(self._cef_version):
      file_path = os.path.join(self.cef_path, 'VERSION.in')
      assert os.path.isfile(file_path), file_path
      read_version_file(file_path, self._cef_version)
    return self._cef_version

  def get_cef_commit_components(self):
    """ Returns CEF commit components. """
    if not bool(self._cef_commit):
      hash = git.get_hash(self.cef_path)
      number = git.get_commit_number(self.cef_path)
      self._cef_commit = {'HASH': hash, 'NUMBER': number}
    return self._cef_commit

  def get_cef_branch_version_components(self):
    """ Computes the CEF branch version. """
    if not bool(self._branch_version):
      minor = 0
      bugfix = 0

      # Retrieve the list of commits that have been applied on the current
      # branch since branching from origin/master.
      hashes = git.get_branch_hashes(self.cef_path)
      for hash in hashes:
        # Determine if the API hash file was modified by the commit.
        found = False
        files = git.get_changed_files(self.cef_path, hash)
        for file in files:
          if file.find('cef_api_hash.h') >= 0:
            found = True
            break

        if found:
          minor += 1
          bugfix = 0
        else:
          bugfix += 1

      self._branch_version = {'MINOR': minor, 'PATCH': bugfix}
    return self._branch_version

  def get_chromium_version_string(self):
    """ Returns the Chromium version number string. """
    chrome_version = self.get_chrome_version_components()
    return '%s.%s.%s.%s' % (chrome_version['MAJOR'], chrome_version['MINOR'],
                            chrome_version['BUILD'], chrome_version['PATCH'])

  @staticmethod
  def _format_commit_hash(hash):
    return 'g%s' % hash[:7]

  # Computes old version numbers in the format "X.YYYY.A.gHHHHHHH".
  #
  # Where:
  # - "X" is the CEF major version (currently 3).
  # - "YYYY" is the Chromium branch.
  # - "A" is an incremental number representing the number of commits in the
  #   current branch. This is roughly equivalent to the SVN revision number but
  #   on a per-branch basis and assists people in quickly determining the order
  #   of builds in the same branch (for bug reports, etc).
  # - "gHHHHHHH" is the 7-character abbreviation for the Git commit hash. This
  #   facilitates lookup of the relevant commit history in Git.
  #
  # Example: "3.3729.1921.g62d140e"
  def _compute_old_version(self):
    if not self._old_version_string is None:
      return

    chrome_version = self.get_chrome_version_components()
    cef_version = self.get_cef_version_components()
    cef_commit = self.get_cef_commit_components()
    cef_commit_hash = self._format_commit_hash(cef_commit['HASH'])

    self._old_version_parts = {
        'MAJOR': int(cef_version['CEF_MAJOR']),
        'MINOR': int(chrome_version['BUILD']),
        'PATCH': int(cef_commit['NUMBER'])
    }
    self._old_version_string = \
        '%s.%s.%s.%s' % (cef_version['CEF_MAJOR'], chrome_version['BUILD'],
                         cef_commit['NUMBER'], cef_commit_hash)

  def _get_old_version_string(self):
    self._compute_old_version()
    return self._old_version_string

  def _get_old_version_parts(self):
    self._compute_old_version()
    return self._old_version_parts

  # Computes version numbers in the format:
  # - "X.Y.Z+gHHHHHHH+chromium-A.B.C.D" for release branch builds.
  # - "X.0.0-master.N+gHHHHHHH+chromium-A.B.C.D" for master branch builds.
  #
  # Where:
  # - "X" is the Chromium major version (e.g. 74).
  # - "Y" is an incremental number that starts at 0 when a release branch is
  #   created and changes only when the CEF C/C++ API changes (similar to how
  #   the CEF_API_HASH_UNIVERSAL value behaves in cef_version.h) (release branch
  #   only).
  # - "Z" is an incremental number that starts at 0 when a release branch is
  #   created and changes on each commit, with reset to 0 when "Y" changes
  #   (release branch only).
  # - "N" is an incremental number representing the number of commits (master
  #   branch only).
  # - "gHHHHHHH" is the 7-character abbreviation for the Git commit hash. This
  #   facilitates lookup of the relevant commit history in Git.
  # - "A.B.C.D" is the Chromium version (e.g. 74.0.3729.6).
  #
  # Examples:
  # - "74.0.1+g62d140e+chromium-74.0.3729.6" for a release build.
  # - "74.0.0-master.1920+g725ed88+chromium-74.0.3729.0" for a master build.
  def _compute_version(self):
    if not self._version_string is None:
      return

    chrome_version = self.get_chrome_version_components()
    chrome_major = chrome_version['MAJOR']
    chrome_version_part = 'chromium-' + self.get_chromium_version_string()

    cef_commit = self.get_cef_commit_components()
    cef_commit_hash = self._format_commit_hash(cef_commit['HASH'])

    # Determine whether the current commit is on a release branch. For example,
    # if using Chrome build 3683, are we on CEF branch "3683" or "origin/3683"?
    release_branch = chrome_version['BUILD']
    on_release_branch = (
        git.is_ancestor(self.cef_path, 'HEAD', release_branch) or
        git.is_ancestor(self.cef_path, 'HEAD', 'origin/' + release_branch))

    if not on_release_branch:
      # Not on a commit that is part of an official named release branch. See
      # if we can get the name of the branch we are on (may be just "HEAD").
      cef_branch_name = git.get_branch_name(self.cef_path).split('/')[-1]

      self._version_parts = {'MAJOR': int(chrome_major), 'MINOR': 0, 'PATCH': 0}
      self._version_string = '%s.0.0-%s.%s+%s+%s' % \
             (chrome_major, cef_branch_name, cef_commit['NUMBER'],
              cef_commit_hash, chrome_version_part)
    else:
      cef_branch = self.get_cef_branch_version_components()

      self._version_parts = {
          'MAJOR': int(chrome_major),
          'MINOR': cef_branch['MINOR'],
          'PATCH': cef_branch['PATCH']
      }
      self._version_string = '%s.%d.%d+%s+%s' % \
             (chrome_major, cef_branch['MINOR'], cef_branch['PATCH'],
              cef_commit_hash, chrome_version_part)

  def _get_version_string(self):
    self._compute_version()
    return self._version_string

  def _get_version_parts(self):
    self._compute_version()
    return self._version_parts

  def get_version_string(self, oldFormat=None):
    """ Returns the CEF version number string based on current checkout state.
    """
    if oldFormat is None:
      oldFormat = self._old_format_default

    if oldFormat:
      return self._get_old_version_string()
    return self._get_version_string()

  def get_version_parts(self, oldFormat=None):
    """ Returns the CEF version number parts based on current checkout state.
    """
    if oldFormat is None:
      oldFormat = self._old_format_default

    if oldFormat:
      return self._get_old_version_parts()
    return self._get_version_parts()

  def get_plist_version_string(self, oldFormat=None):
    """ Returns the CEF version number string for plist files based on current
        checkout state. """
    parts = self.get_version_parts(oldFormat=oldFormat)
    return "%d.%d.%d.0" % (parts['MAJOR'], parts['MINOR'], parts['PATCH'])

  def get_dylib_version_string(self, oldFormat=None):
    """ Returns the CEF version number string for dylib files based on current
        checkout state. """
    parts = self.get_version_parts(oldFormat=oldFormat)
    # Dylib format supports a max value of 255 for the 2nd and 3rd components.
    return "%d%d.%d.%d" % (parts['MAJOR'], parts['MINOR'], parts['PATCH'] / 255,
                           parts['PATCH'] % 255)


# Test the module.
if __name__ == "__main__":
  import sys

  # Optionally specify a format.
  formats = ['current', 'old', 'plist', 'dylib']
  if len(sys.argv) >= 2:
    formats = [sys.argv[1]]

  # Optionally specify the path to chromium/src.
  chromium_src_path = None
  if len(sys.argv) >= 3:
    chromium_src_path = sys.argv[2]

  formatter = VersionFormatter(chromium_src_path)

  for format in formats:
    if len(formats) > 1:
      print(format)

    if format == 'old':
      print(formatter.get_version_string(True))
    elif format == 'dylib':
      print(formatter.get_dylib_version_string())
    elif format == 'plist':
      print(formatter.get_plist_version_string())
    else:
      print(formatter.get_version_string(False))
