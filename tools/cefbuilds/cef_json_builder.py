# Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import datetime
import json
import os
import re
import urllib

# Class used to build the cefbuilds JSON file. See cef_json_builder_example.py
# for example usage. See cef_json_builder_test.py for unit tests.
#
# Example JSON structure:
# {
#   "linux32": {
#     "versions": [
#       {
#         "cef_version": "3.2704.1414.g185cd6c",
#         "chromium_version": "51.0.2704.47"
#         "files": [
#           {
#             "last_modified": "2016-05-18T22:42:14.066Z"
#             "name": "cef_binary_3.2704.1414.g185cd6c_linux32.tar.bz2",
#             "sha1": "47c5cfea43912a1d1771f343de35b205f388415f"
#             "size": "48549450",
#             "type": "standard",
#           }, ...
#         ],
#       }, ...
#     ]
#   }, ...
# }
#
# Notes:
# - "files" in a given version will be sorted from newest to oldest based on the
#   "last_modified" value.
# - "versions" in a given platform will be sorted from newest to oldest based on
#   the "last_modified" value of the first (newest) "file" sub-value.
# - There will be at most one record at the "files" level for each "type".

# This date format intentionally matches the format used in Artifactory
# directory listings.
_CEF_DATE_FORMAT = "%Y-%m-%dT%H:%M:%S.%fZ"

def parse_date(date):
  return datetime.datetime.strptime(date, _CEF_DATE_FORMAT)

def format_date(date):
  return date.strftime(_CEF_DATE_FORMAT)

# Helpers to format datetime values on JSON read/write.
def cef_from_json(json_object):
  if 'last_modified' in json_object:
    json_object['last_modified'] = parse_date(json_object['last_modified'])
  return json_object

class cef_json_encoder(json.JSONEncoder):
  def default(self, o):
    if isinstance(o, datetime.datetime):
      return format_date(o)
    return o


