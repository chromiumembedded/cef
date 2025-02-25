# Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from __future__ import print_function
from typing import Dict, List
from clang_util import clang_eval
from file_util import *
import hashlib
import os
import re
import sys
from version_util import EXP_VERSION

API_TOKEN = '#if CEF_API'
OS_TOKEN = '#if defined(OS_'
INCLUDE_START_MARKER = 'int begin_includes_tag;\n'
INCLUDE_DIRS = [
    '.',  # Includes relative to the 'src/cef' directory.
    '..',  # Includes relative to the 'src' directory.
]
PLATFORM_DEFINES = {
    'windows': ['OS_WIN', 'ARCH_CPU_X86_64'],
    'mac': ['OS_MAC', 'OS_POSIX', 'ARCH_CPU_ARM64'],
    'linux': ['OS_LINUX', 'OS_POSIX', 'CEF_X11', 'ARCH_CPU_X86_64'],
}

SYSTEM_INCLUDES_PATTERN = re.compile(r'(#include <[^>]+>)')
FUNCTION_DECLARATION_PATTERN = re.compile(
    r'\nCEF_EXPORT\s+?.*?\s+?(\w+)\s*?\(.*?\)\s*?;', re.DOTALL)
STRUCT_PATTERN = re.compile(
    r'\ntypedef\s+?struct\s+?(\w+)\s+?\{.*?\}\s+?(\w+)\s*?;', re.DOTALL)
ENUM_PATTERN = re.compile(r'\ntypedef\s+?enum\s+?\{.*?\}\s+?(\w+)\s*?;',
                          re.DOTALL)
TYPEDEF_PATTERN = re.compile(r'\ntypedef\s+?.*?\s+(\w+);')
WHITESPACE_PATTERN = re.compile(r'\s+')
PARENTHESIS_PATTERN = re.compile(r'\(\s+')
SINGLE_LINE_COMMENT_PATTERN = re.compile(r'//.*\n')
CEF_STRING_TYPE_PATTERN = re.compile(
    r'\n\s*?#\s*?define\s+?(CEF_STRING_TYPE_\w+)\s+?.*?\n')
DEFINE_PATTERN = re.compile(r'#define\s+?.*?')


def _prepare_content(content: str) -> str:
  """
  Add a marker to the content to indicate where the header-specific output
  begins and replace system includes with a placeholder
  """
  find = '#ifdef __cplusplus\nextern "C" {'
  pos = content.find(find)
  assert pos > 0, f'Cannot find "{find}" in {content}'
  content = content[0:pos] + INCLUDE_START_MARKER + content[pos:]

  content = DEFINE_PATTERN.sub('//DEFINE_PLACEHOLDER//', content)
  return SYSTEM_INCLUDES_PATTERN.sub('//INCLUDE_PLACEHOLDER//', content)


def _process_result(result: str) -> str:
  """
  Remove the non header-specific output and undo substitutions from cef_export.h
  """
  pos = result.find(INCLUDE_START_MARKER)
  if pos == -1:
    return result
  result = result[pos + len(INCLUDE_START_MARKER):]

  replacements = [
      ['__declspec(dllimport)', 'CEF_EXPORT'],
      ['__attribute__((visibility("default")))', 'CEF_EXPORT'],
      ['__stdcall', ''],
  ]
  for find, replace in replacements:
    result = result.replace(find, replace)
  # Must always start with newline as required by _parse_objects()
  return '\n' + result


def _get_defines(filename: str, added_defines: list) -> List[str]:
  defines = [
      # Makes sure CEF_EXPORT is defined.
      'USING_CEF_SHARED',
      # Avoid include of generated headers.
      'GENERATING_CEF_API_HASH',
  ]

  # Avoids errors parsing test includes.
  if filename.find('test/') >= 0:
    defines.append('UNIT_TEST')

  defines.extend(added_defines)
  return defines


def parse_versioned_content(filename: str,
                            content: str,
                            api_version: str,
                            added_defines: list,
                            debug_dir,
                            verbose) -> list:
  """
  Parse the header file content using clang with the specified API version
  Used for files that are version-specific but not platform-specific
  """
  content = _prepare_content(content)
  defines = _get_defines(filename, added_defines)

  if api_version != EXP_VERSION:
    # Specify the exact version.
    defines.append(f'CEF_API_VERSION={api_version}')

  result = clang_eval(filename, content, defines, INCLUDE_DIRS, verbose)
  assert result, f'clang failed to eval {filename} with {api_version=}'

  result = _process_result(result)

  if debug_dir:
    _write_debug_file(debug_dir, 'clang-' + filename.replace('/', '-'), result)

  return _parse_objects(filename, result, 'universal')


