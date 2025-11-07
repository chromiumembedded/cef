#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
"""
Unit tests for gn_args.py

Running the tests:
  # Run all tests with verbose output
  python3 -m unittest gn_args_test.py -v

  # Run all tests (simple)
  python3 gn_args_test.py

  # Run a specific test class
  python3 -m unittest gn_args_test.TestParseValue -v

  # Run a specific test
  python3 -m unittest gn_args_test.TestParseValue.test_parse_boolean_true -v
"""

import unittest
import unittest.mock
import os
import sys
import tempfile
import shutil
from gn_args import (
    ParseValue,
    FormatValue,
    ParseArgsFile,
    ParseNameValueList,
    ShlexEnv,
    MergeDicts,
    GetChromiumDefaultArgs,
    GetArgValue,
    GetRecommendedDefaultArgs,
    GetRequiredArgs,
    GetMergedArgs,
    ValidateArgs,
    GetConfigArgs,
    LinuxSysrootExists,
    GetAllPlatformConfigs,
    GetConfigFileContents,)


class TestParseValue(unittest.TestCase):
  """Test cases for ParseValue function"""

  def test_parse_boolean_true(self):
    """Test parsing boolean true values"""
    self.assertTrue(ParseValue('true'))
    self.assertTrue(ParseValue('True'))
    self.assertTrue(ParseValue('TRUE'))
    self.assertTrue(ParseValue('  true  '))

  def test_parse_boolean_false(self):
    """Test parsing boolean false values"""
    self.assertFalse(ParseValue('false'))
    self.assertFalse(ParseValue('False'))
    self.assertFalse(ParseValue('FALSE'))
    self.assertFalse(ParseValue('  false  '))

  def test_parse_integer(self):
    """Test parsing integer values"""
    self.assertEqual(ParseValue('42'), 42)
    self.assertEqual(ParseValue('0'), 0)
    self.assertEqual(ParseValue('-123'), -123)
    self.assertEqual(ParseValue('  100  '), 100)

  def test_parse_float(self):
    """Test parsing float values"""
    self.assertEqual(ParseValue('3.14'), 3.14)
    self.assertEqual(ParseValue('0.5'), 0.5)
    self.assertEqual(ParseValue('-2.718'), -2.718)

  def test_parse_double_quoted_string(self):
    """Test parsing double-quoted strings"""
    self.assertEqual(ParseValue('"hello"'), 'hello')
    self.assertEqual(ParseValue('"hello world"'), 'hello world')
    self.assertEqual(ParseValue('"path/to/file"'), 'path/to/file')

  def test_parse_single_quoted_string(self):
    """Test parsing single-quoted strings"""
    self.assertEqual(ParseValue("'hello'"), 'hello')
    self.assertEqual(ParseValue("'hello world'"), 'hello world')

  def test_parse_empty_array(self):
    """Test parsing empty array"""
    self.assertEqual(ParseValue('[]'), [])
    self.assertEqual(ParseValue('[  ]'), [])

  def test_parse_array_of_strings(self):
    """Test parsing array of strings"""
    result = ParseValue('["a", "b", "c"]')
    self.assertEqual(result, ['a', 'b', 'c'])

  def test_parse_array_of_integers(self):
    """Test parsing array of integers"""
    result = ParseValue('[1, 2, 3]')
    self.assertEqual(result, [1, 2, 3])

  def test_parse_array_of_booleans(self):
    """Test parsing array of booleans"""
    result = ParseValue('[true, false, true]')
    self.assertEqual(result, [True, False, True])

  def test_parse_unquoted_string(self):
    """Test parsing unquoted strings (fallback)"""
    self.assertEqual(ParseValue('some_identifier'), 'some_identifier')
    self.assertEqual(ParseValue('x64'), 'x64')


class TestFormatValue(unittest.TestCase):
  """Test cases for FormatValue function"""

  def test_format_boolean_true(self):
    """Test formatting boolean true"""
    self.assertEqual(FormatValue(True), 'true')

  def test_format_boolean_false(self):
    """Test formatting boolean false"""
    self.assertEqual(FormatValue(False), 'false')

  def test_format_integer(self):
    """Test formatting integers"""
    self.assertEqual(FormatValue(42), 42)
    self.assertEqual(FormatValue(0), 0)

  def test_format_float(self):
    """Test formatting floats"""
    self.assertEqual(FormatValue(3.14), 3.14)

  def test_format_string(self):
    """Test formatting strings"""
    self.assertEqual(FormatValue('hello'), '"hello"')
    self.assertEqual(FormatValue('path/to/file'), '"path/to/file"')


