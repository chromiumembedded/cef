#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""E2E tests for WM_COPYDATA progress notification."""

import ctypes
import json
import os
import subprocess
import time
import unittest

from e2e_test_base import (E2ETestBase, EXIT_SUCCESS, FLAG_HEADLESS,
                           FLAG_PARENT, FLAG_UPDATE)


class TestProgress(E2ETestBase):

  def test_parent_receives_progress(self):
    """Parent window receives WM_COPYDATA progress updates."""
    helper_exe = os.path.join(self.build_dir, 'cef_e2e_progress_helper.exe')
    if not os.path.isfile(helper_exe):
      self.skipTest('cef_e2e_progress_helper.exe not found')

    version, abi_hash = self.build_cdn()
    appid = 'e2e00025-2525-2525-2525-252525252525'
    self.embed_test_config(appid=appid, vmin=version, abi_hash=abi_hash)

    messages_path = os.path.join(self.temp_dir, 'progress_messages.json')

    # Start the progress helper — it prints HWND to stdout then pumps.
    helper_proc = subprocess.Popen([helper_exe, messages_path],
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE,
                                   cwd=self.build_dir)

    try:
      # Read HWND from first line of stdout.
      hwnd_line = helper_proc.stdout.readline().decode().strip()
      self.assertTrue(hwnd_line, 'No HWND from progress helper')
      hwnd = int(hwnd_line)

      # Run installer with parent window handle.
      exit_code, _, stderr = self.run_installer(
          [FLAG_UPDATE, FLAG_HEADLESS, f'{FLAG_PARENT}={hwnd}'])
      self.assertEqual(EXIT_SUCCESS, exit_code,
                       f'Install failed: {stderr.decode(errors="replace")}')
    finally:
      # Send WM_CLOSE for clean shutdown (helper writes output on exit).
      WM_CLOSE = 0x0010
      ctypes.windll.user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
      helper_proc.wait(timeout=10)

    # Read collected messages.
    self.assertTrue(os.path.isfile(messages_path),
                    'Progress messages file not written')
    with open(messages_path) as f:
      messages = json.load(f)

    self.assertIsInstance(messages, list)
    self.assertGreater(len(messages), 0, 'No progress messages received')

    # Verify message structure.
    for msg in messages:
      self.assertIn('step_name', msg, f'Missing step_name: {msg}')
      self.assertIn('step', msg, f'Missing step: {msg}')
      self.assertIn('total_steps', msg, f'Missing total_steps: {msg}')
      self.assertIn('bytes_done', msg, f'Missing bytes_done: {msg}')
      self.assertIn('bytes_total', msg, f'Missing bytes_total: {msg}')
      self.assertIn('overall_percent', msg, f'Missing overall_percent: {msg}')
      self.assertGreaterEqual(msg['overall_percent'], 0)
      self.assertLessEqual(msg['overall_percent'], 100)
      self.assertIn('message', msg, f'Missing message field: {msg}')