def parse_platformed_content(filename: str,
                             content: str,
                             debug_dir,
                             verbose,
                             api_version,
                             added_defines: list) -> list:
  """
  Parse the header file content using clang for every supported platform
  with the specified API version. Used for files that are both version-specific
  and platform-specific.
  """
  content = _prepare_content(content)
  defines = _get_defines(filename, added_defines)

  if api_version and api_version != EXP_VERSION:
    # Specify the exact version.
    defines.append(f'CEF_API_VERSION={api_version}')

  content_objects = []
  for platform, platform_defines in PLATFORM_DEFINES.items():
    result = clang_eval(filename, content, defines + platform_defines,
                        INCLUDE_DIRS, verbose)
    assert result, f'clang failed to eval {filename} on {platform=}'

    result = _process_result(result)
    if debug_dir:
      _write_debug_file(
          debug_dir, f'clang-{platform}-' + filename.replace('/', '-'), result)

    objects = _parse_objects(filename, result, platform)
    content_objects.extend(objects)

  return content_objects


def _write_debug_file(debug_dir, filename, content) -> None:
  make_dir(debug_dir)
  outfile = os.path.join(debug_dir, filename)
  dir = os.path.dirname(outfile)
  make_dir(dir)
  if not isinstance(content, str):
    content = '\n'.join(content)
  write_file(outfile, content)


def _parse_objects(filename: str, content: str, platform: str) -> list:
  objects = []
  content = SINGLE_LINE_COMMENT_PATTERN.sub('', content)

  for m in FUNCTION_DECLARATION_PATTERN.finditer(content):
    objects.append({
        'filename': filename,
        'name': m.group(1),
        'platform': platform,
        'text': _prepare_text(m.group(0))
    })

  for m in STRUCT_PATTERN.finditer(content):
    # Remove 'CEF_CALLBACK' to normalize cross-platform clang output
    objects.append({
        'filename': filename,
        'name': m.group(2),
        'platform': platform,
        'text': _prepare_text(m.group(0).replace('CEF_CALLBACK', ''))
    })

  for m in ENUM_PATTERN.finditer(content):
    objects.append({
        'filename': filename,
        'name': m.group(1),
        'platform': platform,
        'text': _prepare_text(m.group(0))
    })

  for m in TYPEDEF_PATTERN.finditer(content):
    if m.group(1) == 'char16_t':
      # Skip char16_t typedefs to avoid platform-specific differences.
      continue

    objects.append({
        'filename': filename,
        'name': m.group(1),
        'platform': platform,
        'text': _prepare_text(m.group(0))
    })

  return objects


def _prepare_text(text: str) -> str:
  """ Normalize text for hashing. """
  text = WHITESPACE_PATTERN.sub(' ', text.strip())
  return PARENTHESIS_PATTERN.sub('(', text)


def _parse_string_type(filename: str, content: str) -> list:
  """ Grab defined CEF_STRING_TYPE_xxx """
  objects = []
  for m in CEF_STRING_TYPE_PATTERN.finditer(content):
    objects.append({
        'filename': filename,
        'name': m.group(1),
        'text': _prepare_text(m.group(0)),
        'platform': 'universal'
    })
  return objects


def _get_final_sig(objects, platform) -> str:
  return '\n'.join(o['text'] for o in objects
                   if o['platform'] == 'universal' or o['platform'] == platform)


def _get_filenames(header_dir) -> List[str]:
  """ Return file names to be processed, relative to header_dir """
  cef_dir = os.path.abspath(os.path.join(header_dir, os.pardir))

  # Read the variables list from the autogenerated cef_paths.gypi file.
  cef_paths = eval_file(os.path.join(cef_dir, 'cef_paths.gypi'))
  cef_paths = cef_paths['variables']
  # Read the variables list from the manually edited cef_paths2.gypi file.
  cef_paths2 = eval_file(os.path.join(cef_dir, 'cef_paths2.gypi'))
  cef_paths2 = cef_paths2['variables']

  # List of all C API include/ files.
  paths = cef_paths2['includes_capi'] + cef_paths2['includes_common_capi'] + \
      cef_paths2['includes_linux_capi'] + cef_paths2['includes_mac_capi'] + \
      cef_paths2['includes_win_capi'] + cef_paths['autogen_capi_includes']

  return [
      os.path.relpath(os.path.join(cef_dir, filename), header_dir).replace(
          '\\', '/').lower() for filename in paths
  ]


