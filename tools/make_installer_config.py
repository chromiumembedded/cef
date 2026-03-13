#!/usr/bin/env python3
# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
"""
Generate installer configuration files from a template.

Reads cef_api_versions.json, extracts:
  - vmin: API version XXXYY -> "XXX.YY" (e.g., 14600 -> "146.0", 14601 -> "146.1")
    Uses --api-version if provided, otherwise uses "last" from JSON
  - abi_hash: hashes[last]["sandbox_compat"] (always from "last", not affected
    by --api-version)

Substitutes ${VMIN} and ${ABI_HASH} in template, writes JSON and RC files.
"""

from __future__ import absolute_import
from __future__ import print_function

import argparse
import json
import os
import re
import sys

from installer_util import (
    compute_vmin,
    format_vmin,
    load_api_versions,
    get_api_version_last,
    write_json_output,
    write_rc_file,
)


def validate_uuid(value):
  """Validate UUID format (e.g., A3B9C4D5-E6F7-4A8B-9C0D-E1F2A3B4C5D6)."""
  pattern = r'^[A-F0-9]{8}-[A-F0-9]{4}-[A-F0-9]{4}-[A-F0-9]{4}-[A-F0-9]{12}$'
  return bool(re.match(pattern, value, re.IGNORECASE))


def validate_version(value):
  """Validate version format (e.g., "146.0")."""
  pattern = r'^\d+\.\d+$'
  return bool(re.match(pattern, value))


def validate_abi_hash(value):
  """Validate abi_hash is exactly 16 hex characters."""
  pattern = r'^[a-f0-9]{16}$'
  return bool(re.match(pattern, value, re.IGNORECASE))


def main():
  parser = argparse.ArgumentParser(
      description='Generate installer configuration files from a template.')
  parser.add_argument('--template',
                      required=True,
                      help='Path to input template file (.json.in)')
  parser.add_argument('--api-versions',
                      required=True,
                      help='Path to cef_api_versions.json')
  parser.add_argument(
      '--api-version',
      help='Override API version for vmin calculation (e.g., "14500")')
  parser.add_argument('--output-json',
                      required=True,
                      help='Path to output JSON file')
  parser.add_argument('--output-rc',
                      required=True,
                      help='Path to output RC file')
  parser.add_argument(
      '--resource-name',
      required=True,
      help='Resource name for RC file (e.g., CEF_INSTALLER_CONFIG)')
  args = parser.parse_args()

  # Read API versions
  api_versions = load_api_versions(args.api_versions)
  if api_versions is None:
    return 1

  last = get_api_version_last(api_versions)
  if last is None:
    return 1

  # Compute vmin
  vmin_tuple = compute_vmin(api_versions, args.api_version)
  if vmin_tuple is None:
    return 1
  vmin = format_vmin(*vmin_tuple)

  # abi_hash: always from "last" (not affected by --api-version)
  hashes = api_versions.get('hashes', {})
  last_hashes = hashes.get(last, {})
  abi_hash = last_hashes.get('sandbox_compat')
  if not abi_hash:
    print('Error: "sandbox_compat" not found in hashes[%s]' % last,
          file=sys.stderr)
    return 1

  # Read and substitute template
  try:
    with open(args.template, 'r', encoding='utf-8') as f:
      template = f.read()
  except Exception as e:
    print('Error reading %s: %s' % (args.template, e), file=sys.stderr)
    return 1

  config = template.replace('${VMIN}', vmin)
  config = config.replace('${ABI_HASH}', abi_hash)

  # Remove JSON comments (lines starting with //) and empty lines
  lines = [
      l for l in config.split('\n')
      if not l.strip().startswith('//') and l.strip()
  ]
  config = '\n'.join(lines)

  # Validate output JSON
  try:
    parsed = json.loads(config)
  except json.JSONDecodeError as e:
    print('Error: Generated JSON is invalid: %s' % e, file=sys.stderr)
    return 1

  # Validate fields
  appid = parsed.get('appid', '')
  if not validate_uuid(appid):
    print('Error: Invalid appid format "%s" (expected UUID)' % appid,
          file=sys.stderr)
    return 1

  generated_vmin = parsed.get('vmin', '')
  if not validate_version(generated_vmin):
    print('Error: Invalid vmin format "%s" (expected X.Y)' % generated_vmin,
          file=sys.stderr)
    return 1

  generated_abi_hash = parsed.get('abi_hash', '')
  if not validate_abi_hash(generated_abi_hash):
    print('Error: Invalid abi_hash "%s" (expected 16 hex chars)' %
          generated_abi_hash,
          file=sys.stderr)
    return 1

  # Write outputs
  write_json_output(args.output_json, config)

  json_basename = os.path.basename(args.output_json)
  write_rc_file(args.output_rc, args.resource_name, json_basename)

  return 0


if __name__ == '__main__':
  sys.exit(main())
