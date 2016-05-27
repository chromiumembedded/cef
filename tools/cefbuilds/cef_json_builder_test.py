# Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from cef_json_builder import cef_json_builder
import datetime
import unittest

class TestCefJSONBuilder(unittest.TestCase):

  # Write builder contents to string and then read in.
  def _verify_write_read(self, builder):
    output = str(builder)
    builder2 = cef_json_builder()
    builder2.load(output)
    self.assertEqual(output, str(builder2))

  # Add a file record for testing purposes.
  def _add_test_file(self, builder, platform='linux32', version='3.2704.1414.g185cd6c',
                     type='standard', attrib_idx=0, shouldfail=False):
    name = cef_json_builder.get_file_name(version, platform, type) + '.tar.gz'

    # Some random attribute information. sha1 must be different to trigger replacement.
    attribs = [
      {
        'date_str': '2016-05-18T22:42:15.487Z',
        'date_val': datetime.datetime(2016, 5, 18, 22, 42, 15, 487000),
        'sha1': '2d48ee05ea6385c8fe80879c98c5dd505ad4b100',
        'size': 48395610
      },
      {
        'date_str': '2016-05-14T22:42:15.487Z',
        'date_val': datetime.datetime(2016, 5, 14, 22, 42, 15, 487000),
        'sha1': '2d48ee05ea6385c8fe80879c98c5dd505ad4b200',
        'size': 48395620
      }
    ]

    # Populate the Chromium version to avoid queries.
    chromium_version = '49.0.2705.50'
    self.assertEqual(chromium_version, builder.set_chromium_version(version, chromium_version))
    self.assertEqual(0, builder.get_query_count())

    result = builder.add_file(name,
                              attribs[attrib_idx]['size'],
                              attribs[attrib_idx]['date_str'],
                              attribs[attrib_idx]['sha1'])
    # Failure should be expected when adding the same file multiple times with the same sha1.
    self.assertEqual(not shouldfail, result)

    # Return the result expected from get_files().
    return {
      'chromium_version': chromium_version,
      'sha1': attribs[attrib_idx]['sha1'],
      'name': name,
      'platform': platform,
      'last_modified': attribs[attrib_idx]['date_val'],
      'cef_version': version,
      'type': type,
      'size': attribs[attrib_idx]['size']
    }

  # Test with no file contents.
  def test_empty(self):
    builder = cef_json_builder()

    self._verify_write_read(builder)

    files = builder.get_files()
    self.assertEqual(len(files), 0)

  # Test add/get of a single file with the specified type.
  def _test_add_file(self, type):
    builder = cef_json_builder()

    expected = self._add_test_file(builder, type=type)
    self._verify_write_read(builder)

    files = builder.get_files()
    self.assertEqual(len(files), 1)
    self.assertEqual(expected, files[0])

  # Test add/get of a standard type file.
  def test_add_standard_file(self):
    self._test_add_file('standard')

  # Test add/get of a minimal type file.
  def test_add_minimal_file(self):
    self._test_add_file('minimal')

  # Test add/get of a client type file.
  def test_add_client_file(self):
    self._test_add_file('client')

  # Test add/get of a debug_symbols type file.
  def test_add_debug_symbols_file(self):
    self._test_add_file('debug_symbols')

  # Test add/get of a release_symbols type file.
  def test_add_release_symbols_file(self):
    self._test_add_file('release_symbols')

  # Test get_files() behavior with a single file.
  def test_get_files_single(self):
    builder = cef_json_builder()

    # Specify all values just in case the defaults change.
    expected = self._add_test_file(builder,
        platform='linux32', version='3.2704.1414.g185cd6c', type='standard')

    # No filter.
    files = builder.get_files()
    self.assertEqual(len(files), 1)
    self.assertEqual(expected, files[0])

    # Platform filter.
    files = builder.get_files(platform='linux32')
    self.assertEqual(len(files), 1)
    self.assertEqual(expected, files[0])
    files = builder.get_files(platform='linux64')
    self.assertEqual(len(files), 0)

    # Version filter exact.
    files = builder.get_files(version='3.2704.1414.g185cd6c')
    self.assertEqual(len(files), 1)
    self.assertEqual(expected, files[0])

    # Version filter partial.
    files = builder.get_files(version='3.2704')
    self.assertEqual(len(files), 1)
    self.assertEqual(expected, files[0])
    files = builder.get_files(version='3.2623')
    self.assertEqual(len(files), 0)

    # Type filter.
    files = builder.get_files(type='standard')
    self.assertEqual(len(files), 1)
    self.assertEqual(expected, files[0])
    files = builder.get_files(type='client')
    self.assertEqual(len(files), 0)

    # All filters.
    files = builder.get_files(platform='linux32', version='3.2704', type='standard')
    self.assertEqual(len(files), 1)
    self.assertEqual(expected, files[0])
    files = builder.get_files(platform='linux32', version='3.2704', type='minimal')
    self.assertEqual(len(files), 0)
    files = builder.get_files(platform='linux32', version='3.2623', type='standard')
    self.assertEqual(len(files), 0)
    files = builder.get_files(platform='linux64', version='3.2704', type='standard')
    self.assertEqual(len(files), 0)

  # Test add/get of multiple files.
  def test_add_multiple_files(self):
    builder = cef_json_builder()

    expected = []

    platforms = cef_json_builder.get_platforms()
    versions = ['3.2704.1414.g185cd6c', '3.2704.1400.gde36543']
    types = cef_json_builder.get_distrib_types()
    for platform in platforms:
      for version in versions:
        for type in types:
          expected.append(self._add_test_file(builder, platform=platform, type=type, version=version))

    self._verify_write_read(builder)

    # No filter.
    files = builder.get_files()
    self.assertEqual(len(files), len(platforms) * len(versions) * len(types))

    # Version filter.
    files = builder.get_files(version=version)
    self.assertEqual(len(files), len(platforms) * len(types))

    # Type filter.
    files = builder.get_files(type='client')
    self.assertEqual(len(files), len(platforms) * len(versions))

    # Platform filter.
    files = builder.get_files(platform='windows32')
    self.assertEqual(len(files), len(types) * len(versions))

    # All filters.
    idx = 0
    for platform in platforms:
      for version in versions:
        for type in types:
          files = builder.get_files(platform=platform, type=type, version=version)
          self.assertEqual(len(files), 1)
          self.assertEqual(expected[idx], files[0])
          idx = idx + 1

  # Test add/get/replace of multiple files.
  def test_replace_all_files(self):
    builder = cef_json_builder()

    version = '3.2704.1414.g185cd6c'
    platforms = ['linux32', 'linux64']
    types = ['standard', 'minimal']

    # Initial file versions.
    for platform in platforms:
      for type in types:
        self._add_test_file(builder, platform=platform, type=type, version=version)

    # No filter.
    files = builder.get_files()
    self.assertEqual(len(files), len(platforms) * len(types))

    expected = []

    # Replace all file versions (due to new sha1).
    for platform in platforms:
      for type in types:
        expected.append(self._add_test_file(builder,
            platform=platform, type=type, version=version, attrib_idx=1))

    # No filter.
    files = builder.get_files()
    self.assertEqual(len(files), len(platforms) * len(types))

    # All filters.
    idx = 0
    for platform in platforms:
      for type in types:
        files = builder.get_files(platform=platform, type=type, version=version)
        self.assertEqual(len(files), 1)
        self.assertEqual(expected[idx], files[0])
        idx = idx + 1

  # Test add/get/no replace of multiple files.
  def test_replace_no_files(self):
    builder = cef_json_builder()

    version = '3.2704.1414.g185cd6c'
    platforms = ['linux32', 'linux64']
    types = ['standard', 'minimal']

    # Initial file versions.
    for platform in platforms:
      for type in types:
        self._add_test_file(builder, platform=platform, type=type, version=version)

    # No filter.
    files = builder.get_files()
    self.assertEqual(len(files), len(platforms) * len(types))

    expected = []

    # Replace no file versions (due to same sha1).
    for platform in platforms:
      for type in types:
        expected.append(self._add_test_file(builder,
            platform=platform, type=type, version=version, shouldfail=True))

    # No filter.
    files = builder.get_files()
    self.assertEqual(len(files), len(platforms) * len(types))

    # All filters.
    idx = 0
    for platform in platforms:
      for type in types:
        files = builder.get_files(platform=platform, type=type, version=version)
        self.assertEqual(len(files), 1)
        self.assertEqual(expected[idx], files[0])
        idx = idx + 1

  # Test Chromium version.
  def test_chromium_version(self):
    builder = cef_json_builder()

    self.assertTrue(builder.is_valid_chromium_version('master'))
    self.assertTrue(builder.is_valid_chromium_version('49.0.2704.0'))
    self.assertTrue(builder.is_valid_chromium_version('49.0.2704.50'))
    self.assertTrue(builder.is_valid_chromium_version('49.0.2704.100'))

    self.assertFalse(builder.is_valid_chromium_version(None))
    self.assertFalse(builder.is_valid_chromium_version('09.0.2704.50'))
    self.assertFalse(builder.is_valid_chromium_version('00.0.0000.00'))
    self.assertFalse(builder.is_valid_chromium_version('foobar'))

    # The Git hashes must exist but the rest of the CEF version can be fake.
    versions = (
      ('3.2704.1414.g185cd6c', '51.0.2704.47'),
      ('3.2623.9999.gb90a3be', '49.0.2623.110'),
      ('3.2623.9999.g2a6491b', '49.0.2623.87'),
      ('3.9999.9999.gab2636b', 'master'),
    )

    # Test with no query.
    for (cef, chromium) in versions:
      self.assertFalse(builder.has_chromium_version(cef))
      # Valid version specified, so no query sent.
      self.assertEqual(chromium, builder.set_chromium_version(cef, chromium))
      self.assertEqual(chromium, builder.get_chromium_version(cef))
      self.assertTrue(builder.has_chromium_version(cef))
      # Ignore attempts to set invalid version after setting valid version.
      self.assertEqual(chromium, builder.set_chromium_version(cef, None))

    self.assertEqual(0, builder.get_query_count())
    builder.clear()

    # Test with query.
    for (cef, chromium) in versions:
      self.assertFalse(builder.has_chromium_version(cef))
      # No version known, so query sent.
      self.assertEqual(chromium, builder.get_chromium_version(cef))
      self.assertTrue(builder.has_chromium_version(cef))

    self.assertEqual(len(versions), builder.get_query_count())


# Program entry point.
if __name__ == '__main__':
  unittest.main()