class TestParseArgsFile(unittest.TestCase):
  """Test cases for ParseArgsFile function"""

  def setUp(self):
    """Create a temporary directory for test files"""
    self.test_dir = tempfile.mkdtemp()

  def tearDown(self):
    """Clean up temporary directory"""
    shutil.rmtree(self.test_dir)

  def test_parse_simple_args_file(self):
    """Test parsing a simple args file"""
    args_file = os.path.join(self.test_dir, 'args.gn')
    with open(args_file, 'w') as f:
      f.write('is_debug = true\n')
      f.write('is_component_build = false\n')
      f.write('target_cpu = "x64"\n')

    result = ParseArgsFile(args_file)
    self.assertEqual(result['is_debug'], True)
    self.assertEqual(result['is_component_build'], False)
    self.assertEqual(result['target_cpu'], 'x64')

  def test_parse_args_file_with_comments(self):
    """Test that comments are skipped"""
    args_file = os.path.join(self.test_dir, 'args.gn')
    with open(args_file, 'w') as f:
      f.write('# This is a comment\n')
      f.write('is_debug = true\n')
      f.write('# Another comment\n')
      f.write('target_cpu = "x64"\n')

    result = ParseArgsFile(args_file)
    self.assertEqual(len(result), 2)
    self.assertEqual(result['is_debug'], True)
    self.assertEqual(result['target_cpu'], 'x64')

  def test_parse_args_file_with_empty_lines(self):
    """Test that empty lines are skipped"""
    args_file = os.path.join(self.test_dir, 'args.gn')
    with open(args_file, 'w') as f:
      f.write('is_debug = true\n')
      f.write('\n')
      f.write('  \n')
      f.write('target_cpu = "x64"\n')

    result = ParseArgsFile(args_file)
    self.assertEqual(len(result), 2)

  def test_parse_nonexistent_file_raises_error(self):
    """Test that parsing nonexistent file raises FileNotFoundError"""
    with self.assertRaises(FileNotFoundError):
      ParseArgsFile('/nonexistent/path/args.gn')

  def test_parse_args_file_with_integers(self):
    """Test parsing args file with integer values"""
    args_file = os.path.join(self.test_dir, 'args.gn')
    with open(args_file, 'w') as f:
      f.write('symbol_level = 0\n')
      f.write('some_number = 42\n')

    result = ParseArgsFile(args_file)
    self.assertEqual(result['symbol_level'], 0)
    self.assertEqual(result['some_number'], 42)


class TestParseNameValueList(unittest.TestCase):
  """Test cases for ParseNameValueList function"""

  def test_parse_simple_name_value_pairs(self):
    """Test parsing simple NAME=VALUE pairs"""
    result = ParseNameValueList(['is_debug=true', 'target_cpu=x64'])
    self.assertEqual(result['is_debug'], True)
    self.assertEqual(result['target_cpu'], 'x64')

  def test_parse_name_without_value(self):
    """Test parsing NAME without value (defaults to True)"""
    result = ParseNameValueList(['enable_feature'])
    self.assertEqual(result['enable_feature'], True)

  def test_parse_mixed_list(self):
    """Test parsing mixed list with and without values"""
    result = ParseNameValueList(['is_debug=false', 'enable_feature', 'count=5'])
    self.assertEqual(result['is_debug'], False)
    self.assertEqual(result['enable_feature'], True)
    self.assertEqual(result['count'], 5)

  def test_parse_quoted_values(self):
    """Test parsing quoted values"""
    result = ParseNameValueList(['name="value"', 'path="/usr/bin"'])
    self.assertEqual(result['name'], 'value')
    self.assertEqual(result['path'], '/usr/bin')

  def test_parse_empty_list(self):
    """Test parsing empty list"""
    result = ParseNameValueList([])
    self.assertEqual(result, {})

  def test_parse_value_with_spaces(self):
    """Test parsing values with spaces around equals"""
    result = ParseNameValueList(['key = value', 'num = 42'])
    self.assertEqual(result['key'], 'value')
    self.assertEqual(result['num'], 42)


