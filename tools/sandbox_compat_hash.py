# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
import hashlib
import os
import sys


def calculate_sandbox_compat_hash(file_paths, verbose=False):
  """
  Calculate a compatibility hash from the contents of the given files.
  Returns a truncated SHA-256 hash (first 16 hex characters).
  All files must exist or an error is raised.
  """
  hasher = hashlib.sha256()

  for file_path in sorted(file_paths):  # Sort for deterministic order
    if not os.path.exists(file_path):
      raise FileNotFoundError(f"Required file not found: {file_path}")

    with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
      content = f.read()
      # Normalize line endings for cross-platform consistency
      content = content.replace('\r\n', '\n')
      hasher.update(content.encode('utf-8'))
      if verbose:
        print(f"Hashed: {file_path} ({len(content)} bytes)")

  return hasher.hexdigest()[:16]


def main(argv):
  if len(argv) < 2:
    print(f"Usage: {argv[0]} <file1> [file2] ...")
    sys.exit(1)

  hash_value = calculate_sandbox_compat_hash(argv[1:], verbose=True)
  print(f"Sandbox compatibility hash: {hash_value}")


if __name__ == '__main__':
  main(sys.argv)
