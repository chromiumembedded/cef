# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from __future__ import print_function
from exec_util import exec_cmd
from file_util import backup_file, copy_file, make_dir, read_file, write_file
import os
import platform as python_platform
import sys

COMPILE_COMMANDS_JSON = 'compile_commands.json'
LLDBINIT = '.lldbinit'


def UpdateCompileCommandsJSON(src_dir, out_dir, create):
  out_path = os.path.join(src_dir, COMPILE_COMMANDS_JSON)
  if not create and not os.path.isfile(out_path):
    return True

  print(f'Writing {COMPILE_COMMANDS_JSON} for clangd...')
  with open(out_path, 'w') as f:
    result = exec_cmd(
        sys.executable +
        f' tools/clang/scripts/generate_compdb.py -p {out_dir}',
        src_dir,
        output_file=f)
    if result['err']:
      print('ERROR: generate_compdb.py failed: %s' % result['err'])
      return False
  return True


def GetPreferredOutputDirectory(all_dirs):
  if sys.platform == 'win32':
    # Windows machines report 'ARM64' or 'AMD64'.
    machine = 'arm64' if python_platform.machine() == 'ARM64' else 'x64'
  elif sys.platform == 'darwin':
    # Mac machines report 'arm64' or 'x86_64'.
    machine = 'arm64' if python_platform.machine() == 'arm64' else 'x64'
  elif sys.platform.startswith('linux'):
    # Linux machines report 'aarch64', 'armv7l', 'x86_64', 'i386', etc.
    machine = 'arm64' if python_platform.machine() == 'aarch64' else 'x64'

  # Return the preferred directory that matches the host architecture.
  for dir in (f'Debug_GN_{machine}', f'Release_GN_{machine}'):
    if dir in all_dirs:
      return dir

  return None


