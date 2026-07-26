# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import json
import os
import tempfile
import unittest
from unittest import mock

from generator_test_util import read_golden
from generator_test_util import run_generator_script
from generator_test_util import testdata_dir
from make_installer_config import main
from make_installer_config import validate_abi_hash
from make_installer_config import validate_uuid
from make_installer_config import validate_version


class MakeInstallerConfigTest(unittest.TestCase):

  def _arguments(self,
                 directory,
                 template=None,
                 api_versions=None,
                 output_basename='installer'):
    return [
        '--template', template or testdata_dir('installer.json.in'),
        '--api-versions', api_versions or testdata_dir('api_versions.json'),
        '--output-json',
        os.path.join(directory, output_basename + '.json'), '--output-rc',
        os.path.join(directory, output_basename + '.rc'), '--resource-name',
        'CEF_INSTALLER_CONFIG'
    ]

  def test_validators_accept_boundaries_and_reject_bad_values(self):
    self.assertTrue(validate_uuid('A3B9C4D5-E6F7-4A8B-9C0D-E1F2A3B4C5D6'))
    self.assertTrue(validate_uuid('a3b9c4d5-e6f7-4a8b-9c0d-e1f2a3b4c5d6'))
    self.assertFalse(validate_uuid('not-a-uuid'))
    self.assertTrue(validate_version('0.0'))
    self.assertTrue(validate_version('146.12'))
    self.assertFalse(validate_version('146'))
    self.assertFalse(validate_version('146.1.0'))
    self.assertTrue(validate_abi_hash('0123456789abcdef'))
    self.assertTrue(validate_abi_hash('ABCDEF0123456789'))
    self.assertFalse(validate_abi_hash('short'))

  def test_success_exact_bytes_override_and_stable_rewrite(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      arguments = self._arguments(temporary_directory)
      first = run_generator_script('make_installer_config.py', *arguments)
      self.assertEqual(first.returncode, 0, first.stderr)
      json_path = os.path.join(temporary_directory, 'installer.json')
      rc_path = os.path.join(temporary_directory, 'installer.rc')
      with open(json_path, encoding='utf-8') as output:
        first_json = output.read()
      with open(rc_path, encoding='utf-8') as output:
        first_rc = output.read()
      self.assertEqual(first_json,
                       read_golden('make_installer_config', 'installer.json'))
      self.assertEqual(first_rc,
                       read_golden('make_installer_config', 'installer.rc'))
      self.assertNotIn('//', first_json)
      self.assertNotIn('\n\n', first_json)

      second = run_generator_script('make_installer_config.py', *arguments)
      self.assertEqual(second.returncode, 0, second.stderr)
      with open(json_path, encoding='utf-8') as output:
        self.assertEqual(output.read(), first_json)
      with open(rc_path, encoding='utf-8') as output:
        self.assertEqual(output.read(), first_rc)

      override = run_generator_script('make_installer_config.py', *arguments,
                                      '--api-version', '13300')
      self.assertEqual(override.returncode, 0, override.stderr)
      with open(json_path, encoding='utf-8') as output:
        data = json.load(output)
      self.assertEqual(data['vmin'], '133.0')
      self.assertEqual(data['abi_hash'], '0123456789abcdef')

  def test_managed_vmax_default_override_and_stable_rewrite(self):
    with tempfile.TemporaryDirectory() as temporary_directory:
      arguments = self._arguments(
          temporary_directory,
          template=testdata_dir('installer_managed.json.in'),
          output_basename='installer_managed')
      first = run_generator_script('make_installer_config.py', *arguments)
      self.assertEqual(first.returncode, 0, first.stderr)
      json_path = os.path.join(temporary_directory, 'installer_managed.json')
      rc_path = os.path.join(temporary_directory, 'installer_managed.rc')
      with open(json_path, encoding='utf-8') as output:
        first_json = output.read()
      with open(rc_path, encoding='utf-8') as output:
        first_rc = output.read()
      self.assertEqual(
          first_json,
          read_golden('make_installer_config',
                      'installer_managed.json').rstrip('\n'))
      self.assertEqual(
          first_rc, read_golden('make_installer_config',
                                'installer_managed.rc'))
      self.assertNotIn('${VMAX}', first_json)
      first_json_mtime = os.stat(json_path).st_mtime_ns
      first_rc_mtime = os.stat(rc_path).st_mtime_ns

      second = run_generator_script('make_installer_config.py', *arguments)
      self.assertEqual(second.returncode, 0, second.stderr)
      self.assertEqual(os.stat(json_path).st_mtime_ns, first_json_mtime)
      self.assertEqual(os.stat(rc_path).st_mtime_ns, first_rc_mtime)

      override = run_generator_script('make_installer_config.py', *arguments,
                                      '--api-version', '13300')
      self.assertEqual(override.returncode, 0, override.stderr)
      with open(json_path, encoding='utf-8') as output:
        data = json.load(output)
      self.assertEqual(data['vmin'], '133.0')
      self.assertEqual(data['vmax'], '133.99')
      self.assertEqual(data['abi_hash'], '0123456789abcdef')

  def test_argparse_and_prewrite_validation_failures_leave_outputs_missing(
      self):
    invalid = run_generator_script('make_installer_config.py')
    self.assertEqual(invalid.returncode, 2)
    self.assertIn('required', invalid.stderr)

    cases = [
        ('invalid JSON', '{invalid'),
        ('invalid appid', '{"appid":"bad","vmin":"${VMIN}",'
         '"abi_hash":"${ABI_HASH}"}'),
        ('invalid vmin', '{"appid":"A3B9C4D5-E6F7-4A8B-9C0D-E1F2A3B4C5D6",'
         '"vmin":"bad","abi_hash":"${ABI_HASH}"}'),
        ('invalid vmax', '{"appid":"A3B9C4D5-E6F7-4A8B-9C0D-E1F2A3B4C5D6",'
         '"vmin":"${VMIN}","vmax":"bad",'
         '"abi_hash":"${ABI_HASH}"}'),
        ('invalid ABI', '{"appid":"A3B9C4D5-E6F7-4A8B-9C0D-E1F2A3B4C5D6",'
         '"vmin":"${VMIN}","abi_hash":"bad"}'),
    ]
    for name, contents in cases:
      with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
        template = os.path.join(directory, 'template.json.in')
        with open(template, 'w', encoding='utf-8') as output:
          output.write(contents)
        result = run_generator_script('make_installer_config.py',
                                      *self._arguments(directory, template))
        self.assertEqual(result.returncode, 1)
        self.assertFalse(
            os.path.exists(os.path.join(directory, 'installer.json')))
        self.assertFalse(os.path.exists(os.path.join(directory,
                                                     'installer.rc')))

  def test_empty_vmax_remains_valid(self):
    with tempfile.TemporaryDirectory() as directory:
      template = os.path.join(directory, 'template.json.in')
      with open(template, 'w', encoding='utf-8') as output:
        output.write('{"appid":"A3B9C4D5-E6F7-4A8B-9C0D-E1F2A3B4C5D6",'
                     '"vmin":"${VMIN}","vmax":"",'
                     '"abi_hash":"${ABI_HASH}"}')
      result = run_generator_script('make_installer_config.py',
                                    *self._arguments(directory, template))
      self.assertEqual(result.returncode, 0, result.stderr)
      with open(os.path.join(directory, 'installer.json'),
                encoding='utf-8') as output:
        self.assertEqual(json.load(output)['vmax'], '')

  def test_missing_api_fields_unreadable_template_and_partial_io_failure(self):
    with tempfile.TemporaryDirectory() as directory:
      api_path = os.path.join(directory, 'api.json')
      with open(api_path, 'w', encoding='utf-8') as output:
        json.dump({'last': '13302', 'hashes': {'13302': {}}}, output)
      missing_hash = run_generator_script(
          'make_installer_config.py',
          *self._arguments(directory, api_versions=api_path))
      self.assertEqual(missing_hash.returncode, 1)
      self.assertIn('sandbox_compat', missing_hash.stderr)

      unreadable = run_generator_script(
          'make_installer_config.py',
          *self._arguments(directory, template='/missing/template'))
      self.assertEqual(unreadable.returncode, 1)
      self.assertIn('Error reading', unreadable.stderr)

      argv = ['make_installer_config.py', *self._arguments(directory)]
      with mock.patch('make_installer_config.sys.argv', argv), mock.patch(
          'make_installer_config.write_rc_file',
          side_effect=OSError('fixture rc failure')):
        with self.assertRaisesRegex(OSError, 'fixture rc failure'):
          main()
      self.assertTrue(os.path.isfile(os.path.join(directory, 'installer.json')))
      self.assertFalse(os.path.exists(os.path.join(directory, 'installer.rc')))


if __name__ == '__main__':
  unittest.main()
