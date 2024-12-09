# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from __future__ import print_function
from file_util import read_json_file

# Experimental version. Always the last value.
EXP_VERSION = '999999'
EXP_NAME = 'EXPERIMENTAL'

# Next version. Always the next to last value.
NEXT_VERSION = '999998'
NEXT_NAME = 'NEXT'

UNTRACKED_VERSIONS = (EXP_VERSION, NEXT_VERSION)
UNTRACKED_NAMES = (EXP_NAME, NEXT_NAME)

VERSIONS_JSON_FILE = 'cef_api_versions.json'
UNTRACKED_JSON_FILE = 'cef_api_untracked.json'


def version_make(major_version, revision):
  """ Make a tracked version from components. """
  # 9999 is reserved for untracked placeholder values. This allows for ~898 years
  # of Chromium major versions at the current (end of 2024) rate of 11 per year.
  # If this breaks for you please file an issue via Ouija board and/or psychic medium.
  assert major_version > 0 and major_version < 9999, major_version
  assert revision >= 0 and revision <= 99, revision
  if major_version < 1000:
    return '%03d%02d' % (major_version, revision)
  return '%d%02d' % (major_version, revision)


def version_tracked(version):
  """ Returns true if version is in tracked format. """
  return (len(version) == 5 or len(version) == 6) and version.isnumeric() and \
         not version in UNTRACKED_VERSIONS


def version_parse(version):
  """ Parse a tracked version into components. """
  assert version_tracked(version), version
  split = 3 if len(version) == 5 else 4
  return (int(version[0:split]), int(version[split:]))


def version_valid(version):
  """ Returns true if version is valid. """
  # Untracked versions must be referenced by name instead of number.
  return version in UNTRACKED_NAMES or version_tracked(version)


def version_valid_for_next(version, ref_version, allow_exp=True):
  """ Returns true if version is valid as a replacement for NEXT. """
  version = version.upper()
  if allow_exp and version == EXP_NAME:
    return True
  # Must be valid and not NEXT.
  if not version_valid(version) or version == NEXT_NAME:
    return False
  # Must be >= ref_version.
  if version_as_numeric(version) < int(ref_version):
    return False
  # Must have the same major version number as ref_version.
  return version_parse(version)[0] == version_parse(ref_version)[0]


def read_version_last(api_versions_file):
  json_versions = read_json_file(api_versions_file)
  if not bool(json_versions):
    return None
  assert 'last' in json_versions, api_versions_file
  return json_versions['last']


def read_version_files(api_versions_file,
                       api_untracked_file,
                       initialize,
                       combine=False):
  initialized = False

  if combine:
    initialize = True

  json_versions = read_json_file(api_versions_file)
  if not bool(json_versions):
    if initialize:
      json_versions = {
          'hashes': {},
      }
      initialized = True
    else:
      json_version = None
  else:
    assert 'hashes' in json_versions, api_versions_file
    assert 'last' in json_versions, api_versions_file
    assert 'min' in json_versions, api_versions_file

  json_untracked = read_json_file(api_untracked_file)
  if not bool(json_untracked):
    if initialize:
      json_untracked = {
          'hashes': {},
      }
    else:
      json_untracked = None
  else:
    assert 'hashes' in json_untracked, api_untracked_file
    for version in json_untracked['hashes']:
      assert version in UNTRACKED_VERSIONS, api_untracked_file

  if combine:
    if bool(json_untracked['hashes']):
      json_versions['hashes'].update(json_untracked['hashes'])
    return (json_versions, initialized)

  return (json_versions, json_untracked, initialized)


def version_label(version):
  if version == EXP_VERSION or version == EXP_NAME:
    return 'experimental version'
  if version == NEXT_VERSION or version == NEXT_NAME:
    return 'next version'
  return 'version ' + version


def version_as_numeric(version):
  """ Returns version as a numeric value. """
  version = version.upper()
  assert version_valid(version), version
  if version == EXP_NAME:
    version = EXP_VERSION
  elif version == NEXT_NAME:
    version = NEXT_VERSION
  return int(version)


def version_as_variable(version):
  """ Returns version as a variable for use in C/C++ files. """
  version = version.upper()
  assert version_valid(version), version
  if not version.isnumeric():
    return 'CEF_' + version
  return version


def version_as_metadata(version):
  """ Returns version as metadata for comments in C++ header files. """
  version = version.upper()
  assert version_valid(version), version
  return version.lower()
