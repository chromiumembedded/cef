# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from __future__ import print_function
from typing import Dict
from cef_api_hash import CefApiHasher
from cef_version import VersionFormatter
from date_util import get_date
from file_util import read_file, read_json_file, write_file, write_json_file
from git_util import exec_git_cmd
import os
import sys
from translator import translate
from version_util import *


def get_next_api_revision(api_versions_file, major_version):
  """ Returns the next available API revision for |major_version|.
  """
  json = read_json_file(api_versions_file)
  if not bool(json):
    return 0

  assert 'last' in json, api_versions_file
  last_version, last_revision = version_parse(json['last'])

  if major_version < last_version:
    sys.stderr.write(
        'ERROR: Cannot add new API versions on old branches/checkouts '
        '(found %d, expected >= %d)\b' % (major_version, last_version))
    return -1

  if major_version == last_version:
    # Increment the revision for the current major version.
    new_revision = last_revision + 1
    assert new_revision <= 99, new_revision
    return new_revision

  # Reset the revision for a new major version.
  return 0


def compute_api_hashes(api_version: str,
                       hasher: CefApiHasher,
                       next_allowed: bool) -> Dict[str, str]:
  """ Computes API hashes for the specified |api_version|. """
  if not next_allowed:
    # Next usage is banned with explicit API versions.
    assert api_version not in UNTRACKED_VERSIONS, api_version
    added_defines = [
        # Using CEF_API_VERSION_NEXT is an error.
        'CEF_API_VERSION_NEXT="Please_specify_an_exact_CEF_version"',
    ]
  else:
    added_defines = []

  hashes = hasher.calculate(api_version, added_defines)
  if hashes:
    if api_version in UNTRACKED_VERSIONS:
      label = version_label(api_version)
      label = label[0:1].upper() + label[1:]
      hashes['comment'] = f'{label} last updated {get_date()}.'
    else:
      hashes['comment'] = f'Added {get_date()}.'
  return hashes


def same_api_hashes(hashes1, hashes2):
  return all(hashes1[key] == hashes2[key]
             for key in ['linux', 'mac', 'windows'])


def compute_next_api_version(api_versions_file):
  """ Computes the next available API version number.
  """
  major_version = int(VersionFormatter().get_chrome_major_version())
  next_revision = get_next_api_revision(api_versions_file, major_version)
  if next_revision < 0:
    return None

  return version_make(major_version, next_revision)


def git_grep_next(cef_dir):
  cmd = "grep --no-color -n -E (CEF_NEXT|CEF_NEXT)|=next -- :!include/cef_api_hash.h *.h *.cc"
  if sys.platform == 'win32':
    # Pass the pipe (|) character as a literal argument.
    cmd = cmd.replace('|', '^|')
  return exec_git_cmd(cmd, cef_dir)


def find_next_usage(cpp_header_dir):
  cef_dir = os.path.abspath(os.path.join(cpp_header_dir, os.pardir))
  result = git_grep_next(cef_dir)
  if result is None:
    return False

  sys.stderr.write('ERROR: NEXT usage found in CEF source files:\n\n' + result +
                   '\n\nFix manually or run with --replace-next.\n')
  return True


def replace_next_usage(file, linenums, as_variable, as_metadata):
  assert len(linenums) > 0

  contents = read_file(file)
  if contents is None:
    sys.stderr.write('ERROR: Failed to read file %s\n' % file)
    return 0

  lines = contents.split('\n')
  changect = 0

  messages = []

  for num in linenums:
    idx = num - 1
    if idx < 0 or idx >= len(lines):
      sys.stderr.write('ERROR: Invalid line number %d in file %s\n' % (num,
                                                                       file))
      return 0

    line = lines[idx]

    replaced = False
    if line.find('CEF_NEXT') >= 0:
      line = line.replace('CEF_NEXT', as_variable)
      replaced = True
    if line.find('=next') >= 0:
      line = line.replace('=next', '=' + as_metadata)
      replaced = True

    if replaced:
      lines[idx] = line
      changect += 1
    else:
      messages.append(
          'WARNING: No NEXT instances found on line number %d' % num)

  if changect > 0 and write_file(file, '\n'.join(lines)):
    messages.append('Replaced %d of %d NEXT instances' % (changect,
                                                          len(linenums)))
  else:
    changect = 0

  if len(messages) > 0:
    print('For file %s:' % file)
    for msg in messages:
      print('  %s' % msg)
    print()

  return changect


