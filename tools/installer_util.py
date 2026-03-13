#!/usr/bin/env python3
# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
"""
Shared utilities for CEF installer build-time scripts.

Used by make_installer_config.py and make_revocation_resource.py.
"""

from __future__ import absolute_import
from __future__ import print_function

import json
import os
import sys

from file_util import write_file_if_changed


def load_api_versions(path):
  """Load and return the parsed cef_api_versions.json.

  Returns the parsed dict, or None on error (prints to stderr).
  """
  try:
    with open(path, 'r', encoding='utf-8') as f:
      return json.load(f)
  except Exception as e:
    print('Error reading %s: %s' % (path, e), file=sys.stderr)
    return None


def get_api_version_last(api_versions):
  """Get the 'last' API version string from parsed api_versions dict.

  Returns the string (e.g., '14601'), or None on error (prints to stderr).
  """
  last = api_versions.get('last')
  if not last:
    print('Error: "last" field not found in api_versions', file=sys.stderr)
    return None
  return last


def compute_vmin(api_versions, api_version_override=None):
  """Compute vmin (major, minor) from api_versions and optional override.

  API version format: XXXYY where XXX is major and YY is minor.
  e.g., 14601 -> (146, 1), 14610 -> (146, 10)

  Args:
    api_versions: Parsed cef_api_versions.json dict.
    api_version_override: Optional string override (e.g., "14500").

  Returns:
    Tuple (major, minor) on success, or None on error (prints to stderr).
  """
  last = get_api_version_last(api_versions)
  if last is None:
    return None

  api_version_str = api_version_override if api_version_override else last
  try:
    api_version_int = int(api_version_str)
    major = api_version_int // 100
    minor = api_version_int % 100
    return (major, minor)
  except ValueError:
    print('Error: Invalid API version "%s"' % api_version_str, file=sys.stderr)
    return None


def format_vmin(major, minor):
  """Format vmin tuple as a version string (e.g., '146.1')."""
  return '%d.%d' % (major, minor)


def ensure_parent_dir(path):
  """Create parent directory of path if it doesn't exist."""
  parent = os.path.dirname(path)
  if parent and not os.path.exists(parent):
    os.makedirs(parent)


def write_rc_file(output_rc_path, resource_name, json_basename):
  """Write an RC file that references a JSON file as RCDATA.

  Args:
    output_rc_path: Path to write the RC file.
    resource_name: Resource name (e.g., 'CEF_INSTALLER_CONFIG').
    json_basename: Basename of the JSON file (e.g., 'installer_config.json').
  """
  rc_content = '%s RCDATA "%s"\n' % (resource_name, json_basename)
  ensure_parent_dir(output_rc_path)
  write_file_if_changed(output_rc_path, rc_content)


def write_json_output(output_json_path, content):
  """Write JSON content to a file (only if changed).

  Args:
    output_json_path: Path to write the JSON file.
    content: JSON string to write.
  """
  ensure_parent_dir(output_json_path)
  write_file_if_changed(output_json_path, content)