def _save_objects_to_file(debug_dir: str, objects: list) -> None:
  name_len = max([len(o['name']) for o in objects])
  filename_len = max([len(o['filename']) for o in objects])
  platform_len = max([len(o['platform']) for o in objects])
  dump_sig = [
      f"{o['name']:<{name_len}}|{o['filename']:<{filename_len}}|{o['platform']:<{platform_len}}|{o['text']}"
      for o in objects
  ]
  _write_debug_file(debug_dir, 'objects.txt', dump_sig)


class CefApiHasher:
  """ CEF API hash calculator """

  def __init__(self, header_dir, debug_dir, verbose=False):
    if header_dir is None or len(header_dir) == 0:
      raise AssertionError('header_dir is not specified')

    self.filenames = _get_filenames(header_dir)
    self.debug_dir = debug_dir
    self.verbose = verbose
    self.versioned_contents = {}
    self.platformed_contents = {}
    self.file_content_objs = {}

    # Cache values that will not change between calls to calculate().
    for filename in self.filenames:
      self.print(f'Processing {filename} ...')

      if filename in self.versioned_contents \
          or filename in self.platformed_contents \
              or filename in self.file_content_objs:
        self.print(f'{filename} already processed, skipping...')
        continue

      content = read_file(os.path.join(header_dir, filename), normalize=True)
      content_objects = None

      is_platform_specific = OS_TOKEN in content
      is_version_specific = API_TOKEN in content

      # Special case for parsing cef_string.h:
      # Only extract declarations of the form #define CEF_STRING_TYPE_xxx
      if filename == 'internal/cef_string.h':
        content_objects = _parse_string_type(filename, content)
      elif is_platform_specific and not is_version_specific:
        # Parse once and cache for all platforms
        content_objects = parse_platformed_content(
            filename,
            content,
            debug_dir,
            verbose,
            api_version=None,
            added_defines=[])
      elif is_platform_specific and is_version_specific:
        # Needs to be passed to clang with version and platform defines.
        self.platformed_contents[filename] = content
      elif is_version_specific:
        # Needs to be passed to clang with version defines.
        self.versioned_contents[filename] = content
      else:
        content_objects = _parse_objects(filename, content, 'universal')

      if content_objects is not None:
        self.file_content_objs[filename] = content_objects

  def calculate(self, api_version: str, added_defines: list) -> Dict[str, str]:
    """ Calculate the API hash per platform for the specified API version """
    debug_dir = os.path.join(self.debug_dir,
                             api_version) if self.debug_dir else None

    objects = []
    for filename in self.filenames:
      self.print(f'Processing {filename}...')

      if filename in self.versioned_contents:
        content = self.versioned_contents.get(filename, None)
        content_objects = parse_versioned_content(filename, content,
                                                  api_version, added_defines,
                                                  debug_dir, self.verbose)
      elif filename in self.platformed_contents:
        content = self.platformed_contents.get(filename, None)
        content_objects = parse_platformed_content(filename, content, debug_dir,
                                                   self.verbose, api_version,
                                                   added_defines)
      else:
        content_objects = self.file_content_objs.get(filename, None)

      assert content_objects, f'content_objects is None for {filename}'
      objects.extend(content_objects)

    # objects will be sorted including filename to make stable hashes
    objects = sorted(objects, key=lambda o: f"{o['name']}@{o['filename']}")

    if debug_dir:
      _save_objects_to_file(debug_dir, objects)

    hashes = {}
    for platform in PLATFORM_DEFINES.keys():
      sig = _get_final_sig(objects, platform)
      if debug_dir:
        _write_debug_file(debug_dir, f'{platform}.sig', sig)
      hashes[platform] = hashlib.sha1(sig.encode('utf-8')).hexdigest()

    return hashes

  def print(self, msg):
    if self.verbose:
      print(msg)


if __name__ == '__main__':
  from optparse import OptionParser

  parser = OptionParser(description='This utility calculates CEF API hash')
  parser.add_option(
      '--cpp-header-dir',
      dest='cpp_header_dir',
      metavar='DIR',
      help='input directory for C++ header files [required]')
  parser.add_option(
      '--debug-dir',
      dest='debug_dir',
      metavar='DIR',
      help='intermediate directory for easy debugging')
  parser.add_option(
      '-v',
      '--verbose',
      action='store_true',
      dest='verbose',
      default=False,
      help='output detailed status information')
  (options, args) = parser.parse_args()

  # the cpp_header_dir option is required
  if options.cpp_header_dir is None:
    parser.print_help(sys.stdout)
    sys.exit()

  calc = CefApiHasher(options.cpp_header_dir, options.debug_dir,
                      options.verbose)
  revisions = calc.calculate(EXP_VERSION, [])

  print('{')
  for k in sorted(revisions.keys()):
    print(format('"' + k + '"', '>12s') + ': "' + revisions[k] + '"')
  print('}')