if __name__ == "__main__":
  from optparse import OptionParser

  desc = """
This utility sets up Visual Studio Code (VSCode) integration for CEF.
"""

  epilog = """
This utility sets up Visual Studio Code (VSCode) integration for an existing
CEF/Chromium development environment. See
https://github.com/chromiumembedded/cef/blob/master/docs/master_build_quick_start.md for
prerequisite environment setup instructions.

The VSCode application and recommended extensions should be installed manually
upon completion of this setup. Instructions for this will be provided.

This setup is an automation and customization of the Chromium setup documented
at https://chromium.googlesource.com/chromium/src/+/main/docs/vscode.md. After
completion of this setup the VSCode configuration can be further customized by
modifying JSON files in the src/.vscode directory (either directly or via the
VSCode interface).

This setup includes configuration of clangd for improved C/C++ IntelliSense.
This functionality depends on a src/compile_commands.json file that will
be created by this utility and then regenerated each time cef_create_projects
is called in the future. Delete this json file manually if you do not wish to
utilize clangd with VSCode.
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
      help='force update all JSON configuration files')
  (options, args) = parser.parse_args()

  print(epilog)

  if not options.headless:
    input("Press Enter to proceed with setup...")

  quiet = not options.verbose

  script_dir = os.path.dirname(__file__)
  cef_dir = os.path.abspath(os.path.join(script_dir, os.pardir))
  src_dir = os.path.abspath(os.path.join(cef_dir, os.pardir))
  chromium_dir = os.path.abspath(os.path.join(src_dir, os.pardir))
  in_dir = os.path.join(cef_dir, 'tools', 'vscode')

  print('Running setup for VSCode environment...\n')

  # Determine the platform and architecture.
  # Chromium development only supports ARM64 and x64 host systems. All other
  # configurations will be cross-compiled.
  if sys.platform == 'win32':
    platform = 'windows'
    # Windows machines report 'ARM64' or 'AMD64'.
    machine = 'arm64' if python_platform.machine() == 'ARM64' else 'x64'
    sampleapp = 'cefclient.exe'
    testsapp = 'ceftests.exe'
  elif sys.platform == 'darwin':
    platform = 'mac'
    # Mac machines report 'arm64' or 'x86_64'.
    machine = 'arm64' if python_platform.machine() == 'arm64' else 'x64'
    sampleapp = 'cefclient.app/Contents/MacOS/cefclient'
    testsapp = 'ceftests.app/Contents/MacOS/ceftests'
  elif sys.platform.startswith('linux'):
    platform = 'linux'
    # Linux machines report 'aarch64', 'armv7l', 'x86_64', 'i386', etc.
    machine = 'arm64' if python_platform.machine() == 'aarch64' else 'x64'
    sampleapp = 'cefclient'
    testsapp = 'ceftests'
  else:
    print('ERROR: Unknown operating system platform.')
    sys.exit(1)

  debug_out_dir = os.path.join(src_dir, 'out', 'Debug_GN_' + machine)
  debug_out_dir_exists = os.path.isdir(debug_out_dir)
  release_out_dir = os.path.join(src_dir, 'out', 'Release_GN_' + machine)
  release_out_dir_exists = os.path.isdir(release_out_dir)

  if not debug_out_dir_exists and not release_out_dir_exists:
    print(
        f'ERROR: Output directories matching your host architecture ({machine}) do not exist.\n'
        'Check your GN_OUT_CONFIGS environment variable and re-run cef_create_projects before proceeding.'
    )
    sys.exit(1)

  out_dir = debug_out_dir if debug_out_dir_exists else release_out_dir

  print(
      f'Configuring VSCode project files for your host architecture ({machine})...\n'
  )

  variables = {
      'ARCH': machine,
      'DEFAULT': 'Debug' if debug_out_dir_exists else 'Release',
      'DEBUGGER': 'cppvsdbg' if platform == 'windows' else 'cppdbg',
      'EXEEXT': '.exe' if platform == 'windows' else '',
      'SAMPLEAPP': sampleapp,
      'TESTSAPP': testsapp,
  }

  vscode_dir = os.path.join(src_dir, '.vscode')
  if not os.path.isdir(vscode_dir):
    make_dir(vscode_dir, quiet)

  change_ct = 0

  # Update JSON files if necessary.
  for json_name in ('cpp.code-snippets', 'extensions.json', 'keybindings.json',
                    'settings.json', 'launch.json', 'tasks.json'):
    out_path = os.path.join(vscode_dir, json_name)
    if os.path.isfile(out_path):
      if not options.force_update:
        print(f'Skipping existing file {json_name}')
        continue
      else:
        print(f'Backing up existing file {json_name}')
        backup_file(out_path)

    in_path = os.path.join(in_dir, json_name)
    if os.path.isfile(in_path):
      # Copying a CEF file as-is.
      copy_file(in_path, out_path, quiet)
      change_ct += 1
      continue

    in_path += '.in'
    if os.path.isfile(in_path):
      # Copying a CEF file with variable substitution.
      content = read_file(in_path)
      for name, val in variables.items():
        content = content.replace('{{' + name + '}}', val)
      write_file(out_path, content, quiet=quiet)
      change_ct += 1
      continue

    in_path = os.path.join(src_dir, 'tools', 'vscode', json_name)
    if os.path.isfile(in_path):
      # Copying a Chromium file as-is.
      copy_file(in_path, out_path, quiet)
      change_ct += 1
      continue

    print(f'ERROR: Required input file {json_name} does not exist.')
    sys.exit(1)

  gclient_path = os.path.join(chromium_dir, '.gclient')
  if not os.path.isfile(gclient_path):
    print(f'ERROR: Required input file {gclient_path} does not exist.')
    sys.exit(1)

  # Setup for clangd.
  # https://chromium.googlesource.com/chromium/src/+/main/docs/clangd.md
  content = read_file(gclient_path)
  if content.find('checkout_clangd') < 0:
    insert = "'custom_vars': {"
    content = content.replace(insert, insert + "'checkout_clangd': True, ")
    write_file(gclient_path, content, quiet=quiet)
    change_ct += 1

    print('Downloading clangd...')
    result = exec_cmd('gclient sync --with_branch_heads --nohooks', src_dir)
    if len(result['err']) > 0:
      print('ERROR: gclient sync failed: %s' % result['err'])
      sys.exit(1)

  if not os.path.isfile(os.path.join(src_dir, COMPILE_COMMANDS_JSON)):
    if UpdateCompileCommandsJSON(src_dir, out_dir, create=True):
      change_ct += 1
    else:
      sys.exit(1)

  if platform == 'mac':
    # Setup for lldb.
    # https://chromium.googlesource.com/chromium/src/+/main/docs/lldbinit.md
    lldbinit_path = os.path.join(os.path.expanduser("~"), LLDBINIT)
    if os.path.isfile(lldbinit_path):
      if not options.force_update:
        print(f'Skipping existing file {LLDBINIT}')
      else:
        print(f'Backing up existing file {LLDBINIT}')
        backup_file(lldbinit_path)

    if not os.path.isfile(lldbinit_path):
      content = "# So that lldbinit.py takes precedence.\n" \
                f"script sys.path[:0] = ['{src_dir}/tools/lldb']\n" \
                "script import lldbinit\n" \
                "script import chromium_visualizers"
      write_file(lldbinit_path, content, quiet=quiet)
      change_ct += 1

  if change_ct == 0:
    print('No work performed.')
  else:
    print(f'Updated {change_ct} files.')

  missing_dirs = []
  if not debug_out_dir_exists:
    missing_dirs.append('Debug')
  if not release_out_dir_exists:
    missing_dirs.append('Release')

  for dir in missing_dirs:
    print(
        f'\nWARNING: A {dir} output directory matching your host architecture ({machine}) does\n'
        f'not exist. You will not be able to build or run {dir} builds with VSCode.'
    )

  print(
      '\nFIRST TIME USAGE INSTRUCTIONS\n\n'
      '1. Install VSCode (including command-line integration) by following the instructions at'
      '\n   https://code.visualstudio.com/docs/setup/setup-overview'
      '\n\n2. Launch VSCode with the following console commands:\n\n'
      f'   $ cd {src_dir}\n'
      '   $ code .\n'
      '\n3. Install recommended VSCode extensions when prompted or by following the instructions at'
      '\n   https://chromium.googlesource.com/chromium/src/+/main/docs/vscode.md#Install-Recommended-Extensions\n'
  )
