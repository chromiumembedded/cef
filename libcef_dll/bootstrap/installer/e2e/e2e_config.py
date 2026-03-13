#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared helpers for reading build-time installer configuration."""

import json
import os


def read_installer_config(build_dir):
  """Read gen/cef/installer_config.json from the build directory.

    Returns the parsed dict, or None if the file is missing.
    """
  config_path = os.path.join(build_dir, 'gen', 'cef', 'installer_config.json')
  if not os.path.isfile(config_path):
    return None
  with open(config_path) as f:
    return json.load(f)


def get_test_versions(build_dir):
  """Derive test version strings from the build's installer config.

    Returns (test_version, test_version_update, test_version_higher, abi_hash)
    or None if the config cannot be read.

    The versions are constructed from the milestone in vmin so they stay
    in sync when the API version changes:
      test_version:        <milestone>.1
      test_version_update: <milestone>.1.1
      test_version_higher: <milestone>.2
    """
  config = read_installer_config(build_dir)
  if config is None:
    return None
  vmin = config.get('vmin', '')
  if not vmin:
    return None
  milestone = vmin.split('.')[0]
  abi_hash = config.get('abi_hash', '')
  return (f'{milestone}.1', f'{milestone}.1.1', f'{milestone}.2', abi_hash)
