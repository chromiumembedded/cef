# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
import codecs
import fnmatch
from glob import iglob
from io import open
import json
import os
import shutil
import sys
import time


def read_file(path, normalize=True):
  """ Read a file. """
  if os.path.isfile(path):
    try:
      with open(path, 'r', encoding='utf-8') as f:
        # read the data
        data = f.read()
        if normalize:
          # normalize line endings
          data = data.replace("\r\n", "\n")
        return data
    except IOError as e:
      (errno, strerror) = e.args
      sys.stderr.write('ERROR: Failed to read file ' + path + ': ' + strerror)
      raise
  return None


def write_file(path, data, overwrite=True, quiet=True):
  """ Write a file. """
  if not overwrite and os.path.exists(path):
    return False

  if not quiet:
    print('Writing file %s' % path)

  try:
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
      # write the data
      if sys.version_info.major == 2:
        f.write(data.decode('utf-8'))
      else:
        f.write(data)
  except IOError as e:
    (errno, strerror) = e.args
    sys.stderr.write('ERROR: Failed to write file ' + path + ': ' + strerror)
    raise
  return True


def path_exists(path):
  """ Returns true if the path currently exists. """
  return os.path.exists(path)


def write_file_if_changed(path, data, quiet=True):
  """ Write a file if the contents have changed. Returns True if the file was written. """
  if path_exists(path):
    old_contents = read_file(path)
    assert not old_contents is None, path
  else:
    old_contents = ''

  if (data != old_contents):
    write_file(path, data, quiet=quiet)
    return True
  return False


def backup_file(path):
  """ Rename the file to a name that includes the current time stamp. """
  move_file(path, path + '.' + time.strftime('%Y-%m-%d-%H-%M-%S'))


def copy_file(src, dst, quiet=True):
  """ Copy a file. """
  if not os.path.isfile(src):
    return False

  try:
    if not quiet:
      sys.stdout.write('Transferring ' + src + ' file.\n')
    shutil.copy2(src, dst)
  except IOError as e:
    (errno, strerror) = e.args
    sys.stderr.write('ERROR: Failed to copy file from ' + src + ' to ' + dst +
                     ': ' + strerror)
    raise
  return True


def move_file(src, dst, quiet=True):
  """ Move a file. """
  if not os.path.isfile(src):
    return False

  try:
    if not quiet:
      print('Moving ' + src + ' file.')
    shutil.move(src, dst)
  except IOError as e:
    (errno, strerror) = e.args
    sys.stderr.write('ERROR: Failed to move file from ' + src + ' to ' + dst +
                     ': ' + strerror)
    raise
  return True


def copy_files(src_glob, dst_folder, quiet=True):
  """ Copy multiple files. """
  for fname in iglob(src_glob):
    dst = os.path.join(dst_folder, os.path.basename(fname))
    if os.path.isdir(fname):
      copy_dir(fname, dst, quiet)
    else:
      copy_file(fname, dst, quiet)


def remove_file(path, quiet=True):
  """ Remove the specified file. """
  try:
    if path_exists(path):
      if not quiet:
        print('Removing ' + path + ' file.')
      os.remove(path)
      return True
  except IOError as e:
    (errno, strerror) = e.args
    sys.stderr.write('ERROR: Failed to remove file ' + path + ': ' + strerror)
    raise
  return False


def copy_dir(src, dst, quiet=True):
  """ Copy a directory tree. """
  try:
    remove_dir(dst, quiet)
    if not quiet:
      print('Transferring ' + src + ' directory.')
    shutil.copytree(src, dst)
  except IOError as e:
    (errno, strerror) = e.args
    sys.stderr.write('ERROR: Failed to copy directory from ' + src + ' to ' +
                     dst + ': ' + strerror)
    raise


def remove_dir(path, quiet=True):
  """ Remove the specified directory. """
  try:
    if path_exists(path):
      if not quiet:
        print('Removing ' + path + ' directory.')
      shutil.rmtree(path)
      return True
  except IOError as e:
    (errno, strerror) = e.args
    sys.stderr.write('ERROR: Failed to remove directory ' + path + ': ' +
                     strerror)
    raise
  return False


def make_dir(path, quiet=True):
  """ Create the specified directory. """
  try:
    if not path_exists(path):
      if not quiet:
        print('Creating ' + path + ' directory.')
      os.makedirs(path)
  except IOError as e:
    (errno, strerror) = e.args
    sys.stderr.write('ERROR: Failed to create directory ' + path + ': ' +
                     strerror)
    raise


def get_files(search_glob):
  """ Returns all files matching |search_glob|. """
  recursive_glob = '**' + os.path.sep
  if recursive_glob in search_glob:
    if sys.version_info >= (3, 5):
      result = iglob(search_glob, recursive=True)
    else:
      # Polyfill for recursive glob pattern matching added in Python 3.5.
      result = get_files_recursive(*search_glob.split(recursive_glob))
  else:
    result = iglob(search_glob)

  # Sort the result for consistency across platforms.
  return sorted(result)


def get_files_recursive(directory, pattern):
  """ Returns all files in |directory| matching |pattern| recursively. """
  for root, dirs, files in os.walk(directory):
    for basename in files:
      if fnmatch.fnmatch(basename, pattern):
        filename = os.path.join(root, basename)
        yield filename


def read_version_file(path, args):
  """ Read and parse a version file (key=value pairs, one per line). """
  contents = read_file(path)
  if contents is None:
    return False
  lines = contents.split("\n")
  for line in lines:
    parts = line.split('=', 1)
    if len(parts) == 2:
      args[parts[0]] = parts[1]
  return True


def eval_file(src):
  """ Loads and evaluates the contents of the specified file. """
  return eval(read_file(src), {'__builtins__': None}, None)


def normalize_path(path):
  """ Normalizes the path separator to match the Unix standard. """
  if sys.platform == 'win32':
    return path.replace('\\', '/')
  return path


def read_json_file(path):
  """ Read and parse a JSON file. Returns a list/dictionary or None. """
  if os.path.isfile(path):
    try:
      with codecs.open(path, 'r', encoding='utf-8') as fp:
        return json.load(fp)
    except IOError as e:
      (errno, strerror) = e.args
      sys.stderr.write('ERROR: Failed to read file ' + path + ': ' + strerror)
      raise
  return None


def _bytes_encoder(z):
  if isinstance(z, bytes):
    return str(z, 'utf-8')
  else:
    type_name = z.__class__.__name__
    raise TypeError(f"Object of type {type_name} is not serializable")


def write_json_file(path, data, overwrite=True, quiet=True):
  """ Serialize and write a JSON file. Returns True on success. """
  if not overwrite and os.path.exists(path):
    return False

  if not quiet:
    print('Writing file %s' % path)

  try:
    with open(path, 'w', encoding='utf-8') as fp:
      json.dump(
          data,
          fp,
          ensure_ascii=False,
          indent=2,
          sort_keys=True,
          default=_bytes_encoder)
  except IOError as e:
    (errno, strerror) = e.args
    sys.stderr.write('ERROR: Failed to write file ' + path + ': ' + strerror)
    raise
  return True
