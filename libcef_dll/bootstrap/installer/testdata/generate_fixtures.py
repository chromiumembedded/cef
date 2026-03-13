#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generate test fixtures for XZ multi-block parallel extraction tests.

Creates .tar.xz archives with varying block counts for testing:
  - single_block.tar.xz:     1 block  (standard single-block xz)
  - multi_block_4.tar.xz:    4+ blocks (small block size forces multiple blocks)
  - multi_block_large.tar.xz: 4+ blocks (larger content, small block size)
  - small_single.tar.xz:     1 block  (tiny archive, single file)

Usage:
  python generate_fixtures.py [--xz-exe PATH]

  If --xz-exe is not provided, uses XZ_EXE env var.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile


def find_xz_exe(args_xz_exe=None):
  """Find xz.exe from args or XZ_EXE env var."""
  if args_xz_exe and os.path.isfile(args_xz_exe):
    return args_xz_exe
  env_xz = os.environ.get('XZ_EXE')
  if env_xz and os.path.isfile(env_xz):
    return env_xz
  raise FileNotFoundError(
      'xz.exe not found. Set XZ_EXE env var or pass --xz-exe.')


def create_tar_xz(source_dir, output_path, xz_exe, block_size_bytes=None):
  """Create a .tar.xz archive, optionally with multi-block compression.

    Args:
        source_dir: Directory to archive.
        output_path: Output .tar.xz path.
        xz_exe: Path to xz.exe.
        block_size_bytes: If set, pass --block-size to xz for multi-block.
                          If None, create standard single-block archive.
    """
  with tempfile.NamedTemporaryFile(suffix='.tar', delete=False) as tmp:
    tar_path = tmp.name

  try:
    with tarfile.open(tar_path, 'w') as tar:
      tar.add(source_dir, arcname=os.path.basename(source_dir))

    cmd = [xz_exe, '-6', '--force']
    if block_size_bytes:
      cmd.append(f'--block-size={block_size_bytes}')
    cmd.append(tar_path)

    subprocess.run(cmd, check=True)
    shutil.move(tar_path + '.xz', output_path)
  finally:
    for p in [tar_path, tar_path + '.xz']:
      if os.path.exists(p):
        os.remove(p)


def create_test_tree(base_dir, name, num_files, file_size):
  """Create a directory tree with generated files.

    Returns the path to the created directory.
    """
  tree_dir = os.path.join(base_dir, name)
  os.makedirs(tree_dir, exist_ok=True)

  # Create subdirectories
  sub1 = os.path.join(tree_dir, 'subdir1')
  sub2 = os.path.join(tree_dir, 'subdir2')
  os.makedirs(sub1, exist_ok=True)
  os.makedirs(sub2, exist_ok=True)

  # Distribute files across directories
  dirs = [tree_dir, sub1, sub2]
  for i in range(num_files):
    target_dir = dirs[i % len(dirs)]
    file_path = os.path.join(target_dir, f'file_{i:04d}.dat')
    # Deterministic content that compresses but isn't trivial
    content = bytes([(i + j) & 0xFF for j in range(file_size)])
    with open(file_path, 'wb') as f:
      f.write(content)

  return tree_dir


def verify_fixture(xz_exe, path, expected_min_blocks):
  """Verify a fixture has the expected number of blocks."""
  result = subprocess.run([xz_exe, '--robot', '--list', path],
                          capture_output=True,
                          text=True,
                          check=True)

  # --robot --list output: last line starting with "totals" has block count
  # Format: totals\tstreams\tblocks\t...
  lines = result.stdout.strip().split('\n')
  totals_line = [l for l in lines if l.startswith('totals')]
  if not totals_line:
    # Count 'block' lines instead
    block_lines = [l for l in lines if l.startswith('block')]
    block_count = len(block_lines)
  else:
    parts = totals_line[0].split('\t')
    block_count = int(parts[2])  # Third field is block count

  size_kb = os.path.getsize(path) / 1024
  status = 'OK' if block_count >= expected_min_blocks else 'FAIL'
  print(f'  [{status}] {os.path.basename(path)}: '
        f'{block_count} block(s), {size_kb:.1f} KB')

  if block_count < expected_min_blocks:
    print(f'    Expected >= {expected_min_blocks} blocks, got {block_count}')
    return False
  return True


def main():
  parser = argparse.ArgumentParser(description='Generate XZ test fixtures')
  parser.add_argument('--xz-exe', help='Path to xz.exe')
  args = parser.parse_args()

  xz_exe = find_xz_exe(args.xz_exe)
  print(f'Using xz.exe: {xz_exe}')

  script_dir = os.path.dirname(os.path.abspath(__file__))

  with tempfile.TemporaryDirectory() as tmp_dir:
    print('\nGenerating test data...')

    # 1. small_single.tar.xz -- tiny archive, 1 file, 1 block
    small_dir = os.path.join(tmp_dir, 'small')
    os.makedirs(small_dir)
    with open(os.path.join(small_dir, 'hello.txt'), 'w') as f:
      f.write('Hello CEF\n')
    create_tar_xz(small_dir,
                  os.path.join(script_dir, 'small_single.tar.xz'),
                  xz_exe,
                  block_size_bytes=None)

    # 2. single_block.tar.xz -- medium archive, 1 block (large block size)
    med_tree = create_test_tree(tmp_dir, 'medium', num_files=20, file_size=4096)
    create_tar_xz(med_tree,
                  os.path.join(script_dir, 'single_block.tar.xz'),
                  xz_exe,
                  block_size_bytes=None)  # Default = single block

    # 3. multi_block_4.tar.xz -- medium archive, 4+ blocks
    #    ~80KB uncompressed with 4KB block size -> several blocks
    multi_tree = create_test_tree(tmp_dir,
                                  'multi4',
                                  num_files=20,
                                  file_size=4096)
    create_tar_xz(multi_tree,
                  os.path.join(script_dir, 'multi_block_4.tar.xz'),
                  xz_exe,
                  block_size_bytes=4096)  # 4 KB blocks -> many blocks

    # 4. multi_block_large.tar.xz -- larger archive, 4+ blocks
    #    ~200KB uncompressed with 8KB block size
    large_tree = create_test_tree(tmp_dir,
                                  'large',
                                  num_files=50,
                                  file_size=4096)
    create_tar_xz(large_tree,
                  os.path.join(script_dir, 'multi_block_large.tar.xz'),
                  xz_exe,
                  block_size_bytes=8192)  # 8 KB blocks

  print('\nVerifying fixtures...')
  all_ok = True
  all_ok &= verify_fixture(xz_exe,
                           os.path.join(script_dir, 'small_single.tar.xz'), 1)
  all_ok &= verify_fixture(xz_exe,
                           os.path.join(script_dir, 'single_block.tar.xz'), 1)
  all_ok &= verify_fixture(xz_exe,
                           os.path.join(script_dir, 'multi_block_4.tar.xz'), 4)
  all_ok &= verify_fixture(xz_exe,
                           os.path.join(script_dir, 'multi_block_large.tar.xz'),
                           4)

  if all_ok:
    print('\nAll fixtures generated and verified successfully.')
  else:
    print('\nSome fixtures did not meet block count requirements.',
          file=sys.stderr)
    sys.exit(1)


if __name__ == '__main__':
  main()
