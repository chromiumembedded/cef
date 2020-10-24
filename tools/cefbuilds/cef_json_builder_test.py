# Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from cef_json_builder import cef_json_builder
import datetime
import unittest


class TestCefJSONBuilder(unittest.TestCase):

  # Test CEF version number matching.
  def test_valid_version(self):
    # Old-style version numbers.
    self.assertTrue(cef_json_builder.is_valid_version('3.2704.1414.g185cd6c'))
    # New-style version numbers.
    self.assertTrue(
        cef_json_builder.is_valid_version(
            '74.0.1+g62d140e+chromium-74.0.3729.6'))
    self.assertTrue(
        cef_json_builder.is_valid_version(
            '74.0.0-master.1920+g725ed88+chromium-74.0.3729.0'))
    self.assertTrue(
        cef_json_builder.is_valid_version(
            '74.0.0-my_branch.1920+g725ed88+chromium-74.0.3729.0'))

    # Must be a completely qualified version number.
    self.assertFalse(cef_json_builder.is_valid_version(None))
    self.assertFalse(cef_json_builder.is_valid_version('foobar'))
    self.assertFalse(cef_json_builder.is_valid_version('3.2704.1414'))
    self.assertFalse(cef_json_builder.is_valid_version('74.0.1+g62d140e'))
    self.assertFalse(
        cef_json_builder.is_valid_version('74.0.0-master.1920+g725ed88'))
    self.assertFalse(
        cef_json_builder.is_valid_version('74.0.0+chromium-74.0.3729.0'))

  # Test Chromium version number matching.
  def test_valid_chromium_version(self):
    self.assertTrue(cef_json_builder.is_valid_chromium_version('74.0.3729.0'))
    self.assertTrue(cef_json_builder.is_valid_chromium_version('74.0.3729.6'))
    self.assertTrue(
        cef_json_builder.is_valid_chromium_version('100.0.59200.602'))

    # self.assertFalse(cef_json_builder.is_valid_version(None))
    self.assertFalse(cef_json_builder.is_valid_chromium_version('foobar'))
    # Must be 4 numbers.
    self.assertFalse(cef_json_builder.is_valid_chromium_version('74.0.3729'))
    self.assertFalse(cef_json_builder.is_valid_chromium_version('74.0.3729.A'))
    # 1st number is max 3 digits with no leading 0's.
    self.assertFalse(
        cef_json_builder.is_valid_chromium_version('7400.0.3729.6'))
    self.assertFalse(cef_json_builder.is_valid_chromium_version('04.0.3729.6'))
    # 2nd number is always 0.
    self.assertFalse(cef_json_builder.is_valid_chromium_version('74.1.3729.6'))
    # 3rd number is max 5 digits with no leading 0's.
    self.assertFalse(cef_json_builder.is_valid_chromium_version('74.0.0372.6'))
    self.assertFalse(
        cef_json_builder.is_valid_chromium_version('74.0.372900.6'))
    # 4rd number is max 3 digits with no leading 0's.
    self.assertFalse(
        cef_json_builder.is_valid_chromium_version('74.0.3729.6000'))
    self.assertFalse(cef_json_builder.is_valid_chromium_version('74.0.3729.06'))
    pass

  # Write builder contents to string and then read in.
  def _verify_write_read(self, builder):
    output = str(builder)
    builder2 = cef_json_builder()
    builder2.load(output)
    self.assertEqual(output, str(builder2))

  # Add a file record for testing purposes.
  def _add_test_file(self,
                     builder,
                     platform='linux32',
                     version='3.2704.1414.g185cd6c',
                     type='standard',
                     channel='stable',
                     attrib_idx=0,
                     shouldfail=False):
    name = cef_json_builder.get_file_name(version, platform, type,
                                          channel) + '.tar.gz'

    # Some random attribute information. sha1 must be different to trigger replacement.
    attribs = [{
        'date_str': '2016-05-18T22:42:15.487Z',
        'date_val': datetime.datetime(2016, 5, 18, 22, 42, 15, 487000),
        'sha1': '2d48ee05ea6385c8fe80879c98c5dd505ad4b100',
        'size': 48395610
    }, {
        'date_str': '2016-05-14T22:42:15.487Z',
        'date_val': datetime.datetime(2016, 5, 14, 22, 42, 15, 487000),
        'sha1': '2d48ee05ea6385c8fe80879c98c5dd505ad4b200',
        'size': 48395620
    }]

    # Populate the Chromium version to avoid queries.
    chromium_version = '49.0.2705.50'
    self.assertEqual(chromium_version,
                     builder.set_chromium_version(version, chromium_version))
    self.assertEqual(0, builder.get_query_count())

    result = builder.add_file(name, attribs[attrib_idx]['size'],
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
        'channel': channel,
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
  def _test_add_file(self, type, channel='stable'):
    builder = cef_json_builder()

    expected = self._add_test_file(builder, type=type, channel=channel)
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

  # Test add/get of a standard type file in beta channel.
  def test_add_standard_file_beta(self):
    self._test_add_file('standard', channel='beta')

  # Test add/get of a minimal type file in beta channel.
  def test_add_minimal_file_beta(self):
    self._test_add_file('minimal', channel='beta')

  # Test add/get of a client type file in beta channel.
  def test_add_client_file_beta(self):
    self._test_add_file('client', channel='beta')

  # Test add/get of a debug_symbols type file in beta channel.
  def test_add_debug_symbols_file_beta(self):
    self._test_add_file('debug_symbols', channel='beta')

  # Test add/get of a release_symbols type file in beta channel.
  def test_add_release_symbols_file_beta(self):
    self._test_add_file('release_symbols', channel='beta')

  # Test get_files() behavior with a single file.
  def test_get_files_single(self):
    # yapf: disable
    versions = (
        ('3.2704.1414.g185cd6c', '3.2704'),
        ('74.0.1+g62d140e+chromium-74.0.3729.6', '74.0'),
    )
    # yapf: enable

    for version, partial_version in versions:
      builder = cef_json_builder()

      # Specify all values just in case the defaults change.
      expected = self._add_test_file(
          builder, platform='linux32', version=version, type='standard')

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
      files = builder.get_files(version=version)
      self.assertEqual(len(files), 1)
      self.assertEqual(expected, files[0])

      # Version filter partial.
      files = builder.get_files(version=partial_version)
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
      files = builder.get_files(
          platform='linux32', version=partial_version, type='standard')
      self.assertEqual(len(files), 1)
      self.assertEqual(expected, files[0])
      files = builder.get_files(
          platform='linux32', version=partial_version, type='minimal')
      self.assertEqual(len(files), 0)
      files = builder.get_files(
          platform='linux32', version='3.2623', type='standard')
      self.assertEqual(len(files), 0)
      files = builder.get_files(
          platform='linux64', version=partial_version, type='standard')
      self.assertEqual(len(files), 0)

  # Test add/get of multiple files.
  def test_add_multiple_files(self):
    builder = cef_json_builder()

    expected = []

    platforms = cef_json_builder.get_platforms()
    versions = [
        # Old-style version numbers.
        '3.2704.1414.g185cd6c',
        '3.2704.1400.gde36543',
        # New-style version numbers.
        '74.0.1+g62d140e+chromium-74.0.3729.6'
    ]
    types = cef_json_builder.get_distrib_types()
    for platform in platforms:
      for version in versions:
        for type in types:
          expected.append(
              self._add_test_file(
                  builder, platform=platform, type=type, version=version))

    self._verify_write_read(builder)

    # No filter.
    files = builder.get_files()
    self.assertEqual(len(files), len(platforms) * len(versions) * len(types))

    # Version filter.
    for version in versions:
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
          files = builder.get_files(
              platform=platform, type=type, version=version)
          self.assertEqual(len(files), 1)
          self.assertEqual(expected[idx], files[0])
          idx += 1

  # Test add/get/replace of multiple files.
  def test_replace_all_files(self):
    versions = ['3.2704.1414.g185cd6c', '74.0.1+g62d140e+chromium-74.0.3729.6']
    platforms = ['linux32', 'linux64']
    types = ['standard', 'minimal']

    for version in versions:
      builder = cef_json_builder()

      # Initial file versions.
      for platform in platforms:
        for type in types:
          self._add_test_file(
              builder, platform=platform, type=type, version=version)

      # No filter.
      files = builder.get_files()
      self.assertEqual(len(files), len(platforms) * len(types))

      expected = []

      # Replace all file versions (due to new sha1).
      for platform in platforms:
        for type in types:
          expected.append(
              self._add_test_file(
                  builder,
                  platform=platform,
                  type=type,
                  version=version,
                  attrib_idx=1))

      # No filter.
      files = builder.get_files()
      self.assertEqual(len(files), len(platforms) * len(types))

      # All filters.
      idx = 0
      for platform in platforms:
        for type in types:
          files = builder.get_files(
              platform=platform, type=type, version=version)
          self.assertEqual(len(files), 1)
          self.assertEqual(expected[idx], files[0])
          idx += 1

  # Test add/get/no replace of multiple files.
  def test_replace_no_files(self):
    versions = ['3.2704.1414.g185cd6c', '74.0.1+g62d140e+chromium-74.0.3729.6']
    platforms = ['linux32', 'linux64']
    types = ['standard', 'minimal']

    for version in versions:
      builder = cef_json_builder()

      # Initial file versions.
      for platform in platforms:
        for type in types:
          self._add_test_file(
              builder, platform=platform, type=type, version=version)

      # No filter.
      files = builder.get_files()
      self.assertEqual(len(files), len(platforms) * len(types))

      expected = []

      # Replace no file versions (due to same sha1).
      for platform in platforms:
        for type in types:
          expected.append(
              self._add_test_file(
                  builder,
                  platform=platform,
                  type=type,
                  version=version,
                  shouldfail=True))

      # No filter.
      files = builder.get_files()
      self.assertEqual(len(files), len(platforms) * len(types))

      # All filters.
      idx = 0
      for platform in platforms:
        for type in types:
          files = builder.get_files(
              platform=platform, type=type, version=version)
          self.assertEqual(len(files), 1)
          self.assertEqual(expected[idx], files[0])
          idx += 1

  # Test Chromium version.
  def test_chromium_version(self):
    builder = cef_json_builder()

    # For old-style CEF version numbers the Git hashes must exist but the rest
    # of the CEF version can be fake.
    # yapf: disable
    versions = (
        # Old-style version numbers.
        ('3.2704.1414.g185cd6c', '51.0.2704.47'),
        ('3.2623.9999.gb90a3be', '49.0.2623.110'),
        ('3.2623.9999.g2a6491b', '49.0.2623.87'),
        ('3.9999.9999.gab2636b', 'master'),
        # New-style version numbers.
        ('74.0.1+g62d140e+chromium-74.0.3729.6', '74.0.3729.6'),
        ('74.0.0-master.1920+g725ed88+chromium-74.0.3729.0', '74.0.3729.0'),
        ('74.0.0-my_branch.1920+g725ed88+chromium-74.0.3729.0', '74.0.3729.0'),
    )
    # yapf: enable

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
      # No version known, so query sent for old-style version numbers.
      self.assertEqual(chromium, builder.get_chromium_version(cef))
      self.assertTrue(builder.has_chromium_version(cef))

    # Only old-style version numbers.
    self.assertEqual(4, builder.get_query_count())


# Program entry point.
if __name__ == '__main__':
  unittest.main()
