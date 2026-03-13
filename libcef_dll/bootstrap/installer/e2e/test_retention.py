#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""E2E tests for explicit registration-retention maintenance."""

import json
import os

from e2e_test_base import (E2ETestBase, EXIT_CONFIG_ERROR, EXIT_SUCCESS,
                           FLAG_RETENTION_APPLY, FLAG_RETENTION_DRY_RUN,
                           FLAG_RETENTION_MAX_AGE_DAYS)


class TestRetention(E2ETestBase):

  def _run_export(self, app_exe, config, command, marker_name):
    marker_path = os.path.join(self.temp_dir, marker_name)
    env = {
        'CEF_E2E_MARKER_PATH': marker_path,
        'CEF_E2E_CONFIG_JSON': json.dumps(config),
        'CEF_E2E_COMMAND': command,
    }
    self.run_installer(exe=app_exe, use_local_cdn=False, env=env, timeout=60)
    self.assertTrue(os.path.isfile(marker_path), 'Marker file not written')
    with open(marker_path) as f:
      return json.load(f)['result']

  def _install(self, app_exe, appid, version, abi_hash, marker):
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
    result = self._run_export(app_exe, config, 'install', marker)
    self.assertTrue(result.get('success'), result)

  def _retention(self, app_exe, command, marker, days=180):
    return self._run_export(app_exe, {
        'install_path': self.install_dir,
        'max_age_days': days,
    }, command, marker)

  def _store_snapshot(self):
    snapshot = {}
    if not os.path.isdir(self.install_dir):
      return snapshot
    for root, _, files in os.walk(self.install_dir):
      for name in files:
        if name == 'cef_installer.log':
          continue
        path = os.path.join(root, name)
        with open(path, 'rb') as f:
          snapshot[os.path.relpath(path, self.install_dir)] = f.read()
    return snapshot

  def test_dry_run_and_apply(self):
    version, abi_hash = self.build_cdn()
    appid = 'a1000000-0000-4000-8000-000000000001'
    app_exe, _ = self.setup_launcher('RetentionE2EApply')
    self._install(app_exe, appid, version, abi_hash, 'install.json')
    evidence_path = self.write_liveness(
        appid,
        self.filetime_now() - 181 * 24 * 60 * 60 * 10000000)
    before = self._store_snapshot()

    dry_run = self._retention(app_exe, 'retention_dry_run', 'dry_run.json')

    self.assertTrue(dry_run.get('success'), dry_run)
    self.assertEqual('reclaim', dry_run['registrations'][0]['decision'])
    self.assertTrue(dry_run['versions'][0]['expected_removal'])
    self.assertEqual(before, self._store_snapshot())

    apply = self._retention(app_exe, 'retention_apply', 'apply.json')

    self.assertTrue(apply.get('success'), apply)
    self.assertTrue(apply.get('registrations_committed'))
    self.assertTrue(apply.get('versions_pruned'))
    self.assertEqual([], self.read_database().get('apps'))
    self.assertFalse(os.path.exists(evidence_path))
    self.assert_version_not_installed(version)

  def test_unknown_evidence_preserves_shared_version(self):
    version, abi_hash = self.build_cdn()
    old_appid = 'a1000000-0000-4000-8000-000000000002'
    unknown_appid = 'a1000000-0000-4000-8000-000000000003'
    app_exe, _ = self.setup_launcher('RetentionE2EShared')
    self._install(app_exe, old_appid, version, abi_hash, 'install.json')
    self.write_registration_database([{
        'uuid': old_appid,
        'platform': 'windows64',
        'vmin': version,
        'vmax': '',
        'abi_hash': abi_hash,
    }, {
        'uuid': unknown_appid,
        'platform': 'windows64',
        'vmin': version,
        'vmax': '',
        'abi_hash': abi_hash,
    }])
    self.write_liveness(old_appid,
                        self.filetime_now() - 181 * 24 * 60 * 60 * 10000000)

    result = self._retention(app_exe, 'retention_apply', 'apply.json')

    self.assertTrue(result.get('success'), result)
    apps = self.read_database().get('apps')
    self.assertEqual([unknown_appid], [app['uuid'] for app in apps])
    self.assert_version_installed(version)
    decisions = {
        item['appid']: item['decision'] for item in result['registrations']
    }
    self.assertEqual('reclaim', decisions[old_appid])
    self.assertEqual('protected', decisions[unknown_appid])

  def test_fresh_evidence_between_dry_run_and_apply_rescues(self):
    version, abi_hash = self.build_cdn()
    appid = 'a1000000-0000-4000-8000-000000000004'
    app_exe, _ = self.setup_launcher('RetentionE2ERescue')
    self._install(app_exe, appid, version, abi_hash, 'install.json')
    self.write_liveness(appid,
                        self.filetime_now() - 181 * 24 * 60 * 60 * 10000000)
    dry_run = self._retention(app_exe, 'retention_dry_run', 'dry_run.json')
    self.assertEqual('reclaim', dry_run['registrations'][0]['decision'])
    self.write_liveness(appid, self.filetime_now())

    apply = self._retention(app_exe, 'retention_apply', 'apply.json')

    self.assertTrue(apply.get('success'), apply)
    self.assertEqual([], [
        item for item in apply['registrations'] if item['decision'] == 'reclaim'
    ])
    self.assertEqual(1, len(self.read_database().get('apps')))
    self.assert_version_installed(version)

  def test_cli_custom_store_and_threshold_validation(self):
    version, abi_hash = self.build_cdn()
    appid = 'a1000000-0000-4000-8000-000000000005'
    self.install_dir = os.path.join(self.temp_dir, 'Program Files', 'CEF')
    app_exe, _ = self.setup_launcher('RetentionE2ECliInstall')
    self._install(app_exe, appid, version, abi_hash, 'install.json')
    self.write_liveness(appid,
                        self.filetime_now() - 91 * 24 * 60 * 60 * 10000000)
    self.embed_test_config(appid, version, abi_hash)

    code, stdout, _ = self.run_installer(
        args=[FLAG_RETENTION_DRY_RUN, f'{FLAG_RETENTION_MAX_AGE_DAYS}=90'])

    self.assertEqual(EXIT_SUCCESS, code)
    report = json.loads(stdout.decode('utf-8'))
    self.assertEqual('reclaim', report['registrations'][0]['decision'])

    before = self._store_snapshot()
    code, _, _ = self.run_installer(
        args=[FLAG_RETENTION_APPLY, f'{FLAG_RETENTION_MAX_AGE_DAYS}=89'])
    self.assertEqual(EXIT_CONFIG_ERROR, code)
    self.assertEqual(before, self._store_snapshot())


if __name__ == '__main__':
  import unittest
  unittest.main()
