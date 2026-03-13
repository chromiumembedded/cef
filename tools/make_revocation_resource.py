#!/usr/bin/env python3
# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
"""
Generate revocation list resource files for the CEF installer bootstrap.

Reads revoked.json (canonical revocation list) and cef_api_versions.json,
prunes entries for versions older than vmin (which can never be loaded),
and outputs a pruned JSON file plus an RC file for embedding as a Windows
resource.
"""

from __future__ import absolute_import
from __future__ import print_function

import argparse
import json
import os
import sys

from installer_util import (
    compute_vmin,
    load_api_versions,
    write_json_output,
    write_rc_file,
)


def parse_version(version_str):
  """Parse a version string like '148.2.0' into a tuple of ints.

  Returns None if the version string is invalid.
  """
  parts = version_str.split('.')
  if len(parts) < 2:
    return None
  try:
    return tuple(int(p) for p in parts)
  except ValueError:
    return None


def version_at_least(version_tuple, vmin_major, vmin_minor):
  """Check if version_tuple >= (vmin_major, vmin_minor)."""
  if version_tuple is None:
    return False
  return (version_tuple[0], version_tuple[1]) >= (vmin_major, vmin_minor)


def prune_revocation_list(revoked_versions, vmin_major, vmin_minor):
  """Remove entries for versions older than vmin (can never be loaded).

  Entries with unparseable version strings are kept (defensive).

  Args:
    revoked_versions: List of revocation entry dicts.
    vmin_major: Minimum major version.
    vmin_minor: Minimum minor version.

  Returns:
    Pruned list of revocation entry dicts.
  """
  pruned = []
  for entry in revoked_versions:
    version_str = entry.get('version', '')
    version_tuple = parse_version(version_str)
    if version_tuple is None:
      pruned.append(entry)
      continue
    if version_at_least(version_tuple, vmin_major, vmin_minor):
      pruned.append(entry)
  return pruned


def main():
  parser = argparse.ArgumentParser(
      description='Generate revocation list resource files.')
  parser.add_argument(
      '--revoked-json',
      required=True,
      help='Path to input revoked.json (canonical revocation list)')
  parser.add_argument(
      '--api-versions',
      required=True,
      help='Path to cef_api_versions.json (for vmin calculation)')
  parser.add_argument(
      '--api-version',
      help='Override API version for vmin calculation (e.g., "14500")')
  parser.add_argument('--output-json',
                      required=True,
                      help='Path to output pruned JSON file')
  parser.add_argument('--output-rc',
                      required=True,
                      help='Path to output RC file')
  parser.add_argument(
      '--resource-name',
      required=True,
      help='Resource name for RC file (e.g., CEF_REVOCATION_LIST)')
  args = parser.parse_args()

  # Read canonical revocation list.
  try:
    with open(args.revoked_json, 'r', encoding='utf-8') as f:
      revoked_data = json.load(f)
  except Exception as e:
    print('Error reading %s: %s' % (args.revoked_json, e), file=sys.stderr)
    return 1

  # Read API versions to determine vmin.
  api_versions = load_api_versions(args.api_versions)
  if api_versions is None:
    return 1

  # Compute vmin.
  vmin_tuple = compute_vmin(api_versions, args.api_version)
  if vmin_tuple is None:
    return 1

  # Prune entries for versions older than vmin.
  revoked_versions = revoked_data.get('revoked_versions', [])
  pruned = prune_revocation_list(revoked_versions, *vmin_tuple)

  # Build output JSON with the pruned list.
  output_data = {'revoked_versions': pruned}
  output_json = json.dumps(output_data, indent=2, sort_keys=True)

  # Write outputs.
  write_json_output(args.output_json, output_json)

  json_basename = os.path.basename(args.output_json)
  write_rc_file(args.output_rc, args.resource_name, json_basename)

  return 0


if __name__ == '__main__':
  sys.exit(main())
