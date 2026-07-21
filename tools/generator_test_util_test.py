# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import io
import os
import stat
import tarfile
import tempfile
import unittest
import zipfile

from generator_test_util import archive_manifest
from generator_test_util import comparable_archive_manifest
from generator_test_util import copy_fixture_tree
from generator_test_util import output_manifest
from generator_test_util import sha256_bytes


class GeneratorTestUtilTest(unittest.TestCase):

  def test_output_manifest_records_files_directories_modes_and_symlinks(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      child = os.path.join(temporary_directory, 'child')
      os.mkdir(child)
      file_path = os.path.join(child, 'value.txt')
      with open(file_path, 'wb') as output:
        output.write(b'value\n')

      manifest = output_manifest(temporary_directory)

      self.assertEqual([entry['path'] for entry in manifest],
                       ['child', 'child/value.txt'])
      self.assertEqual(manifest[0]['type'], 'directory')
      self.assertEqual(manifest[1]['type'], 'file')
      self.assertEqual(manifest[1]['size'], 6)
      self.assertEqual(manifest[1]['sha256'], sha256_bytes(b'value\n'))

  @unittest.skipIf(os.name == 'nt',
                   'Windows symlink creation and modes are host-dependent')
  def test_output_manifest_records_posix_modes_and_symlinks(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      file_path = os.path.join(temporary_directory, 'value.txt')
      with open(file_path, 'wb') as output:
        output.write(b'value\n')
      os.chmod(file_path, 0o640)
      os.symlink('value.txt', os.path.join(temporary_directory, 'value-link'))

      by_path = {
          entry['path']: entry for entry in output_manifest(temporary_directory)
      }
      self.assertEqual(by_path['value.txt']['mode'], 0o640)
      self.assertEqual(by_path['value-link']['type'], 'symlink')
      self.assertEqual(by_path['value-link']['target'], 'value.txt')

  def test_zip_manifest_compares_logical_members_not_timestamps(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      archives = []
      for index, year in enumerate((2020, 2024)):
        path = os.path.join(temporary_directory, 'archive%d.zip' % index)
        with zipfile.ZipFile(path, 'w') as archive:
          info = zipfile.ZipInfo('bin/tool', (year, 1, 2, 3, 4, 6))
          info.external_attr = (stat.S_IFREG | 0o755) << 16
          archive.writestr(info, b'tool')
        archives.append(path)

      with open(archives[0], 'rb') as first_archive:
        first_bytes = first_archive.read()
      with open(archives[1], 'rb') as second_archive:
        second_bytes = second_archive.read()
      self.assertNotEqual(first_bytes, second_bytes)
      self.assertEqual(comparable_archive_manifest(archives[0]),
                       comparable_archive_manifest(archives[1]))
      self.assertEqual(archive_manifest(archives[0])[0]['data'], b'tool')

  def test_tar_manifest_records_member_bytes_mode_and_symlink_target(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      archive_path = os.path.join(temporary_directory, 'archive.tar')
      with tarfile.open(archive_path, 'w') as archive:
        directory = tarfile.TarInfo('package')
        directory.type = tarfile.DIRTYPE
        directory.mode = 0o755
        archive.addfile(directory)
        file_info = tarfile.TarInfo('package/tool')
        file_info.mode = 0o755
        file_info.size = len(b'contents')
        archive.addfile(file_info, io.BytesIO(b'contents'))
        link_info = tarfile.TarInfo('package/tool-link')
        link_info.type = tarfile.SYMTYPE
        link_info.linkname = 'tool'
        archive.addfile(link_info)

      manifest = archive_manifest(archive_path)

      by_path = {entry['path']: entry for entry in manifest}
      self.assertEqual(by_path['package/tool']['data'], b'contents')
      self.assertEqual(by_path['package/tool']['mode'], 0o755)
      self.assertEqual(by_path['package/tool-link']['type'], 'symlink')
      self.assertEqual(by_path['package/tool-link']['data'], b'tool')

  def test_fixture_copy_is_isolated(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      copy_fixture_tree(temporary_directory)
      copied = os.path.join(temporary_directory, 'api_versions.json')
      self.assertTrue(os.path.isfile(copied))
      with open(copied, 'a', encoding='utf-8') as output:
        output.write('local change\n')


if __name__ == '__main__':
  unittest.main()