class TestShlexEnv(unittest.TestCase):
  """Test cases for ShlexEnv function"""

  def test_parse_simple_env_var(self):
    """Test parsing simple environment variable"""
    with unittest.mock.patch.dict(os.environ, {'TEST_VAR': 'value1 value2'}):
      result = ShlexEnv('TEST_VAR')
      self.assertEqual(result, ['value1', 'value2'])

  def test_parse_quoted_env_var(self):
    """Test parsing environment variable with quoted values"""
    with unittest.mock.patch.dict(os.environ,
                                  {'TEST_VAR': 'key="quoted value" other'}):
      result = ShlexEnv('TEST_VAR')
      self.assertEqual(result, ['key=quoted value', 'other'])

  def test_parse_missing_env_var(self):
    """Test parsing missing environment variable returns empty list"""
    with unittest.mock.patch.dict(os.environ, {}, clear=True):
      result = ShlexEnv('MISSING_VAR')
      self.assertEqual(result, [])

  def test_parse_empty_env_var(self):
    """Test parsing empty environment variable"""
    with unittest.mock.patch.dict(os.environ, {'EMPTY_VAR': ''}):
      result = ShlexEnv('EMPTY_VAR')
      # Empty string returns '', not []
      self.assertEqual(result, '')


class TestMergeDicts(unittest.TestCase):
  """Test cases for MergeDicts function"""

  def test_merge_two_dicts(self):
    """Test merging two dictionaries"""
    dict1 = {'a': 1, 'b': 2}
    dict2 = {'c': 3, 'd': 4}
    result = MergeDicts(dict1, dict2)
    self.assertEqual(result, {'a': 1, 'b': 2, 'c': 3, 'd': 4})

  def test_merge_with_override(self):
    """Test that later dicts override earlier ones"""
    dict1 = {'a': 1, 'b': 2}
    dict2 = {'b': 3, 'c': 4}
    result = MergeDicts(dict1, dict2)
    self.assertEqual(result, {'a': 1, 'b': 3, 'c': 4})

  def test_merge_multiple_dicts(self):
    """Test merging multiple dictionaries"""
    dict1 = {'a': 1}
    dict2 = {'b': 2}
    dict3 = {'c': 3}
    result = MergeDicts(dict1, dict2, dict3)
    self.assertEqual(result, {'a': 1, 'b': 2, 'c': 3})

  def test_merge_empty_dicts(self):
    """Test merging empty dictionaries"""
    result = MergeDicts({}, {})
    self.assertEqual(result, {})

  def test_merge_preserves_original(self):
    """Test that merge doesn't modify original dicts"""
    dict1 = {'a': 1}
    dict2 = {'b': 2}
    result = MergeDicts(dict1, dict2)
    self.assertEqual(dict1, {'a': 1})
    self.assertEqual(dict2, {'b': 2})
    self.assertEqual(result, {'a': 1, 'b': 2})


class TestGetChromiumDefaultArgs(unittest.TestCase):
  """Test cases for GetChromiumDefaultArgs function"""

  def test_debug_defaults(self):
    """Test default args for debug build"""
    result = GetChromiumDefaultArgs(is_debug=True)
    self.assertEqual(result['is_debug'], True)
    self.assertFalse(result['is_official_build'])
    self.assertFalse(result['is_asan'])
    self.assertFalse(result['dcheck_always_on'])

  def test_release_defaults(self):
    """Test default args for release build"""
    result = GetChromiumDefaultArgs(is_debug=False)
    self.assertEqual(result['is_debug'], True)  # Still True in defaults
    self.assertFalse(result['is_official_build'])

  def test_component_build_default_for_debug(self):
    """Test that is_component_build defaults based on is_debug"""
    result = GetChromiumDefaultArgs(is_debug=True)
    self.assertTrue(result['is_component_build'])

  def test_component_build_default_for_release(self):
    """Test that is_component_build is False for release by default"""
    result = GetChromiumDefaultArgs(is_debug=False)
    self.assertFalse(result['is_component_build'])

  def test_target_cpu_default(self):
    """Test default target_cpu"""
    result = GetChromiumDefaultArgs(is_debug=True)
    self.assertEqual(result['target_cpu'], 'x64')


class TestGetArgValue(unittest.TestCase):
  """Test cases for GetArgValue function"""

  def test_get_existing_arg(self):
    """Test getting an arg that exists in the dict"""
    args = {'is_debug': False}
    result = GetArgValue(args, 'is_debug')
    self.assertFalse(result)

  def test_get_default_arg(self):
    """Test getting default value when arg not in dict"""
    args = {}
    result = GetArgValue(args, 'is_asan', is_debug=True)
    self.assertFalse(result)

  def test_get_arg_requiring_is_debug(self):
    """Test getting arg that requires is_debug parameter"""
    args = {}
    result = GetArgValue(args, 'is_component_build', is_debug=True)
    self.assertTrue(result)  # Default for debug

  def test_assertion_for_undefined_arg(self):
    """Test that accessing undefined arg raises assertion"""
    args = {}
    with self.assertRaises(AssertionError):
      GetArgValue(args, 'nonexistent_arg', is_debug=True)


