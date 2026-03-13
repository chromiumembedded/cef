#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""E2E tests for the RunInstaller DLL export."""

import json
import os
import sys
import unittest

from e2e_test_base import E2ETestBase, EXIT_SUCCESS


class TestRunInstaller(E2ETestBase):

  def _run_export(self, app_exe, config, command, marker_name, extra_env=None):
    marker_path = os.path.join(self.temp_dir, marker_name)
    env = {
        'CEF_E2E_MARKER_PATH': marker_path,
        'CEF_E2E_CONFIG_JSON': json.dumps(config),
        'CEF_E2E_COMMAND': command,
    }
    if extra_env:
      env.update(extra_env)
    self.run_installer(exe=app_exe, use_local_cdn=False, env=env, timeout=60)
    self.assertTrue(os.path.isfile(marker_path), 'Marker file not written')
    with open(marker_path) as f:
      return json.load(f)['result']

  def test_run_installer_returns_valid_json(self):
    """RunInstaller returns valid JSON with success and libcef_path."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00022-2222-2222-2222-222222222222'

    app_exe, _ = self.setup_launcher('E2E22App')

    config = {
        'appid': appid,
        'vmin': version,
        'abi_hash': abi_hash,
        'install_path': self.install_dir,
        'local_download_path': self.cdn_dir,
        'show_progress_ui': False,
    }
    if self.test_thumbprint:
      config['certificate_thumbprint'] = self.test_thumbprint
    config_json = json.dumps(config)

    marker_path = os.path.join(self.temp_dir, 'e2e_marker.json')
    env = {
        'CEF_E2E_MARKER_PATH': marker_path,
        'CEF_E2E_CONFIG_JSON': config_json,
        'CEF_E2E_COMMAND': 'install',
    }

    exit_code, _, stderr = self.run_installer(args=[],
                                              exe=app_exe,
                                              use_local_cdn=False,
                                              env=env)

    self.assertEqual(
        EXIT_SUCCESS, exit_code, f'RunInstaller failed (exit {exit_code}): '
        f'{stderr.decode(errors="replace")}')
    self.assertTrue(os.path.isfile(marker_path), 'Marker file not written')

    with open(marker_path) as f:
      marker = json.load(f)

    # Result is valid JSON with expected fields.
    result = marker.get('result', {})
    self.assertIsInstance(result, dict)
    self.assertTrue(result.get('success'), f'Not successful: {result}')
    self.assertEqual('committed', result.get('outcome'))
    self.assertNotIn('restart_recommended', result)
    self.assertTrue(result.get('libcef_path'), 'No libcef_path')
    self.assertIn('installed_version', result)
    self.assertIn(version, result.get('version_full', ''),
                  f'version_full does not contain version: {result}')

    # Version was actually installed on disk.
    self.assert_version_installed(version)

  def test_expanded_index_failure_is_hard_and_retryable(self):
    """Checked expanded-index failure leaves no fabricated registration."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e0f001-f001-f001-f001-f001f001f001'
    app_exe, _ = self.setup_launcher('E2EFaultIndexApp')
    os.makedirs(self.install_dir, exist_ok=True)
    with open(os.path.join(self.install_dir, 'versions.json'), 'w') as f:
      json.dump({'versions': []}, f)
    config = {
        'appid': appid,
        'vmin': version,
        'abi_hash': abi_hash,
        'install_path': self.install_dir,
        'local_download_path': self.cdn_dir,
        'show_progress_ui': False,
    }
    if self.test_thumbprint:
      config['certificate_thumbprint'] = self.test_thumbprint

    failed = self._run_export(app_exe, config, 'install',
                              'fault_index_failed.json',
                              {'CEF_INSTALLER_TEST_INDEX_FAULT': 'write'})
    self.assertFalse(failed['success'])
    self.assertEqual('failed', failed['outcome'])
    self.assertEqual(114, failed['error_code'])
    self.assertEqual('INDEX_ERROR', failed['error_name'])
    self.assertFalse(
        os.path.exists(os.path.join(self.install_dir, 'installer.json')))

    retried = self._run_export(app_exe, config, 'install',
                               'fault_index_retry.json')
    self.assertTrue(retried['success'], retried)
    self.assertEqual('committed', retried['outcome'])
    self.assert_version_installed(version)

  def test_database_failure_leaves_safe_partial_add(self):
    """Directory/index publication precedes the required registration save."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e0f002-f002-f002-f002-f002f002f002'
    app_exe, _ = self.setup_launcher('E2EFaultDatabaseApp')
    config = {
        'appid': appid,
        'vmin': version,
        'abi_hash': abi_hash,
        'install_path': self.install_dir,
        'local_download_path': self.cdn_dir,
        'show_progress_ui': False,
    }
    if self.test_thumbprint:
      config['certificate_thumbprint'] = self.test_thumbprint

    failed = self._run_export(app_exe, config, 'install',
                              'fault_database_failed.json',
                              {'CEF_INSTALLER_TEST_DATABASE_SAVE_FAILURE': '1'})
    self.assertFalse(failed['success'])
    self.assertEqual('DATABASE_ERROR', failed['error_name'])
    self.assert_version_installed(version)

    retried = self._run_export(app_exe, config, 'install',
                               'fault_database_retry.json')
    self.assertTrue(retried['success'], retried)
    db = self.read_database()
    self.assertIn(appid, [entry['uuid'] for entry in db.get('apps', [])])

  def test_trash_reclamation_reports_cleanup_deferred(self):
    """Logical uninstall commits before faulted physical trash reclamation."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e0f003-f003-f003-f003-f003f003f003'
    app_exe, _ = self.setup_launcher('E2EFaultTrashApp')
    config = {
        'appid': appid,
        'vmin': version,
        'abi_hash': abi_hash,
        'install_path': self.install_dir,
        'local_download_path': self.cdn_dir,
        'show_progress_ui': False,
    }
    if self.test_thumbprint:
      config['certificate_thumbprint'] = self.test_thumbprint

    installed = self._run_export(app_exe, config, 'install',
                                 'fault_trash_install.json')
    self.assertTrue(installed['success'], installed)
    deferred = self._run_export(
        app_exe, config, 'uninstall', 'fault_trash_uninstall.json',
        {'CEF_INSTALLER_TEST_FILE_OPS_FAULT': 'trash_reclaim'})
    self.assertTrue(deferred['success'], deferred)
    self.assertEqual('cleanup_deferred', deferred['outcome'])
    self.assertTrue(deferred.get('warnings'))
    self.assert_version_not_installed(version)
    trash_dir = os.path.join(self.install_dir, '.trash')
    self.assertTrue(os.path.isdir(trash_dir) and os.listdir(trash_dir))

  def test_existing_destination_quarantine_failure_and_retry(self):
    """An existing-target collision fails quarantine, then replaces on retry."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e0f004-f004-f004-f004-f004f004f004'
    app_exe, _ = self.setup_launcher('E2EFaultRepairApp')
    destination = os.path.join(self.install_dir, 'Versions', version,
                               'windows64')
    os.makedirs(destination, exist_ok=True)
    partial = os.path.join(destination, 'partial.txt')
    with open(partial, 'w') as f:
      f.write('partial')
    config = {
        'appid': appid,
        'vmin': version,
        'abi_hash': abi_hash,
        'install_path': self.install_dir,
        'local_download_path': self.cdn_dir,
        'show_progress_ui': False,
    }
    if self.test_thumbprint:
      config['certificate_thumbprint'] = self.test_thumbprint

    failed = self._run_export(
        app_exe, config, 'install', 'fault_quarantine_failed.json',
        {'CEF_INSTALLER_TEST_FILE_OPS_FAULT': 'quarantine_move'})
    self.assertFalse(failed['success'])
    self.assertEqual(117, failed['error_code'])
    self.assertEqual('QUARANTINE_ERROR', failed['error_name'])
    self.assertTrue(os.path.isfile(partial))
    staging_dir = os.path.join(self.install_dir, '.staging')
    self.assertFalse(os.path.isdir(staging_dir) and os.listdir(staging_dir))

    retried = self._run_export(app_exe, config, 'install',
                               'fault_quarantine_retry.json')
    self.assertTrue(retried['success'], retried)
    self.assertEqual('committed', retried['outcome'])
    self.assertFalse(os.path.exists(partial))
    self.assert_version_installed(version)
    self.assertFalse(os.path.isdir(staging_dir) and os.listdir(staging_dir))

  def test_invalid_parent_window_failed_schema(self):
    app_exe, _ = self.setup_launcher('E2EParentInvalid')
    config = {
        'appid': 'e2e0bad0-bad0-bad0-bad0-bad000000000',
        'vmin': self.test_version,
        'parent_window': 'invalid',
        'show_progress_ui': False,
    }
    marker_path = os.path.join(self.temp_dir, 'parent_marker.json')
    env = {
        'CEF_E2E_MARKER_PATH': marker_path,
        'CEF_E2E_CONFIG_JSON': json.dumps(config),
        'CEF_E2E_COMMAND': 'query',
    }
    self.run_installer(exe=app_exe, use_local_cdn=False, env=env)
    with open(marker_path) as f:
      result = json.load(f)['result']
    self.assertFalse(result['success'])
    self.assertEqual('failed', result['outcome'])
    self.assertIsInstance(result['error_code'], int)
    self.assertEqual('CONFIG_ERROR', result['error_name'])
    self.assertIn('parent_window', result['error_message'])

  @unittest.skipUnless(sys.maxsize > 2**32, 'requires a 64-bit process')
  def test_parent_window_above_uint32_is_accepted(self):
    app_exe, _ = self.setup_launcher('E2EParent64Bit')
    config = {
        'appid': 'e2e06464-6464-6464-6464-646464646464',
        'vmin': self.test_version,
        'parent_window': str(2**32 + 1),
        'show_progress_ui': False,
    }
    marker_path = os.path.join(self.temp_dir, 'parent_64bit_marker.json')
    env = {
        'CEF_E2E_MARKER_PATH': marker_path,
        'CEF_E2E_CONFIG_JSON': json.dumps(config),
        'CEF_E2E_COMMAND': 'query',
    }
    self.run_installer(exe=app_exe, use_local_cdn=False, env=env)
    with open(marker_path) as f:
      result = json.load(f)['result']
    self.assertNotEqual('CONFIG_ERROR', result.get('error_name'))
    self.assertNotIn('parent_window', result.get('error_message', ''))

  def test_run_installer_thread_safety(self):
    """RunInstaller thread-local storage is thread-safe."""
    version, abi_hash = self.build_cdn()
    appid_a = 'e2e00023-aaaa-aaaa-aaaa-aaaaaaaaaaaa'
    appid_b = 'e2e00023-bbbb-bbbb-bbbb-bbbbbbbbbbbb'

    app_exe, _ = self.setup_launcher('E2E23App')

    config_base = {
        'vmin': version,
        'abi_hash': abi_hash,
        'install_path': self.install_dir,
        'local_download_path': self.cdn_dir,
        'show_progress_ui': False,
    }
    if self.test_thumbprint:
      config_base['certificate_thumbprint'] = self.test_thumbprint

    config_a = dict(config_base, appid=appid_a)
    config_b = dict(config_base, appid=appid_b)

    marker_path = os.path.join(self.temp_dir, 'e2e_marker.json')
    env = {
        'CEF_E2E_MARKER_PATH': marker_path,
        'CEF_E2E_COMMAND': 'install',
        'CEF_E2E_THREADING_MODE': 'multi',
        'CEF_E2E_CONFIG_JSON_A': json.dumps(config_a),
        'CEF_E2E_CONFIG_JSON_B': json.dumps(config_b),
    }

    exit_code, _, stderr = self.run_installer(args=[],
                                              exe=app_exe,
                                              use_local_cdn=False,
                                              env=env,
                                              timeout=60)

    self.assertEqual(
        EXIT_SUCCESS, exit_code,
        f'Multi-threaded RunInstaller failed (exit {exit_code}): '
        f'{stderr.decode(errors="replace")}')
    self.assertTrue(os.path.isfile(marker_path), 'Marker file not written')

    with open(marker_path) as f:
      marker = json.load(f)

    result_a = marker.get('result_a', {})
    result_b = marker.get('result_b', {})
    self.assertTrue(result_a.get('success'),
                    f'Thread A not successful: {result_a}')
    self.assertTrue(result_b.get('success'),
                    f'Thread B not successful: {result_b}')

    # Both apps should be in the database.
    db = self.read_database()
    uuids = [a.get('uuid') for a in db.get('apps', [])]
    self.assertIn(appid_a, uuids, f'App A not in database: {db}')
    self.assertIn(appid_b, uuids, f'App B not in database: {db}')

  def test_run_installer_parallel(self):
    """RunInstaller is safe to call from multiple threads in parallel."""
    version, abi_hash = self.build_cdn()
    appid_a = 'e2e00024-aaaa-aaaa-aaaa-aaaaaaaaaaaa'
    appid_b = 'e2e00024-bbbb-bbbb-bbbb-bbbbbbbbbbbb'

    app_exe, _ = self.setup_launcher('E2E24App')

    config_base = {
        'vmin': version,
        'abi_hash': abi_hash,
        'install_path': self.install_dir,
        'local_download_path': self.cdn_dir,
        'show_progress_ui': False,
    }
    if self.test_thumbprint:
      config_base['certificate_thumbprint'] = self.test_thumbprint

    config_a = dict(config_base, appid=appid_a)
    config_b = dict(config_base, appid=appid_b)

    marker_path = os.path.join(self.temp_dir, 'e2e_marker.json')
    env = {
        'CEF_E2E_MARKER_PATH': marker_path,
        'CEF_E2E_COMMAND': 'install',
        'CEF_E2E_THREADING_MODE': 'parallel',
        'CEF_E2E_CONFIG_JSON_A': json.dumps(config_a),
        'CEF_E2E_CONFIG_JSON_B': json.dumps(config_b),
    }

    exit_code, _, stderr = self.run_installer(args=[],
                                              exe=app_exe,
                                              use_local_cdn=False,
                                              env=env,
                                              timeout=60)

    self.assertEqual(
        EXIT_SUCCESS, exit_code,
        f'Parallel RunInstaller failed (exit {exit_code}): '
        f'{stderr.decode(errors="replace")}')
    self.assertTrue(os.path.isfile(marker_path), 'Marker file not written')

    with open(marker_path) as f:
      marker = json.load(f)

    result_a = marker.get('result_a', {})
    result_b = marker.get('result_b', {})
    self.assertTrue(result_a.get('success'),
                    f'Thread A not successful: {result_a}')
    self.assertTrue(result_b.get('success'),
                    f'Thread B not successful: {result_b}')

    # Both apps should be in the database.
    db = self.read_database()
    uuids = [a.get('uuid') for a in db.get('apps', [])]
    self.assertIn(appid_a, uuids, f'App A not in database: {db}')
    self.assertIn(appid_b, uuids, f'App B not in database: {db}')
