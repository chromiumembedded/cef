#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""E2E tests for launch health tracking and version rollback.

Uses launcher mode (bootstrap.exe + mock client DLL) with CEF_E2E_EXIT_CODE
to control exit codes and test the sentinel lifecycle.
"""

import json
import os
import shutil
import time
import unittest

from e2e_test_base import (E2ETestBase, EXIT_SUCCESS, FLAG_HEADLESS,
                           FLAG_UNINSTALL, FLAG_UPDATE, FLAG_FORCECHECK)

# Exit codes the bootstrap treats as success or neutral (see
# IsSuccessOrNeutralExitCode): a launch ending in one of these leaves the
# sentinel running=false. 0 = normal exit; 21 = CEF_RESULT_CODE_PROFILE_IN_USE.
# Any other exit code, without an explicit launch_success confirmation, leaves
# the sentinel running=true.
_SUCCESS_OR_NEUTRAL_EXIT_CODES = frozenset({0, 21})


class TestLaunchHealth(E2ETestBase):

  def _embed_launcher_config(self,
                             app_exe,
                             appid,
                             vmin,
                             abi_hash='a1b2c3d4e5f6',
                             launch_health='exit_code'):
    """Embed CEF_INSTALLER_CONFIG resource into the launcher exe.

        The mock client has no application-config resource, so selection falls
        back to this bootstrap resource. This activates the bootstrap's
        auto-install path, which is required for launch health sentinel writes.
        """
    config = {
        'appid': appid,
        'vmin': vmin,
        'abi_hash': abi_hash,
        'launch_health': launch_health,
    }
    if self.test_thumbprint:
      config['certificate_thumbprint'] = self.test_thumbprint
    self.embed_config_resource(app_exe, config)

  def test_default_off_writes_no_health_sentinel(self):
    """Missing launch_health selects normally without a health sentinel."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00a00-0000-0000-0000-000000000000'
    self._standalone_install(version, abi_hash, appid)
    app_exe, _ = self.setup_launcher('LH00App')

    config = {'appid': appid, 'vmin': version, 'abi_hash': abi_hash}
    if self.test_thumbprint:
      config['certificate_thumbprint'] = self.test_thumbprint
    self.embed_config_resource(app_exe, config)

    rc, marker = self._run_launcher_with_exit_code(app_exe, 1)
    self.assertEqual(1, rc)
    self.assertIsNotNone(marker)
    self.assert_no_launch_state(version, self.get_appid_hash(appid))

  def test_explicit_ordinary_exit_is_neutral(self):
    """Explicit mode requires launch_success even when RunWinMain returns 0."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00a00-0000-0000-0000-000000000001'
    self._standalone_install(version, abi_hash, appid)
    app_exe, _ = self.setup_launcher('LHE0App')
    self._embed_launcher_config(app_exe,
                                appid,
                                version,
                                abi_hash,
                                launch_health='explicit')

    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)
    self.assert_launch_state(version,
                             self.get_appid_hash(appid),
                             running=False,
                             failures=0)
    state = self.read_launch_state(version, self.get_appid_hash(appid))
    self.assertFalse(state.get('confirmed'))

  def _run_launcher_with_exit_code(self,
                                   app_exe,
                                   exit_code,
                                   timeout=30,
                                   launch_success=False):
    """Run launcher with CEF_E2E_EXIT_CODE to simulate a specific exit.

        When launch_success=True, also sets CEF_E2E_LAUNCH_SUCCESS so the mock
        client calls RunInstaller("launch_success") to confirm launch health
        before returning the exit code (simulates a client that confirms CEF
        is healthy, possibly before a later crash).

        If this launch leaves the sentinel running=true (an unconfirmed failure
        exit — not success/neutral, and no launch_success), this method sleeps
        briefly before returning. The next launch's scan calls IsProcessAlive()
        on this process's PID, and the sleep gives that next process a distinct
        creation time so a same-granularity-window PID reuse can't make a dead
        crasher look alive. Success/neutral exits and launch_success leave
        running=false and are never liveness-checked, so they don't sleep.
        """
    marker_path = os.path.join(self.temp_dir, 'e2e_marker.json')
    env = {
        'CEF_E2E_MARKER_PATH': marker_path,
        'CEF_E2E_EXIT_CODE': str(exit_code),
    }
    if launch_success:
      env['CEF_E2E_LAUNCH_SUCCESS'] = '1'
    rc, stdout, stderr = self.run_installer(args=[],
                                            exe=app_exe,
                                            use_local_cdn=True,
                                            env=env,
                                            timeout=timeout)
    marker_data = None
    if os.path.isfile(marker_path):
      with open(marker_path) as f:
        marker_data = json.load(f)
    if exit_code not in _SUCCESS_OR_NEUTRAL_EXIT_CODES and not launch_success:
      time.sleep(0.5)
    return rc, marker_data

  def _standalone_install(self, version, abi_hash, appid, vmin=None, vmax=''):
    """Install a version using standalone /cef-update."""
    self.embed_test_config(appid=appid,
                           vmin=vmin or version,
                           abi_hash=abi_hash,
                           vmax=vmax,
                           launch_health='exit_code',
                           enable_explicit_modes=True)
    exit_code, _, stderr = self.run_installer(
        [FLAG_UPDATE, FLAG_HEADLESS, FLAG_FORCECHECK])
    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Install failed: {stderr.decode(errors="replace")}')
    self.assert_version_installed(version)

  def test_fresh_install_clean_exit(self):
    """Install a version, run launcher with exit 0, verify confirmed state."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00a01-0001-0001-0001-000000000001'

    # Install version via standalone.
    self._standalone_install(version, abi_hash, appid)

    # Set up launcher.
    app_exe, app_name = self.setup_launcher('LH01App')
    self._embed_launcher_config(app_exe, appid, version, abi_hash)

    # Run launcher with exit code 0.
    rc, marker = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)
    self.assertIsNotNone(marker, 'Marker file not written')

    # Verify launch state is confirmed in .launch/ dir.
    appid_hash = self.get_appid_hash(appid)
    self.assert_launch_state(version, appid_hash, running=False, failures=0)

  def test_three_crashes_trigger_rollback(self):
    """Three crashes on newer version trigger rollback to older."""
    v1 = self.test_version
    v1_1 = self.test_version_update
    _, abi_hash = self.build_cdn(version=v1)
    self.build_cdn(version=v1_1, abi_hash=abi_hash)
    appid = 'e2e00a02-0002-0002-0002-000000000002'

    # Install v1 via standalone with vmax pinning.
    self._standalone_install(v1, abi_hash, appid, vmin=v1, vmax=v1)

    # Set up launcher.
    app_exe, app_name = self.setup_launcher('LH02App')
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)

    # Confirm v1 (exit 0).
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)

    # Install v1_1 (both versions now available). Use vmin=v1_1 to force
    # the installer to install the newer version (v1 already satisfies
    # vmin=v1).
    self._standalone_install(v1_1, abi_hash, appid, vmin=v1_1)

    # Re-embed config with vmin=v1 so both versions are eligible.
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)

    # Crash v1_1 three times (exit code 1).
    for i in range(3):
      rc, _ = self._run_launcher_with_exit_code(app_exe, 1)

    # Run launcher with exit 0 — should rollback to v1.
    rc, marker = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)
    self.assertIsNotNone(marker)

    # Verify v1 is now confirmed in .launch/ dir.
    appid_hash = self.get_appid_hash(appid)
    self.assert_launch_state(v1, appid_hash, running=False, failures=0)

    # v1_1 should still show running=true, failures=2 (sentinel from last crash).
    state = self.read_launch_state(v1_1, appid_hash)
    self.assertIsNotNone(state)
    self.assertTrue(state.get('running'))
    self.assertEqual(2, state.get('consecutive_failures'))

  def test_neutral_exit_preserves_history(self):
    """Neutral exit preserves failure history without incrementing."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00a03-0003-0003-0003-000000000003'

    self._standalone_install(version, abi_hash, appid)
    app_exe, app_name = self.setup_launcher('LH03App')
    self._embed_launcher_config(app_exe, appid, version, abi_hash)

    # One crash.
    rc, _ = self._run_launcher_with_exit_code(app_exe, 1)

    # Neutral exit (code 21 = CEF_RESULT_CODE_PROFILE_IN_USE).
    rc, _ = self._run_launcher_with_exit_code(app_exe, 21)
    self.assertEqual(21, rc)

    # Verify launch state in .launch/ dir: running=false, failures=1 (preserved).
    appid_hash = self.get_appid_hash(appid)
    self.assert_launch_state(version, appid_hash, running=False, failures=1)

    # Run launcher with exit 0 — version still selected (under threshold).
    rc, marker = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)

  def test_clean_exit_deletes_older_launch_files(self):
    """Confirming a newer version deletes older versions' confirmed launch files."""
    v1 = self.test_version
    v1_1 = self.test_version_update
    _, abi_hash = self.build_cdn(version=v1)
    self.build_cdn(version=v1_1, abi_hash=abi_hash)
    appid = 'e2e00a04-0004-0004-0004-000000000004'

    # Install v1 only first, so the launcher picks it (no other choice).
    # Use a second app with vmin=v1 to keep global vmin low so GC
    # doesn't delete v1's .launch/ file when v1_1 is installed later.
    appid2 = 'e2e00a04-0004-0004-0004-000000000002'
    self._standalone_install(v1, abi_hash, appid, vmin=v1, vmax=v1)
    self.embed_test_config(appid=appid2,
                           vmin=v1,
                           vmax=v1,
                           abi_hash=abi_hash,
                           enable_explicit_modes=True)
    self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])

    # Confirm v1 via launcher (only version installed).
    app_exe, app_name = self.setup_launcher('LH04App')
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)

    appid_hash = self.get_appid_hash(appid)
    self.assert_launch_state(v1, appid_hash, running=False, failures=0)

    # Now install v1_1. The standalone install's prune won't GC v1's
    # file because appid2 keeps global vmin at v1.
    self._standalone_install(v1_1, abi_hash, appid, vmin=v1_1)

    # Confirm v1_1 — cleanup should delete v1's confirmed file.
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)

    # v1's confirmed launch file should be deleted by cleanup-on-confirm.
    self.assert_no_launch_state(v1, appid_hash)
    # v1_1's launch file should exist (confirmed).
    self.assert_launch_state(v1_1, appid_hash, running=False, failures=0)

  def test_previously_confirmed_version_starts_crashing(self):
    """A previously confirmed version can be disqualified if it starts crashing."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00a08-0008-0008-0008-000000000008'

    self._standalone_install(version, abi_hash, appid)
    app_exe, app_name = self.setup_launcher('LH08App')
    self._embed_launcher_config(app_exe, appid, version, abi_hash)

    # Confirm the version.
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)

    appid_hash = self.get_appid_hash(appid)
    self.assert_launch_state(version, appid_hash, running=False, failures=0)

    # Now crash 3 times.
    for _ in range(3):
      rc, _ = self._run_launcher_with_exit_code(app_exe, 1)

    # The version is the only one so it should still be selected (last resort),
    # but the launch state should show it was crashing.
    state = self.read_launch_state(version, appid_hash)
    self.assertIsNotNone(state)
    self.assertTrue(state.get('running'))
    self.assertEqual(2, state.get('consecutive_failures'))

  def test_post_exit_pruning(self):
    """Confirming a newer version allows older to be pruned post-exit."""
    v1 = self.test_version
    v1_1 = self.test_version_update
    _, abi_hash = self.build_cdn(version=v1)
    self.build_cdn(version=v1_1, abi_hash=abi_hash)
    appid = 'e2e00a05-0005-0005-0005-000000000005'

    # Install and confirm v1.
    self._standalone_install(v1, abi_hash, appid, vmin=v1, vmax=v1)
    app_exe, app_name = self.setup_launcher('LH05App')
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)

    # v1 is confirmed — should be protected from pruning.
    self.assert_version_installed(v1)

    # Install v1_1 and confirm it.
    self._standalone_install(v1_1, abi_hash, appid, vmin=v1_1)
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)

    # v1's launch file was deleted (cleanup on confirm). Post-exit pruning
    # should have pruned v1 since it's no longer protected.
    self.assert_version_not_installed(v1)
    self.assert_version_installed(v1_1)

  def test_disqualified_version_stays_disqualified(self):
    """A disqualified version stays disqualified across subsequent launches."""
    v1 = self.test_version
    v1_1 = self.test_version_update
    _, abi_hash = self.build_cdn(version=v1)
    self.build_cdn(version=v1_1, abi_hash=abi_hash)
    appid = 'e2e00a06-0006-0006-0006-000000000006'

    # Install and confirm v1.
    self._standalone_install(v1, abi_hash, appid, vmin=v1, vmax=v1)
    app_exe, app_name = self.setup_launcher('LH06App')
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)

    # Install v1_1.
    self._standalone_install(v1_1, abi_hash, appid, vmin=v1_1)
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)

    # Crash v1_1 three times.
    for _ in range(3):
      rc, _ = self._run_launcher_with_exit_code(app_exe, 1)

    appid_hash = self.get_appid_hash(appid)

    # Record v1_1's launch state after disqualification.
    state_after_crashes = self.read_launch_state(v1_1, appid_hash)
    self.assertIsNotNone(state_after_crashes)

    # Run launcher twice more with exit 0 — v1 should be selected both times.
    for _ in range(2):
      rc, marker = self._run_launcher_with_exit_code(app_exe, 0)
      self.assertEqual(0, rc)
      self.assertIsNotNone(marker)
      # Marker should show v1 (rollback), not v1_1.
      self.assertIn(v1, marker.get('libcef_version_full', ''))
      self.assertNotIn(v1_1, marker.get('libcef_version_full', ''))

    # v1_1's launch state should be unchanged (still disqualified).
    state_now = self.read_launch_state(v1_1, appid_hash)
    self.assertEqual(state_after_crashes, state_now)

  def test_bundled_preferred_over_disqualified(self):
    """Non-revoked bundled version is selected over a disqualified installed."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00a09-0009-0009-0009-000000000009'

    # Install and confirm the version.
    self._standalone_install(version, abi_hash, appid)
    app_exe, app_name = self.setup_launcher('LH09App')
    self._embed_launcher_config(app_exe, appid, version, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)

    # Crash 3 times to disqualify.
    for _ in range(3):
      rc, _ = self._run_launcher_with_exit_code(app_exe, 1)

    appid_hash = self.get_appid_hash(appid)
    state = self.read_launch_state(version, appid_hash)
    self.assertIsNotNone(state)
    self.assertTrue(state.get('running'))
    self.assertEqual(2, state.get('consecutive_failures'))

    # Create a bundled CEF directory with the same version.
    bundled_dir = os.path.join(self.temp_dir, 'bundled', 'windows64')
    release_dir = os.path.join(bundled_dir, 'Release')
    os.makedirs(release_dir, exist_ok=True)
    with open(os.path.join(bundled_dir, 'cef_version.json'), 'w') as f:
      json.dump(
          {
              'version': version,
              'abi_hash': abi_hash,
              'platform': 'windows64',
          }, f)
    with open(os.path.join(release_dir, 'libcef.dll'), 'wb') as f:
      f.write(b'fake bundled dll')
    with open(os.path.join(bundled_dir, 'catalog.cat'), 'wb') as f:
      f.write(b'fake bundled catalog')

    # Embed config with bundled_cef_path into the client DLL. The bootstrap
    # only parses bundled_cef_path from the client DLL resource (not the exe
    # fallback), because allow_bundled_cef_path is only set for that source.
    app_dll = os.path.join(self.build_dir, f'{app_name}.dll')
    config = {
        'appid': appid,
        'vmin': version,
        'abi_hash': abi_hash,
        'bundled_cef_path': bundled_dir,
        'launch_health': 'exit_code',
    }
    if self.test_thumbprint:
      config['certificate_thumbprint'] = self.test_thumbprint
    self.embed_config_resource(app_dll, config)

    # Run launcher — should select bundled over disqualified installed.
    rc, marker = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)
    self.assertIsNotNone(marker, 'Marker file not written')
    self.assertEqual(1, marker.get('libcef_is_bundled'),
                     f'Expected bundled version, got marker: {marker}')

  def test_pid_guard_prevents_stale_update(self):
    """PID guard prevents a foreign PID from being treated as a crash."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00a07-0007-0007-0007-000000000007'

    self._standalone_install(version, abi_hash, appid)
    app_exe, app_name = self.setup_launcher('LH07App')
    self._embed_launcher_config(app_exe, appid, version, abi_hash)

    # Confirm the version.
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)

    appid_hash = self.get_appid_hash(appid)
    self.assert_launch_state(version, appid_hash, running=False, failures=0)

    # Overwrite launch state in .launch/ dir with a foreign PID.
    launch_dir = self.get_launch_dir()
    os.makedirs(launch_dir, exist_ok=True)
    ls_path = os.path.join(launch_dir, f'{appid_hash}_{version}_windows64')
    foreign_state = {
        'appid': appid,
        'pid': 99999,
        'pid_start_time': '999999999999999',
        'consecutive_failures': 2,
        'running': True,
        'confirmed': False,
        'version': version,
        'platform': 'windows64',
        'last_update': str(self.filetime_now()),
    }
    self.write_integrity_json(ls_path, foreign_state)

    # Run launcher with exit 0. The controller should see the foreign PID
    # as a dead process (PID 99999 doesn't exist), compute projected=3,
    # and disqualify the version. But since it's the only version, it falls
    # back to it. The bootstrap writes a new sentinel with its own PID,
    # then confirms on exit 0.
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)

    # After clean exit, the state should be confirmed in .launch/ dir.
    state = self.read_launch_state(version, appid_hash)
    self.assertIsNotNone(state)
    self.assertFalse(state.get('running'))
    self.assertEqual(0, state.get('consecutive_failures'))
    # PID should be different from the foreign one.
    self.assertNotEqual(99999, state.get('pid'))

  # ===========================================================================
  # Doom loop prevention
  # ===========================================================================

  def test_crash_history_survives_pruning(self):
    """Crash history in .launch/ survives version directory pruning."""
    v1 = self.test_version
    v1_1 = self.test_version_update
    v1_2 = self.test_version_higher
    _, abi_hash = self.build_cdn(version=v1)
    self.build_cdn(version=v1_1, abi_hash=abi_hash)
    self.build_cdn(version=v1_2, abi_hash=abi_hash)
    appid = 'e2e00a10-0010-0010-0010-000000000010'

    # Install v1 and v1_1.
    self._standalone_install(v1, abi_hash, appid, vmin=v1, vmax=v1)
    self._standalone_install(v1_1, abi_hash, appid, vmin=v1_1, vmax=v1_1)

    # Crash v1_1 (one crash, then neutral exit to get running=false).
    app_exe, app_name = self.setup_launcher('LH10App')
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 1)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 21)
    self.assertEqual(21, rc)

    appid_hash = self.get_appid_hash(appid)
    state_v1_1 = self.read_launch_state(v1_1, appid_hash)
    self.assertIsNotNone(state_v1_1)
    self.assertEqual(1, state_v1_1.get('consecutive_failures'))

    # Register a second app with vmin=v1 to keep the global vmin low
    # (GC only deletes below the minimum vmin across ALL apps). Then
    # install v1_2 via the first app with vmin=v1_2.
    appid2 = 'e2e00a10-0010-0010-0010-000000000002'
    self.embed_test_config(appid=appid2,
                           vmin=v1,
                           vmax=v1,
                           abi_hash=abi_hash,
                           enable_explicit_modes=True)
    self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self._standalone_install(v1_2, abi_hash, appid, vmin=v1_2)
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)

    # v1_1's directory should be pruned (not required, not confirmed).
    self.assert_version_not_installed(v1_1)

    # But v1_1's .launch/ crash history should still exist.
    state_after = self.read_launch_state(v1_1, appid_hash)
    self.assertIsNotNone(state_after,
                         '.launch/ crash history should survive pruning')

  def test_cleanup_preserves_crash_history(self):
    """Confirming a newer version preserves an older version's crash-history file."""
    v1 = self.test_version
    v1_1 = self.test_version_update
    _, abi_hash = self.build_cdn(version=v1)
    self.build_cdn(version=v1_1, abi_hash=abi_hash)
    appid = 'e2e00a11-0011-0011-0011-000000000011'

    # Install v1 only. Use a second app with vmin=v1 to keep global
    # vmin low when v1_1 is installed later.
    appid2 = 'e2e00a11-0011-0011-0011-000000000002'
    self._standalone_install(v1, abi_hash, appid, vmin=v1, vmax=v1)
    self.embed_test_config(appid=appid2,
                           vmin=v1,
                           vmax=v1,
                           abi_hash=abi_hash,
                           enable_explicit_modes=True)
    self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])

    # Crash v1 once (it's the only version), then neutral exit.
    app_exe, app_name = self.setup_launcher('LH11App')
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 1)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 21)
    self.assertEqual(21, rc)

    appid_hash = self.get_appid_hash(appid)
    state_v1 = self.read_launch_state(v1, appid_hash)
    self.assertIsNotNone(state_v1)
    self.assertEqual(1, state_v1.get('consecutive_failures'))

    # Install v1_1.
    self._standalone_install(v1_1, abi_hash, appid, vmin=v1_1)

    # Confirm v1_1 (launcher selects it as newest).
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)

    # v1's crash-history file should be preserved (not deleted by cleanup).
    state_v1_after = self.read_launch_state(v1, appid_hash)
    self.assertIsNotNone(state_v1_after,
                         'v1 crash history should survive cleanup')
    self.assertEqual(1, state_v1_after.get('consecutive_failures'))

    # v1_1 should be confirmed.
    self.assert_launch_state(v1_1, appid_hash, running=False, failures=0)

  def test_stale_launch_files_gc(self):
    """Recent below-vmin history survives and explicitly old history expires."""
    v1 = self.test_version
    v1_1 = self.test_version_update
    v1_2 = self.test_version_higher
    _, abi_hash = self.build_cdn(version=v1)
    self.build_cdn(version=v1_1, abi_hash=abi_hash)
    appid = 'e2e00a12-0012-0012-0012-000000000012'

    # Install v1 and publish recent unconfirmed crash history.
    self._standalone_install(v1, abi_hash, appid, vmin=v1, vmax=v1)
    appid_hash = self.get_appid_hash(appid)
    self.write_health(appid,
                      v1,
                      self.filetime_now(),
                      confirmed=False,
                      consecutive_failures=1)

    # Install v1_1 with vmin=v1_1 (moves the minimum forward).
    self._standalone_install(v1_1, abi_hash, appid, vmin=v1_1)
    self.assert_version_not_installed(v1)
    self.assert_launch_state(v1, appid_hash, running=False, failures=1)

    # An already-old record is removed on the first eligible prune.
    old_time = self.filetime_now() - 91 * 24 * 60 * 60 * 10000000
    self.write_health(appid,
                      v1,
                      self.filetime_now(),
                      last_update=old_time,
                      confirmed=False,
                      consecutive_failures=1)
    self.build_cdn(version=v1_2, abi_hash=abi_hash)
    self._standalone_install(v1_2, abi_hash, appid, vmin=v1_2)
    self.assert_no_launch_state(v1, appid_hash)

  def test_uninstall_reinstall_retains_disqualification(self):
    """Recent failure history prevents redownloading the same bad version."""
    good = self.test_version_update
    bad = self.test_version_higher
    _, abi_hash = self.build_cdn(version=bad)
    appid = 'e2e00a13-0013-0013-0013-000000000013'
    self._standalone_install(bad, abi_hash, appid, vmin=bad, vmax=bad)
    appid_hash = self.get_appid_hash(appid)
    self.write_health(appid,
                      bad,
                      self.filetime_now(),
                      running=True,
                      confirmed=False,
                      consecutive_failures=2)

    exit_code, _, stderr = self.run_installer(
        [FLAG_UNINSTALL, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Uninstall failed: {stderr.decode(errors="replace")}')
    self.assert_version_not_installed(bad)
    self.assert_launch_state(bad, appid_hash, running=True, failures=2)

    self.build_cdn(version=good, abi_hash=abi_hash)
    cert_path, key_path = self.get_loopback_tls_certificate()
    cdn_url, requests = self.start_https_file_server(
        self.cdn_dir, cert_path, key_path)
    self.embed_test_config(appid=appid,
                           vmin=good,
                           vmax=bad,
                           abi_hash=abi_hash,
                           launch_health='exit_code',
                           enable_explicit_modes=True,
                           cdn_urls=[cdn_url])
    exit_code, _, stderr = self.run_installer(
        [FLAG_UPDATE, FLAG_HEADLESS, FLAG_FORCECHECK],
        use_local_cdn=False,
        allow_configured_cdn=True,
        timeout=60,
        env={'CEF_INSTALLER_IGNORE_CERTIFICATE_ERRORS_FOR_TESTING': '1'})

    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Reinstall failed: {stderr.decode(errors="replace")}')
    self.assert_version_installed(good)
    self.assert_version_not_installed(bad)
    good_archive = f'cef_{good}_windows64.tar.xz'
    bad_archive = f'cef_{bad}_windows64.tar.xz'
    self.assertTrue(any(good_archive in request for request in requests),
                    f'Good archive was not requested: {requests}')
    self.assertFalse(any(bad_archive in request for request in requests),
                     f'Disqualified archive was requested: {requests}')

  # ===========================================================================
  # Explicit launch success confirmation (kLaunchSuccess)
  # ===========================================================================

  def test_launch_success_prevents_crash_penalty(self):
    """launch_success before a crash leaves the version confirmed."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00b01-0b01-0b01-0b01-00000000b001'

    self._standalone_install(version, abi_hash, appid)
    app_exe, app_name = self.setup_launcher('LS01App')
    self._embed_launcher_config(app_exe, appid, version, abi_hash)

    # First leave a pre-launch running publication behind.
    rc, _ = self._run_launcher_with_exit_code(app_exe, 1)
    self.assertEqual(1, rc)
    appid_hash = self.get_appid_hash(appid)
    before = self.read_launch_state(version, appid_hash)
    self.assertIsNotNone(before)
    before_update = int(before.get('last_update', '0'))

    # Client confirms launch health, then the app crashes (exit 1). Because
    # the sentinel was confirmed during RunWinMain, the crash exit does not
    # revert it: the post-exit handler doesn't run for a failure exit, and the
    # sentinel is already running=false, failures=0.
    rc, _ = self._run_launcher_with_exit_code(app_exe, 1, launch_success=True)
    self.assertEqual(1, rc)

    self.assert_launch_state(version, appid_hash, running=False, failures=0)
    confirmed = self.read_launch_state(version, appid_hash)
    self.assertGreater(int(confirmed.get('last_update', '0')), before_update)

    # Next launch: version is confirmed (not disqualified) and selected again.
    rc, marker = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)
    self.assertIsNotNone(marker)
    self.assertIn(version, marker.get('libcef_version_full', ''))

  def test_launch_success_clean_exit(self):
    """launch_success + clean exit confirms and prunes/cleans up older files."""
    v1 = self.test_version
    v1_1 = self.test_version_update
    _, abi_hash = self.build_cdn(version=v1)
    self.build_cdn(version=v1_1, abi_hash=abi_hash)
    appid = 'e2e00b02-0b02-0b02-0b02-00000000b002'

    # Install v1 only first; keep global vmin low with a second app so v1's
    # .launch file isn't GC'd when v1_1 is installed later.
    appid2 = 'e2e00b02-0b02-0b02-0b02-000000000002'
    self._standalone_install(v1, abi_hash, appid, vmin=v1, vmax=v1)
    self.embed_test_config(appid=appid2,
                           vmin=v1,
                           vmax=v1,
                           abi_hash=abi_hash,
                           enable_explicit_modes=True)
    self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])

    app_exe, app_name = self.setup_launcher('LS02App')
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0, launch_success=True)
    self.assertEqual(0, rc)

    appid_hash = self.get_appid_hash(appid)
    self.assert_launch_state(v1, appid_hash, running=False, failures=0)

    # Install v1_1, then confirm it via launch_success on a clean exit. The
    # post-exit handler (exit 0) deletes v1's older confirmed .launch file.
    self._standalone_install(v1_1, abi_hash, appid, vmin=v1_1)
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0, launch_success=True)
    self.assertEqual(0, rc)

    self.assert_launch_state(v1_1, appid_hash, running=False, failures=0)
    self.assert_no_launch_state(v1, appid_hash)

  def test_without_launch_success_crash_still_counts(self):
    """Without launch_success, an early crash counts against the version."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00b03-0b03-0b03-0b03-00000000b003'

    self._standalone_install(version, abi_hash, appid)
    app_exe, app_name = self.setup_launcher('LS03App')
    self._embed_launcher_config(app_exe, appid, version, abi_hash)

    # Crash without confirming: sentinel left running=true (detected next time).
    rc, _ = self._run_launcher_with_exit_code(app_exe, 1)
    self.assertEqual(1, rc)

    appid_hash = self.get_appid_hash(appid)
    state = self.read_launch_state(version, appid_hash)
    self.assertIsNotNone(state)
    self.assertTrue(state.get('running'))

    # A second crash launch: the prior crash is projected (failures=1) and
    # persisted into the new sentinel before RunWinMain.
    rc, _ = self._run_launcher_with_exit_code(app_exe, 1)
    self.assertEqual(1, rc)
    self.assert_launch_state(version, appid_hash, running=True, failures=1)

  def test_launch_success_resets_prior_crash_history(self):
    """launch_success clears a projected failure from a prior crash."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00b04-0b04-0b04-0b04-00000000b004'

    self._standalone_install(version, abi_hash, appid)
    app_exe, app_name = self.setup_launcher('LS04App')
    self._embed_launcher_config(app_exe, appid, version, abi_hash)

    # Crash once without confirming.
    rc, _ = self._run_launcher_with_exit_code(app_exe, 1)
    self.assertEqual(1, rc)

    appid_hash = self.get_appid_hash(appid)
    self.assertTrue(self.read_launch_state(version, appid_hash).get('running'))

    # Next launch confirms via launch_success then exits cleanly. The
    # projected failure from the prior crash is reset to 0.
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0, launch_success=True)
    self.assertEqual(0, rc)
    self.assert_launch_state(version, appid_hash, running=False, failures=0)

  def test_launch_success_then_three_early_crashes_disqualify(self):
    """Confirmation is per-launch: later early crashes still disqualify."""
    v1 = self.test_version
    v1_1 = self.test_version_update
    _, abi_hash = self.build_cdn(version=v1)
    self.build_cdn(version=v1_1, abi_hash=abi_hash)
    appid = 'e2e00b05-0b05-0b05-0b05-00000000b005'

    # Install and confirm v1 (the known-good fallback).
    self._standalone_install(v1, abi_hash, appid, vmin=v1, vmax=v1)
    app_exe, app_name = self.setup_launcher('LS05App')
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)

    # Install v1_1 and confirm it once via launch_success, then crash (exit 1).
    # The confirmation is durable (written during RunWinMain), and the failure
    # exit skips post-exit cleanup/prune so v1's protected .launch file — and
    # thus the v1 fallback — survives for the rollback assertion below.
    self._standalone_install(v1_1, abi_hash, appid, vmin=v1_1)
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 1, launch_success=True)
    self.assertEqual(1, rc)

    appid_hash = self.get_appid_hash(appid)
    self.assert_launch_state(v1_1, appid_hash, running=False, failures=0)

    # Now crash v1_1 three times WITHOUT confirming (early crashes). The prior
    # confirmation does not carry over — each launch starts fresh.
    for _ in range(3):
      rc, _ = self._run_launcher_with_exit_code(app_exe, 1)

    # Next clean launch: v1_1 is disqualified (projected=3) → rollback to v1.
    rc, marker = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)
    self.assertIsNotNone(marker)
    self.assertIn(v1, marker.get('libcef_version_full', ''))
    self.assertNotIn(v1_1, marker.get('libcef_version_full', ''))

  def test_neutral_exit_does_not_prune(self):
    """Neutral exit does not prune older unused versions; a clean exit does."""
    v1 = self.test_version
    v1_1 = self.test_version_update
    _, abi_hash = self.build_cdn(version=v1)
    self.build_cdn(version=v1_1, abi_hash=abi_hash)
    appid = 'e2e00b06-0b06-0b06-0b06-00000000b006'

    # Install and confirm v1.
    self._standalone_install(v1, abi_hash, appid, vmin=v1, vmax=v1)
    app_exe, app_name = self.setup_launcher('LS06App')
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)
    self.assert_version_installed(v1)

    # Keep the platform global vmin at v1 without requiring v1 once v1_1 is
    # installed. This isolates confirmed protection from below-vmin GC.
    appid2 = 'e2e00b06-0b06-0b06-0b06-000000000002'
    self.embed_test_config(appid=appid2,
                           vmin=v1,
                           vmax=v1_1,
                           abi_hash=abi_hash,
                           enable_explicit_modes=True)
    self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])

    # Install v1_1 (now the best match; v1 becomes prunable).
    self._standalone_install(v1_1, abi_hash, appid, vmin=v1_1)
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)

    # Run v1_1 with a neutral exit (21 = CEF_RESULT_CODE_PROFILE_IN_USE).
    # Pruning must NOT run on a neutral exit (a concurrent live instance may
    # hold libcef.dll), so v1 survives.
    rc, _ = self._run_launcher_with_exit_code(app_exe, 21)
    self.assertEqual(21, rc)
    self.assert_version_installed(v1)
    self.assert_version_installed(v1_1)

    # A following clean exit DOES prune the now-unused v1.
    self._embed_launcher_config(app_exe, appid, v1, abi_hash)
    rc, _ = self._run_launcher_with_exit_code(app_exe, 0)
    self.assertEqual(0, rc)
    self.assert_version_not_installed(v1)
    self.assert_version_installed(v1_1)


if __name__ == '__main__':
  unittest.main()
