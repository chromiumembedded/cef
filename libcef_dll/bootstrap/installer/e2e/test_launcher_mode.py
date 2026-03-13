#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""E2E tests for the CEF installer launcher mode.

Launcher mode: bootstrap.exe is renamed, a client DLL is adjacent.
The bootstrap loads the DLL and calls RunWinMain. The mock client DLL
calls RunInstaller (exported by the bootstrap exe) to query or install CEF.

Tests also cover bootstrap-driven CEF resolution: when a client DLL is
present and installer config is embedded in the bootstrap exe, the
bootstrap resolves the CEF path before loading the client DLL and passes
it via version_info->libcef_path.
"""

import ctypes
import hashlib
import json
import os
import shutil
import time
import unittest

from e2e_test_base import (E2ETestBase, EXIT_CONFIG_ERROR, EXIT_SUCCESS,
                           FLAG_FORCECHECK, FLAG_HEADLESS, FLAG_UNINSTALL,
                           FLAG_UPDATE)


class TestLauncherMode(E2ETestBase):

  def _make_config_json(self,
                        appid,
                        vmin,
                        abi_hash='a1b2c3d4e5f6',
                        include_cdn=True,
                        **extra):
    """Build RunInstaller config JSON for the mock client DLL."""
    config = {
        'appid': appid,
        'vmin': vmin,
        'abi_hash': abi_hash,
        'install_path': self.install_dir,
        'show_progress_ui': False,
    }
    if include_cdn and os.path.isdir(self.cdn_dir):
      config['local_download_path'] = self.cdn_dir
    if self.test_thumbprint:
      config['certificate_thumbprint'] = self.test_thumbprint
    config.update(extra)
    return json.dumps(config)

  def _run_launcher(self, app_exe, config_json, command='install', timeout=30):
    """Run a launcher exe with mock DLL environment variables.

        Returns (exit_code, marker_data) where marker_data is parsed JSON
        from the marker file, or None if the file was not written.
        """
    marker_path = os.path.join(self.temp_dir, 'e2e_marker.json')
    env = {
        'CEF_E2E_MARKER_PATH': marker_path,
        'CEF_E2E_CONFIG_JSON': config_json,
        'CEF_E2E_COMMAND': command,
    }
    exit_code, stdout, stderr = self.run_installer(args=[],
                                                   exe=app_exe,
                                                   use_local_cdn=False,
                                                   env=env,
                                                   timeout=timeout)

    marker_data = None
    if os.path.isfile(marker_path):
      with open(marker_path) as f:
        marker_data = json.load(f)
    return exit_code, marker_data

  def test_dll_finds_installed_version(self):
    """Launcher DLL queries a pre-installed CEF version."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00011-1111-1111-1111-111111111111'

    # Pre-install using standalone mode.
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)
    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Pre-install failed: {stderr.decode(errors="replace")}')
    self.assert_version_installed(version)

    # Set up launcher mode.
    app_exe, _ = self.setup_launcher('E2E11App')

    # Query via mock DLL.
    config_json = self._make_config_json(appid=appid,
                                         vmin=version,
                                         abi_hash=abi_hash,
                                         include_cdn=False)
    exit_code, marker = self._run_launcher(app_exe,
                                           config_json,
                                           command='query')

    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Launcher query failed (exit {exit_code})')
    self.assertIsNotNone(marker, 'Marker file not written')
    result = marker.get('result', {})
    self.assertTrue(result.get('success'), f'Query not successful: {result}')
    self.assertTrue(result.get('libcef_path', ''), 'No libcef_path in result')

  def test_dll_installs_cef(self):
    """Launcher DLL installs CEF via RunInstaller."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00012-1212-1212-1212-121212121212'

    # Set up launcher mode (no pre-install).
    app_exe, _ = self.setup_launcher('E2E12App')

    # Install via mock DLL.
    config_json = self._make_config_json(appid=appid,
                                         vmin=version,
                                         abi_hash=abi_hash)
    exit_code, marker = self._run_launcher(app_exe,
                                           config_json,
                                           command='install')

    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Launcher install failed (exit {exit_code})')
    self.assertIsNotNone(marker, 'Marker file not written')
    result = marker.get('result', {})
    self.assertTrue(result.get('success'), f'Install not successful: {result}')
    self.assert_version_installed(version)

  def test_dll_fails_to_install(self):
    """Launcher DLL fails to install with no CDN available."""
    appid = 'e2e00013-1313-1313-1313-131313131313'

    # Set up launcher mode with no pre-installed version and no CDN.
    app_exe, _ = self.setup_launcher('E2E13App')

    empty_cdn = os.path.join(self.temp_dir, 'empty_cdn')
    os.makedirs(empty_cdn)
    config_json = self._make_config_json(appid=appid,
                                         vmin=self.test_version,
                                         include_cdn=False,
                                         local_download_path=empty_cdn)
    exit_code, marker = self._run_launcher(app_exe,
                                           config_json,
                                           command='install')

    # Mock DLL returns 3 when RunInstaller reports failure.
    self.assertNotEqual(EXIT_SUCCESS, exit_code)
    self.assertIsNotNone(marker, 'Marker file not written')
    result = marker.get('result', {})
    self.assertFalse(result.get('success', True))

  def test_wrong_thumbprint_fails(self):
    """Wrong certificate thumbprint causes signature error."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00016-1616-1616-1616-161616161616'

    app_exe, _ = self.setup_launcher('E2E16App')

    config_json = self._make_config_json(appid=appid,
                                         vmin=version,
                                         abi_hash=abi_hash,
                                         certificate_thumbprint='a' * 40)
    exit_code, marker = self._run_launcher(app_exe,
                                           config_json,
                                           command='install')

    self.assertNotEqual(EXIT_SUCCESS, exit_code)
    self.assertIsNotNone(marker, 'Marker file not written')
    result = marker.get('result', {})
    self.assertEqual(102, result.get('error_code'))
    self.assertEqual('SIGNATURE_ERROR', result.get('error_name'),
                     f'Expected SIGNATURE_ERROR, got: {result}')
    self.assertEqual('failed', result.get('outcome'))

  def test_bundled_cef_fallback(self):
    """Bundled CEF fallback when no CDN available."""
    version, abi_hash = self.build_cdn()
    appid_pre = 'e2e00004-0004-0004-0004-000000000004'

    # Step 1: Standalone install to create version dir on disk.
    self.embed_test_config(appid=appid_pre, vmin=version, abi_hash=abi_hash)
    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Pre-install failed: {stderr.decode(errors="replace")}')
    self.assert_version_installed(version)

    # The version dir at install_dir/Versions/<ver>/windows64/ has
    # cef_version.json + Release/libcef.dll — CheckBundledCef structure.
    bundled_path = os.path.join(self.install_dir, 'Versions', version,
                                'windows64')

    # Step 2: Launcher mode with bundled_cef_path, empty CDN, fresh install.
    fresh_install = os.path.join(self.temp_dir, 'fresh_install')
    empty_cdn = os.path.join(self.temp_dir, 'empty_cdn')
    os.makedirs(empty_cdn)

    appid = 'e2e00004-bbbb-bbbb-bbbb-bbbbbbbbbbbb'
    app_exe, _ = self.setup_launcher('E2E4App')

    config_json = self._make_config_json(appid=appid,
                                         vmin=version,
                                         abi_hash=abi_hash,
                                         include_cdn=False,
                                         local_download_path=empty_cdn,
                                         install_path=fresh_install,
                                         bundled_cef_path=bundled_path)
    exit_code, marker = self._run_launcher(app_exe,
                                           config_json,
                                           command='install')

    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Bundled fallback failed (exit {exit_code})')
    self.assertIsNotNone(marker, 'Marker file not written')
    result = marker.get('result', {})
    self.assertTrue(result.get('success'), f'Not successful: {result}')
    self.assertTrue(result.get('libcef_path', ''), 'No libcef_path')


