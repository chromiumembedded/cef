# Copyright (c) 2017 The Chromium Embedded Framework Authors.
# Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os, re, sys
from clang_util import clang_format
from file_util import *
from git_util import get_changed_files

# Valid extensions for files we want to clang-format.
DEFAULT_LINT_WHITELIST_REGEX = r"(.*\.cpp|.*\.cc|.*\.h|.*\.mm)$"
DEFAULT_LINT_BLACKLIST_REGEX = r"$^"

def msg(filename, status):
  if sys.platform == 'win32':
    # Use Unix path separator.
    filename = filename.replace("\\", "/")

  if len(filename) > 60:
    # Truncate the file path in a nice way.
    filename = filename[-57:]
    pos = filename.find("/")
    if pos > 0:
      filename = filename[pos:]
    filename = "..." + filename

  print "%-60s %s" % (filename, status)

updatect = 0
def update_file(filename):
  oldcontents = read_file(filename)
  if len(oldcontents) == 0:
    msg(filename, "empty")
    return;

  newcontents = clang_format(oldcontents)
  if newcontents is None:
    raise Exception("Failed to process %s" % filename)

  if newcontents != oldcontents:
    msg(filename, "fixed")
    global updatect
    updatect += 1
    write_file(filename, newcontents)
  else:
    msg(filename, "ok")
  return

def fix_style(filenames, white_list = None, black_list = None):
  """ Execute clang-format with the specified arguments. """
  if not white_list:
    white_list = DEFAULT_LINT_WHITELIST_REGEX
  white_regex = re.compile(white_list)
  if not black_list:
    black_list = DEFAULT_LINT_BLACKLIST_REGEX
  black_regex = re.compile(black_list)

  for filename in filenames:
    if filename.find('*') > 0:
      # Expand wildcards.
      filenames.extend(get_files(filename))
      continue

    if os.path.isdir(filename):
      # Add directory contents.
      filenames.extend(get_files(os.path.join(filename, "*")))
      continue

    if not os.path.exists(filename):
      files = get_changed_files(".", filename)
      if len(files) > 0:
        filenames.extend(files)
      else:
        msg(filename, "missing")
      continue

    if white_regex.match(filename):
      if black_regex.match(filename):
        msg(filename, "ignored")
      else:
        update_file(filename)
    else:
      msg(filename, "skipped")

if __name__ == "__main__":
  if len(sys.argv) == 1:
    print "Usage: %s [file-path|git-hash|unstaged|staged] ..." % sys.argv[0]
    print "\n Format C, C++ and ObjC files using Chromium's clang-format style."
    print "\nOptions:"
    print " file-path\tProcess the specified file or directory."
    print " \t\tDirectories will be processed recursively."
    print " \t\tThe \"*\" wildcard character is supported."
    print " git-hash\tProcess all files changed in the specified Git commit."
    print " unstaged\tProcess all unstaged files in the Git repo."
    print " staged\t\tProcess all staged files in the Git repo."
    sys.exit(1)

  # Process anything passed on the command-line.
  fix_style(sys.argv[1:])
  print 'Done - Wrote %d files.' % updatect
