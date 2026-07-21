# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import contextlib
import io
import json
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock

from generator_test_util import archive_manifest
from generator_test_util import output_manifest
from generator_test_util import read_golden
from generator_test_util import run_generator_script
import make_distrib


class FakeVersionFormatter:

  def get_version_string(self):
    return '123.4.5+gabcdef0+chromium-123.0.4567.89'

  def get_chromium_version_string(self):
    return '123.0.4567.89'

  def get_short_version_string(self):
    return '123.4.5'

  def get_plist_version_string(self):
    return '123.4.5.42'


def default_options(**overrides):
  parser = make_distrib.create_option_parser()
  options, _ = parser.parse_args([])
  for name, value in overrides.items():
    setattr(options, name, value)
  return options


@contextlib.contextmanager
def known_umask(mask):
  previous = os.umask(mask)
  try:
    yield
  finally:
    os.umask(previous)


class MakeDistribSelectionTest(unittest.TestCase):

  def test_import_has_no_side_effects_and_help_is_equivalent(self):
    audit = r'''
import os
import subprocess
import sys
import types

git = types.ModuleType('git_util')


def unexpected_git_access(name):
  raise AssertionError('git_util accessed during import: %s' % name)


git.__getattr__ = unexpected_git_access
sys.modules['git_util'] = git


def audit_import(event, arguments):
  if event == 'open':
    mode = arguments[1]
    flags = arguments[2]
    write_flags = os.O_WRONLY | os.O_RDWR | os.O_CREAT | os.O_TRUNC
    if (mode and any(value in mode for value in 'wax+')) or flags & write_flags:
      raise AssertionError('filesystem write during import: %r' %
                           (arguments, ))
  elif event in ('os.mkdir', 'os.remove', 'os.rename', 'os.rmdir',
                 'os.symlink', 'shutil.copyfile'):
    raise AssertionError('filesystem mutation during import: %s' % event)
  elif event == 'subprocess.Popen':
    raise AssertionError('external command during import')


sys.addaudithook(audit_import)
before = os.listdir('.')
import make_distrib
assert callable(make_distrib.main)
assert os.listdir('.') == before
'''
    environment = os.environ.copy()
    tools_directory = os.path.dirname(__file__)
    environment['PYTHONPATH'] = tools_directory + os.pathsep + environment.get(
        'PYTHONPATH', '')
    environment['PYTHONDONTWRITEBYTECODE'] = '1'
    with tempfile.TemporaryDirectory() as temporary_directory:
      imported = subprocess.run([sys.executable, '-c', audit],
                                cwd=temporary_directory,
                                env=environment,
                                capture_output=True,
                                text=True,
                                check=False)
      self.assertEqual(imported.returncode, 0, imported.stderr)
      self.assertEqual(imported.stdout, '')
      self.assertEqual(imported.stderr, '')
      self.assertEqual(os.listdir(temporary_directory), [])

    result = run_generator_script('make_distrib.py', '--help')
    self.assertEqual(result.returncode, 0)
    self.assertEqual(result.stdout, read_golden('make_distrib', 'help.stdout'))
    self.assertEqual(result.stderr, '')

  def test_platform_architecture_mode_and_output_name_helpers(self):
    self.assertEqual(make_distrib.get_platform('win32'), 'windows')
    self.assertEqual(make_distrib.get_platform('darwin'), 'mac')
    self.assertEqual(make_distrib.get_platform('linux2'), 'linux')
    self.assertEqual(make_distrib.get_platform('other'), '')

    cases = [
        ({}, ('32', 'x86', '_GN_x86')),
        ({
            'x64build': True
        }, ('64', 'x64', '_GN_x64')),
        ({
            'armbuild': True
        }, ('arm', 'arm', '_GN_arm')),
        ({
            'arm64build': True
        }, ('arm64', 'arm64', '_GN_arm64')),
    ]
    for values, expected in cases:
      self.assertEqual(make_distrib.get_architecture(default_options(**values)),
                       expected)

    modes = ('minimal', 'client', 'sandbox', 'tools', 'symbolsonly',
             'debugsymbolsonly', 'releasesymbolsonly')
    expected = ('minimal', 'client', 'sandbox', 'tools', 'symbols',
                'debug-symbols', 'release-symbols')
    for flag, mode in zip(modes, expected):
      self.assertEqual(
          make_distrib.get_distribution_mode(default_options(**{flag: True})),
          mode)
    self.assertEqual(make_distrib.get_distribution_mode(default_options()),
                     'standard')

    options = default_options(distribsubdirsuffix='fixture')
    self.assertEqual(
        make_distrib.get_output_name('1.2.3', 'mac', '64', 'standard', options),
        ('cef_binary_1.2.3', 'cef_binary_1.2.3_macosx64_fixture'))
    options = default_options(distribsubdir='custom', tools=True, ozone=True)
    self.assertEqual(
        make_distrib.get_output_name('1', 'linux', '32', 'tools', options)[1],
        'custom_tools_ozone')

  def test_validation_and_current_zero_status_invalid_cli(self):
    output_directory = os.path.join(tempfile.gettempdir(), 'cef-output')
    cases = [
        (default_options(), 'mac', '--output-dir is required.'),
        (default_options(outputdir=output_directory, minimal=True,
                         client=True), 'mac', 'Cannot specify both'),
        (default_options(outputdir=output_directory,
                         x64build=True,
                         armbuild=True,
                         ninjabuild=True), 'linux', 'Invalid combination'),
        (default_options(outputdir=output_directory,
                         armbuild=True,
                         ninjabuild=True), 'mac', '--arm-build'),
        (default_options(outputdir=output_directory,
                         sandbox=True,
                         ninjabuild=True), 'linux', '--sandbox'),
        (default_options(outputdir=output_directory), 'mac', '--ninja-build'),
        (default_options(outputdir=output_directory,
                         ozone=True,
                         ninjabuild=True), 'mac', '--ozone'),
        (default_options(outputdir=output_directory,
                         nosymbols=True,
                         symbolsonly=True,
                         ninjabuild=True), 'mac', 'Invalid combination'),
    ]
    for options, platform, message in cases:
      self.assertIn(message, make_distrib.validate_options(options, platform))
    valid = default_options(outputdir=output_directory, ninjabuild=True)
    self.assertIsNone(make_distrib.validate_options(valid, 'mac'))

    # Characterize the known bug: invalid CLI paths call sys.exit() without a
    # nonzero status.
    result = run_generator_script('make_distrib.py')
    self.assertEqual(result.returncode, 0)
    self.assertIn('--output-dir is required', result.stdout)