class TestBootstrapCefResolution(E2ETestBase):
  """Tests for bootstrap-driven CEF resolution with a client DLL present.

    When installer config is embedded in the bootstrap exe, the bootstrap
    resolves the installed CEF path and passes it to the client DLL via
    version_info->libcef_path — before RunWinMain is called.
    """

  def _embed_bootstrap_config(self,
                              app_exe,
                              appid,
                              vmin,
                              abi_hash='',
                              enable_explicit_modes=False):
    """Embed a CEF_INSTALLER_CONFIG resource in the bootstrap exe."""
    config = {
        'appid': appid,
        'vmin': vmin,
    }
    if abi_hash:
      config['abi_hash'] = abi_hash
    if enable_explicit_modes:
      config['enable_explicit_modes'] = True
    if self.test_thumbprint:
      config['certificate_thumbprint'] = self.test_thumbprint
    self.embed_config_resource(app_exe, config)

  def _read_marker(self, marker_path):
    """Read and parse the marker file written by the mock client DLL."""
    self.assertTrue(os.path.isfile(marker_path),
                    f'Marker file not written: {marker_path}')
    with open(marker_path) as f:
      return json.load(f)

  def _run_launcher_with_marker(self,
                                app_exe,
                                args=None,
                                env=None,
                                use_local_cdn=False,
                                install_path=None,
                                use_install_path_override=True):
    """Run launcher and return (exit_code, marker_data, stderr)."""
    marker_path = os.path.join(self.temp_dir, 'e2e_marker.json')
    run_env = {
        'CEF_E2E_MARKER_PATH': marker_path,
    }
    if env:
      run_env.update(env)
    exit_code, _, stderr = self.run_installer(
        args=args or [],
        exe=app_exe,
        use_local_cdn=use_local_cdn,
        install_path=install_path,
        use_install_path_override=use_install_path_override,
        env=run_env)
    marker = None
    if os.path.isfile(marker_path):
      with open(marker_path) as f:
        marker = json.load(f)
    return exit_code, marker, stderr

  def test_bootstrap_resolves_preinstalled_cef(self):
    """Bootstrap resolves a pre-installed CEF version and passes path via
        version_info to the client DLL."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e0b001-b001-b001-b001-b00100000001'

    # Pre-install using standalone mode.
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)
    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Pre-install failed: {stderr.decode(errors="replace")}')
    self.assert_version_installed(version)

    # Set up launcher mode with embedded config in bootstrap.
    app_exe, _ = self.setup_launcher('BootResolve1')
    self._embed_bootstrap_config(app_exe, appid, version, abi_hash)

    # Run with no explicit command and no RunInstaller config.
    # The bootstrap resolves CEF from embedded config and passes
    # version_info->libcef_path to the mock client DLL.
    exit_code, marker, _ = self._run_launcher_with_marker(app_exe)

    self.assertIsNotNone(marker, 'Mock client DLL was not loaded')
    libcef_path = marker.get('libcef_path', '')
    self.assertTrue(libcef_path,
                    f'Bootstrap did not resolve libcef_path: {marker}')
    self.assertIn('libcef.dll', libcef_path.lower(),
                  f'Unexpected libcef_path: {libcef_path}')
    version_full = marker.get('libcef_version_full', '')
    self.assertIn(version, version_full,
                  f'libcef_version_full does not contain version: {marker}')
    self.assertEqual(0, marker.get('installer_error_code'))
    self.assertIsNone(marker.get('installer_error_message'))
    liveness = self.read_liveness(self.get_appid_hash(appid))
    self.assertIsNotNone(liveness)
    self.assertEqual(appid, liveness.get('appid'))
    self.assertEqual('windows64', liveness.get('platform'))
    self.assertTrue(int(liveness.get('last_launch', '0')) > 0)

  def test_bootstrap_local_hit_ignores_held_writer_mutex(self):
    """A local automatic startup launches while another writer owns mutex."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e0b010-b010-b010-b010-b01000000010'
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)
    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Pre-install failed: {stderr.decode(errors="replace")}')

    app_exe, _ = self.setup_launcher('BootLockFree1')
    self._embed_bootstrap_config(app_exe, appid, version, abi_hash)
    normalized = self.install_dir.lower().replace('\\', '/')
    suffix = hashlib.sha1(normalized.encode('utf-8')).hexdigest()[:16].upper()
    mutex_name = f'Global\\CEF_Installer_{suffix}'
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel32.CreateMutexW.argtypes = [
        ctypes.c_void_p, ctypes.c_bool, ctypes.c_wchar_p
    ]
    kernel32.CreateMutexW.restype = ctypes.c_void_p
    kernel32.ReleaseMutex.argtypes = [ctypes.c_void_p]
    kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
    mutex = kernel32.CreateMutexW(None, True, mutex_name)
    self.assertTrue(mutex, f'CreateMutexW failed: {ctypes.get_last_error()}')
    try:
      start = time.monotonic()
      exit_code, marker, _ = self._run_launcher_with_marker(app_exe)
      elapsed = time.monotonic() - start
    finally:
      kernel32.ReleaseMutex(mutex)
      kernel32.CloseHandle(mutex)

    self.assertIsNotNone(marker)
    self.assertTrue(marker.get('libcef_path'),
                    f'No resolved path (exit={exit_code}, marker={marker})')
    self.assertLess(elapsed, 3.0)

  def test_bootstrap_recovers_missing_and_corrupt_version_index(self):
    """Automatic startup can recover a valid distribution without an index."""
    version, abi_hash = self.build_cdn()
    preinstall_appid = 'e2e0b011-b011-b011-b011-b01100000011'
    self.embed_test_config(appid=preinstall_appid,
                           vmin=version,
                           abi_hash=abi_hash)
    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Pre-install failed: {stderr.decode(errors="replace")}')

    index_path = os.path.join(self.install_dir, 'versions.json')
    cases = [('missing', None, 'e2e0b012-b012-b012-b012-b01200000012'),
             ('corrupt', b'not an index',
              'e2e0b013-b013-b013-b013-b01300000013')]
    for suffix, corrupt_contents, appid in cases:
      with self.subTest(index_state=suffix):
        if corrupt_contents is None:
          if os.path.exists(index_path):
            os.remove(index_path)
        else:
          with open(index_path, 'wb') as index_file:
            index_file.write(corrupt_contents)

        app_exe, _ = self.setup_launcher(f'BootIndexRecovery{suffix}')
        self._embed_bootstrap_config(app_exe, appid, version, abi_hash)
        exit_code, marker, stderr = self._run_launcher_with_marker(app_exe)

        self.assertIsNotNone(
            marker,
            f'Client DLL was not loaded (exit={exit_code}, stderr={stderr!r})')
        self.assertTrue(marker.get('libcef_path'), marker)
        self.assertIn(version, marker.get('libcef_version_full', ''))
        if corrupt_contents is None:
          self.assertFalse(os.path.exists(index_path))
        else:
          with open(index_path, 'rb') as index_file:
            self.assertEqual(corrupt_contents, index_file.read())

  def test_bootstrap_installs_from_cdn_for_client_dll(self):
    """Bootstrap downloads CEF from CDN when no local version exists,
        then passes the path to the client DLL via version_info."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e0b002-b002-b002-b002-b00200000002'

    # Set up launcher mode — no pre-install, CDN available.
    app_exe, _ = self.setup_launcher('BootResolve2')
    self._embed_bootstrap_config(app_exe, appid, version, abi_hash)

    # Run with no explicit command. The bootstrap should auto-install
    # from CDN and pass the path via version_info.
    exit_code, marker, _ = self._run_launcher_with_marker(app_exe,
                                                          use_local_cdn=True)

    self.assertIsNotNone(marker, 'Mock client DLL was not loaded')
    libcef_path = marker.get('libcef_path', '')
    self.assertTrue(libcef_path,
                    f'Bootstrap did not resolve libcef_path: {marker}')
    version_full = marker.get('libcef_version_full', '')
    self.assertIn(version, version_full,
                  f'libcef_version_full does not contain version: {marker}')
    self.assert_version_installed(version)

  def test_update_falls_through_to_client_dll(self):
    """Explicit /cef-update with a client DLL runs the update and then
        loads the client DLL (instead of exiting)."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e0b003-b003-b003-b003-b00300000003'

    # Set up launcher mode with embedded config and CDN.
    app_exe, _ = self.setup_launcher('BootResolve3')
    self._embed_bootstrap_config(app_exe,
                                 appid,
                                 version,
                                 abi_hash,
                                 enable_explicit_modes=True)

    # Run with /cef-update. Previously this would exit without loading
    # the client DLL. Now it should run the update, then load the DLL.
    exit_code, marker, _ = self._run_launcher_with_marker(
        app_exe, args=[FLAG_UPDATE, FLAG_HEADLESS], use_local_cdn=True)

    self.assertIsNotNone(marker, 'Client DLL not loaded after /cef-update')
    libcef_path = marker.get('libcef_path', '')
    self.assertTrue(libcef_path, f'No libcef_path after /cef-update: {marker}')
    self.assert_version_installed(version)
    self.assert_no_launch_state(version, self.get_appid_hash(appid))

  def test_resolution_failure_falls_through(self):
    """When CEF resolution fails (no CDN, no installed version), the
        bootstrap still loads the client DLL with empty libcef_path."""
    appid = 'e2e0b004-b004-b004-b004-b00400000004'

    # Set up launcher mode with NO CDN and NO pre-installed version.
    app_exe, _ = self.setup_launcher('BootResolve4')
    self._embed_bootstrap_config(app_exe, appid, self.test_version)

    empty_cdn = os.path.join(self.temp_dir, 'empty_cdn')
    os.makedirs(empty_cdn)
    exit_code, marker, _ = self._run_launcher_with_marker(app_exe)

    # Client DLL should still be loaded even though resolution failed.
    self.assertIsNotNone(marker,
                         'Client DLL not loaded after resolution failure')
    libcef_path = marker.get('libcef_path', '')
    self.assertEqual('', libcef_path,
                     f'Expected empty libcef_path on failure: {marker}')
    version_full = marker.get('libcef_version_full', '')
    self.assertEqual(
        '', version_full,
        f'Expected empty libcef_version_full on failure: {marker}')
    self.assertIsInstance(marker.get('installer_error_code'), int)
    self.assertNotEqual(0, marker.get('installer_error_code'))
    self.assertTrue(marker.get('installer_error_message'))

  def test_installer_not_configured_clears_error(self):
    app_exe, _ = self.setup_launcher('BootNotConfigured')
    exit_code, marker, _ = self._run_launcher_with_marker(
        app_exe, env={'CEF_E2E_EXIT_CODE': '0'})
    self.assertEqual(EXIT_SUCCESS, exit_code)
    self.assertIsNotNone(marker)
    self.assertEqual('', marker.get('libcef_path', ''))
    self.assertEqual(0, marker.get('installer_error_code'))
    self.assertIsNone(marker.get('installer_error_message'))

  def test_malformed_client_resource_exits_config_error(self):
    app_exe, app_name = self.setup_launcher('BootMalformedClient')
    self._embed_bootstrap_config(app_exe,
                                 'e2e0b008-b008-b008-b008-b00800000008',
                                 self.test_version)
    app_dll = os.path.join(self.build_dir, f'{app_name}.dll')
    self.embed_raw_config_resource(app_dll, '{"appid":')
    exit_code, marker, _ = self._run_launcher_with_marker(
        app_exe, env={'CEF_E2E_EXIT_CODE': '0'})
    self.assertEqual(EXIT_CONFIG_ERROR, exit_code)
    self.assertIsNone(marker, 'Malformed trusted config loaded the client DLL')

  def test_post_clamp_empty_range_exits_config_error(self):
    app_exe, app_name = self.setup_launcher('BootEmptyRange')
    app_dll = os.path.join(self.build_dir, f'{app_name}.dll')
    self.embed_raw_config_resource(
        app_dll, '{"appid":"e2e0b009-b009-b009-b009-b00900000009",'
        '"vmin":"1.0","vmax":"2.0"}')
    exit_code, marker, stderr = self._run_launcher_with_marker(
        app_exe, env={'CEF_E2E_EXIT_CODE': '0'})
    self.assertEqual(EXIT_CONFIG_ERROR, exit_code)
    self.assertIsNone(marker)
    diagnostic = stderr.decode(errors='replace')
    self.assertIn('Configured vmin 1.0', diagnostic)
    self.assertIn('effective vmin', diagnostic)
    self.assertIn('configured vmax 2.0', diagnostic)

  def test_unchecked_cef_path(self):
    """Bootstrap uses unchecked_cef_path from client DLL resource to resolve
        libcef.dll directly, skipping installer version checks."""
    version, abi_hash = self.build_cdn()
    appid_pre = 'e2e0b005-b005-b005-b005-b00500000005'

    # Step 1: Standalone install to create a Release dir with libcef.dll.
    self.embed_test_config(appid=appid_pre, vmin=version, abi_hash=abi_hash)
    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Pre-install failed: {stderr.decode(errors="replace")}')
    self.assert_version_installed(version)

    # The Release dir has libcef.dll — use it as unchecked_cef_path.
    unchecked_dir = os.path.join(self.install_dir, 'Versions', version,
                                 'windows64', 'Release')
    self.assertTrue(os.path.isfile(os.path.join(unchecked_dir, 'libcef.dll')))

    # Step 2: Set up launcher with unchecked_cef_path embedded in the
    # client DLL resource. No RunInstaller config — the bootstrap reads
    # the DLL resource, resolves the path, and passes it via version_info.
    appid = 'e2e0b005-cccc-cccc-cccc-cccccccccccc'
    app_exe, app_name = self.setup_launcher('BootResolve5')
    app_dll = os.path.join(self.build_dir, f'{app_name}.dll')

    dll_config = {
        'appid': appid,
        'vmin': version,
        'abi_hash': abi_hash,
        'unchecked_cef_path': unchecked_dir,
    }
    self.embed_config_resource(app_dll, dll_config)

    exit_code, marker, _ = self._run_launcher_with_marker(app_exe)

    self.assertIsNotNone(marker, 'Mock client DLL was not loaded')
    libcef_path = marker.get('libcef_path', '')
    self.assertTrue(libcef_path,
                    f'Bootstrap did not resolve libcef_path: {marker}')
    self.assertIn('libcef.dll', libcef_path.lower(),
                  f'Unexpected libcef_path: {libcef_path}')

  def test_install_path_relative_in_client_dll_resource(self):
    """A client-resource install_path is DLL-relative and exclusive."""
    version, abi_hash = self.build_cdn()
    app_exe, app_name = self.setup_launcher('BootInstallPath1')
    app_dll = os.path.join(self.build_dir, f'{app_name}.dll')
    # Only a default-named bootstrap accepts --module. Use the console variant
    # so a regression reports on stderr instead of displaying a message box.
    # The E2E base's self.bootstrap_exe is a renamed CefTestApp.exe and
    # intentionally ignores that switch.
    host_exe = os.path.join(self.build_dir, 'bootstrapc.exe')
    custom_store = os.path.join(self.build_dir, f'{app_name}_relative_store')
    shutil.rmtree(custom_store, ignore_errors=True)
    self.addCleanup(lambda: shutil.rmtree(custom_store, ignore_errors=True))
    relative_store = os.path.basename(custom_store)
    appid = 'e2e0b008-c008-c008-c008-c00800000008'
    config = {
        'appid': appid,
        'vmin': version,
        'abi_hash': abi_hash,
        'install_path': relative_store,
    }
    if self.test_thumbprint:
      config['certificate_thumbprint'] = self.test_thumbprint
    self.embed_config_resource(app_dll, config)

    exit_code, marker, stderr = self._run_launcher_with_marker(
        app_exe, use_local_cdn=True, use_install_path_override=False)
    self.assertIsNotNone(marker, stderr.decode(errors='replace'))
    self.assertEqual(0, marker.get('installer_error_code'),
                     stderr.decode(errors='replace'))
    libcef_path = os.path.normcase(os.path.abspath(marker['libcef_path']))
    self.assertTrue(
        libcef_path.startswith(os.path.normcase(os.path.abspath(custom_store))))
    self.assertTrue(os.path.isfile(os.path.join(custom_store,
                                                'installer.json')))
    self.assertTrue(os.path.isfile(os.path.join(custom_store, 'versions.json')))
    self.assertFalse(os.path.isdir(os.path.join(self.install_dir, 'Versions')))

    # The host executable is outside the DLL-relative custom store, so default
    # uninstall is synchronous and keeps the already-resolved absolute target.
    env = {'PATH': self.build_dir + os.pathsep + os.environ.get('PATH', '')}
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    exit_code, _, stderr = self.run_installer(
        args=[f'--module={app_name}', FLAG_UNINSTALL, FLAG_HEADLESS],
        exe=host_exe,
        use_local_cdn=False,
        use_install_path_override=False,
        env=env)
    self.assertEqual(EXIT_SUCCESS, exit_code, stderr.decode(errors='replace'))
    self.assertEqual(relaunch_dirs, self.snapshot_uninstall_relaunch_dirs())
    self.assertFalse(
        os.path.isdir(os.path.join(custom_store, 'Versions', version)))

  def test_bundled_cef_path_in_client_dll_resource(self):
    """Bootstrap resolves bundled_cef_path from client DLL resource, selects
        the bundled version when no installed version exists."""
    version, abi_hash = self.build_cdn()
    appid_pre = 'e2e0b006-b006-b006-b006-b00600000006'

    # Step 1: Standalone install to create a version dir with valid metadata.
    self.embed_test_config(appid=appid_pre, vmin=version, abi_hash=abi_hash)
    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Pre-install failed: {stderr.decode(errors="replace")}')
    self.assert_version_installed(version)

    # The version dir has cef_version.json + Release/libcef.dll —
    # valid for CheckBundledCef.
    bundled_path = os.path.join(self.install_dir, 'Versions', version,
                                'windows64')

    # Step 2: Set up launcher with bundled_cef_path embedded in the client
    # DLL resource (not RunInstaller config). Use a fresh install dir so
    # there are no pre-installed versions.
    fresh_install = os.path.join(self.temp_dir, 'fresh_install')
    os.makedirs(fresh_install)

    appid = 'e2e0b006-cccc-cccc-cccc-cccccccccccc'
    app_exe, app_name = self.setup_launcher('BootBundled1')
    app_dll = os.path.join(self.build_dir, f'{app_name}.dll')

    dll_config = {
        'appid': appid,
        'vmin': version,
        'abi_hash': abi_hash,
        'bundled_cef_path': bundled_path,
    }
    if self.test_thumbprint:
      dll_config['certificate_thumbprint'] = self.test_thumbprint
    self.embed_config_resource(app_dll, dll_config)

    exit_code, marker, _ = self._run_launcher_with_marker(
        app_exe, install_path=fresh_install)

    self.assertIsNotNone(marker, 'Mock client DLL was not loaded')
    libcef_path = marker.get('libcef_path', '')
    self.assertTrue(libcef_path,
                    f'Bootstrap did not resolve libcef_path: {marker}')
    self.assertIn('libcef.dll', libcef_path.lower(),
                  f'Unexpected libcef_path: {libcef_path}')
    # libcef_path should point into the bundled directory, not fresh_install.
    norm = lambda p: p.replace('\\', '/').lower()
    self.assertIn(norm(bundled_path), norm(libcef_path),
                  f'libcef_path not from bundled dir: {libcef_path}')

  def test_bundled_cef_path_relative_in_client_dll_resource(self):
    """Bootstrap resolves a relative bundled_cef_path against the client
        DLL directory."""
    import shutil
    version, abi_hash = self.build_cdn()
    appid_pre = 'e2e0b007-b007-b007-b007-b00700000007'

    # Step 1: Standalone install to create version dir.
    self.embed_test_config(appid=appid_pre, vmin=version, abi_hash=abi_hash)
    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Pre-install failed: {stderr.decode(errors="replace")}')
    self.assert_version_installed(version)

    # Copy the bundled version adjacent to the DLL so a relative path works.
    app_exe, app_name = self.setup_launcher('BootBundled2')
    app_dll = os.path.join(self.build_dir, f'{app_name}.dll')
    bundled_rel_dir = os.path.join(self.build_dir, 'bundled_cef')
    src_version_dir = os.path.join(self.install_dir, 'Versions', version,
                                   'windows64')
    if os.path.isdir(bundled_rel_dir):
      shutil.rmtree(bundled_rel_dir)
    shutil.copytree(src_version_dir, bundled_rel_dir)
    self.addCleanup(lambda: shutil.rmtree(bundled_rel_dir, ignore_errors=True))

    # Step 2: Embed a relative bundled_cef_path in the client DLL resource.
    fresh_install = os.path.join(self.temp_dir, 'fresh_install2')
    os.makedirs(fresh_install)

    appid = 'e2e0b007-cccc-cccc-cccc-cccccccccccc'
    dll_config = {
        'appid': appid,
        'vmin': version,
        'abi_hash': abi_hash,
        'bundled_cef_path': 'bundled_cef',
    }
    if self.test_thumbprint:
      dll_config['certificate_thumbprint'] = self.test_thumbprint
    self.embed_config_resource(app_dll, dll_config)

    exit_code, marker, _ = self._run_launcher_with_marker(
        app_exe, install_path=fresh_install)

    self.assertIsNotNone(marker, 'Mock client DLL was not loaded')
    libcef_path = marker.get('libcef_path', '')
    self.assertTrue(libcef_path,
                    f'Bootstrap did not resolve libcef_path: {marker}')
    self.assertIn('libcef.dll', libcef_path.lower(),
                  f'Unexpected libcef_path: {libcef_path}')
