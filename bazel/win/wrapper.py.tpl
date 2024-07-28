# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import os
import subprocess
import sys
from rules_python.python.runfiles import runfiles

REPLACEMENTS = {
  "XXX_GETCWD_XXX": os.getcwd(),
}

def replace_in_str(input):
  output = input
  for placeholder, replacement in REPLACEMENTS.items():
    if placeholder in output:
      output = output.replace(placeholder, replacement)
  return output

def print_error(str):
  print(str, file=sys.stderr)

r = runfiles.Create()
wrapped_binary = r.Rlocation("winsdk/${binary}")
args = list(map(replace_in_str, sys.argv[1:]))

# Optionally execute a script before tool execution.
if args[0].endswith('.bat') or args[0].endswith('.cmd'):
  if sys.platform != 'win32':
    raise RuntimeError("Error running bat file; unsupported platform")

  # Execute the .bat file first.
  bat_file = args[0].replace('/', '\\')
  p = subprocess.run(
      bat_file, shell=True,
      stdout = subprocess.PIPE,
      stderr = subprocess.STDOUT,
      text=True)
  if p.returncode != 0:
    print_error("[Generated python wrapper] Error running bat file:")
    print_error(f"CWD: {os.getcwd()}")
    print_error(f"EXEC: {bat_file}")
    print_error(f"Exec output:")
    print_error(p.stdout)
    raise RuntimeError(f"Error running bat file; {bat_file}")

  args = args[1:]

try:
  p = subprocess.run(
      [wrapped_binary] + args,
      stdout = subprocess.PIPE,
      stderr = subprocess.STDOUT,
      text=True)
  if p.returncode != 0:
    print_error("[Generated python wrapper] Error running command:")
    print_error(f"CWD: {os.getcwd()}")
    print_error(f"EXEC: {wrapped_binary}")
    print_error(f"ARGS: {args}")
    print_error(f"Exec output:")
    print_error(p.stdout)
    raise RuntimeError(f"Error running wrapped command; {wrapped_binary}")
except OSError as e:
  print_error("[Generated python wrapper] Error running command:")
  print_error(f"CWD: {os.getcwd()}")
  print_error(f"EXEC: {wrapped_binary}")
  print_error(f"ARGS: {args}")
  raise