def find_replace_next_usage(cpp_header_dir, next_version):
  cef_dir = os.path.abspath(os.path.join(cpp_header_dir, os.pardir))
  result = git_grep_next(cef_dir)
  if result is None:
    return 0

  print('Attempting to replace NEXT usage with %s in CEF headers:\n' %
        next_version)
  print(result + '\n')

  as_variable = version_as_variable(next_version)
  as_metadata = version_as_metadata(next_version)

  files = {}

  # Parse values like:
  # include/test/cef_translator_test.h:879:#if CEF_API_ADDED(CEF_NEXT)
  # include/test/cef_translator_test.h:883:  /*--cef(added=next)--*/
  for line in result.split('\n'):
    parts = line.split(':', maxsplit=2)
    name = parts[0]
    linenum = int(parts[1])
    if not name in files:
      files[name] = [linenum]
    else:
      files[name].append(linenum)

  for file, linenums in files.items():
    if replace_next_usage(
        os.path.join(cef_dir, file), linenums, as_variable,
        as_metadata) != len(linenums):
      sys.stderr.write('ERROR: Failed to replace all NEXT usage in %s\n' % file)
      return 1

  # Sanity-check that all instances were fixed.
  if find_next_usage(cpp_header_dir):
    return 1

  print('All NEXT instances successfully replaced.')
  return 0


def exec_apply(api_versions_file,
               api_untracked_file,
               next_version,
               apply_next,
               hasher: CefApiHasher) -> int:
  """ Updates untracked API hashes if necessary.
      Saves the hash for the next API version if |apply_next| is true.
  """
  json_versions, json_untracked, initialized = \
      read_version_files(api_versions_file, api_untracked_file, True)
  if initialized:
    # Also need to generate hashes for the first version.
    apply_next = True
    json_versions['min'] = next_version

  untracked_changed = False
  for version in UNTRACKED_VERSIONS:
    label = version_label(version)
    hashes = compute_api_hashes(version, hasher, next_allowed=True)
    if not hashes:
      sys.stderr.write('ERROR: Failed to process %s\n' % label)
      return 1

    if version in json_untracked['hashes'] and same_api_hashes(
        hashes, json_untracked['hashes'][version]):
      print('Hashes for %s are unchanged.' % label)
    else:
      untracked_changed = True
      print('Updating hashes for %s.' % label)
      json_untracked['hashes'][version] = hashes

  next_changed = apply_next
  if apply_next:
    next_label = version_label(next_version)

    hashes = compute_api_hashes(next_version, hasher, next_allowed=False)
    if not hashes:
      sys.stderr.write('ERROR: Failed to process %s\n' % next_label)
      return 1

    last_version = json_versions.get('last', None)
    if not last_version is None and last_version in json_versions['hashes']:
      if same_api_hashes(hashes, json_versions['hashes'][last_version]):
        print('Hashes for last %s are unchanged.' % version_label(last_version))
        next_changed = False

    if next_changed:
      print('Adding hashes for %s.' % next_label)
      json_versions['last'] = next_version
      json_versions['hashes'][next_version] = hashes

      if NEXT_VERSION in json_untracked['hashes'] and not \
         same_api_hashes(hashes, json_untracked['hashes'][NEXT_VERSION]):
        print('NOTE: Additional versions are available to generate.')

  write_versions = next_changed or not os.path.isfile(api_versions_file)
  write_untracked = untracked_changed or not os.path.isfile(api_untracked_file)

  if not write_versions and not write_untracked:
    print('No hash updates required.')
    return -1

  if write_versions and not write_json_file(
      api_versions_file, json_versions, quiet=False):
    return 1
  if write_untracked and not write_json_file(
      api_untracked_file, json_untracked, quiet=False):
    return 1

  return 0


