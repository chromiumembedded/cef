#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""E2E tests for the CEF installer uninstall flow."""

import ctypes
import json
import os
import shutil
import subprocess
import tempfile
import time
import unittest

from e2e_test_base import (E2ETestBase, EXIT_CANCELLED, EXIT_CONFIG_ERROR,
                           EXIT_DATABASE_ERROR, EXIT_INSTALL_ERROR,
                           EXIT_INDEX_ERROR, EXIT_RELAUNCHED, EXIT_SUCCESS,
                           FLAG_HEADLESS, FLAG_INSTALL_PATH, FLAG_UNINSTALL,
                           FLAG_UNINSTALL_RELAUNCHED, FLAG_UPDATE,
                           FLAG_BACKGROUND, FLAG_PARENT)
from lifecycle_receiver import correlate_helper_output


class TestUninstallFlow(E2ETestBase):

  def _stage_default_console_launcher(self, module_name):
    """Stage default-named bootstrapc.exe and a selected mock client DLL."""
    host_dir = os.path.join(self.temp_dir, f'host-{module_name}')
    os.makedirs(host_dir)
    host_exe = os.path.join(host_dir, 'bootstrapc.exe')
    module_path = os.path.join(host_dir, f'{module_name}.dll')
    shutil.copy2(os.path.join(self.build_dir, 'bootstrapc.exe'), host_exe)
    shutil.copy2(os.path.join(self.build_dir, 'chrome_elf.dll'),
                 os.path.join(host_dir, 'chrome_elf.dll'))
    crash_config = os.path.join(self.build_dir, 'crash_reporter.cfg')
    if os.path.isfile(crash_config):
      shutil.copy2(crash_config, os.path.join(host_dir, 'crash_reporter.cfg'))
    shutil.copy2(self.mock_client_dll_src, module_path)
    return host_exe, module_path

  def _run_default_console_uninstall(self, host_exe, module_name, parent_window,
                                     env, use_install_path_override):
    """Run the async console parent without capturing child-inherited pipes."""
    command = [
        host_exe, f'--module={module_name}', FLAG_UNINSTALL, FLAG_BACKGROUND,
        FLAG_HEADLESS, f'{FLAG_PARENT}={parent_window}'
    ]
    if use_install_path_override:
      command.append(f'{FLAG_INSTALL_PATH}={self.install_dir}')
    run_env = os.environ.copy()
    run_env.update(env)
    process = subprocess.run(command,
                             stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL,
                             timeout=30,
                             cwd=self.build_dir,
                             env=run_env)
    return process.returncode

  def _start_lifecycle_receiver(self, *options):
    helper = os.path.join(self.build_dir, 'cef_e2e_progress_helper.exe')
    if not os.path.isfile(helper):
      self.skipTest('cef_e2e_progress_helper.exe not found')
    output = os.path.join(self.temp_dir, f'lifecycle-{time.time_ns()}.json')
    process = subprocess.Popen([helper, output, '--lifecycle', *options],
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE,
                               cwd=self.build_dir)
    line = process.stdout.readline().decode().strip()
    self.assertTrue(line, 'No HWND from lifecycle receiver')
    return process, int(line), output

  def _finish_lifecycle_receiver(self, process, hwnd, output, await_terminal):
    try:
      if await_terminal:
        try:
          process.wait(timeout=15)
        except subprocess.TimeoutExpired:
          pass
      if process.poll() is None:
        ctypes.windll.user32.PostMessageW(hwnd, 0x0010, 0, 0)
        process.wait(timeout=10)
    finally:
      if process.poll() is None:
        process.kill()
        process.wait(timeout=10)
    self.assertTrue(os.path.isfile(output), 'Receiver output was not written')
    with open(output, encoding='utf-8') as output_file:
      captured = json.load(output_file)
    return captured, correlate_helper_output(captured)

  def _install_for_lifecycle(self, appid):
    version, abi_hash = self.build_cdn()
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)
    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code, stderr.decode(errors='replace'))
    return version

  def _wait_for_relaunch_child_exit(self, relaunch_dir, timeout=10):
    """Wait for the child image to close before removing its test directory."""
    deadline = time.time() + timeout
    while time.time() < deadline:
      try:
        shutil.rmtree(relaunch_dir)
        return
      except OSError:
        time.sleep(0.05)
    self.fail(f'Relaunched child did not exit before timeout: {relaunch_dir}')

  def _wait_for_path(self, path, description, timeout=10):
    deadline = time.time() + timeout
    while time.time() < deadline and not os.path.isfile(path):
      time.sleep(0.05)
    self.assertTrue(os.path.isfile(path), description)

  def _run_lifecycle_fault(self, appid, environment=None, receiver_options=()):
    version = self._install_for_lifecycle(appid)
    staged_exe = self.stage_bootstrap_in_install_root(self.install_dir,
                                                      'LifecycleFault')
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    receiver, hwnd, output = self._start_lifecycle_receiver(*receiver_options)
    env = {'PATH': self.build_dir + os.pathsep + os.environ.get('PATH', '')}
    env.update(environment or {})
    exit_code, _, stderr = self.run_installer(
        [FLAG_UNINSTALL, FLAG_HEADLESS, f'{FLAG_PARENT}={hwnd}'],
        exe=staged_exe,
        env=env)
    self.assertEqual(EXIT_RELAUNCHED, exit_code,
                     stderr.decode(errors='replace'))
    captured, correlated = self._finish_lifecycle_receiver(receiver,
                                                           hwnd,
                                                           output,
                                                           await_terminal=True)
    self.assertEqual(1, len(correlated['handoffs']), captured)
    self.assertEqual(1, len(correlated['results']), captured)
    operation_id = next(iter(correlated['handoffs']))
    self.assertIn(operation_id, correlated['results'])
    for path in self.snapshot_uninstall_relaunch_dirs() - relaunch_dirs:
      self.addCleanup(shutil.rmtree, path, ignore_errors=True)
    return version, captured, correlated['results'][operation_id]

  def test_uninstall_removes_app(self):
    """External uninstall is synchronous and prunes the version."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00008-0008-0008-0008-000000000008'
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)

    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Install failed: {stderr.decode(errors="replace")}')
    self.assert_version_installed(version)

    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    exit_code, _, stderr = self.run_installer([FLAG_UNINSTALL, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Uninstall failed: {stderr.decode(errors="replace")}')
    self.assertEqual(relaunch_dirs, self.snapshot_uninstall_relaunch_dirs())

    self.assert_version_not_installed(version)
    self.assertFalse(self._database_contains_app(self.install_dir, appid))

  def test_uninstall_with_other_apps(self):
    """Synchronous uninstall preserves another app's version."""
    version, abi_hash = self.build_cdn()
    appid_a = 'e2e00009-aaaa-aaaa-aaaa-aaaaaaaaaaaa'
    appid_b = 'e2e00009-bbbb-bbbb-bbbb-bbbbbbbbbbbb'

    self.embed_test_config(appid=appid_a, vmin=version, abi_hash=abi_hash)
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)
    self.embed_test_config(appid=appid_b, vmin=version, abi_hash=abi_hash)
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)

    self.embed_test_config(appid=appid_a, vmin=version, abi_hash=abi_hash)
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    exit_code, _, _ = self.run_installer([FLAG_UNINSTALL, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)
    self.assertEqual(relaunch_dirs, self.snapshot_uninstall_relaunch_dirs())

    self.assert_version_installed(version)
    db = self.read_database()
    uuids = [app.get('uuid') for app in db.get('apps', [])]
    self.assertNotIn(appid_a, uuids)
    self.assertIn(appid_b, uuids)

  def test_uninstall_contained_executable_relocation(self):
    """An executable inside the writable target returns exactly 109."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00010-0010-0010-0010-000000000010'
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)
    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code,
                     f'Install failed: {stderr.decode(errors="replace")}')
    self.assert_version_installed(version)

    staged_exe = self.stage_bootstrap_in_install_root(self.install_dir)
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    env = {'PATH': self.build_dir + os.pathsep + os.environ.get('PATH', '')}
    exit_code, _, stderr = self.run_installer([FLAG_UNINSTALL, FLAG_HEADLESS],
                                              exe=staged_exe,
                                              env=env)
    self.assertEqual(
        EXIT_RELAUNCHED, exit_code, f'Unexpected exit code {exit_code}: '
        f'{stderr.decode(errors="replace")}')
    self.wait_for_relaunched_uninstall(appid,
                                       relaunch_dirs,
                                       os.path.basename(staged_exe),
                                       expected_index_versions=())

    self.assert_version_not_installed(version)
    self.assertFalse(self._database_contains_app(self.install_dir, appid))

  def test_uninstall_required_relaunch_failure_is_105(self):
    """A required relaunch failure does not fall back to direct mutation."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00010-0105-0105-0105-000000000105'
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)
    exit_code, _, stderr = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code, stderr.decode(errors='replace'))
    staged_exe = self.stage_bootstrap_in_install_root(
        self.install_dir, 'FailedContainedUninstall')
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    env = {
        'PATH': self.build_dir + os.pathsep + os.environ.get('PATH', ''),
        'CEF_INSTALLER_TEST_RELAUNCH_FAILURE': '1',
    }

    exit_code, _, stderr = self.run_installer([FLAG_UNINSTALL, FLAG_HEADLESS],
                                              exe=staged_exe,
                                              env=env)

    self.assertEqual(EXIT_INSTALL_ERROR, exit_code,
                     stderr.decode(errors='replace'))
    self.assertEqual(relaunch_dirs, self.snapshot_uninstall_relaunch_dirs())
    self.assertTrue(self._database_contains_app(self.install_dir, appid))
    self.assert_version_installed(version)

  def test_uninstall_marker_only_is_rejected(self):
    """The internal marker cannot bypass trusted relaunch state."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00010-0100-0100-0100-000000000100'
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()

    exit_code, _, _ = self.run_installer(
        [FLAG_UNINSTALL, FLAG_HEADLESS, FLAG_UNINSTALL_RELAUNCHED])

    self.assertEqual(EXIT_CONFIG_ERROR, exit_code)
    self.assertEqual(relaunch_dirs, self.snapshot_uninstall_relaunch_dirs())
    self.assertTrue(self._database_contains_app(self.install_dir, appid))
    self.assert_version_installed(version)

  def test_uninstall_state_on_update_is_rejected(self):
    """Relaunch marker/state transport is invalid on every other command."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00010-0200-0200-0200-000000000200'
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)
    relaunch_dir = tempfile.mkdtemp(prefix='cef_uninstall_')
    self.addCleanup(shutil.rmtree, relaunch_dir, ignore_errors=True)
    staged_exe = self.stage_bootstrap_in_install_root(relaunch_dir,
                                                      'StateOnUpdate')
    nonce = 'valid-state-on-update'
    with open(os.path.join(relaunch_dir, 'cef_uninstall_state.json'),
              'w',
              encoding='utf-8') as state_file:
      json.dump({'install_path': self.install_dir, 'nonce': nonce}, state_file)
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    env = {'PATH': self.build_dir + os.pathsep + os.environ.get('PATH', '')}

    exit_code, _, _ = self.run_installer([
        FLAG_UPDATE, FLAG_HEADLESS, FLAG_UNINSTALL_RELAUNCHED,
        f'/cef-uninstall-state={nonce}'
    ],
                                         exe=staged_exe,
                                         env=env)

    self.assertEqual(EXIT_CONFIG_ERROR, exit_code)
    self.assertEqual(relaunch_dirs, self.snapshot_uninstall_relaunch_dirs())
    self.assertTrue(self._database_contains_app(self.install_dir, appid))
    self.assert_version_installed(version)

  def test_uninstall_invalid_exclusive_custom_has_no_relaunch(self):
    """An invalid exclusive target fails normally without creating a child."""
    version, abi_hash = self.build_cdn()
    self.embed_test_config(appid='e2e00010-0300-0300-0300-000000000300',
                           vmin=version,
                           abi_hash=abi_hash)
    invalid_target = os.path.join(self.temp_dir, 'not-a-directory')
    with open(invalid_target, 'w') as invalid_file:
      invalid_file.write('file')
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()

    exit_code, _, _ = self.run_installer([FLAG_UNINSTALL, FLAG_HEADLESS],
                                         install_path=invalid_target)

    self.assertEqual(EXIT_CONFIG_ERROR, exit_code)
    self.assertEqual(relaunch_dirs, self.snapshot_uninstall_relaunch_dirs())

  def test_uninstall_read_only_exclusive_custom_has_no_relaunch(self):
    """A real read-only exclusive custom store fails without a child."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00010-0301-0301-0301-000000000301'
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)
    self.assertTrue(self._database_contains_app(self.install_dir, appid))
    self.assert_version_installed(version)
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()

    with self.scoped_read_only_directory(self.install_dir):
      exit_code, _, _ = self.run_installer([FLAG_UNINSTALL, FLAG_HEADLESS])

      self.assertEqual(EXIT_CONFIG_ERROR, exit_code)
      self.assertEqual(relaunch_dirs, self.snapshot_uninstall_relaunch_dirs())
      self.assertTrue(self._database_contains_app(self.install_dir, appid))
      self.assert_version_installed(version)

  def test_uninstall_mutation_ineligible_admin_has_no_relaunch(self):
    """An elevated admin role denied mutation never falls through per-user."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00010-0302-0302-0302-000000000302'
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)
    role_root = os.path.join(self.temp_dir, 'role-root')
    admin_store = os.path.join(role_root, 'Admin')
    per_user_store = os.path.join(role_root, 'PerUser')
    os.makedirs(per_user_store)
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS],
                                         install_path=admin_store)
    self.assertEqual(EXIT_SUCCESS, exit_code)
    self.assertTrue(self._database_contains_app(admin_store, appid))
    version_path = os.path.join(admin_store, 'Versions', version, 'windows64')
    self.assertTrue(os.path.isdir(version_path))
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    env = {
        'CEF_INSTALLER_TEST_DIRECTORY_SCENARIO': 'admin_mutation_denied',
        'CEF_INSTALLER_TEST_DIRECTORY_ROOT': role_root,
    }

    exit_code, _, _ = self.run_installer([FLAG_UNINSTALL, FLAG_HEADLESS],
                                         use_install_path_override=False,
                                         env=env)

    self.assertEqual(EXIT_CONFIG_ERROR, exit_code)
    self.assertEqual(relaunch_dirs, self.snapshot_uninstall_relaunch_dirs())
    self.assertTrue(self._database_contains_app(admin_store, appid))
    self.assertTrue(os.path.isdir(version_path))
    self.assertEqual([], os.listdir(per_user_store))

  def test_uninstall_database_failure_is_synchronous(self):
    """An external caller receives the exact controller database failure."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00010-0106-0106-0106-000000000106'
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()

    exit_code, _, _ = self.run_installer(
        [FLAG_UNINSTALL, FLAG_HEADLESS],
        env={'CEF_INSTALLER_TEST_DATABASE_SAVE_FAILURE': '1'})

    self.assertEqual(EXIT_DATABASE_ERROR, exit_code)
    self.assertEqual(relaunch_dirs, self.snapshot_uninstall_relaunch_dirs())
    self.assertTrue(self._database_contains_app(self.install_dir, appid))
    self.assert_version_installed(version)

  def test_uninstall_index_failure_is_synchronous(self):
    """An external caller receives the exact checked-index failure."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00010-0114-0114-0114-000000000114'
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()

    exit_code, _, _ = self.run_installer(
        [FLAG_UNINSTALL, FLAG_HEADLESS],
        env={'CEF_INSTALLER_TEST_INDEX_FAULT': 'write'})

    self.assertEqual(EXIT_INDEX_ERROR, exit_code)
    self.assertEqual(relaunch_dirs, self.snapshot_uninstall_relaunch_dirs())
    self.assertFalse(self._database_contains_app(self.install_dir, appid))
    self.assert_version_installed(version)

  def test_uninstall_cleanup_failure_is_synchronous_success(self):
    """Post-commit physical cleanup failure remains exact exit zero."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00010-0000-0000-0000-000000000109'
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()

    exit_code, _, _ = self.run_installer(
        [FLAG_UNINSTALL, FLAG_HEADLESS],
        env={'CEF_INSTALLER_TEST_FILE_OPS_FAULT': 'trash_move'})

    self.assertEqual(EXIT_SUCCESS, exit_code)
    self.assertEqual(relaunch_dirs, self.snapshot_uninstall_relaunch_dirs())
    self.assertFalse(self._database_contains_app(self.install_dir, appid))
    self.assert_version_installed(version)

  def test_lifecycle_contained_uninstall_correlates_terminal(self):
    appid = 'e2e00010-1001-1001-1001-000000001001'
    version = self._install_for_lifecycle(appid)
    staged_exe = self.stage_bootstrap_in_install_root(self.install_dir,
                                                      'LifecycleContained')
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    receiver, hwnd, output = self._start_lifecycle_receiver()
    env = {'PATH': self.build_dir + os.pathsep + os.environ.get('PATH', '')}
    exit_code, _, stderr = self.run_installer(
        [FLAG_UNINSTALL, FLAG_HEADLESS, f'{FLAG_PARENT}={hwnd}'],
        exe=staged_exe,
        env=env)
    self.assertEqual(EXIT_RELAUNCHED, exit_code,
                     stderr.decode(errors='replace'))
    captured, correlated = self._finish_lifecycle_receiver(receiver,
                                                           hwnd,
                                                           output,
                                                           await_terminal=True)
    self.assertEqual(0, captured['rejected'])
    self.assertEqual(1, len(correlated['handoffs']),
                     f'captured={captured!r}, correlated={correlated!r}')
    self.assertEqual(1, len(correlated['results']))
    operation_id = next(iter(correlated['handoffs']))
    self.assertIn(operation_id, correlated['results'])
    result = correlated['results'][operation_id]
    self.assertTrue(result['success'])
    self.assertEqual('committed', result['outcome'])
    self.assertEqual(0, result['exit_code'])
    serialized = json.dumps(result)
    self.assertNotIn(self.temp_dir, serialized)
    self.assertNotIn('nonce', serialized.lower())
    self.wait_for_relaunched_uninstall(appid, relaunch_dirs,
                                       os.path.basename(staged_exe), ())
    self.assert_version_not_installed(version)

  def test_lifecycle_external_uninstall_remains_synchronous(self):
    appid = 'e2e00010-1011-1011-1011-000000001011'
    self._install_for_lifecycle(appid)
    receiver, hwnd, output = self._start_lifecycle_receiver()
    exit_code, _, stderr = self.run_installer(
        [FLAG_UNINSTALL, FLAG_HEADLESS, f'{FLAG_PARENT}={hwnd}'])
    self.assertEqual(EXIT_SUCCESS, exit_code, stderr.decode(errors='replace'))
    captured, correlated = self._finish_lifecycle_receiver(receiver,
                                                           hwnd,
                                                           output,
                                                           await_terminal=False)
    self.assertGreater(len(captured['progress']), 0)
    self.assertEqual([], captured['lifecycle'])
    self.assertEqual({}, correlated['handoffs'])
    self.assertEqual({}, correlated['results'])

  def test_lifecycle_no_parent_has_no_bound_context(self):
    appid = 'e2e00010-1012-1012-1012-000000001012'
    self._install_for_lifecycle(appid)
    staged_exe = self.stage_bootstrap_in_install_root(self.install_dir,
                                                      'LifecycleNoParent')
    barrier = os.path.join(self.temp_dir, 'no-parent-child-state-ready')
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    env = {
        'PATH': self.build_dir + os.pathsep + os.environ.get('PATH', ''),
        'CEF_INSTALLER_TEST_CHILD_STATE_BARRIER': barrier,
    }
    exit_code, _, _ = self.run_installer([FLAG_UNINSTALL, FLAG_HEADLESS],
                                         exe=staged_exe,
                                         env=env)
    self.assertEqual(EXIT_RELAUNCHED, exit_code)
    self._wait_for_path(barrier, 'Child did not reach trusted-state barrier')
    candidates = self.snapshot_uninstall_relaunch_dirs() - relaunch_dirs
    self.assertEqual(1, len(candidates))
    with open(os.path.join(next(iter(candidates)), 'cef_uninstall_state.json'),
              encoding='utf-8') as state_file:
      state = json.load(state_file)
    self.assertNotIn('operation_id', state)
    self.assertNotIn('parent_window', state)
    os.remove(barrier)
    for path in candidates:
      self._wait_for_relaunch_child_exit(path)

  def test_lifecycle_invalid_trusted_invocation_emits_nothing(self):
    appid = 'e2e00010-1013-1013-1013-000000001013'
    self._install_for_lifecycle(appid)
    receiver, hwnd, output = self._start_lifecycle_receiver()
    exit_code, _, _ = self.run_installer([
        FLAG_UNINSTALL, FLAG_HEADLESS, FLAG_UNINSTALL_RELAUNCHED,
        f'{FLAG_PARENT}={hwnd}'
    ])
    self.assertEqual(EXIT_CONFIG_ERROR, exit_code)
    captured, correlated = self._finish_lifecycle_receiver(receiver,
                                                           hwnd,
                                                           output,
                                                           await_terminal=False)
    self.assertEqual([], captured['lifecycle'])
    self.assertEqual({}, correlated['handoffs'])
    self.assertEqual({}, correlated['results'])

  def test_lifecycle_background_uninstall_is_headless_and_correlated(self):
    appid = 'e2e00010-1002-1002-1002-000000001002'
    self._install_for_lifecycle(appid)
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    receiver, hwnd, output = self._start_lifecycle_receiver(
        '--lifecycle-return=2')
    env = {'PATH': self.build_dir + os.pathsep + os.environ.get('PATH', '')}
    exit_code, _, stderr = self.run_installer([
        FLAG_UNINSTALL, FLAG_HEADLESS, FLAG_BACKGROUND, f'{FLAG_PARENT}={hwnd}'
    ],
                                              env=env)
    self.assertEqual(EXIT_RELAUNCHED, exit_code,
                     stderr.decode(errors='replace'))
    captured, correlated = self._finish_lifecycle_receiver(receiver,
                                                           hwnd,
                                                           output,
                                                           await_terminal=True)
    self.assertEqual(1, len(correlated['results']),
                     f'captured={captured!r}, correlated={correlated!r}')
    self.assertTrue(next(iter(correlated['results'].values()))['success'])
    self.wait_for_relaunched_uninstall(appid, relaunch_dirs,
                                       os.path.basename(self.bootstrap_exe), ())

  def test_selected_client_resource_relaunch_preserves_module_selection(self):
    appid = 'e2e00010-1101-1101-1101-000000001101'
    fallback_appid = 'e2e00010-1101-1101-1101-000000001199'
    version = self._install_for_lifecycle(appid)
    self.assertTrue(self._database_contains_app(self.install_dir, appid))
    self.assertFalse(
        self._database_contains_app(self.install_dir, fallback_appid))
    host_exe, module_path = self._stage_default_console_launcher(
        'SelectedRelaunchClient')
    self.embed_test_config(appid=fallback_appid,
                           vmin=version,
                           exe=host_exe,
                           enable_explicit_modes=True)
    client_config = {
        'appid': appid,
        'vmin': version,
        'install_path': self.install_dir,
    }
    if self.test_thumbprint:
      client_config['certificate_thumbprint'] = self.test_thumbprint
    self.embed_config_resource(module_path, client_config)

    barrier = os.path.join(self.temp_dir, 'selected-client-child-ready')
    marker = os.path.join(self.temp_dir, 'unexpected-client-marker.json')
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    receiver, hwnd, output = self._start_lifecycle_receiver()
    env = {
        'PATH': self.build_dir + os.pathsep + os.environ.get('PATH', ''),
        'CEF_INSTALLER_TEST_CHILD_STATE_BARRIER': barrier,
        'CEF_E2E_MARKER_PATH': marker,
    }
    exit_code = self._run_default_console_uninstall(
        host_exe,
        'SelectedRelaunchClient',
        hwnd,
        env,
        use_install_path_override=False)
    self.assertEqual(EXIT_RELAUNCHED, exit_code)
    self._wait_for_path(barrier, 'Selected-client child did not accept state')
    candidates = self.snapshot_uninstall_relaunch_dirs() - relaunch_dirs
    self.assertEqual(1, len(candidates))
    relaunch_dir = next(iter(candidates))
    relaunch_files = os.listdir(relaunch_dir)
    self.assertTrue(
        os.path.isfile(os.path.join(relaunch_dir,
                                    'SelectedRelaunchClient.dll')),
        f'Relaunch file set: {relaunch_files}')
    with open(os.path.join(relaunch_dir, 'cef_uninstall_state.json'),
              encoding='utf-8') as state_file:
      state = json.load(state_file)
    self.assertEqual(os.path.normcase(os.path.abspath(self.install_dir)),
                     os.path.normcase(os.path.abspath(state['install_path'])))
    self.assertNotIn('unchecked_cef_path', state)
    self.assertNotIn('bundled_cef_path', state)
    self.assertNotIn('config', state)
    os.remove(barrier)
    _, correlated = self._finish_lifecycle_receiver(receiver,
                                                    hwnd,
                                                    output,
                                                    await_terminal=True)
    result = next(iter(correlated['results'].values()))
    self.assertTrue(result['success'])
    self.assertFalse(os.path.exists(marker),
                     'Temporary child entered normal client execution')
    self.wait_for_relaunched_uninstall(appid, relaunch_dirs, 'bootstrapc.exe',
                                       ())
    self.assertFalse(self._database_contains_app(self.install_dir, appid))

  def test_bootstrap_fallback_relaunch_omits_no_resource_client(self):
    appid = 'e2e00010-1102-1102-1102-000000001102'
    version = self._install_for_lifecycle(appid)
    host_exe, _ = self._stage_default_console_launcher('NoResourceClient')
    self.embed_test_config(appid=appid, vmin=version, exe=host_exe)

    barrier = os.path.join(self.temp_dir, 'bootstrap-fallback-child-ready')
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    receiver, hwnd, output = self._start_lifecycle_receiver()
    env = {
        'PATH': self.build_dir + os.pathsep + os.environ.get('PATH', ''),
        'CEF_INSTALLER_TEST_CHILD_STATE_BARRIER': barrier,
    }
    exit_code = self._run_default_console_uninstall(
        host_exe, 'NoResourceClient', hwnd, env, use_install_path_override=True)
    self.assertEqual(EXIT_RELAUNCHED, exit_code)
    self._wait_for_path(barrier,
                        'Bootstrap-fallback child did not accept state')
    candidates = self.snapshot_uninstall_relaunch_dirs() - relaunch_dirs
    self.assertEqual(1, len(candidates))
    relaunch_dir = next(iter(candidates))
    self.assertFalse(
        os.path.exists(os.path.join(relaunch_dir, 'NoResourceClient.dll')))
    os.remove(barrier)
    _, correlated = self._finish_lifecycle_receiver(receiver,
                                                    hwnd,
                                                    output,
                                                    await_terminal=True)
    self.assertTrue(next(iter(correlated['results'].values()))['success'])
    self.wait_for_relaunched_uninstall(appid, relaunch_dirs, 'bootstrapc.exe',
                                       ())

  def test_lifecycle_child_config_failure_reports_terminal(self):
    appid = 'e2e00010-1003-1003-1003-000000001003'
    self._install_for_lifecycle(appid)
    staged_exe = self.stage_bootstrap_in_install_root(self.install_dir,
                                                      'LifecycleConfigFailure')
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    receiver, hwnd, output = self._start_lifecycle_receiver()
    env = {
        'PATH': self.build_dir + os.pathsep + os.environ.get('PATH', ''),
        'CEF_INSTALLER_TEST_CHILD_CONFIG_FAILURE': '1',
    }
    exit_code, _, stderr = self.run_installer(
        [FLAG_UNINSTALL, FLAG_HEADLESS, f'{FLAG_PARENT}={hwnd}'],
        exe=staged_exe,
        env=env)
    self.assertEqual(EXIT_RELAUNCHED, exit_code,
                     stderr.decode(errors='replace'))
    _, correlated = self._finish_lifecycle_receiver(receiver,
                                                    hwnd,
                                                    output,
                                                    await_terminal=True)
    result = next(iter(correlated['results'].values()))
    self.assertFalse(result['success'])
    self.assertEqual(EXIT_CONFIG_ERROR, result['exit_code'])
    self.assertEqual(EXIT_CONFIG_ERROR, result['error_code'])
    self.assertTrue(self._database_contains_app(self.install_dir, appid))
    for path in self.snapshot_uninstall_relaunch_dirs() - relaunch_dirs:
      self.addCleanup(shutil.rmtree, path, ignore_errors=True)

  def test_lifecycle_relaunch_failure_emits_no_events(self):
    appid = 'e2e00010-1004-1004-1004-000000001004'
    self._install_for_lifecycle(appid)
    staged_exe = self.stage_bootstrap_in_install_root(self.install_dir,
                                                      'LifecycleLaunchFailure')
    receiver, hwnd, output = self._start_lifecycle_receiver()
    env = {
        'PATH': self.build_dir + os.pathsep + os.environ.get('PATH', ''),
        'CEF_INSTALLER_TEST_RELAUNCH_FAILURE': '1',
    }
    exit_code, _, _ = self.run_installer(
        [FLAG_UNINSTALL, FLAG_HEADLESS, f'{FLAG_PARENT}={hwnd}'],
        exe=staged_exe,
        env=env)
    self.assertEqual(EXIT_INSTALL_ERROR, exit_code)
    captured, correlated = self._finish_lifecycle_receiver(receiver,
                                                           hwnd,
                                                           output,
                                                           await_terminal=False)
    self.assertEqual([], captured['lifecycle'])
    self.assertEqual({}, correlated['handoffs'])
    self.assertEqual({}, correlated['results'])

  def test_lifecycle_database_failure_reports_exact_terminal(self):
    appid = 'e2e00010-1007-1007-1007-000000001007'
    version, captured, result = self._run_lifecycle_fault(
        appid, {'CEF_INSTALLER_TEST_DATABASE_SAVE_FAILURE': '1'})
    self.assertFalse(result['success'])
    self.assertEqual('failed', result['outcome'])
    self.assertEqual(EXIT_DATABASE_ERROR, result['exit_code'])
    self.assertEqual(EXIT_DATABASE_ERROR, result['error_code'])
    self.assertNotIn('retry_required', result)
    self.assertNotIn(self.temp_dir, json.dumps(captured))
    self.assertTrue(self._database_contains_app(self.install_dir, appid))
    self.assert_version_installed(version)

  def test_lifecycle_index_failure_does_not_claim_rollback(self):
    appid = 'e2e00010-1008-1008-1008-000000001008'
    version, captured, result = self._run_lifecycle_fault(
        appid, {'CEF_INSTALLER_TEST_INDEX_FAULT': 'write'})
    self.assertFalse(result['success'])
    self.assertEqual(EXIT_INDEX_ERROR, result['exit_code'])
    self.assertEqual(EXIT_INDEX_ERROR, result['error_code'])
    self.assertNotIn('registrations_committed', result)
    self.assertNotIn('rollback', json.dumps(result).lower())
    self.assertNotIn(self.temp_dir, json.dumps(captured))
    self.assertFalse(self._database_contains_app(self.install_dir, appid))
    self.assert_version_installed(version)

  def test_lifecycle_deferred_cleanup_is_success_with_warning(self):
    appid = 'e2e00010-1009-1009-1009-000000001009'
    version, captured, result = self._run_lifecycle_fault(
        appid, {'CEF_INSTALLER_TEST_FILE_OPS_FAULT': 'trash_move'})
    self.assertTrue(result['success'])
    self.assertEqual('cleanup_deferred', result['outcome'])
    self.assertEqual(EXIT_SUCCESS, result['exit_code'])
    self.assertGreater(len(result.get('warnings', [])), 0)
    self.assertNotIn(self.temp_dir, json.dumps(captured))
    self.assertFalse(self._database_contains_app(self.install_dir, appid))
    self.assert_version_installed(version)

  def test_lifecycle_progress_cancellation_reports_exact_terminal(self):
    appid = 'e2e00010-1010-1010-1010-000000001010'
    version, captured, result = self._run_lifecycle_fault(
        appid, receiver_options=('--cancel',))
    self.assertGreater(len(captured['progress']), 0)
    self.assertFalse(result['success'])
    self.assertEqual('failed', result['outcome'])
    self.assertEqual(EXIT_CANCELLED, result['exit_code'])
    self.assertEqual(EXIT_CANCELLED, result['error_code'])
    self.assertTrue(self._database_contains_app(self.install_dir, appid))
    self.assert_version_installed(version)

  def test_lifecycle_forced_child_termination_is_indeterminate(self):
    appid = 'e2e00010-1006-1006-1006-000000001006'
    self._install_for_lifecycle(appid)
    staged_exe = self.stage_bootstrap_in_install_root(
        self.install_dir, 'LifecycleForcedTermination')
    barrier = os.path.join(self.temp_dir, 'child-state-ready')
    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    receiver, hwnd, output = self._start_lifecycle_receiver(
        '--close-after-handoff')
    env = {
        'PATH': self.build_dir + os.pathsep + os.environ.get('PATH', ''),
        'CEF_INSTALLER_TEST_CHILD_STATE_BARRIER': barrier,
    }
    exit_code, _, _ = self.run_installer(
        [FLAG_UNINSTALL, FLAG_HEADLESS, f'{FLAG_PARENT}={hwnd}'],
        exe=staged_exe,
        env=env)
    self.assertEqual(EXIT_RELAUNCHED, exit_code)
    captured, correlated = self._finish_lifecycle_receiver(receiver,
                                                           hwnd,
                                                           output,
                                                           await_terminal=True)
    self.assertEqual(1, len(captured['lifecycle']))
    self.assertEqual({}, correlated['results'])

    self._wait_for_path(barrier, 'Child did not reach state barrier')
    child_pid = int(next(iter(correlated['handoffs'].values()))['child_pid'])
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel32.OpenProcess.restype = ctypes.c_void_p
    kernel32.OpenProcess.argtypes = [
        ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32
    ]
    kernel32.TerminateProcess.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    kernel32.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
    process_handle = kernel32.OpenProcess(0x00100001, False, child_pid)
    self.assertTrue(process_handle, 'Failed to open relaunched child')
    try:
      self.assertTrue(kernel32.TerminateProcess(process_handle, 108))
      self.assertEqual(0, kernel32.WaitForSingleObject(process_handle, 10000))
    finally:
      kernel32.CloseHandle(process_handle)
    os.remove(barrier)
    self.assertTrue(self._database_contains_app(self.install_dir, appid))
    for path in self.snapshot_uninstall_relaunch_dirs() - relaunch_dirs:
      self.addCleanup(shutil.rmtree, path, ignore_errors=True)


if __name__ == '__main__':
  unittest.main()
