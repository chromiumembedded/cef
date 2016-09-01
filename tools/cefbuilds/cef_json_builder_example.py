# Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

# This example utility uses the cef_json_builder class to create and update an
# index.json file in the same directory as this file. Example usage:
#
# Add files for macosx64 platform at the specified version:
# > python cef_json_builder_example.py add macosx64 3.2704.1416.g185cd6c 51.0.2704.47
#
# Add files for all platforms at the specified version:
# > python cef_json_builder_example.py add all 3.2704.1416.g185cd6c 51.0.2704.47
#
# See cef_json_builder.get_platforms() for the list of supported platforms.
#
# After creating an index.json file you can use the cef_html_builder.py tool to
# create an HTML file.

from cef_json_builder import cef_json_builder
import datetime
import os
import random
import string
import sys

# Create a fake sha1 checksum value.
def make_fake_sha1():
  return ''.join(random.SystemRandom().choice('abcdef' + string.digits) for _ in range(40))

# Create a fake file size value.
def make_fake_size():
  return random.randint(30000000, 60000000)

# Create fake file info based on |platform| and |version|.
def make_fake_file_info(platform, version, type):
  return {
    'name': cef_json_builder.get_file_name(version, platform, type) + '.tar.gz',
    'size': make_fake_size(),
    'lastModified': datetime.datetime.now(),
    'sha1': make_fake_sha1()
  }

# Returns a list of fake files based on |platform| and |version|.
def create_fake_files(platform, version):
  files = []

  # All platforms create standard, minimal and client distributions.
  files.append(make_fake_file_info(platform, version, 'standard'))
  files.append(make_fake_file_info(platform, version, 'minimal'))
  files.append(make_fake_file_info(platform, version, 'client'))

  # Windows and OS X platforms create debug and release symbols.
  if platform.find('windows') == 0 or platform.find('macosx') == 0:
    files.append(make_fake_file_info(platform, version, 'debug_symbols'))
    files.append(make_fake_file_info(platform, version, 'release_symbols'))

  return files


# Program entry point.
if __name__ == '__main__':
  # Verify command-line arguments.
  if len(sys.argv) < 5 or sys.argv[1] != 'add':
    sys.stderr.write('Usage: %s add <platform> <cef_version> <chromium_version>' % sys.argv[0])
    sys.exit()

  # Requested platform.
  if sys.argv[2] == 'all':
    platforms = cef_json_builder.get_platforms()
  elif sys.argv[2] in cef_json_builder.get_platforms():
    platforms = [sys.argv[2]]
  else:
    sys.stderr.write('Invalid platform: %s' % platform)
    sys.exit()

  # Requested CEF version.
  cef_version = sys.argv[3]
  if not cef_json_builder.is_valid_version(cef_version):
    sys.stderr.write('Invalid CEF version: %s' % cef_version)
    sys.exit()

  # Requested Chromium version.
  chromium_version = sys.argv[4]
  if not cef_json_builder.is_valid_chromium_version(chromium_version):
    sys.stderr.write('Invalid Chromium version: %s' % chromium_version)
    sys.exit()

  # Write the JSON file in the same directory as this file.
  current_dir = os.path.dirname(__file__)
  json_file = os.path.join(current_dir, 'index.json')

  # Create the builder object. Enable pretty printing and extra output for
  # example purposes.
  builder = cef_json_builder(prettyprint=True, silent=False)

  # Map the CEF version to the Chromium version to avoid a remote query.
  builder.set_chromium_version(cef_version, chromium_version)

  # Load the existing JSON file, if any. Ignore format errors for example
  # purposes.
  if os.path.exists(json_file):
    print '--> Reading index.json'
    with open(json_file, 'r') as f:
      builder.load(f.read(), fatalerrors=False)

  # Create fake file info based on |platform| and |version|. A real
  # implementation should retrieve the list of files from an external source
  # like a Web or filesystem directory listing. If using Artifactory, for
  # example, then "size", "lastModified" and "sha1" attributes would be included
  # in the directory listing metadata.
  # For this example we:
  # - Always use now() as the last modified date. Consequently newly added files
  #   will always be listed at the top of the JSON platform versions list.
  # - Always create a new (fake) sha1 checksum value for each file. Consequently
  #   duplicate calls for the same |platform| + |version| will always replace
  #   the existing entries. In real implementations the sha1 may be the same
  #   across calls if the file contents have not changed, in which case
  #   cef_json_builder.add_file() will return False and upload of the file
  #   should be skipped.
  new_files = []
  for platform in platforms:
    new_files.extend(create_fake_files(platform, cef_version))

  # Add new files to the builder.
  changed_files = []
  for file in new_files:
    if builder.add_file(file['name'], file['size'], file['lastModified'], file['sha1']):
      changed_files.append(file)

  if len(changed_files) > 0:
    # Write the updated JSON file.
    print '--> Writing index.json'
    with open(json_file, 'w') as f:
      f.write(str(builder))

    # A real implementation would now upload the changed files.
    for file in changed_files:
      print '--> Upload file %s' % file['name']
