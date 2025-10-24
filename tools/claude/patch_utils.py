#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""
Shared utilities for patch analysis and verification tools.
"""

import os
import subprocess
from typing import Optional


def detect_file_movement(file_path: str, old_version: str, new_version: str,
                        project_root: str) -> Optional[str]:
    """Detect if a missing file was moved/renamed using git diff.

    This function can detect:
    - Simple renames (R status in git diff)
    - Files moved to different directories with same basename (D/A pairs)

    This function CANNOT detect:
    - File deletions (file removed entirely)
    - File splits (1 file → many files)
    - File merges (many files → 1 file)
    - Complex refactorings where code moves between differently-named files

    Args:
        file_path: Path to the file that's missing
        old_version: Old Chromium version tag (e.g., '142.0.7444.0')
        new_version: New Chromium version tag (e.g., '143.0.7491.0')
        project_root: Path to Chromium source root

    Returns:
        The new location if detected, None otherwise (including deletions,
        splits, merges, and complex refactorings).
    """
    # Need all parameters to detect movements
    if not old_version or not new_version or not project_root:
        return None

    try:
        # Use git diff with wildcard pattern to catch files moved to subdirectories
        dir_path = os.path.dirname(file_path)
        filename = os.path.basename(file_path)
        search_pattern = f'{dir_path}/*{filename}'

        result = subprocess.run(
            ['git', 'diff', '--name-status', '-M',
             f'refs/tags/{old_version}', f'refs/tags/{new_version}',
             '--', search_pattern],
            capture_output=True,
            text=True,
            timeout=3,
            cwd=project_root
        )

        if result.returncode == 0 and result.stdout.strip():
            # Parse git diff output for renames and D/A pairs
            deleted_file = None
            added_files = []

            for line in result.stdout.strip().split('\n'):
                parts = line.split('\t')
                if line.startswith('R') and len(parts) >= 3:
                    old_path = parts[1]
                    new_path = parts[2]
                    if old_path == file_path:
                        return new_path
                elif line.startswith('D') and len(parts) >= 2:
                    if parts[1] == file_path:
                        deleted_file = file_path
                elif line.startswith('A') and len(parts) >= 2:
                    added_files.append(parts[1])

            # If file was deleted and there's a similar file added, suggest that
            if deleted_file and added_files:
                for added in added_files:
                    if os.path.basename(added) == filename:
                        return added

    except (subprocess.TimeoutExpired, FileNotFoundError, Exception):
        # If git command fails or times out, just return None
        pass

    return None
