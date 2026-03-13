#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""E2E tests for the beta channel install path."""

import glob
import os
import shutil
import unittest

from e2e_test_base import (E2ETestBase, EXIT_SUCCESS, FLAG_HEADLESS,
                           FLAG_UPDATE)


class TestBetaInstall(E2ETestBase):

  def _build_beta_cdn(self, version=None, abi_hash='a1b2c3d4e5f6'):
    """Build a local CDN with beta-channel manifest naming.

    Calls the standard build_cdn helper, then copies each platform/abi-hash
    manifest to the _beta variant (e.g., 150_windows64.json ->
    150_windows64_beta.json). The archive files are channel-agnostic.
    """
    if version is None:
      version = self.test_version
    self.build_cdn(version=version, abi_hash=abi_hash)

    for manifest in glob.glob(os.path.join(self.cdn_dir, '*.json')):
      name = os.path.basename(manifest)
      if name == 'revoked.json':
        continue
      base, ext = os.path.splitext(name)
      beta_name = base + '_beta' + ext
      shutil.copy2(manifest, os.path.join(self.cdn_dir, beta_name))

    return version, abi_hash

  def test_beta_install_uses_beta_manifest(self):
    """Install with channel=beta fetches _beta manifest and succeeds."""
    version, abi_hash = self._build_beta_cdn()
    appid = 'e2e00030-3030-3030-3030-303030303030'

    self.embed_test_config(appid=appid,
                           vmin=version,
                           abi_hash=abi_hash,
                           channel='beta')
    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])

    self.assertEqual(
        EXIT_SUCCESS, exit_code, f'Beta install failed (exit {exit_code}): '
        f'{stderr.decode(errors="replace")}')
    self.assert_version_installed(version)

    db = self.read_database()
    found = any(a.get('uuid') == appid for a in db.get('apps', []))
    self.assertTrue(found, f'App not in database after beta install: {db}')

  def test_beta_install_without_beta_manifest_fails(self):
    """Install with channel=beta fails when only stable manifests exist."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00031-3131-3131-3131-313131313131'

    self.embed_test_config(appid=appid,
                           vmin=version,
                           abi_hash=abi_hash,
                           channel='beta')
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])

    # Should fail because there's no _beta.json manifest.
    self.assertNotEqual(EXIT_SUCCESS, exit_code)
    self.assert_version_not_installed(version)