class TestGetRequiredArgs(unittest.TestCase):
  """Test cases for GetRequiredArgs function"""

  def test_has_optimize_webui(self):
    """Test that optimize_webui is required"""
    result = GetRequiredArgs()
    self.assertTrue(result['optimize_webui'])

  def test_has_enable_widevine(self):
    """Test that enable_widevine is required"""
    result = GetRequiredArgs()
    self.assertTrue(result['enable_widevine'])

  def test_disables_chrome_plugins(self):
    """Test that clang_use_chrome_plugins is disabled"""
    result = GetRequiredArgs()
    self.assertFalse(result['clang_use_chrome_plugins'])


class TestGetMergedArgs(unittest.TestCase):
  """Test cases for GetMergedArgs function"""

  def test_merge_with_empty_build_args(self):
    """Test merging with empty build args"""
    with unittest.mock.patch.dict(os.environ, {}, clear=True):
      result = GetMergedArgs({})
      # Should have required and recommended defaults
      self.assertTrue(result['optimize_webui'])
      self.assertFalse(result['is_component_build'])

  def test_user_args_override_recommended(self):
    """Test that user args override recommended defaults"""
    with unittest.mock.patch.dict(os.environ, {}, clear=True):
      result = GetMergedArgs({'is_component_build': True})
      self.assertTrue(result['is_component_build'])

  def test_required_args_cannot_be_overridden(self):
    """Test that required args cannot be overridden"""
    with unittest.mock.patch.dict(os.environ, {}, clear=True):
      with self.assertRaises(AssertionError):
        GetMergedArgs({'optimize_webui': False})

  def test_required_args_matching_is_allowed(self):
    """Test that specifying required args with correct value is OK"""
    with unittest.mock.patch.dict(os.environ, {}, clear=True):
      result = GetMergedArgs({'optimize_webui': True})
      self.assertTrue(result['optimize_webui'])


class TestValidateArgs(unittest.TestCase):
  """Test cases for ValidateArgs function"""

  def test_asan_requires_release(self):
    """Test that is_asan requires is_debug=false"""
    args = {'is_asan': True, 'is_debug': True}
    with self.assertRaises(AssertionError):
      ValidateArgs(args, is_debug=True)

  def test_asan_with_release_is_valid(self):
    """Test that is_asan with is_debug=false is valid"""
    args = {'is_asan': True, 'is_debug': False, 'dcheck_always_on': True}
    # Should not raise
    ValidateArgs(args, is_debug=False)

  def test_asan_without_dcheck_shows_recommendation(self):
    """Test that is_asan without dcheck_always_on shows recommendation"""
    args = {'is_asan': True, 'is_debug': False}
    # Mock msg to suppress output and verify it's called
    with unittest.mock.patch('gn_args.msg') as mock_msg:
      ValidateArgs(args, is_debug=False)
      # Verify recommendation message was shown
      mock_msg.assert_called_once_with(
          'is_asan=true recommends dcheck_always_on=true')

  def test_official_build_requires_release(self):
    """Test that is_official_build requires is_debug=false"""
    args = {'is_official_build': True, 'is_debug': True}
    with self.assertRaises(AssertionError):
      ValidateArgs(args, is_debug=True)

  def test_official_build_with_release_is_valid(self):
    """Test that is_official_build with is_debug=false is valid"""
    args = {'is_official_build': True, 'is_debug': False}
    # Should not raise
    ValidateArgs(args, is_debug=False)