class MakeDistribArchiveAndFileTest(unittest.TestCase):

  def setUp(self):
    make_distrib.options = default_options(quiet=True, allowpartial=False)
    make_distrib.archive_dirs = []
    make_distrib.platform = 'mac'

  def _package(self, directory):
    package = os.path.join(directory, 'package')
    os.mkdir(package)
    tool = os.path.join(package, 'tool')
    with open(tool, 'wb') as output:
      output.write(b'tool\n')
    if os.name != 'nt':
      os.chmod(tool, 0o755)
    return package

  def test_zip_tar_and_mocked_7z_archives(self):
    with tempfile.TemporaryDirectory() as directory:
      package = self._package(directory)
      make_distrib.create_zip_archive(package)
      make_distrib.create_tar_archive(package, 'gz')
      zip_members = archive_manifest(package + '.zip')
      tar_members = archive_manifest(package + '.tar.gz')
      self.assertEqual(zip_members[0]['path'], 'package/tool')
      self.assertEqual(zip_members[0]['data'], b'tool\n')
      self.assertEqual(tar_members[-1]['data'], b'tool\n')
      if os.name != 'nt':
        self.assertEqual(tar_members[-1]['mode'], 0o755)

      calls = []
      with mock.patch.dict(os.environ, {'CEF_COMMAND_7ZIP': '/fixture/7z'}), \
          mock.patch.object(make_distrib, 'run',
                            side_effect=lambda command, cwd: calls.append(
                                (command, cwd))), \
          mock.patch.object(make_distrib, 'remove_file') as remove:
        make_distrib.create_7z_archive(package, '7z')
        make_distrib.create_7z_archive(package, 'xz')
      self.assertEqual(len(calls), 3)
      self.assertIn('a -t7z -y', calls[0][0])
      self.assertIn('a -ttar -y', calls[1][0])
      self.assertIn('a -txz -y', calls[2][0])
      remove.assert_called_once_with(package + '.tar')

  def test_output_directory_replacement_copy_list_and_delete_guards(self):
    with tempfile.TemporaryDirectory() as directory:
      existing = os.path.join(directory, 'output')
      os.mkdir(existing)
      with open(os.path.join(existing, 'old'), 'w') as output:
        output.write('old')
      created = make_distrib.create_output_dir('output', directory)
      self.assertFalse(os.path.exists(os.path.join(created, 'old')))
      self.assertEqual(make_distrib.archive_dirs, [created])

      build = os.path.join(directory, 'build')
      os.mkdir(build)
      with open(os.path.join(build, 'required'), 'w') as output:
        output.write('required')
      with contextlib.redirect_stdout(io.StringIO()) as stdout:
        make_distrib.copy_files_list(build, created, [
            {
                'path': 'required',
                'out_path': 'renamed'
            },
            {
                'path': 'optional',
                'conditional': True
            },
        ])
      self.assertTrue(os.path.isfile(os.path.join(created, 'renamed')))
      self.assertEqual(
          stdout.getvalue(),
          'Missing conditional path: %s.\n' % os.path.join(build, 'optional'))
      with self.assertRaisesRegex(Exception, 'Missing required path'):
        make_distrib.copy_files_list(build, created, [{'path': 'missing'}])

      source_dir = os.path.join(build, 'tree')
      os.mkdir(source_dir)
      os.mkdir(os.path.join(source_dir, 'nested.info'))
      with self.assertRaisesRegex(Exception, 'Refusing to delete directory'):
        make_distrib.copy_files_list(build, created, [{
            'path': 'tree',
            'delete': '*.info'
        }])

  def test_normalize_transfer_config_doxyfile_and_gypi_formatting(self):
    with tempfile.TemporaryDirectory() as directory:
      cef_root = os.path.join(directory, 'cef')
      script_root = os.path.join(cef_root, 'tools')
      os.makedirs(os.path.join(script_root, 'distrib'))
      output_root = os.path.join(directory, 'output')
      os.mkdir(output_root)
      with open(os.path.join(cef_root, 'source.h'), 'w') as output:
        output.write('#include "old/path.h"\n#include "include/keep.h"\n')
      with open(os.path.join(script_root, 'distrib', 'README-TRANSFER.txt'),
                'w') as output:
        output.write('Transferred:\n')
      config_directory = os.path.join(directory, 'config')
      os.mkdir(config_directory)
      config = os.path.join(config_directory, 'transfer.cfg')
      with open(config, 'w') as output:
        output.write(
            repr([{
                'source': 'source.h',
                'target': 'dst/source.h',
                'post-process': 'normalize_headers',
                'new_header_path': 'new/'
            }]))
      make_distrib.transfer_files(cef_root, script_root, config_directory,
                                  'fixture', output_root, True)
      transferred = os.path.join(output_root, 'dst', 'source.h')
      with open(transferred) as output:
        data = output.read()
      self.assertIn('#include "new/path.h"', data)
      self.assertIn('#include "include/keep.h"', data)

      make_distrib.cef_dir = cef_root
      make_distrib.cef_ver = 'fixture-version'
      with open(os.path.join(cef_root, 'Doxyfile'), 'w') as output:
        output.write('PROJECT_NUMBER = $(PROJECT_NUMBER)\n')
      make_distrib.transfer_doxyfile(output_root, True)
      with open(os.path.join(output_root, 'Doxyfile')) as output:
        self.assertIn('fixture-version', output.read())

      formatted = []
      with mock.patch.object(
          make_distrib, 'clang_format_inplace',
          side_effect=formatted.append), contextlib.redirect_stdout(
              io.StringIO()) as stdout:
        make_distrib.transfer_gypi_files(cef_root, ['source.h'],
                                         '',
                                         output_root,
                                         True,
                                         format=True)
      self.assertEqual(formatted, [os.path.join(output_root, 'source.h')])
      self.assertEqual(stdout.getvalue(), formatted[0] + '\n')

  def test_bazel_filtering_substitution_and_run_construction(self):
    with tempfile.TemporaryDirectory() as directory:
      bazel = os.path.join(directory, 'bazel')
      output = os.path.join(directory, 'output')
      os.mkdir(bazel)
      os.mkdir(output)
      for name in ('linux-file.txt', 'mac-file.txt', 'common.txt'):
        with open(os.path.join(bazel, name), 'w') as file:
          file.write(name)
      make_distrib.platform = 'mac'
      with contextlib.redirect_stdout(io.StringIO()) as stdout:
        make_distrib.transfer_bazel_files(bazel, output, {}, False)
      self.assertFalse(os.path.exists(os.path.join(output, 'linux',
                                                   'file.txt')))
      self.assertTrue(os.path.isfile(os.path.join(output, 'mac', 'file.txt')))
      self.assertTrue(os.path.isfile(os.path.join(output, 'common.txt')))
      self.assertEqual(
          stdout.getvalue(),
          'Skipping %s file.\n' % os.path.join(bazel, 'linux-file.txt'))

      template = os.path.join(directory, 'template.in')
      target = os.path.join(directory, 'target')
      with open(template, 'w') as file:
        file.write('template')
      with mock.patch.object(make_distrib,
                             'bazel_substitute',
                             return_value='substituted'), mock.patch.object(
                                 make_distrib,
                                 'bazel_last_error',
                                 return_value=None):
        make_distrib.copy_bazel_file_with_substitution(template, target, {},
                                                       'relative')
      with open(target) as file:
        self.assertEqual(file.read(), 'substituted')
      with mock.patch.object(make_distrib,
                             'bazel_substitute',
                             return_value=None), mock.patch.object(
                                 make_distrib,
                                 'bazel_last_error',
                                 return_value='fixture substitution error'):
        with self.assertRaisesRegex(Exception, 'fixture substitution error'):
          make_distrib.copy_bazel_file_with_substitution(
              template, target, {}, 'relative')

      with mock.patch.object(
          make_distrib.subprocess, 'check_call',
          return_value=7) as check_call, contextlib.redirect_stdout(
              io.StringIO()) as stdout:
        result = make_distrib.run('tool --flag value', directory)
      self.assertEqual(result, 7)
      self.assertEqual(
          stdout.getvalue(),
          '-------- Running "tool --flag value" in "%s"...\n' % directory)
      self.assertEqual(check_call.call_args.kwargs['cwd'], directory)
      self.assertEqual(check_call.call_args.args[0],
                       ['tool', '--flag', 'value'])