def exec_check(api_versions_file,
               api_untracked_file,
               fast_check,
               force_update,
               skip_untracked,
               hasher: CefApiHasher) -> int:
  """ Checks existing API version hashes.
      Resaves all API hashes if |force_update| is true. Otherwise, hash
      changes are considered an error.
  """
  assert not (fast_check and force_update)

  json_versions, json_untracked, initialized = read_version_files(
      api_versions_file, api_untracked_file, initialize=False)
  assert not initialized

  versions = []
  len_versioned_existing = len_versioned_checked = len_versioned_failed = 0
  len_untracked_existing = len_untracked_checked = len_untracked_failed = 0

  if json_versions is not None:
    keys = json_versions['hashes'].keys()
    len_versioned_existing = len(keys)
    if len_versioned_existing > 0:
      if fast_check:
        # Only checking a subset of versions.
        for key in ['last', 'min']:
          if key in json_versions:
            version = json_versions[key]
            assert version in json_versions['hashes'], version
            versions.append(version)
            len_versioned_checked += 1
      else:
        versions.extend(keys)
        len_versioned_checked = len_versioned_existing

  if json_untracked is not None:
    keys = json_untracked['hashes'].keys()
    len_untracked_existing = len(keys)
    if len_untracked_existing > 0 and not skip_untracked:
      versions.extend(keys)
      len_untracked_checked = len_untracked_existing

  if not versions:
    print('No hashes to check.')
    return 0

  write_versions = False
  write_untracked = False

  for version in versions:
    untracked = version in UNTRACKED_VERSIONS
    if untracked:
      stored_hashes = json_untracked['hashes'][version]
    else:
      stored_hashes = json_versions['hashes'][version]
    label = version_label(version)
    computed_hashes = compute_api_hashes(version, hasher, next_allowed=True)
    if not bool(computed_hashes):
      sys.stderr.write('ERROR: Failed to process %s\n' % label)
      return 1
    if not same_api_hashes(computed_hashes, stored_hashes):
      if force_update:
        print('Updating hashes for %s' % label)
        if untracked:
          json_untracked['hashes'][version] = computed_hashes
          write_untracked = True
        else:
          json_versions['hashes'][version] = computed_hashes
          write_versions = True
      else:
        sys.stderr.write('ERROR: Hashes for %s do not match!\n' % label)
        if untracked:
          len_untracked_failed += 1
        else:
          len_versioned_failed += 1

  len_failed = len_untracked_failed + len_versioned_failed
  if len_failed == 0:
    if write_versions and not write_json_file(
        api_versions_file, json_versions, quiet=False):
      return 1
    if write_untracked and not write_json_file(
        api_untracked_file, json_untracked, quiet=False):
      return 1

    if write_versions:
      print('WARNING: This change can break back/forward binary compatibility.')
  else:
    sys.stderr.write('ERROR: %d hashes checked and failed\n' % len_failed)
    sys.stderr.write(
        '\nFor debugging tips/tricks see\n' +
        'https://github.com/chromiumembedded/cef/issues/3836#issuecomment-2587767028\n\n'
    )

  print('%d hashes checked and match (%d/%d versioned, %d/%d untracked).' %
        (len(versions) - len_failed,
         len_versioned_checked - len_versioned_failed, len_versioned_existing,
         len_untracked_checked - len_untracked_failed, len_untracked_existing))

  return 0 if len_failed == 0 else 1