class cef_json_builder:
  """ Class used to build the cefbuilds JSON file. """

  def __init__(self, prettyprint=False, silent=True):
    """ Create a new cef_json_builder object. """
    self._prettyprint = prettyprint
    self._silent = silent
    self._fatalerrors = False
    self.clear()

  @staticmethod
  def get_platforms():
    """ Returns the list of supported platforms. """
    return ('linux32', 'linux64', 'linuxarm', 'macosx64', 'windows32', 'windows64')

  @staticmethod
  def get_distrib_types():
    """ Returns the list of supported distribution types. """
    return ('standard', 'minimal', 'client', 'release_symbols', 'debug_symbols')

  @staticmethod
  def is_valid_version(version):
    """ Returns true if the specified CEF version is fully qualified and valid. """
    return bool(re.compile('^3.[0-9]{4,5}.[0-9]{4,5}.g[0-9a-f]{7}$').match(version))

  @staticmethod
  def is_valid_chromium_version(version):
    """ Returns true if the specified Chromium version is fully qualified and valid. """
    if version is None:
      return False
    return version == 'master' or \
        re.compile('^[1-9]{1}[0-9]{1}\.0\.[0-9]{4,5}.[0-9]{1,3}$').match(version)

  @staticmethod
  def get_file_name(version, platform, type):
    """ Returns the expected distribution file name excluding extension based on
        the input parameters. """
    if type != 'standard':
      type_str = '_' + type
    else:
      type_str = ''
    return 'cef_binary_%s_%s%s' % (version, platform, type_str)

  def clear(self):
    """ Clear the contents of this object. """
    self._data = {}
    for platform in self.get_platforms():
      self._data[platform] = {'versions': []}
    self._versions = {}
    self._queryct = 0

  def __repr__(self):
    # Return a string representation of this object.
    self._sort_versions()
    if self._prettyprint:
      return json.dumps(self._data, cls=cef_json_encoder, sort_keys=True,
                        indent=2, separators=(',', ': '))
    else:
      return json.dumps(self._data, cls=cef_json_encoder, sort_keys=True)

  def _print(self, msg):
    if self._fatalerrors:
      raise Exception(msg)
    if not self._silent:
      print msg

  def get_query_count(self):
    """ Returns the number of queries sent while building. """
    return self._queryct

  def set_chromium_version(self, cef_version, chromium_version=None):
    """ Set the matching Chromium version. If the specified Chromium version is
        invalid then it will be queried remotely. """
    if not self.is_valid_version(cef_version):
      raise Exception('Invalid CEF version: %s' % cef_version)

    if not self.is_valid_chromium_version(chromium_version):
      if cef_version in self._versions:
        # Keep the Chromium version that we already know about.
        return self._versions[cef_version]

      # Try to identify the Chromium version.
      chromium_version = 'master'
      git_hash = cef_version[-7:]
      query_url = 'https://bitbucket.org/chromiumembedded/cef/raw/%s/CHROMIUM_BUILD_COMPATIBILITY.txt' % git_hash
      self._queryct = self._queryct + 1
      if not self._silent:
        print 'Reading %s' % query_url

      try:
        # Read the remote URL contents.
        handle = urllib.urlopen(query_url)
        compat_value = handle.read().strip()
        handle.close()

        # Parse the contents.
        config = eval(compat_value, {'__builtins__': None}, None)
        if not 'chromium_checkout' in config:
          raise Exception('Unexpected contents')

        val = config['chromium_checkout']
        if val.find('refs/tags/') == 0:
          chromium_version = val[10:]
      except Exception, e:
        print 'Failed to read Chromium version information'
        raise

    if not self.is_valid_chromium_version(chromium_version):
      raise Exception('Invalid Chromium version: %s' % chromium_version)

    self._versions[cef_version] = chromium_version
    return chromium_version

  def get_chromium_version(self, cef_version):
    """ Return the matching Chromium version. If not currently known it will
        be queried remotely. """
    if cef_version in self._versions:
      return self._versions[cef_version]
    # Identify the Chromium version.
    return self.set_chromium_version(cef_version)

  def has_chromium_version(self, cef_version):
    """ Return True if a matching Chromium version is known. """
    return cef_version in self._versions

  def load(self, json_string, fatalerrors=True):
    """ Load new JSON into this object. Any existing contents will be cleared.
        If |fatalerrors| is True then any errors while loading the JSON file
        will cause an Exception to be thrown. Otherwise, malformed entries will
        will be discarded. Unrecognized keys will always be discarded silently.
    """
    self.clear()

    self._fatalerrors = fatalerrors

    new_data = json.JSONDecoder(object_hook = cef_from_json).decode(json_string)

    # Validate the new data's structure.
    for platform in self._data.keys():
      if not platform in new_data:
        self._print('load: Platform %s not found' % platform)
        continue
      if not 'versions' in new_data[platform]:
        self._print('load: Missing platform key(s) for %s' % platform)
        continue

      valid_versions = []
      for version in new_data[platform]['versions']:
        if not 'cef_version' in version or \
            not 'chromium_version' in version or \
            not 'files' in version:
          self._print('load: Missing version key(s) for %s' % platform)
          continue

        valid_files = []
        found_types = []
        for file in version['files']:
          if not 'type' in file or \
              not 'name' in file or \
              not 'size' in file or \
              not 'last_modified' in file or \
              not 'sha1' in file:
            self._print('load: Missing file key(s) for %s %s' % (platform, version['cef_version']))
            continue
          (expected_platform, expected_version, expected_type) = self._parse_name(file['name'])
          if expected_platform != platform or \
              expected_version != version['cef_version'] or \
              expected_type != file['type']:
            self._print('load: File name/attribute mismatch for %s %s %s' %
                (platform, version['cef_version'], file['name']))
            continue
          self._validate_args(platform, version['cef_version'], file['type'],
                              file['size'], file['last_modified'], file['sha1'])
          if file['type'] in found_types:
            self._print('load: Duplicate %s type for %s %s' % (file['type'], platform, version['cef_version']))
            continue
          found_types.append(file['type'])
          valid_files.append({
            'type': file['type'],
            'name': file['name'],
            'size': file['size'],
            'last_modified': file['last_modified'],
            'sha1': file['sha1'],
          })

        if len(valid_files) > 0:
          valid_versions.append({
            'cef_version': version['cef_version'],
            'chromium_version': self.set_chromium_version(version['cef_version'], version['chromium_version']),
            'files': self._sort_files(valid_files)
          })

      if len(valid_versions) > 0:
        self._data[platform]['versions'] = valid_versions

    self._fatalerrors = False

  def _sort_versions(self):
    # Sort version records by first (newest) file last_modified value.
    for platform in self._data.keys():
      for i in range(0, len(self._data[platform]['versions'])):
        self._data[platform]['versions'] = \
          sorted(self._data[platform]['versions'],
                 key=lambda k: k['files'][0]['last_modified'],
                 reverse=True)

  @staticmethod
  def _sort_files(files):
    # Sort file records by last_modified.
    return sorted(files, key=lambda k: k['last_modified'], reverse=True)

  @staticmethod
  def _parse_name(name):
    # Remove file extension.
    name_no_ext = os.path.splitext(name)[0]
    if name_no_ext[-4:] == '.tar':
      name_no_ext = name_no_ext[:-4]
    name_parts = name_no_ext.split('_')
    if len(name_parts) < 4 or name_parts[0] != 'cef' or name_parts[1] != 'binary':
      raise Exception('Invalid filename: %s' % name)

    # Remove 'cef' and 'binary'.
    del name_parts[0]
    del name_parts[0]

    type = None

    # Might be '<version>_<platform>_[debug|release]_symbols'.
    if name_parts[-1] == 'symbols':
      del name_parts[-1]
      if name_parts[-1] == 'debug' or name_parts[-1] == 'release':
        type = name_parts[-1] + '_symbols'
        del name_parts[-1]

    # Might be '<version>_<platform>_minimal'.
    if name_parts[-1] == 'minimal':
      type = 'minimal'
      del name_parts[-1]

    # Might be '<version>_<platform>_client'.
    if name_parts[-1] == 'client':
      type = 'client'
      del name_parts[-1]

    # Remainder must be '<version>_<platform>'.
    if len(name_parts) != 2:
      raise Exception('Invalid filename: %s' % name)

    if type is None:
      type = 'standard'

    version = name_parts[0]
    platform = name_parts[1]

    return [platform, version, type]

  @staticmethod
  def _validate_args(platform, version, type, size, last_modified, sha1):
    # Validate input arguments.
    if not platform in cef_json_builder.get_platforms():
      raise Exception('Unsupported platform: %s' % platform)

    if not cef_json_builder.is_valid_version(version):
      raise Exception('Invalid version: %s' % version)

    if not type in cef_json_builder.get_distrib_types():
      raise Exception('Unsupported distribution type: %s' % type)

    if int(size) <= 0:
      raise Exception('Invalid size: %s' % size)

    if not isinstance(last_modified, datetime.datetime):
      # datetime will throw a ValueException if it doesn't parse.
      parse_date(last_modified)

    if not re.compile('^[0-9a-f]{40}$').match(sha1):
      raise Exception('Invalid sha1: %s' % sha1)

  def add_file(self, name, size, last_modified, sha1):
    """ Add a file record with the specified attributes. Returns True if the
        file is added or False if a file with the same |name| and |sha1|
        already exists. """
    # Parse the file name.
    (platform, version, type) = self._parse_name(name)

    if not isinstance(size, (int, long)):
      size = int(size)
    if not isinstance(last_modified, datetime.datetime):
      last_modified = parse_date(last_modified)

    # Validate arguments.
    self._validate_args(platform, version, type, size, last_modified, sha1)

    # Find the existing version record.
    version_idx = -1;
    for i in range(0, len(self._data[platform]['versions'])):
      if self._data[platform]['versions'][i]['cef_version'] == version:
        # Check the version record.
        self._print('add_file: Check %s %s' % (platform, version))
        version_idx = i
        break

    if version_idx == -1:
      # Add a new version record.
      self._print('add_file: Add %s %s' % (platform, version))
      self._data[platform]['versions'].append({
        'cef_version': version,
        'chromium_version': self.get_chromium_version(version),
        'files': []
      })
      version_idx = len(self._data[platform]['versions']) - 1

    # Find the existing file record with matching type.
    file_changed = True
    for i in range(0, len(self._data[platform]['versions'][version_idx]['files'])):
      if self._data[platform]['versions'][version_idx]['files'][i]['type'] == type:
        existing_sha1 = self._data[platform]['versions'][version_idx]['files'][i]['sha1']
        if existing_sha1 != sha1:
          # Remove the existing file record.
          self._print('  Remove %s %s' % (name, existing_sha1))
          del self._data[platform]['versions'][version_idx]['files'][i]
        else:
          file_changed = False
        break

    if file_changed:
      # Add a new file record.
      self._print('  Add %s %s' % (name, sha1))
      self._data[platform]['versions'][version_idx]['files'].append({
        'type': type,
        'name': name,
        'size': size,
        'last_modified': last_modified,
        'sha1': sha1
      })

      # Sort file records by last_modified.
      # This is necessary for _sort_versions() to function correctly.
      self._data[platform]['versions'][version_idx]['files'] = \
          self._sort_files(self._data[platform]['versions'][version_idx]['files'])

    return file_changed

  def get_files(self, platform=None, version=None, type=None):
    """ Return the files that match the input parameters.
        All parameters are optional. Version will do partial matching. """
    results = []

    if platform is None:
      platforms = self._data.keys()
    else:
      platforms = [platform]

    for platform in platforms:
      for version_obj in self._data[platform]['versions']:
        if version is None or version_obj['cef_version'].find(version) == 0:
          for file_obj in version_obj['files']:
            if type is None or type == file_obj['type']:
              result_obj = file_obj;
              # Add additional metadata.
              result_obj['platform'] = platform
              result_obj['cef_version'] = version_obj['cef_version']
              result_obj['chromium_version'] = version_obj['chromium_version']
              results.append(result_obj)

    return results

  def get_versions(self, platform):
    """ Return all versions for the specified |platform|. """
    return self._data[platform]['versions']