class MakeDistribReadmeAndToolsTest(unittest.TestCase):

  def setUp(self):
    make_distrib.options = default_options(quiet=True, allowpartial=False)

  def test_readme_component_fallback_modes_and_platforms(self):
    with tempfile.TemporaryDirectory() as directory:
      distrib = os.path.join(directory, 'distrib')
      mac = os.path.join(distrib, 'mac')
      os.makedirs(mac)
      components = ('header', 'standard', 'minimal', 'client', 'sandbox',
                    'tools', 'redistrib', 'footer')
      for component in components:
        with open(os.path.join(distrib, 'README.%s.txt' % component),
                  'w') as output:
          output.write(component + ' $PLATFORM$ $DISTRIB_TYPE$ $CEF_VER$')
      with open(os.path.join(mac, 'README.header.txt'), 'w') as output:
        output.write('mac header $PLATFORM$ $DISTRIB_TYPE$ $CEF_VER$')

      make_distrib.script_dir = directory
      make_distrib.cef_url = 'cef-url'
      make_distrib.cef_rev = 'cef-rev'
      make_distrib.cef_ver = 'cef-version'
      make_distrib.chromium_url = 'chromium-url'
      make_distrib.chromium_rev = 'chromium-rev'
      make_distrib.chromium_ver = 'chromium-version'
      make_distrib.date = 'fixture-date'
      for platform, mode in (('windows', 'standard'), ('mac', 'minimal'),
                             ('linux', 'client'), ('windows', 'sandbox'),
                             ('mac', 'sandbox'), ('linux', 'tools')):
        with self.subTest(platform=platform, mode=mode):
          make_distrib.platform = platform
          make_distrib.mode = mode
          make_distrib.output_dir = os.path.join(directory,
                                                 platform + '-' + mode)
          os.mkdir(make_distrib.output_dir)
          make_distrib.create_readme()
          with open(os.path.join(make_distrib.output_dir, 'README.txt')) as f:
            data = f.read()
          self.assertNotIn('$', data)
          self.assertIn('cef-version', data)
      make_distrib.platform = 'mac'
      self.assertTrue(
          make_distrib.get_readme_component('header').startswith('mac header'))
      with self.assertRaisesRegex(Exception, 'not found'):
        make_distrib.get_readme_component('missing')

  def test_fused_gtest_and_gmock_layout(self):
    with tempfile.TemporaryDirectory() as directory:
      cef_root = os.path.join(directory, 'cef')
      source_root = os.path.join(directory, 'src')
      tests = os.path.join(directory, 'tests')
      os.makedirs(os.path.join(source_root, 'third_party', 'googletest', 'src'))
      with open(
          os.path.join(source_root, 'third_party', 'googletest', 'src',
                       'LICENSE'), 'w') as output:
        output.write('license')
      for name in ('gtest', 'gmock'):
        path = os.path.join(cef_root, 'tools', 'distrib', name)
        os.makedirs(path)
        with open(os.path.join(path, 'README.cef'), 'w') as output:
          output.write(name)
      teamcity = os.path.join(cef_root, 'tests', 'gtest', 'teamcity')
      os.makedirs(teamcity)
      with open(os.path.join(teamcity, 'support.txt'), 'w') as output:
        output.write('support')
      make_distrib.cef_dir = cef_root
      make_distrib.src_dir = os.path.realpath(source_root)

      def fuse_gtest(_root, output):
        os.makedirs(os.path.join(output, 'gtest'))
        for filename in ('gtest.h', 'gtest-all.cc'):
          with open(os.path.join(output, 'gtest', filename), 'w') as file:
            file.write(filename)

      def fuse_gmock_header(_root, output):
        with open(os.path.join(output, 'gmock', 'gmock.h'), 'w') as file:
          file.write('#include "gtest/gtest.h"\n')

      def fuse_gmock_source(_root, output):
        output.write('gmock-all')

      with mock.patch.object(
          make_distrib.fuse_gtest_files, 'FuseGTest',
          side_effect=fuse_gtest), mock.patch.object(
              make_distrib.fuse_gmock_files,
              'FuseGMockH',
              side_effect=fuse_gmock_header), mock.patch.object(
                  make_distrib.fuse_gmock_files,
                  'FuseGMockAllCcToFile',
                  side_effect=fuse_gmock_source):
        make_distrib.copy_gtest(tests)
        make_distrib.copy_gmock(tests)
      self.assertTrue(
          os.path.isfile(
              os.path.join(tests, 'gtest', 'include', 'gtest', 'gtest.h')))
      gmock_header = os.path.join(tests, 'gmock', 'include', 'gmock', 'gmock.h')
      with open(gmock_header) as output:
        self.assertIn('tests/gtest/include/gtest/gtest.h', output.read())

  def test_toolchain_extraction_names_and_tools_transfer(self):
    with tempfile.TemporaryDirectory() as directory:
      source_root = os.path.join(directory, 'src')
      build = os.path.join(source_root, 'out', 'Debug_GN_x64')
      tool_dir = os.path.join(build, 'clang_x64')
      os.makedirs(tool_dir)
      with open(os.path.join(build, 'toolchain.ninja'), 'w') as output:
        output.write('command = python run.py ./clang_x64/mksnapshot '
                     '--flag ../../input.dat -o gen/out\n')
      with open(os.path.join(source_root, 'input.dat'), 'w') as output:
        output.write('input')
      for name in ('mksnapshot', 'v8_context_snapshot_generator'):
        with open(os.path.join(tool_dir, name), 'w') as output:
          output.write(name)
      script_root = os.path.join(directory, 'script')
      tools = os.path.join(script_root, 'distrib', 'tools')
      os.makedirs(tools)
      with open(os.path.join(tools, 'run_mksnapshot.sh'), 'w') as output:
        output.write('run')
      destination = os.path.join(directory, 'destination')
      os.mkdir(destination)
      make_distrib.src_dir = os.path.realpath(source_root)
      make_distrib.platform = 'mac'
      with contextlib.redirect_stdout(io.StringIO()) as stdout:
        command, path = make_distrib.extract_toolchain_cmd(
            build, 'mksnapshot', True)
        make_distrib.transfer_tools_files(script_root, (build,), destination)
      self.assertEqual(path, 'clang_x64')
      self.assertIn('../../input.dat', command)
      toolchain_output = ('mksnapshot command: '
                          '--flag ../../input.dat -o gen/out\n'
                          'mksnapshot path: clang_x64\n')
      self.assertEqual(stdout.getvalue(), toolchain_output * 2)
      self.assertTrue(
          os.path.isfile(os.path.join(destination, 'Debug', 'mksnapshot')))
      with open(os.path.join(destination, 'Debug', 'mksnapshot_cmd.txt')) as f:
        command = f.read()
      self.assertIn('input.dat', command)
      self.assertNotIn('../../', command)
      self.assertEqual(make_distrib.get_exe_name('tool'), 'tool')
      self.assertEqual(make_distrib.get_script_name('tool'), 'tool.sh')
      make_distrib.platform = 'windows'
      self.assertEqual(make_distrib.get_exe_name('tool'), 'tool.exe')
      self.assertEqual(make_distrib.get_script_name('tool'), 'tool.bat')

      with open(os.path.join(build, 'toolchain.ninja'), 'w') as output:
        output.write('command = ./clang_x64/mksnapshot ; unsafe\n')
      with contextlib.redirect_stdout(io.StringIO()) as stdout:
        with self.assertRaisesRegex(Exception, 'Failed to extract'):
          make_distrib.extract_toolchain_cmd(build, 'mksnapshot', True)
      self.assertEqual(
          stdout.getvalue(), 'mksnapshot command: ; unsafe\n'
          'mksnapshot path: clang_x64\n')

  def test_toolchain_required_optional_native_and_command_optional(self):
    with tempfile.TemporaryDirectory() as directory:
      build = os.path.join(directory, 'build')
      os.mkdir(build)
      missing_build = os.path.join(directory, 'missing')
      with self.assertRaisesRegex(Exception, 'Missing file'):
        make_distrib.extract_toolchain_cmd(missing_build, 'mksnapshot', True)
      self.assertEqual(
          make_distrib.extract_toolchain_cmd(missing_build, 'mksnapshot',
                                             False), (None, None))

      toolchain = os.path.join(build, 'toolchain.ninja')
      with open(toolchain, 'w') as output:
        output.write('command = ./mksnapshot --native\n')
      with contextlib.redirect_stdout(io.StringIO()) as stdout:
        self.assertEqual(
            make_distrib.extract_toolchain_cmd(build, 'mksnapshot', True),
            ('--native', ''))
      self.assertEqual(stdout.getvalue(), 'mksnapshot command: --native\n'
                       'mksnapshot path: \n')

      with open(toolchain, 'w') as output:
        output.write('rule unrelated\n')
      self.assertEqual(
          make_distrib.extract_toolchain_cmd(build,
                                             'mksnapshot',
                                             True,
                                             require_cmd=False), (None, None))
      with self.assertRaisesRegex(Exception, 'Failed to extract'):
        make_distrib.extract_toolchain_cmd(build, 'mksnapshot', True)

  def test_tools_transfer_rejects_bad_auxiliary_inputs_and_missing_binaries(
      self):
    with tempfile.TemporaryDirectory() as directory:
      source_root = os.path.join(directory, 'src')
      build = os.path.join(source_root, 'out', 'Debug_GN_x64')
      tool_dir = os.path.join(build, 'clang_x64')
      os.makedirs(tool_dir)
      script_root = os.path.join(directory, 'script')
      tools = os.path.join(script_root, 'distrib', 'tools')
      os.makedirs(tools)
      with open(os.path.join(tools, 'run_mksnapshot.sh'), 'w') as output:
        output.write('run')
      for name in ('mksnapshot', 'v8_context_snapshot_generator'):
        with open(os.path.join(tool_dir, name), 'w') as output:
          output.write(name)
      make_distrib.src_dir = os.path.realpath(source_root)
      make_distrib.platform = 'mac'
      make_distrib.options = default_options(quiet=True, allowpartial=False)

      cases = (("../../../outside.dat", 'Invalid mksnapshot command input'),
               ("../../missing.dat", 'Missing mksnapshot command input'),
               ("../../input.dat/", 'Failed to parse mksnapshot command'))
      with contextlib.redirect_stdout(io.StringIO()) as stdout:
        for component, message in cases:
          with self.subTest(component=component):
            with open(os.path.join(build, 'toolchain.ninja'), 'w') as output:
              output.write('command = ./clang_x64/mksnapshot %s\n' % component)
            destination = os.path.join(directory,
                                       'destination-%d' % len(component))
            os.mkdir(destination)
            with self.assertRaisesRegex(Exception, message):
              make_distrib.transfer_tools_files(script_root, (build,),
                                                destination)

        with open(os.path.join(build, 'toolchain.ninja'), 'w') as output:
          output.write('command = ./clang_x64/mksnapshot --flag\n')
        os.remove(os.path.join(tool_dir, 'v8_context_snapshot_generator'))
        missing_binary_output = os.path.join(directory, 'missing-binary')
        os.mkdir(missing_binary_output)
        with self.assertRaisesRegex(Exception, 'Missing required path'):
          make_distrib.transfer_tools_files(script_root, (build,),
                                            missing_binary_output)
      expected = ''.join('mksnapshot command: %s\n'
                         'mksnapshot path: clang_x64\n' % component
                         for component, _ in cases)
      expected += ('mksnapshot command: --flag\n'
                   'mksnapshot path: clang_x64\n')
      self.assertEqual(stdout.getvalue(), expected)

  def test_tools_transfer_allow_partial_and_native_toolchain(self):
    with tempfile.TemporaryDirectory() as directory:
      build = os.path.join(directory, 'Debug_GN_x64')
      os.mkdir(build)
      script_root = os.path.join(directory, 'script')
      tools = os.path.join(script_root, 'distrib', 'tools')
      os.makedirs(tools)
      with open(os.path.join(tools, 'run_mksnapshot.sh'), 'w') as output:
        output.write('run')
      destination = os.path.join(directory, 'destination')
      os.mkdir(destination)
      make_distrib.platform = 'mac'
      make_distrib.options = default_options(quiet=True, allowpartial=True)
      with contextlib.redirect_stdout(io.StringIO()) as stdout:
        make_distrib.transfer_tools_files(script_root, (build,), destination)
      self.assertIn('No Debug build toolchain', stdout.getvalue())

      with open(os.path.join(build, 'toolchain.ninja'), 'w') as output:
        output.write('command = ./mksnapshot --native\n')
      for name in ('mksnapshot', 'v8_context_snapshot_generator'):
        with open(os.path.join(build, name), 'w') as output:
          output.write(name)
      make_distrib.options.allowpartial = False
      with contextlib.redirect_stdout(io.StringIO()) as stdout:
        make_distrib.transfer_tools_files(script_root, (build,), destination)
      self.assertEqual(stdout.getvalue(), 'mksnapshot command: --native\n'
                       'mksnapshot path: \n')
      self.assertTrue(
          os.path.isfile(os.path.join(destination, 'Debug', 'mksnapshot')))
      with open(os.path.join(destination, 'Debug',
                             'mksnapshot_cmd.txt')) as output:
        self.assertEqual(output.read(), '--native')


