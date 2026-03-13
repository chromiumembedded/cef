#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""E2E tests verifying installer exit codes.

Parametrized test covering reachable exit codes.
"""

import os
import unittest

from e2e_test_base import (E2ETestBase, EXIT_CONFIG_ERROR, EXIT_NETWORK_ERROR,
                           EXIT_NO_MATCHING_VERSION, EXIT_RELAUNCHED,
                           EXIT_SUCCESS, FLAG_BACKGROUND, FLAG_DOWNLOAD_PATH,
                           FLAG_HEADLESS, FLAG_UNINSTALL, FLAG_UPDATE)


class TestExitCodes(E2ETestBase):

  def test_exit_code_success(self):
    """Exit code 0: Successful install."""
    version, abi_hash = self.build_cdn()
    self.embed_test_config(
        appid='e2e00021-0000-0000-0000-000000000000',
        vmin=version,
        abi_hash=abi_hash,
    )
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)

  def test_exit_code_config_error(self):
    """Exit code 100: Invalid config."""
    self.embed_raw_config_resource(self.bootstrap_exe, 'NOT VALID JSON {{{')
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS],
                                         use_local_cdn=False)
    self.assertEqual(EXIT_CONFIG_ERROR, exit_code)

  def test_exit_code_network_error(self):
    """Exit code >= 100: Empty CDN causes download/resolve failure."""
    self.embed_test_config(
        appid='e2e00021-0101-0101-0101-010101010101',
        vmin=self.test_version,
        abi_hash='a1b2c3d4e5f6',
    )
    empty_cdn = os.path.join(self.temp_dir, 'empty_cdn')
    os.makedirs(empty_cdn)
    exit_code, _, _ = self.run_installer(
        [FLAG_UPDATE, FLAG_HEADLESS, f'{FLAG_DOWNLOAD_PATH}={empty_cdn}'],
        use_local_cdn=False)
    # Local download mode with missing manifest reports either
    # kExitCodeNetworkError (101) or kExitCodeNoMatchingVersion (103)
    # depending on whether the manifest is missing vs empty.
    self.assertIn(exit_code, (EXIT_NETWORK_ERROR, EXIT_NO_MATCHING_VERSION))

  def test_exit_code_no_matching_version(self):
    """Exit code 103: No CDN version satisfies the config range."""
    abi_hash = 'a1b2c3d4e5f6'
    self.build_cdn(version=self.test_version, abi_hash=abi_hash)
    # vmin higher than what the CDN offers.
    self.embed_test_config(
        appid='e2e00021-0103-0103-0103-010301030103',
        vmin=self.test_version_higher,
        abi_hash=abi_hash,
    )
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_NO_MATCHING_VERSION, exit_code)

  def test_exit_code_relaunched(self):
    """Exit code 109: explicit background uninstall starts one child."""
    version, abi_hash = self.build_cdn()
    appid = 'e2e00021-0109-0109-0109-010901090109'
    self.embed_test_config(
        appid=appid,
        vmin=version,
        abi_hash=abi_hash,
    )
    # Install first.
    exit_code, _, _ = self.run_installer([FLAG_UPDATE, FLAG_HEADLESS])
    self.assertEqual(EXIT_SUCCESS, exit_code)

    relaunch_dirs = self.snapshot_uninstall_relaunch_dirs()
    env = {'PATH': self.build_dir + os.pathsep + os.environ.get('PATH', '')}
    exit_code, _, stderr = self.run_installer(
        [FLAG_UNINSTALL, FLAG_BACKGROUND, FLAG_HEADLESS], env=env)
    self.assertEqual(EXIT_RELAUNCHED, exit_code,
                     stderr.decode(errors='replace'))
    self.wait_for_relaunched_uninstall(appid,
                                       relaunch_dirs,
                                       os.path.basename(self.bootstrap_exe),
                                       expected_index_versions=())