if __name__ == "__main__":
  from optparse import OptionParser

  desc = """
  This utility manages CEF API versions.
"""

  epilog = """
Call this utility without arguments after modifying header files in the CEF
include/ directory. Translated files will be updated if necessary.

If translated files have changed, or when running with -u, unversioned API
hashes (next and experimental) will be checked and potentially updated.

If translated files have changed, or when running with -c, versioned and
unversioned API hashes will be checked. Any changes to versioned API hashes
can break back/forward binary compatibility and are considered an error.

API under development will use placeholder values like CEF_NEXT, added=next,
removed=next in CEF header files. This utility can replace those placeholders
with an actual new version and generate the associated versioned API hashes.

Run with -n to output the next available API version.

Run with -a to apply the next available API version.

For complete usage details see
https://bitbucket.org/chromiumembedded/cef/wiki/ApiVersioning.md

"""

  class CustomParser(OptionParser):

    def format_epilog(self, formatter):
      return self.epilog

  parser = CustomParser(description=desc, epilog=epilog)
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
  parser.add_option(
      '-u',
      '--update',
      action='store_true',
      dest='update',
      default=False,
      help='update next and unversioned API hashes')
  parser.add_option(
      '-n',
      '--next',
      action='store_true',
      dest='next',
      default=False,
      help='output the next available API version')
  parser.add_option(
      '-a',
      '--apply-next',
      action='store_true',
      dest='apply',
      default=False,
      help='add a hash for the next available API version')
  parser.add_option(
      '-c',
      '--check',
      action='store_true',
      dest='check',
      default=False,
      help='check hashes for existing API versions')
  parser.add_option(
      '--fast-check',
      action='store_true',
      dest='fastcheck',
      default=False,
      help=
      'only check minimum, last, next and experimental API hashes (use with -u, -a or -c)'
  )
  parser.add_option(
      '--replace-next',
      action='store_true',
      dest='replacenext',
      default=False,
      help='replace NEXT usage in CEF headers (use with -a)')
  parser.add_option(
      '--replace-next-version',
      dest='replacenextversion',
      metavar='VERSION',
      help='replace NEXT usage with this value (use with --replace-next)')
  parser.add_option(
      '--force-update',
      action='store_true',
      dest='force_update',
      default=False,
      help='force update all API hashes (use with -c)')
  (options, args) = parser.parse_args()

  script_dir = os.path.dirname(__file__)
  cef_dir = os.path.abspath(os.path.join(script_dir, os.pardir))

  cpp_header_dir = os.path.join(cef_dir, 'include')
  if not os.path.isdir(cpp_header_dir):
    sys.stderr.write(
        'ERROR: Missing %s directory is required\n' % cpp_header_dir)
    sys.exit(1)

  api_versions_file = os.path.join(cef_dir, VERSIONS_JSON_FILE)
  api_untracked_file = os.path.join(cef_dir, UNTRACKED_JSON_FILE)

  mode_ct = options.update + options.next + options.apply + options.check
  if mode_ct > 1:
    sys.stderr.write(
        'ERROR: Choose a single execution mode (-u, -n, -a or -c)\n')
    parser.print_help(sys.stdout)
    sys.exit(1)

  next_version = compute_next_api_version(api_versions_file)
  if next_version is None:
    sys.exit(1)

  if options.next:
    print(next_version)
    sys.exit(0)

  will_apply_next = options.apply or not os.path.isfile(api_versions_file)
  if will_apply_next:
    if options.replacenext:
      replace_version = options.replacenextversion
      if replace_version is None:
        replace_version = next_version
      elif not version_valid_for_next(replace_version, next_version):
        sys.stderr.write('ERROR: Invalid value for --replace-next-version\n')
        sys.exit(1)
      result = find_replace_next_usage(cpp_header_dir, replace_version)
      if result != 0:
        sys.exit(result)
    elif find_next_usage(cpp_header_dir):
      sys.exit(1)

  changed = translate(cef_dir, verbose=options.verbose) > 0
  skip_untracked = False

  hasher = CefApiHasher(cpp_header_dir, options.debug_dir, options.verbose)

  if options.update or will_apply_next or changed or not os.path.isfile(
      api_untracked_file):
    skip_untracked = True
    if exec_apply(api_versions_file, api_untracked_file, next_version,
                  options.apply, hasher) > 0:
      # Apply failed.
      sys.exit(1)
  elif not options.check:
    print('Nothing to do.')
    sys.exit(0)

  sys.exit(
      exec_check(api_versions_file, api_untracked_file, options.fastcheck and
                 not options.force_update, options.check and
                 options.force_update, skip_untracked, hasher))
