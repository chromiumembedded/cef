#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""E2E tests for the CEF installer install flow."""

import json
import os
import subprocess
import unittest

from e2e_test_base import (E2ETestBase, EXIT_CONFIG_ERROR,
                           EXIT_NO_MATCHING_VERSION, EXIT_SUCCESS,
                           FLAG_HEADLESS, FLAG_UNINSTALL, FLAG_UPDATE)


class TestInstallFlow(E2ETestBase):

  def _start_https_cdn_failover(self):
    empty_primary = os.path.join(self.temp_dir, 'empty-primary')
    os.makedirs(empty_primary)
    cert_path, key_path = self.get_loopback_tls_certificate()
    primary_url, primary_requests = self.start_https_file_server(
        empty_primary, cert_path, key_path)
    fallback_url, fallback_requests = self.start_https_file_server(
        self.cdn_dir, cert_path, key_path)
    return (primary_url, primary_requests, fallback_url, fallback_requests)

  def _cdn_resource_config(self, appid, version, abi_hash, cdn_urls):
    config = {
        'appid': appid,
        'vmin': version,
        'abi_hash': abi_hash,
        'cdn_urls': cdn_urls,
        'enable_explicit_modes': True,
    }
    if self.test_thumbprint:
      config['certificate_thumbprint'] = self.test_thumbprint
    return config

  def _assert_https_archive_failover(self, version, primary_requests,
                                     fallback_requests):
    self.assertTrue(primary_requests, 'Primary application CDN got no request')
    self.assertTrue(fallback_requests,
                    'Fallback application CDN got no request')
    archive_name = f'cef_{version}_windows64.tar.xz'
    self.assertTrue(any(
        archive_name in request for request in primary_requests))
    self.assertTrue(
        any(archive_name in request for request in fallback_requests))

  def test_fresh_install(self):
    """Fresh install via /cef-update with a bootstrap resource."""
    version, abi_hash = self.build_cdn()
    self.embed_test_config(
        appid='e2e00001-0001-0001-0001-000000000001',
        vmin=version,
        abi_hash=abi_hash,
    )

    exit_code, stdout, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])

    self.assertEqual(
        EXIT_SUCCESS, exit_code, f'Install failed (exit {exit_code}): '
        f'{stderr.decode(errors="replace")}')
    self.assert_version_installed(version)

    # Verify database was updated.
    db = self.read_database()
    self.assertIn('apps', db)
    found = any(
        a.get('uuid') == 'e2e00001-0001-0001-0001-000000000001'
        for a in db['apps'])
    self.assertTrue(found, f'App not in database: {db}')

  def test_bootstrap_resource_cdn_https_failover(self):
    """Bootstrap-resource URLs drive real process HTTPS failover."""
    version, abi_hash = self.build_cdn()
    (primary_url, primary_requests, fallback_url,
     fallback_requests) = self._start_https_cdn_failover()
    app_name = 'E2ECdnBootstrapResource'
    app_exe = os.path.join(self.build_dir, f'{app_name}.exe')
    self._restore_bootstrap_executable(app_exe)
    self.addCleanup(lambda: os.remove(app_exe)
                    if os.path.isfile(app_exe) else None)
    self.embed_config_resource(
        app_exe,
        self._cdn_resource_config('e2e00032-0032-0032-0032-000000000032',
                                  version, abi_hash,
                                  [primary_url, fallback_url]))

    exit_code, _, stderr = self.run_installer(
        [FLAG_UPDATE, FLAG_HEADLESS],
        exe=app_exe,
        use_local_cdn=False,
        allow_configured_cdn=True,
        timeout=60,
        env={'CEF_INSTALLER_IGNORE_CERTIFICATE_ERRORS_FOR_TESTING': '1'})

    self.assertEqual(
        EXIT_SUCCESS, exit_code,
        f'HTTPS bootstrap-resource CDN failover failed: '
        f'{stderr.decode(errors="replace")}')
    self.assert_version_installed(version)
    self._assert_https_archive_failover(version, primary_requests,
                                        fallback_requests)

  def test_client_resource_cdn_https_failover(self):
    """Client-resource URLs drive real bootstrap-process HTTPS failover."""
    version, abi_hash = self.build_cdn()
    (primary_url, primary_requests, fallback_url,
     fallback_requests) = self._start_https_cdn_failover()
    app_exe, app_name = self.setup_launcher('E2ECdnClientResource')
    app_dll = os.path.join(self.build_dir, f'{app_name}.dll')
    self.embed_config_resource(
        app_dll,
        self._cdn_resource_config('e2e00033-0033-0033-0033-000000000033',
                                  version, abi_hash,
                                  [primary_url, fallback_url]))

    exit_code, _, stderr = self.run_installer(
        [FLAG_UPDATE, FLAG_HEADLESS],
        exe=app_exe,
        use_local_cdn=False,
        allow_configured_cdn=True,
        timeout=60,
        env={
            'CEF_E2E_EXIT_CODE': '0',
            'CEF_INSTALLER_IGNORE_CERTIFICATE_ERRORS_FOR_TESTING': '1',
        })

    self.assertEqual(
        EXIT_SUCCESS, exit_code, f'HTTPS client-resource CDN failover failed: '
        f'{stderr.decode(errors="replace")}')
    self.assert_version_installed(version)
    self._assert_https_archive_failover(version, primary_requests,
                                        fallback_requests)

  def test_install_idempotency(self):
    """Running install twice succeeds without re-download."""
    version, abi_hash = self.build_cdn()
    self.embed_test_config(
        appid='e2e00003-0003-0003-0003-000000000003',
        vmin=version,
        abi_hash=abi_hash,
    )

    # First install.
    exit_code1, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code1)
    self.assert_version_installed(version)

    exit_code2, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code2)
    self.assert_version_installed(version)

  def test_next_best_when_newest_archive_missing(self):
    """Local mirror propagation lag falls back once."""
    abi_hash = 'a1b2c3d4e5f6'
    older = self.test_version
    newer = self.test_version_update
    self.build_cdn(version=older, abi_hash=abi_hash)
    self.build_cdn(version=newer, abi_hash=abi_hash)
    missing_archive = os.path.join(self.cdn_dir,
                                   f'cef_{newer}_windows64.tar.xz')
    self.assertTrue(os.path.isfile(missing_archive))
    os.remove(missing_archive)
    self.embed_test_config(
        appid='e2e00030-0030-0030-0030-000000000030',
        vmin=older,
        abi_hash=abi_hash,
    )

    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])

    self.assertEqual(
        EXIT_SUCCESS, exit_code,
        f'Next-best install failed: {stderr.decode(errors="replace")}')
    self.assert_version_installed(older)
    self.assert_version_not_installed(newer)

  def test_standalone_auto_install_with_embedded_config(self):
    """Standalone auto-install with embedded config resource."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00002-0002-0002-0002-000000000002'

    # Create a fresh exe copy with a distinct name for this test.
    auto_exe_name = 'E2E2AutoApp'
    auto_exe = os.path.join(self.build_dir, f'{auto_exe_name}.exe')
    self._restore_bootstrap_executable(auto_exe)
    self.addCleanup(lambda: os.remove(auto_exe)
                    if os.path.isfile(auto_exe) else None)

    # Embed config resource into the exe.
    config_dict = {
        'appid': appid,
        'vmin': version,
        'abi_hash': abi_hash,
        'enable_explicit_modes': True,
    }
    if self.test_thumbprint:
      config_dict['certificate_thumbprint'] = self.test_thumbprint
    self.embed_config_resource(auto_exe, config_dict)

    # Run with /cef-headless only (no /cef-update) — auto-install triggers because
    # no client DLL + config loaded + no explicit command.
    exit_code, _, stderr = self.run_installer([FLAG_HEADLESS], exe=auto_exe)
    self.assertEqual(
        EXIT_SUCCESS, exit_code, f'Auto-install failed (exit {exit_code}): '
        f'{stderr.decode(errors="replace")}')
    self.assert_version_installed(version)

    db = self.read_database()
    found = any(a.get('uuid') == appid for a in db.get('apps', []))
    self.assertTrue(found, f'App not in database: {db}')


class TestErrorHandling(E2ETestBase):

  def test_invalid_config(self):
    """Invalid config resource produces config error 100."""
    self.embed_raw_config_resource(self.bootstrap_exe, 'NOT VALID JSON {{{')

    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS],
                                         use_local_cdn=False)

    self.assertEqual(EXIT_CONFIG_ERROR, exit_code)

  def test_missing_config(self):
    """A freshly restored bootstrap has no config resource."""
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS],
                                         use_local_cdn=False)
    self.assertEqual(EXIT_CONFIG_ERROR, exit_code)

  def test_revoked_version_only(self):
    """Only available version is revoked, install fails."""
    version, abi_hash = self.build_cdn()

    # Overwrite revoked.json to revoke the only available version.
    revoked_path = os.path.join(self.cdn_dir, 'revoked.json')
    revoked = {
        'revoked_versions': [{
            'version': version,
            'reason': 'test',
            'revoked_at': '2026-01-01T00:00:00Z'
        }]
    }
    with open(revoked_path, 'w') as f:
      json.dump(revoked, f)

    self.embed_test_config(
        appid='e2e00017-1717-1717-1717-171717171717',
        vmin=version,
        abi_hash=abi_hash,
    )
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    # kExitCodeNoMatchingVersion (only version is revoked).
    self.assertEqual(EXIT_NO_MATCHING_VERSION, exit_code)


class TestMultiApp(E2ETestBase):

  def test_sequential_install_two_apps(self):
    """Sequential install of two apps, shared version."""
    version, abi_hash = self.build_cdn()

    # Install app A.
    self.embed_test_config(
        appid='e2e00018-aaaa-aaaa-aaaa-aaaaaaaaaaaa',
        vmin=version,
        abi_hash=abi_hash,
    )
    exit_a, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_a)

    # Install app B (different appid, same version range).
    self.embed_test_config(
        appid='e2e00018-bbbb-bbbb-bbbb-bbbbbbbbbbbb',
        vmin=version,
        abi_hash=abi_hash,
    )
    exit_b, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_b)

    # One version directory, two apps in database.
    self.assert_version_installed(version)
    db = self.read_database()
    uuids = [a.get('uuid') for a in db.get('apps', [])]
    self.assertIn('e2e00018-aaaa-aaaa-aaaa-aaaaaaaaaaaa', uuids)
    self.assertIn('e2e00018-bbbb-bbbb-bbbb-bbbbbbbbbbbb', uuids)

  def test_uninstall_one_of_two_apps(self):
    """Uninstall one of two apps, version survives."""
    version, abi_hash = self.build_cdn()

    # Install both apps.
    for appid in [
        'e2e00019-aaaa-aaaa-aaaa-aaaaaaaaaaaa',
        'e2e00019-bbbb-bbbb-bbbb-bbbbbbbbbbbb'
    ]:
      self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)
      exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
      self.assertEqual(EXIT_SUCCESS, exit_code)

    # The external executable runs the uninstall synchronously.
    self.embed_test_config(
        appid='e2e00019-aaaa-aaaa-aaaa-aaaaaaaaaaaa',
        vmin=version,
        abi_hash=abi_hash,
    )
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    exit_u, _, _ = self.run_installer([FLAG_UNINSTALL, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_u)
    self.assertEqual(relaunch_dirs, self.snapshot_uninstall_relaunch_dirs())

    # Version should still exist (app B needs it).
    self.assert_version_installed(version)

    # Database should have only app B.
    db = self.read_database()
    uuids = [a.get('uuid') for a in db.get('apps', [])]
    self.assertNotIn('e2e00019-aaaa-aaaa-aaaa-aaaaaaaaaaaa', uuids)
    self.assertIn('e2e00019-bbbb-bbbb-bbbb-bbbbbbbbbbbb', uuids)

  def test_two_apps_different_milestones(self):
    """Two apps with different milestones, uninstall one."""
    # Derive current version dynamically from build_cdn output.
    current_ver, hash_a = self.build_cdn(abi_hash='aaa111bbb222')

    # Compute next-milestone version (e.g., 150.1 -> 151.0).
    parts = current_ver.split('.')
    next_milestone = str(int(parts[0]) + 1) + '.0'
    hash_b = 'ccc333ddd444'
    self.build_cdn(version=next_milestone, abi_hash=hash_b)

    appid_a = 'e2e00020-aaaa-aaaa-aaaa-aaaaaaaaaaaa'
    appid_b = 'e2e00020-bbbb-bbbb-bbbb-bbbbbbbbbbbb'

    # Install App A (current milestone).
    self.embed_test_config(appid=appid_a, vmin=current_ver, abi_hash=hash_a)
    exit_a, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(
        EXIT_SUCCESS, exit_a,
        f'App A install failed: {stderr.decode(errors="replace")}')
    self.assert_version_installed(current_ver)

    # Install App B (next milestone).
    self.embed_test_config(appid=appid_b, vmin=next_milestone, abi_hash=hash_b)
    exit_b, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(
        EXIT_SUCCESS, exit_b,
        f'App B install failed: {stderr.decode(errors="replace")}')

    # Both version directories should exist.
    db = self.read_database()
    uuids = [a.get('uuid') for a in db.get('apps', [])]
    self.assertIn(appid_a, uuids)
    self.assertIn(appid_b, uuids)

    # External default uninstall is synchronous.
    self.embed_test_config(appid=appid_a, vmin=current_ver, abi_hash=hash_a)
    exit_u, _, _ = self.run_installer([FLAG_UNINSTALL, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_u)

    # Current milestone pruned, next milestone intact.
    self.assert_version_not_installed(current_ver)
    db = self.read_database()
    uuids = [a.get('uuid') for a in db.get('apps', [])]
    self.assertNotIn(appid_a, uuids)
    self.assertIn(appid_b, uuids)
