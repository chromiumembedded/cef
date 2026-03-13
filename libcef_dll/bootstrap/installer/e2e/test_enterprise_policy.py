#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""End-to-end coverage for the isolated enterprise policy registry seam.

Tests use HKCU by default so normal E2E runs exercise real registry I/O.
Set CEF_INSTALLER_E2E_POLICY_HKLM=1 in an elevated environment to rerun the
same coverage against the production hive.
"""

import os
import uuid
import winreg

from e2e_test_base import (E2ETestBase, EXIT_POLICY_DENIED, EXIT_POLICY_ERROR,
                           EXIT_SUCCESS, FLAG_UPDATE)


class TestEnterprisePolicy(E2ETestBase):

  def setUp(self):
    super().setUp()
    self.policy_subkey = ('SOFTWARE\\CEF\\InstallerPolicyTests\\' +
                          str(uuid.uuid4()))
    self.policy_hive_name = 'HKCU'
    self.policy_hive = winreg.HKEY_CURRENT_USER
    if os.environ.get('CEF_INSTALLER_E2E_POLICY_HKLM') == '1':
      self.policy_hive_name = 'HKLM'
      self.policy_hive = winreg.HKEY_LOCAL_MACHINE
    access = winreg.KEY_READ | winreg.KEY_WRITE | winreg.KEY_WOW64_64KEY
    try:
      self.policy_key = winreg.CreateKeyEx(self.policy_hive, self.policy_subkey,
                                           0, access)
    except PermissionError as error:
      self.skipTest(f'isolated {self.policy_hive_name} policy fixture '
                    f'requires permission: {error}')
    self.addCleanup(self._cleanup_policy_key)

  def _cleanup_policy_key(self):
    if getattr(self, 'policy_key', None):
      self.policy_key.Close()
      self.policy_key = None
    try:
      winreg.DeleteKeyEx(self.policy_hive, self.policy_subkey,
                         winreg.KEY_WOW64_64KEY, 0)
    except FileNotFoundError:
      pass

  def _policy_env(self):
    return {
        'CEF_INSTALLER_POLICY_TEST_KEY': self.policy_subkey,
        'CEF_INSTALLER_POLICY_TEST_HIVE': self.policy_hive_name,
    }

  def _embed_config(self):
    self.embed_test_config('e2e00004-0000-0000-0000-000000000001',
                           self.test_version,
                           enable_explicit_modes=True)

  def test_enterprise_policy_malformed_returns_119(self):
    self._embed_config()
    # CdnUrls must be REG_MULTI_SZ; a REG_SZ is a loud policy error.
    winreg.SetValueEx(self.policy_key, 'CdnUrls', 0, winreg.REG_SZ,
                      'https://invalid-type.example/')
    code, _, _ = self.run_installer(args=[FLAG_UPDATE], env=self._policy_env())
    self.assertEqual(EXIT_POLICY_ERROR, code)
    self.assertFalse(os.path.exists(self.install_dir))

  def test_enterprise_policy_disable_downloads_denies_update(self):
    self._embed_config()
    winreg.SetValueEx(self.policy_key, 'DisableDownloads', 0, winreg.REG_DWORD,
                      1)
    code, _, _ = self.run_installer(args=[FLAG_UPDATE], env=self._policy_env())
    self.assertEqual(EXIT_POLICY_DENIED, code)
    self.assertFalse(os.path.exists(self.install_dir))

  def test_enterprise_policy_mirror_overrides_operation_source(self):
    version, _ = self.build_cdn()
    self.embed_test_config('e2e00004-0000-0000-0000-000000000002',
                           version,
                           enable_explicit_modes=True)
    winreg.SetValueEx(self.policy_key, 'DownloadPath', 0, winreg.REG_SZ,
                      self.cdn_dir)
    conflicting = os.path.join(self.temp_dir, 'conflicting-empty-mirror')
    os.makedirs(conflicting)
    code, _, stderr = self.run_installer(
        args=[FLAG_UPDATE, f'--cef-download-path={conflicting}'],
        use_local_cdn=False,
        env=self._policy_env(),
        timeout=60)
    self.assertEqual(EXIT_SUCCESS, code, stderr.decode(errors='replace'))
    self.assert_version_installed(version)


if __name__ == '__main__':
  import unittest
  unittest.main()
