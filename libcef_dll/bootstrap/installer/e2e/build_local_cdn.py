#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Build a local CDN directory for E2E tests.

Delegates to the C++ cef_e2e_build_test_cdn helper which wraps
test::BuildTestDistribution() for signing and archive creation.

When --version/--abi-hash are omitted, reads vmin and abi_hash from
gen/cef/installer_config.json in the build directory (derived from
cef_api_versions.json at build time) and constructs a default version
as <milestone>.1.

Usage:
    # Auto-detect version from build config:
    python3 build_local_cdn.py --build-dir=out/Debug_GN_x64 \
        --output-dir=/tmp/cdn

    # Explicit version:
    python3 build_local_cdn.py --build-dir=out/Debug_GN_x64 \
        --output-dir=/tmp/cdn --version=151.1 --abi-hash=a1b2c3d4e5f6

    # Multi-version:
    python3 build_local_cdn.py --build-dir=out/Debug_GN_x64 \
        --output-dir=/tmp/cdn \
        --version=151.1 --abi-hash=a1b2c3d4e5f6 \
        --version=151.1.1 --abi-hash=a1b2c3d4e5f6
"""

import argparse
import json
import os
import subprocess
import sys

from e2e_config import get_test_versions


def build_cdn(build_dir, output_dir, versions):
  """Build a local CDN directory with the given versions.

    Args:
        build_dir: Path to the build directory containing cef_e2e_build_test_cdn.
        output_dir: Directory to create CDN files in.
        versions: List of (version, abi_hash) tuples.

    Returns:
        True on success.
    """
  helper = os.path.join(build_dir, 'cef_e2e_build_test_cdn.exe')
  if not os.path.isfile(helper):
    print(f'Error: cef_e2e_build_test_cdn.exe not found at {helper}',
          file=sys.stderr)
    print('Build it with: autoninja -C <build_dir> cef_e2e_build_test_cdn',
          file=sys.stderr)
    return False

  os.makedirs(output_dir, exist_ok=True)

  for version, abi_hash in versions:
    cmd = [
        helper, f'--output-dir={output_dir}', f'--version={version}',
        f'--abi-hash={abi_hash}'
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
      print(f'Error building CDN for {version}: {result.stderr}',
            file=sys.stderr)
      return False

  # Write empty revoked.json if not present.
  revoked_path = os.path.join(output_dir, 'revoked.json')
  if not os.path.isfile(revoked_path):
    with open(revoked_path, 'w') as f:
      json.dump({'revoked_versions': []}, f)

  return True


def main():
  parser = argparse.ArgumentParser(
      description='Build a local CDN directory for E2E tests')
  parser.add_argument('--build-dir', required=True)
  parser.add_argument('--output-dir', required=True)
  parser.add_argument('--version', action='append')
  parser.add_argument('--abi-hash', action='append')
  args = parser.parse_args()

  if args.version or args.abi_hash:
    if not args.version or not args.abi_hash:
      print('Error: --version and --abi-hash must both be provided',
            file=sys.stderr)
      return 1
    if len(args.version) != len(args.abi_hash):
      print('Error: --version and --abi-hash must be paired', file=sys.stderr)
      return 1
    versions = list(zip(args.version, args.abi_hash))
  else:
    result = get_test_versions(args.build_dir)
    if result is None:
      print(
          'Error: could not read installer_config.json from '
          f'{args.build_dir}/gen/cef/',
          file=sys.stderr)
      print('Build with: autoninja -C <build_dir> cef_installer_config',
            file=sys.stderr)
      return 1
    version, _, _, abi_hash = result
    versions = [(version, abi_hash)]

  if build_cdn(args.build_dir, args.output_dir, versions):
    return 0
  return 1


if __name__ == '__main__':
  sys.exit(main())