class TestGetConfigArgs(unittest.TestCase):
  """Test cases for GetConfigArgs function"""

  def test_debug_config(self):
    """Test getting debug configuration args"""
    with unittest.mock.patch.dict(os.environ, {}, clear=True):
      args = GetMergedArgs({})
      result = GetConfigArgs(args, is_debug=True, cpu='x64')
      self.assertTrue(result['is_debug'])
      self.assertEqual(result['target_cpu'], 'x64')

  def test_release_config(self):
    """Test getting release configuration args"""
    with unittest.mock.patch.dict(os.environ, {}, clear=True):
      args = GetMergedArgs({})
      result = GetConfigArgs(args, is_debug=False, cpu='x64')
      self.assertFalse(result['is_debug'])
      self.assertEqual(result['target_cpu'], 'x64')

  def test_official_debug_becomes_release_with_dcheck(self):
    """Test that official debug builds become release with dcheck"""
    with unittest.mock.patch.dict(os.environ, {}, clear=True):
      args = GetMergedArgs({'is_official_build': True})
      result = GetConfigArgs(args, is_debug=True, cpu='x64')
      # Should convert to release with dcheck
      self.assertFalse(result['is_debug'])
      self.assertTrue(result['dcheck_always_on'])

  def test_different_cpu_architectures(self):
    """Test configs for different CPU architectures"""
    with unittest.mock.patch.dict(os.environ, {}, clear=True):
      args = GetMergedArgs({})
      # Mock platform to test different CPU architectures
      # Test with Linux platform which supports all CPUs with sysroot
      with unittest.mock.patch('gn_args.platform', 'linux'):
        args_with_sysroot = MergeDicts(args, {'use_sysroot': True})
        for cpu in ['x64', 'arm64', 'arm']:
          result = GetConfigArgs(args_with_sysroot, is_debug=False, cpu=cpu)
          self.assertEqual(result['target_cpu'], cpu)


class TestLinuxSysrootExists(unittest.TestCase):
  """Test cases for LinuxSysrootExists function"""

  def test_sysroot_check(self):
    """Test sysroot existence check"""
    # Mock os.path.isdir to control return value
    with unittest.mock.patch('gn_args.os.path.isdir') as mock_isdir:
      mock_isdir.return_value = True
      result = LinuxSysrootExists('x64')
      self.assertTrue(result)
      # Verify the path contains expected components
      call_path = mock_isdir.call_args[0][0]
      self.assertIn('debian_bullseye_amd64-sysroot', call_path)

  def test_sysroot_missing(self):
    """Test when sysroot doesn't exist"""
    with unittest.mock.patch('gn_args.os.path.isdir') as mock_isdir:
      mock_isdir.return_value = False
      result = LinuxSysrootExists('x64')
      self.assertFalse(result)

  def test_arm_sysroot_path(self):
    """Test ARM sysroot path"""
    with unittest.mock.patch('gn_args.os.path.isdir') as mock_isdir:
      mock_isdir.return_value = True
      LinuxSysrootExists('arm')
      call_path = mock_isdir.call_args[0][0]
      self.assertIn('debian_bullseye_armhf-sysroot', call_path)

  def test_arm64_sysroot_path(self):
    """Test ARM64 sysroot path"""
    with unittest.mock.patch('gn_args.os.path.isdir') as mock_isdir:
      mock_isdir.return_value = True
      LinuxSysrootExists('arm64')
      call_path = mock_isdir.call_args[0][0]
      self.assertIn('debian_bullseye_arm64-sysroot', call_path)

  def test_invalid_cpu_raises_exception(self):
    """Test that invalid CPU raises exception"""
    with self.assertRaises(Exception):
      LinuxSysrootExists('invalid_cpu')


class TestGetConfigFileContents(unittest.TestCase):
  """Test cases for GetConfigFileContents function"""

  def test_simple_config_output(self):
    """Test generating config file contents"""
    args = {'is_debug': True, 'target_cpu': 'x64'}
    result = GetConfigFileContents(args)
    self.assertIn('is_debug=true', result)
    self.assertIn('target_cpu="x64"', result)

  def test_keys_are_sorted(self):
    """Test that keys are sorted alphabetically"""
    args = {'zzz': 1, 'aaa': 2, 'mmm': 3}
    result = GetConfigFileContents(args)
    lines = result.split('\n')
    self.assertTrue(lines[0].startswith('aaa='))
    self.assertTrue(lines[1].startswith('mmm='))
    self.assertTrue(lines[2].startswith('zzz='))

  def test_empty_args(self):
    """Test generating contents for empty args"""
    result = GetConfigFileContents({})
    self.assertEqual(result, '')

  def test_boolean_formatting(self):
    """Test that booleans are formatted correctly"""
    args = {'bool_true': True, 'bool_false': False}
    result = GetConfigFileContents(args)
    self.assertIn('bool_true=true', result)
    self.assertIn('bool_false=false', result)


if __name__ == '__main__':
  unittest.main()
