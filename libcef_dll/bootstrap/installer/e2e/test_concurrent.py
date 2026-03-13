#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""E2E tests for concurrent installer processes."""

import os
import subprocess
import threading
import unittest

from e2e_test_base import (E2ETestBase, EXIT_SUCCESS, FLAG_DOWNLOAD_PATH,
                           FLAG_HEADLESS, FLAG_INSTALL_PATH, FLAG_UPDATE)


class TestConcurrent(E2ETestBase):

  def test_two_processes_install_simultaneously(self):
    """Two processes install simultaneously, both succeed."""
    version, abi_hash = self.build_cdn()
    appid_a = 'e2e00024-aaaa-aaaa-aaaa-aaaaaaaaaaaa'
    appid_b = 'e2e00024-bbbb-bbbb-bbbb-bbbbbbbbbbbb'

    # Create a second executable with an independent embedded resource.
    exe_b = os.path.join(self.build_dir, 'CefTestAppB.exe')
    self._restore_bootstrap_executable(exe_b)
    self.addCleanup(lambda: os.remove(exe_b) if os.path.isfile(exe_b) else None)

    # Embed independent resources for both apps before either process starts.
    self.embed_test_config(appid=appid_a, vmin=version, abi_hash=abi_hash)
    self.embed_test_config(appid=appid_b,
                           vmin=version,
                           abi_hash=abi_hash,
                           exe=exe_b)

    results = [None, None]

    def run_process(exe, idx):
      cmd = [
          exe, FLAG_UPDATE, FLAG_HEADLESS,
          f'{FLAG_DOWNLOAD_PATH}={self.cdn_dir}',
          f'{FLAG_INSTALL_PATH}={self.install_dir}'
      ]
      proc = subprocess.run(cmd,
                            capture_output=True,
                            timeout=60,
                            cwd=self.build_dir)
      results[idx] = proc.returncode

    t_a = threading.Thread(target=run_process, args=(self.bootstrap_exe, 0))
    t_b = threading.Thread(target=run_process, args=(exe_b, 1))
    t_a.start()
    t_b.start()
    t_a.join(timeout=60)
    t_b.join(timeout=60)

    # Both processes should succeed.
    self.assertEqual(EXIT_SUCCESS, results[0],
                     f'Process A failed: exit {results[0]}')
    self.assertEqual(EXIT_SUCCESS, results[1],
                     f'Process B failed: exit {results[1]}')

    # One version directory, two apps in database.
    self.assert_version_installed(version)
    db = self.read_database()
    uuids = [a.get('uuid') for a in db.get('apps', [])]
    self.assertIn(appid_a, uuids)
    self.assertIn(appid_b, uuids)