class MakeDistribMainTest(unittest.TestCase):

  def test_successful_host_tools_orchestration_is_repeatable(self):
    with known_umask(0o022), tempfile.TemporaryDirectory() as directory:
      source_root = os.path.join(directory, 'synthetic-src')
      cef_root = os.path.join(source_root, 'cef')
      script_root = os.path.join(cef_root, 'tools')
      distrib_root = os.path.join(script_root, 'distrib')
      os.makedirs(distrib_root)
      for component, contents in (('header',
                                   '$CEF_VER$ $PLATFORM$ $DISTRIB_TYPE$'),
                                  ('tools', 'tools'), ('redistrib',
                                                       'redistrib'),
                                  ('footer', '$CHROMIUM_VER$ $DATE$')):
        with open(os.path.join(distrib_root, 'README.%s.txt' % component),
                  'w') as output:
          output.write(contents)
      with open(os.path.join(cef_root, 'LICENSE.txt'), 'wb') as output:
        output.write(b'license\n')
      original_exists = os.path.exists

      def exists(path):
        if path.endswith(
            os.path.join('gen', 'components', 'resources',
                         'about_credits.html')):
          return True
        return original_exists(path)

      def copy_file(source, destination, quiet=True):
        if os.path.isdir(destination):
          destination = os.path.join(destination, os.path.basename(source))
        os.makedirs(os.path.dirname(destination), exist_ok=True)
        if os.path.isfile(source):
          shutil.copy2(source, destination)
        else:
          with open(destination, 'w') as output:
            output.write(os.path.basename(source))

      def transfer_tools(_script_dir, _build_dirs, output_dir):
        with open(os.path.join(output_dir, 'tools-marker.txt'), 'wb') as output:
          output.write(b'tools\n')

      arguments = [
          '--output-dir', directory, '--tools', '--allow-partial',
          '--no-archive', '--no-docs', '--ninja-build', '--quiet'
      ]
      patches = (
          mock.patch.object(make_distrib, 'get_platform', return_value='mac'),
          mock.patch.object(make_distrib.git, 'is_checkout', return_value=True),
          mock.patch.object(make_distrib.git,
                            'get_url',
                            side_effect=lambda path: 'cef-url'
                            if path.endswith('cef') else 'chromium-url'),
          mock.patch.object(make_distrib.git,
                            'get_hash',
                            side_effect=lambda path: 'cef-rev'
                            if path.endswith('cef') else 'chromium-rev'),
          mock.patch.object(make_distrib.git,
                            'get_commit_number',
                            return_value=42),
          mock.patch.object(make_distrib,
                            'get_date',
                            return_value='fixture-date'),
          mock.patch.object(make_distrib, 'VersionFormatter',
                            FakeVersionFormatter),
          mock.patch.object(make_distrib,
                            'eval_file',
                            return_value={'variables': {}}),
          mock.patch.object(make_distrib.os.path, 'exists', side_effect=exists),
          mock.patch.object(make_distrib, 'copy_file', side_effect=copy_file),
          mock.patch.object(make_distrib,
                            'transfer_tools_files',
                            side_effect=transfer_tools),
          mock.patch.object(make_distrib, '__file__',
                            os.path.join(script_root, 'make_distrib.py')),
      )
      with contextlib.ExitStack() as stack:
        for patch in patches:
          stack.enter_context(patch)
        self.assertIsNone(make_distrib.main(arguments))
        output_dir = make_distrib.output_dir
        first = output_manifest(output_dir)
        expected = json.loads(read_golden('make_distrib',
                                          'tools_manifest.json'))
        if os.name == 'nt':
          for entry in first:
            entry.pop('mode')
          for entry in expected:
            entry.pop('mode')
        self.assertEqual(first, expected)
        self.assertIsNone(make_distrib.main(arguments))
        second = output_manifest(output_dir)
        if os.name == 'nt':
          for entry in second:
            entry.pop('mode')
      self.assertEqual(first, second)


if __name__ == '__main__':
  unittest.main()
