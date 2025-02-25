# Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file

from __future__ import absolute_import
from __future__ import print_function
from exec_util import exec_cmd
import os
import sys

# Script directory.
script_dir = os.path.dirname(__file__)
cef_dir = os.path.join(script_dir, os.pardir)
src_dir = os.path.abspath(os.path.join(cef_dir, os.pardir))
llvm_bin_dir = os.path.join(src_dir,
                            'third_party/llvm-build/Release+Asserts/bin')

if sys.platform == 'win32':
  # Force use of the clang-format version bundled with depot_tools.
  clang_format_exe = 'clang-format.bat'
  clang_exe = os.path.join(llvm_bin_dir, 'clang-cl.exe')
else:
  clang_format_exe = 'clang-format'
  clang_exe = os.path.join(llvm_bin_dir, 'clang')


def clang_format(file_name, file_contents):
  # -assume-filename is necessary to find the .clang-format file and determine
  # the language when specifying contents via stdin.
  result = exec_cmd("%s -assume-filename=%s" % (clang_format_exe, file_name),
                    cef_dir, file_contents.encode('utf-8'))
  if result['err'] != '':
    sys.stderr.write("clang-format error: %s\n" % result['err'])
  if result['out'] != '':
    output = result['out']
    if sys.platform == 'win32':
      # Convert to Unix line endings.
      output = output.replace("\r", "")
    return output
  return None


def clang_format_inplace(file_name):
  result = exec_cmd("%s -i %s" % (clang_format_exe, file_name), cef_dir)
  if result['err'] != '':
    sys.stderr.write("clang-format error: %s\n" % result['err'])
    return False
  return True


def clang_eval(file_name, file_contents, defines, includes, verbose):
  lang = 'c'
  if file_name.lower().endswith('.h'):
    lang += '-header'
  # The -P option removes unnecessary line markers and whitespace.
  format = '/EP' if sys.platform == 'win32' else '-E -P'

  cmd = "%s -x %s %s %s %s -" % (clang_exe, lang, format,
                                 ' '.join(['-D' + v for v in defines]),
                                 ' '.join(['-I' + v for v in includes]))
  if verbose:
    print(f'--- Running "{cmd}" in "{cef_dir}"')

  result = exec_cmd(cmd, cef_dir, file_contents.encode('utf-8'))
  if result['err'] != '' or result['ret'] != 0:
    error = result['err'].replace('<stdin>', file_name)
    return_code = result['ret']
    sys.stderr.write(f'clang {return_code=} {error=}\n')
    return None

  output = result['out']
  if output and sys.platform == 'win32':
    # Convert to Unix line endings.
    output = output.replace("\r", "")
  return output
