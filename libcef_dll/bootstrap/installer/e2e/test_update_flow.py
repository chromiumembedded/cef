#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""E2E tests for the CEF installer update flow."""

import unittest

from e2e_test_base import (E2ETestBase, EXIT_SUCCESS, FLAG_BACKGROUND,
                           FLAG_FORCECHECK, FLAG_HEADLESS, FLAG_UPDATE)


class TestUpdateFlow(E2ETestBase):

  def test_update_finds_newer_version(self):
    """Update downloads newer version when vmin moves forward."""
    abi_hash = 'a1b2c3d4e5f6'
    appid = 'e2e00005-0005-0005-0005-000000000005'

    # Build CDN with initial version and install.
    v1 = self.test_version
    v1_1 = self.test_version_update
    self.build_cdn(version=v1, abi_hash=abi_hash)
    self.embed_test_config(appid=appid, vmin=v1, abi_hash=abi_hash)
    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(
        EXIT_SUCCESS, exit_code,
        f'Initial install failed: {stderr.decode(errors="replace")}')
    self.assert_version_installed(v1)

    # Add newer version to CDN (manifest merges).
    self.build_cdn(version=v1_1, abi_hash=abi_hash)

    # Raise vmin so the controller downloads the newer version.
    self.embed_test_config(appid=appid, vmin=v1_1, abi_hash=abi_hash)
    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Update failed: {stderr.decode(errors="replace")}')

    # Newer version installed, old version pruned.
    self.assert_version_installed(v1_1)
    self.assert_version_not_installed(v1)

    db = self.read_database()
    found = any(a.get('uuid') == appid for a in db.get('apps', []))
    self.assertTrue(found, f'App not in database: {db}')

  def test_update_with_forcecheck(self):
    """Update with /cef-forcecheck bypasses manifest cache."""
    version, abi_hash = self.build_cdn()
    self.embed_test_config(
        appid='e2e00006-0006-0006-0006-000000000006',
        vmin=version,
        abi_hash=abi_hash,
    )

    # Install.
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)

    # Normal update (may use cached manifest).
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)

    # Forced update bypasses cache.
    exit_code, _, _ = self.run_installer(
        [FLAG_UPDATE, FLAG_HEADLESS, FLAG_FORCECHECK])
    self.assertEqual(EXIT_SUCCESS, exit_code)
    self.assert_version_installed(version)

  def test_explicit_background_update(self):
    """Explicit background update completes without UI."""
    version, abi_hash = self.build_cdn()
    self.embed_test_config(
        appid='e2e00031-0031-0031-0031-000000000031',
        vmin=version,
        abi_hash=abi_hash,
    )

    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_BACKGROUND])

    self.assertEqual(
        EXIT_SUCCESS, exit_code,
        f'Background update failed: {stderr.decode(errors="replace")}')
    self.assert_version_installed(version)

  def test_update_no_newer_version(self):
    """Update with no newer version available is a no-op success."""
    version, abi_hash = self.build_cdn()
    self.embed_test_config(
        appid='e2e00007-0007-0007-0007-000000000007',
        vmin=version,
        abi_hash=abi_hash,
    )

    # Install.
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)
    self.assert_version_installed(version)

    # Second update -- already at latest, should succeed without changes.
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)
    self.assert_version_installed(version)
