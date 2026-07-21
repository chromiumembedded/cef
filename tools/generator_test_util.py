# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
"""Shared deterministic helpers for make_* generator tests."""

from __future__ import absolute_import

import hashlib
import os
import shutil
import stat
import subprocess
import sys
import tarfile
import zipfile

FIXED_YEAR = '2026'
FIXED_DATE = '2026-01-02T03:04:05Z'
FIXED_CEF_VERSION = '123.4.5+gabcdef0+chromium-123.0.4567.89'
FIXED_GIT_HASH = 'abcdef0123456789abcdef0123456789abcdef01'
FIXED_GIT_NUMBER = '42'


def testdata_dir(*parts):
  return os.path.join(os.path.dirname(__file__), 'testdata', 'make_generators',
                      *parts)


def read_fixture(*parts, binary=False):
  mode = 'rb' if binary else 'r'
  kwargs = {} if binary else {'encoding': 'utf-8'}
  with open(testdata_dir(*parts), mode, **kwargs) as source:
    return source.read()


def read_golden(generator, filename, binary=False):
  return read_fixture('golden', generator, filename, binary=binary)


def make_fixture_header():
  """Parse the shared translator inputs into a fresh header object."""
  from cef_parser import obj_header
  header = obj_header()
  include_directory = testdata_dir('include')
  header.set_root_directory(include_directory)
  header.add_file(os.path.join(include_directory, 'cef_fixture.h'))
  header.add_file(os.path.join(include_directory, 'cef_fixture_scoped.h'))
  return header


def run_generator_script(script_name, *arguments, cwd=None):
  return subprocess.run([
      sys.executable,
      os.path.join(os.path.dirname(__file__), script_name), *arguments
  ],
                        cwd=cwd,
                        capture_output=True,
                        text=True,
                        check=False)


def sha256_bytes(data):
  return hashlib.sha256(data).hexdigest()


def sha256_file(path):
  with open(path, 'rb') as source:
    return sha256_bytes(source.read())


def copy_fixture_tree(destination):
  """Copy all shared fixtures into an isolated writable destination."""
  shutil.copytree(testdata_dir(), destination, dirs_exist_ok=True)


def output_manifest(root):
  """Return a deterministic manifest for a regular output tree."""
  root = os.path.abspath(root)
  result = []
  for current_root, directories, files in os.walk(root, followlinks=False):
    directories.sort()
    files.sort()
    entries = [(name, True) for name in directories]
    entries.extend((name, False) for name in files)
    for name, is_directory in entries:
      path = os.path.join(current_root, name)
      relative_path = os.path.relpath(path, root).replace(os.sep, '/')
      info = os.lstat(path)
      entry = {
          'path': relative_path,
          'mode': stat.S_IMODE(info.st_mode),
          'size': info.st_size,
          'sha256': None,
          'target': None,
      }
      if stat.S_ISLNK(info.st_mode):
        entry['type'] = 'symlink'
        entry['target'] = os.readlink(path)
      elif stat.S_ISDIR(info.st_mode):
        entry['type'] = 'directory'
      elif stat.S_ISREG(info.st_mode):
        entry['type'] = 'file'
        entry['sha256'] = sha256_file(path)
      else:
        entry['type'] = 'other'
      result.append(entry)
  return result


def _zip_manifest(path):
  result = []
  with zipfile.ZipFile(path, 'r') as archive:
    for info in sorted(archive.infolist(), key=lambda value: value.filename):
      mode = (info.external_attr >> 16) & 0xFFFF
      is_directory = info.is_dir()
      data = b'' if is_directory else archive.read(info.filename)
      result.append({
          'path': info.filename,
          'type': 'directory' if is_directory else 'file',
          'mode': stat.S_IMODE(mode),
          'data': data,
      })
  return result


def _tar_manifest(path):
  result = []
  with tarfile.open(path, 'r:*') as archive:
    for info in sorted(archive.getmembers(), key=lambda value: value.name):
      if info.isfile():
        extracted = archive.extractfile(info)
        data = extracted.read() if extracted else b''
        entry_type = 'file'
      elif info.isdir():
        data = b''
        entry_type = 'directory'
      elif info.issym():
        data = info.linkname.encode('utf-8')
        entry_type = 'symlink'
      else:
        data = b''
        entry_type = 'other'
      result.append({
          'path': info.name,
          'type': entry_type,
          'mode': stat.S_IMODE(info.mode),
          'data': data,
      })
  return result


def archive_manifest(path):
  """Return logical members, ignoring only archive container timestamps."""
  if zipfile.is_zipfile(path):
    return _zip_manifest(path)
  if tarfile.is_tarfile(path):
    return _tar_manifest(path)
  raise ValueError('Unsupported archive: %s' % path)


def comparable_archive_manifest(path):
  """Return a JSON-serializable logical archive manifest."""
  return [{
      'path': entry['path'],
      'type': entry['type'],
      'mode': entry['mode'],
      'size': len(entry['data']),
      'sha256': sha256_bytes(entry['data']),
  } for entry in archive_manifest(path)]
