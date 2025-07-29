# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from __future__ import print_function
from file_util import copy_file
import os
import sys

if __name__ == "__main__":
  from optparse import OptionParser

  desc = """
This utility sets up Claude Code integration for CEF.
"""

  epilog = """
This utility sets up Claude Code integration for an existing
CEF/Chromium development environment. See
https://bitbucket.org/chromiumembedded/cef/wiki/MasterBuildQuickStart.md for
prerequisite environment setup instructions.

This setup copies the CLAUDE.md file from cef/tools/claude/ to the project
root directory, providing guidance to Claude Code when working with the
codebase.
"""

  class CustomParser(OptionParser):

    def format_epilog(self, formatter):
      return self.epilog

  parser = CustomParser(description=desc, epilog=epilog + '\n')
  parser.add_option(
      '-v',
      '--verbose',
      action='store_true',
      dest='verbose',
      default=False,
      help='output detailed status information')
  parser.add_option(
      '--headless',
      action='store_true',
      dest='headless',
      default=False,
      help='run without requiring user interaction')
  parser.add_option(
      '--force-update',
      action='store_true',
      dest='force_update',
      default=False,
      help='force update CLAUDE.md file even if it exists')
  (options, args) = parser.parse_args()

  print(epilog)

  if not options.headless:
    input("Press Enter to proceed with setup...")

  quiet = not options.verbose

  script_dir = os.path.dirname(__file__)
  cef_dir = os.path.abspath(os.path.join(script_dir, os.pardir))
  src_dir = os.path.abspath(os.path.join(cef_dir, os.pardir))
  in_dir = os.path.join(cef_dir, 'tools', 'claude')

  print('Running setup for Claude Code environment...\n')

  change_ct = 0

  # Copy CLAUDE.md from cef/tools/claude/ to project root
  claude_md_src = os.path.join(in_dir, 'CLAUDE.md')
  claude_md_dst = os.path.join(src_dir, 'CLAUDE.md')

  if not os.path.isfile(claude_md_src):
    print(f'ERROR: Required source file {claude_md_src} does not exist.')
    sys.exit(1)

  if os.path.isfile(claude_md_dst):
    if not options.force_update:
      print('Skipping existing file CLAUDE.md')
    else:
      print('Updating existing file CLAUDE.md')
      copy_file(claude_md_src, claude_md_dst, quiet)
      change_ct += 1
  else:
    print('Creating CLAUDE.md file')
    copy_file(claude_md_src, claude_md_dst, quiet)
    change_ct += 1

  if change_ct == 0:
    print('No work performed.')
  else:
    print(f'Updated {change_ct} files.')

  print(
      '\nClaude Code integration setup complete.\n\n'
      'The CLAUDE.md file has been copied to the project root directory.\n'
      'This file provides guidance to Claude Code when working with the codebase.\n\n'
      'FIRST TIME USAGE INSTRUCTIONS\n\n'
      '1. Install Claude Code CLI by following the instructions at\n'
      '   https://docs.anthropic.com/en/docs/claude-code/quickstart\n\n'
      '2. Navigate to the project root directory and start Claude Code:\n\n'
      f'   $ cd {src_dir}\n'
      '   $ claude\n\n'
      '3. Claude Code will automatically detect the CLAUDE.md file and use it\n'
      '   for context when assisting with Chromium/CEF development tasks.\n\n'
      '4. For build assistance, use the /autoninja prompt like:\n'
      '   /autoninja Debug_GN_x64 cef\n'
      '   Claude will assist with fixing compile errors iteratively.\n')
