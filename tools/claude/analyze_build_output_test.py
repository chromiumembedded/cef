#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

"""
Unit tests for analyze_build_output.py

Running the tests:
  # Run all tests with verbose output
  python3 -m unittest analyze_build_output_test.py -v

  # Run all tests (simple)
  python3 analyze_build_output_test.py

  # Run a specific test class
  python3 -m unittest analyze_build_output_test.TestBuildOutputAnalyzer -v

  # Run a specific test
  python3 -m unittest analyze_build_output_test.TestBuildOutputAnalyzer.test_single_error -v
"""

import unittest
import json
from analyze_build_output import BuildOutputAnalyzer


def make_build_output(content, build_dir='out/Debug_GN_x64'):
    """Helper to create build output with ninja directory line.

    Args:
        content: The build output content
        build_dir: Build directory path, or None to omit the ninja line

    Returns:
        Complete build output with ninja directory line prepended (if build_dir is not None)
    """
    if build_dir is None:
        return content
    return f"ninja: Entering directory `{build_dir}'\n{content}"


class TestBuildDirectoryDetection(unittest.TestCase):
    """Test cases for build directory detection"""

    def test_detect_windows_x64_directory(self):
        """Test detection of Windows x64 build directory"""
        output = make_build_output("""
[1/100] CXX obj/cef/libcef_static/test.o
""", build_dir='out/Debug_GN_x64')
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)
        self.assertEqual(analyzer.analysis.build_directory, 'out/Debug_GN_x64')

    def test_detect_mac_arm64_directory(self):
        """Test detection of Mac ARM64 build directory"""
        output = make_build_output("""
[1/100] CXX obj/cef/libcef_static/test.o
""", build_dir='out/Debug_GN_arm64')
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)
        self.assertEqual(analyzer.analysis.build_directory, 'out/Debug_GN_arm64')

    def test_detect_release_directory(self):
        """Test detection of Release build directory"""
        output = make_build_output("""
[1/100] CXX obj/cef/libcef_static/test.o
""", build_dir='out/Release_GN_x64')
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)
        self.assertEqual(analyzer.analysis.build_directory, 'out/Release_GN_x64')

    def test_detect_custom_directory(self):
        """Test detection of custom build directory"""
        output = make_build_output("""
[1/100] CXX obj/cef/libcef_static/test.o
""", build_dir='out/MyCustomBuild')
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)
        self.assertEqual(analyzer.analysis.build_directory, 'out/MyCustomBuild')

    def test_detect_directory_with_single_quotes(self):
        """Test detection with single quotes (alternative format)"""
        output = "ninja: Entering directory 'out/Debug_GN_x64'\n[1/100] CXX obj/cef/libcef_static/test.o\n"
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)
        self.assertEqual(analyzer.analysis.build_directory, 'out/Debug_GN_x64')

    def test_missing_build_directory_raises_error(self):
        """Test that missing build directory raises ValueError"""
        output = make_build_output("""[1/100] CXX obj/cef/libcef_static/test.o
../../cef/test.cc:10:5: error: something went wrong
""", build_dir=None)
        analyzer = BuildOutputAnalyzer('test_output.txt')
        with self.assertRaises(ValueError) as context:
            analyzer.parse(output)
        self.assertIn('Could not detect build directory', str(context.exception))
        self.assertIn('ninja: Entering directory', str(context.exception))

    def test_empty_output_raises_error(self):
        """Test that empty output raises ValueError"""
        output = ""
        analyzer = BuildOutputAnalyzer('test_output.txt')
        with self.assertRaises(ValueError) as context:
            analyzer.parse(output)
        self.assertIn('Could not detect build directory', str(context.exception))

    def test_build_directory_used_in_summary(self):
        """Test that detected build directory is used in summary report"""
        output = make_build_output("""
FAILED: obj/cef/libcef_static/test.o
../../cef/test.cc:10:5: error: test error
""", build_dir='out/CustomBuild')
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)

        summary = analyzer.generate_summary_report(colorize=False)

        # Should use detected directory, not default
        self.assertIn('autoninja -C out/CustomBuild', summary)
        self.assertNotIn('autoninja -C out/Debug_GN_x64', summary)

    def test_build_directory_with_first_error(self):
        """Test that build directory detection happens before error parsing"""
        output = make_build_output("""
[1/100] CXX obj/cef/libcef_static/test.o
FAILED: obj/cef/libcef_static/test.o
../../cef/test.cc:10:5: error: test error
""", build_dir='out/Release_GN_arm64')
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.build_directory, 'out/Release_GN_arm64')
        self.assertEqual(analyzer.analysis.total_errors, 1)

        summary = analyzer.generate_summary_report(colorize=False)
        self.assertIn('autoninja -C out/Release_GN_arm64', summary)


class TestBuildOutputAnalyzer(unittest.TestCase):
    """Test cases for BuildOutputAnalyzer"""

    def test_no_errors(self):
        """Test parsing successful build output with no errors"""
        output = make_build_output("""
[1/100] CXX obj/cef/libcef_static/browser_host.o
[2/100] CXX obj/cef/libcef_static/request_impl.o
[100/100] LINK ./libcef.dylib
""")
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 0)
        self.assertEqual(analyzer.analysis.total_files, 0)
        self.assertEqual(len(analyzer.analysis.files), 0)

    def test_single_error(self):
        """Test parsing a single compilation error"""
        output = make_build_output("""
FAILED: 70ba6b60-775a-4ac0-8912-69b8f3de807e "./obj/cef/libcef_static/iothread_state.o" CXX obj/cef/libcef_static/iothread_state.o
../../cef/libcef/browser/iothread_state.cc:88:22: error: no viable conversion from 'std::string_view' to 'const std::string'
   88 |   const std::string& scheme_lower = url.scheme();
      |                      ^              ~~~~~~~~~~~~
1 error generated.
""")
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 1)
        self.assertEqual(analyzer.analysis.total_files, 1)

        file_path = 'cef/libcef/browser/iothread_state.cc'
        self.assertIn(file_path, analyzer.analysis.files)

        file_errors = analyzer.analysis.files[file_path]
        self.assertEqual(file_errors.error_count, 1)
        self.assertEqual(file_errors.build_target, 'obj/cef/libcef_static/iothread_state.o')

        error = file_errors.errors[0]
        self.assertEqual(error.line_number, 88)
        self.assertEqual(error.error_line, 4)  # Line 4 in the output (after ninja line)

    def test_multiple_errors_one_file(self):
        """Test parsing multiple errors in one file"""
        output = make_build_output("""
FAILED: abc123 "./obj/cef/libcef_static/drag_data.o" CXX obj/cef/libcef_static/drag_data.o
../../cef/libcef/common/drag_data_impl.cc:51:17: error: no member named 'url' in 'content::DropData'
   51 |   return (data_.url.is_valid() &&
      |           ~~~~~ ^
../../cef/libcef/common/drag_data_impl.cc:57:18: error: no member named 'url' in 'content::DropData'
   57 |   return (!data_.url.is_valid() &&
      |            ~~~~~ ^
2 errors generated.
""")
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 2)
        self.assertEqual(analyzer.analysis.total_files, 1)

        file_path = 'cef/libcef/common/drag_data_impl.cc'
        file_errors = analyzer.analysis.files[file_path]
        self.assertEqual(file_errors.error_count, 2)
        self.assertEqual(len(file_errors.errors), 2)

        self.assertEqual(file_errors.errors[0].line_number, 51)
        self.assertEqual(file_errors.errors[1].line_number, 57)

    def test_multiple_files(self):
        """Test parsing errors from multiple files"""
        output = make_build_output("""
FAILED: abc123 "./obj/cef/libcef_static/file1.o" CXX obj/cef/libcef_static/file1.o
../../cef/libcef/browser/file1.cc:10:5: error: unknown type name 'Foo'
   10 |     Foo bar;
      |     ^
1 error generated.

FAILED: def456 "./obj/cef/libcef_static/file2.o" CXX obj/cef/libcef_static/file2.o
../../cef/libcef/browser/file2.cc:20:10: error: use of undeclared identifier 'baz'
   20 |   return baz();
      |          ^
1 error generated.
""")
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 2)
        self.assertEqual(analyzer.analysis.total_files, 2)

        self.assertIn('cef/libcef/browser/file1.cc', analyzer.analysis.files)
        self.assertIn('cef/libcef/browser/file2.cc', analyzer.analysis.files)

    def test_build_target_extraction(self):
        """Test that build targets are extracted correctly from FAILED lines"""
        output = make_build_output("""
FAILED: 70ba6b60-775a-4ac0-8912-69b8f3de807e "./obj/cef/libcef_static/iothread_state.o" CXX obj/cef/libcef_static/iothread_state.o
../../cef/libcef/browser/iothread_state.cc:88:22: error: test error
""")
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)

        file_path = 'cef/libcef/browser/iothread_state.cc'
        file_errors = analyzer.analysis.files[file_path]
        self.assertEqual(file_errors.build_target, 'obj/cef/libcef_static/iothread_state.o')

    def test_header_file_error(self):
        """Test parsing errors from header files"""
        output = make_build_output("""
FAILED: abc123 "./obj/cef/libcef_static/view_osr.o" CXX obj/cef/libcef_static/view_osr.o
../../cef/libcef/browser/osr/render_widget_host_view_osr.h:176:10: error: incomplete type
  176 |   void Foo();
      |        ^
1 error generated.
""")
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 1)
        file_path = 'cef/libcef/browser/osr/render_widget_host_view_osr.h'
        self.assertIn(file_path, analyzer.analysis.files)

    def test_path_normalization(self):
        """Test that ../../ prefix is removed from file paths"""
        output = make_build_output("""
../../cef/libcef/browser/test.cc:10:5: error: test error
   10 |     Foo bar;
      |     ^
""")
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)

        # Should be normalized to remove ../../
        self.assertIn('cef/libcef/browser/test.cc', analyzer.analysis.files)
        self.assertNotIn('../../cef/libcef/browser/test.cc', analyzer.analysis.files)

    def test_error_line_tracking(self):
        """Test that error_line (line in build_output.txt) is tracked correctly"""
        output = make_build_output("""Line 1
Line 2
../../cef/libcef/browser/test.cc:10:5: error: test error
Line 4
Line 5
""")
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)

        file_errors = analyzer.analysis.files['cef/libcef/browser/test.cc']
        self.assertEqual(file_errors.errors[0].error_line, 4)  # Error is on line 4 (after ninja line)

    def test_json_output(self):
        """Test JSON report generation"""
        output = make_build_output("""
FAILED: abc123 "./obj/cef/libcef_static/test.o" CXX obj/cef/libcef_static/test.o
../../cef/libcef/browser/test.cc:10:5: error: test error
   10 |     Foo bar;
      |     ^
""")
        analyzer = BuildOutputAnalyzer('test.txt')
        analyzer.parse(output)

        json_output = analyzer.generate_json_report()
        data = json.loads(json_output)

        self.assertEqual(data['build_output_file'], 'test.txt')
        self.assertEqual(data['statistics']['total_errors'], 1)
        self.assertEqual(data['statistics']['total_files'], 1)

        self.assertEqual(len(data['files']), 1)
        file_data = data['files'][0]
        self.assertEqual(file_data['file_path'], 'cef/libcef/browser/test.cc')
        self.assertEqual(file_data['error_count'], 1)
        self.assertEqual(file_data['build_target'], 'obj/cef/libcef_static/test.o')

        error = file_data['errors'][0]
        self.assertEqual(error['source_line'], 10)
        self.assertEqual(error['build_output_line'], 4)  # After ninja line

    def test_summary_report_no_errors(self):
        """Test summary report generation with no errors"""
        output = make_build_output("""
[100/100] LINK ./libcef.dylib
""")
        analyzer = BuildOutputAnalyzer('test.txt', old_version='1.0.0', new_version='2.0.0')
        analyzer.parse(output)

        summary = analyzer.generate_summary_report(colorize=False)

        self.assertIn('CEF BUILD ERROR INDEX', summary)
        self.assertIn('Total errors: 0', summary)
        self.assertIn('BUILD SUCCESSFUL', summary)

    def test_summary_report_with_errors(self):
        """Test summary report generation with errors"""
        output = make_build_output("""
FAILED: abc "./obj/cef/libcef_static/test.o" CXX obj/cef/libcef_static/test.o
../../cef/libcef/browser/test.cc:10:5: error: test error
""")
        analyzer = BuildOutputAnalyzer('build.txt', old_version='1.0.0', new_version='2.0.0')
        analyzer.parse(output)

        summary = analyzer.generate_summary_report(colorize=False)

        self.assertIn('CEF BUILD ERROR INDEX', summary)
        self.assertIn('Total errors: 1', summary)
        self.assertIn('cef/libcef/browser/test.cc', summary)
        self.assertIn('build.txt:4', summary)  # Line reference (error is on line 4 after ninja)
        self.assertIn('Rebuild:', summary)
        self.assertIn('autoninja', summary)

    def test_sorting_by_error_count(self):
        """Test that files are sorted by error count (most errors first)"""
        output = make_build_output("""
../../cef/libcef/browser/file1.cc:10:5: error: test error 1
../../cef/libcef/browser/file2.cc:20:5: error: test error 2a
../../cef/libcef/browser/file2.cc:21:5: error: test error 2b
../../cef/libcef/browser/file2.cc:22:5: error: test error 2c
../../cef/libcef/browser/file3.cc:30:5: error: test error 3a
../../cef/libcef/browser/file3.cc:31:5: error: test error 3b
""")
        analyzer = BuildOutputAnalyzer('test.txt')
        analyzer.parse(output)

        json_output = analyzer.generate_json_report()
        data = json.loads(json_output)

        # Files should be sorted by error count: file2 (3), file3 (2), file1 (1)
        self.assertEqual(data['files'][0]['file_path'], 'cef/libcef/browser/file2.cc')
        self.assertEqual(data['files'][0]['error_count'], 3)
        self.assertEqual(data['files'][1]['file_path'], 'cef/libcef/browser/file3.cc')
        self.assertEqual(data['files'][1]['error_count'], 2)
        self.assertEqual(data['files'][2]['file_path'], 'cef/libcef/browser/file1.cc')
        self.assertEqual(data['files'][2]['error_count'], 1)

    def test_missing_build_target(self):
        """Test files without FAILED line (no build target)"""
        output = make_build_output("""
../../cef/libcef/browser/test.cc:10:5: error: test error
""")
        analyzer = BuildOutputAnalyzer('test.txt')
        analyzer.parse(output)

        file_errors = analyzer.analysis.files['cef/libcef/browser/test.cc']
        self.assertIsNone(file_errors.build_target)

    def test_workflow_in_summary(self):
        """Test that WORKFLOW section is present in summary"""
        output = make_build_output("""
../../cef/libcef/browser/test.cc:10:5: error: test error
""")
        analyzer = BuildOutputAnalyzer('build.txt', old_version='1.0.0', new_version='2.0.0')
        analyzer.parse(output)

        summary = analyzer.generate_summary_report(colorize=False)

        self.assertIn('WORKFLOW:', summary)
        self.assertIn('Read error details:', summary)
        self.assertIn('Investigate Chromium changes:', summary)
        self.assertIn('git diff', summary)
        self.assertIn('1.0.0', summary)
        self.assertIn('2.0.0', summary)

    def test_error_without_column_number(self):
        """Test parsing errors without column numbers"""
        output = make_build_output("""
../../cef/libcef/browser/test.cc:10: error: test error without column
""")
        analyzer = BuildOutputAnalyzer('test.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 1)
        file_errors = analyzer.analysis.files['cef/libcef/browser/test.cc']
        self.assertEqual(file_errors.errors[0].line_number, 10)


class TestRealWorldSamples(unittest.TestCase):
    """Test cases based on real build output samples"""

    def test_string_view_conversion_error(self):
        """Test parsing GURL string_view conversion error"""
        output = make_build_output("""
FAILED: 70ba6b60-775a-4ac0-8912-69b8f3de807e "./obj/cef/libcef_static/iothread_state.o" CXX obj/cef/libcef_static/iothread_state.o
../../cef/libcef/browser/iothread_state.cc:88:22: error: no viable conversion from 'std::string_view' (aka 'basic_string_view<char>') to 'const std::string' (aka 'const basic_string<char>')
   88 |   const std::string& scheme_lower = url.scheme();
      |                      ^              ~~~~~~~~~~~~
gen/third_party/libc++/src/include/string:1004:71: note: candidate constructor not viable
1 error generated.
""")
        analyzer = BuildOutputAnalyzer('build_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 1)
        file_errors = analyzer.analysis.files['cef/libcef/browser/iothread_state.cc']
        self.assertEqual(file_errors.errors[0].line_number, 88)
        self.assertEqual(file_errors.errors[0].error_line, 4)  # Error is on line 4 (after ninja line) of the output

    def test_missing_member_error(self):
        """Test parsing missing member error"""
        output = make_build_output("""
FAILED: a6804cc4-8188-43e4-9593-8e6405213f38 "./obj/cef/libcef_static/cookie_helper.o" CXX obj/cef/libcef_static/cookie_helper.o
../../cef/libcef/browser/net_service/cookie_helper.cc:223:33: error: no member named 'scheme_piece' in 'GURL'
  223 |     const auto url_scheme = url.scheme_piece();
      |                             ~~~ ^
1 error generated.
""")
        analyzer = BuildOutputAnalyzer('build_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 1)
        file_errors = analyzer.analysis.files['cef/libcef/browser/net_service/cookie_helper.cc']
        self.assertEqual(file_errors.errors[0].line_number, 223)
        self.assertEqual(file_errors.errors[0].error_line, 4)  # Error is on line 4 (after ninja line)

    def test_multiple_errors_drag_data(self):
        """Test parsing multiple related errors in drag_data_impl.cc"""
        output = make_build_output("""
FAILED: c310a28a-dacb-4bb7-8e3e-3fb31d6f3ae0 "./obj/cef/libcef_static/drag_data_impl.o" CXX obj/cef/libcef_static/drag_data_impl.o
../../cef/libcef/common/drag_data_impl.cc:51:17: error: no member named 'url' in 'content::DropData'
   51 |   return (data_.url.is_valid() &&
      |           ~~~~~ ^
../../cef/libcef/common/drag_data_impl.cc:57:18: error: no member named 'url' in 'content::DropData'
   57 |   return (!data_.url.is_valid() &&
      |            ~~~~~ ^
../../cef/libcef/common/drag_data_impl.cc:70:21: error: no member named 'url' in 'content::DropData'
   70 |   drop_data.url = GURL(url);
      |   ~~~~~~~~~ ^
3 errors generated.
""")
        analyzer = BuildOutputAnalyzer('build_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 3)
        file_errors = analyzer.analysis.files['cef/libcef/common/drag_data_impl.cc']
        self.assertEqual(file_errors.error_count, 3)
        self.assertEqual(file_errors.build_target, 'obj/cef/libcef_static/drag_data_impl.o')

        # Verify all three error line numbers
        line_numbers = [e.line_number for e in file_errors.errors]
        self.assertEqual(line_numbers, [51, 57, 70])

    def test_generated_file_error(self):
        """Test parsing errors from generated files"""
        output = make_build_output("""
gen/third_party/libc++/src/include/__memory/unique_ptr.h:75:10: error: incomplete type 'content::RenderWidgetHostViewOSR' used in nested name specifier
   75 |   static_assert(sizeof(_Tp) >= 0, "cannot delete an incomplete type");
      |          ^
1 error generated.
""")
        analyzer = BuildOutputAnalyzer('build_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 1)
        # Generated files should be tracked too
        self.assertIn('gen/third_party/libc++/src/include/__memory/unique_ptr.h',
                     analyzer.analysis.files)

    def test_parser_impl_multiple_errors(self):
        """Test parsing multiple errors from parser_impl.cc"""
        output = make_build_output("""
FAILED: f2ad0dfd-cfaf-4070-91c0-ab3ee0614ec7 "./obj/cef/libcef_static/parser_impl.o" CXX obj/cef/libcef_static/parser_impl.o
../../cef/libcef/common/parser_impl.cc:39:28: error: no matching member function for call to 'FromString'
   39 |   CefString(&parts.scheme).FromString(gurl.scheme());
      |   ~~~~~~~~~~~~~~~~~~~~~~~~~^~~~~~~~~~
../../cef/libcef/common/parser_impl.cc:40:26: error: no matching member function for call to 'FromString'
   40 |   CefString(&parts.host).FromString(gurl.host());
      |   ~~~~~~~~~~~~~~~~~~~~~~~^~~~~~~~~~
../../cef/libcef/common/parser_impl.cc:41:26: error: no matching member function for call to 'FromString'
   41 |   CefString(&parts.port).FromString(gurl.port());
      |   ~~~~~~~~~~~~~~~~~~~~~~~^~~~~~~~~~
3 errors generated.
""")
        analyzer = BuildOutputAnalyzer('build_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 3)
        file_errors = analyzer.analysis.files['cef/libcef/common/parser_impl.cc']
        self.assertEqual(file_errors.error_count, 3)

        # Check consecutive line numbers
        self.assertEqual(file_errors.errors[0].line_number, 39)
        self.assertEqual(file_errors.errors[1].line_number, 40)
        self.assertEqual(file_errors.errors[2].line_number, 41)


class TestWindowsErrorFormat(unittest.TestCase):
    """Test cases for Windows-style error format: file(line,col): error: message"""

    def test_single_windows_error(self):
        """Test parsing a single Windows-style compilation error"""
        output = make_build_output("""
FAILED: 466b678a-0d52-4ca8-9ca8-26a0169bdf71 "./obj/third_party/blink/renderer/controller/controller/blink_glue.obj" CXX obj/third_party/blink/renderer/controller/controller/blink_glue.obj
../../cef/libcef/renderer/blink_glue.cc(325,8): error: no member named 'SetUseExternalPopupMenusForTesting' in 'blink::ChromeClient'
  322 |   static_cast<blink::WebViewImpl*>(view)
1 error generated.
""")
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 1)
        self.assertEqual(analyzer.analysis.total_files, 1)

        file_path = 'cef/libcef/renderer/blink_glue.cc'
        self.assertIn(file_path, analyzer.analysis.files)

        file_errors = analyzer.analysis.files[file_path]
        self.assertEqual(file_errors.error_count, 1)
        self.assertEqual(file_errors.build_target, 'obj/third_party/blink/renderer/controller/controller/blink_glue.obj')

        error = file_errors.errors[0]
        self.assertEqual(error.line_number, 325)
        self.assertEqual(error.error_line, 4)  # Line 4 (after ninja line)

    def test_multiple_windows_errors_one_file(self):
        """Test parsing multiple Windows-style errors in one file"""
        output = make_build_output("""
FAILED: abc123 "./obj/cef/libcef_static/drag_data.obj" CXX obj/cef/libcef_static/drag_data.obj
../../cef/libcef/common/drag_data_impl.cc(51,17): error: no member named 'url' in 'content::DropData'
   51 |   return (data_.url.is_valid() &&
      |           ~~~~~ ^
../../cef/libcef/common/drag_data_impl.cc(57,18): error: no member named 'url' in 'content::DropData'
   57 |   return (!data_.url.is_valid() &&
      |            ~~~~~ ^
2 errors generated.
""")
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 2)
        self.assertEqual(analyzer.analysis.total_files, 1)

        file_path = 'cef/libcef/common/drag_data_impl.cc'
        file_errors = analyzer.analysis.files[file_path]
        self.assertEqual(file_errors.error_count, 2)
        self.assertEqual(len(file_errors.errors), 2)

        self.assertEqual(file_errors.errors[0].line_number, 51)
        self.assertEqual(file_errors.errors[1].line_number, 57)

    def test_multiple_windows_files(self):
        """Test parsing Windows-style errors from multiple files"""
        output = make_build_output("""
FAILED: abc123 "./obj/cef/libcef_static/file1.obj" CXX obj/cef/libcef_static/file1.obj
../../cef/libcef/browser/file1.cc(10,5): error: unknown type name 'Foo'
   10 |     Foo bar;
      |     ^
1 error generated.

FAILED: def456 "./obj/cef/libcef_static/file2.obj" CXX obj/cef/libcef_static/file2.obj
../../cef/libcef/browser/file2.cc(20,10): error: use of undeclared identifier 'baz'
   20 |   return baz();
      |          ^
1 error generated.
""")
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 2)
        self.assertEqual(analyzer.analysis.total_files, 2)

        self.assertIn('cef/libcef/browser/file1.cc', analyzer.analysis.files)
        self.assertIn('cef/libcef/browser/file2.cc', analyzer.analysis.files)

    def test_mixed_unix_and_windows_errors(self):
        """Test parsing mix of Unix and Windows style errors"""
        output = make_build_output("""
FAILED: abc123 "./obj/cef/libcef_static/file1.o" CXX obj/cef/libcef_static/file1.o
../../cef/libcef/browser/file1.cc:10:5: error: Unix-style error
   10 |     Foo bar;
      |     ^
1 error generated.

FAILED: def456 "./obj/cef/libcef_static/file2.obj" CXX obj/cef/libcef_static/file2.obj
../../cef/libcef/browser/file2.cc(20,10): error: Windows-style error
   20 |   return baz();
      |          ^
1 error generated.
""")
        analyzer = BuildOutputAnalyzer('test_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 2)
        self.assertEqual(analyzer.analysis.total_files, 2)

        # Both Unix and Windows formats should be detected
        self.assertIn('cef/libcef/browser/file1.cc', analyzer.analysis.files)
        self.assertIn('cef/libcef/browser/file2.cc', analyzer.analysis.files)

        # Verify line numbers are correct for both formats
        file1_errors = analyzer.analysis.files['cef/libcef/browser/file1.cc']
        file2_errors = analyzer.analysis.files['cef/libcef/browser/file2.cc']
        self.assertEqual(file1_errors.errors[0].line_number, 10)
        self.assertEqual(file2_errors.errors[0].line_number, 20)

    def test_real_world_windows_blink_glue_error(self):
        """Test parsing the real Windows error from build_output.txt"""
        output = make_build_output("""
FAILED: 466b678a-0d52-4ca8-9ca8-26a0169bdf71 "./obj/third_party/blink/renderer/controller/controller/blink_glue.obj" CXX obj/third_party/blink/renderer/controller/controller/blink_glue.obj
err: exit=1
../../cef/libcef/renderer/blink_glue.cc(325,8): error: no member named 'SetUseExternalPopupMenusForTesting' in 'blink::ChromeClient'
  322 |   static_cast<blink::WebViewImpl*>(view)
      |   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  323 |       ->GetPage()
      |       ~~~~~~~~~~~
  324 |       ->GetChromeClient()
      |       ~~~~~~~~~~~~~~~~~~~
  325 |       .SetUseExternalPopupMenusForTesting(value);
      |        ^
1 error generated.
""")
        analyzer = BuildOutputAnalyzer('build_output.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 1)
        file_errors = analyzer.analysis.files['cef/libcef/renderer/blink_glue.cc']
        self.assertEqual(file_errors.errors[0].line_number, 325)
        self.assertEqual(file_errors.errors[0].error_line, 5)  # Error is on line 5 (after ninja + FAILED + err lines)


class TestEdgeCases(unittest.TestCase):
    """Test edge cases and error handling"""

    def test_malformed_error_line(self):
        """Test handling of malformed error lines"""
        output = make_build_output("""
This is not a valid error line
../../cef/libcef/browser/test.cc:10:5: error: real error
Another invalid line
""")
        analyzer = BuildOutputAnalyzer('test.txt')
        analyzer.parse(output)

        # Should only parse the valid error
        self.assertEqual(analyzer.analysis.total_errors, 1)

    def test_warning_not_counted(self):
        """Test that warnings are not counted as errors"""
        output = make_build_output("""
../../cef/libcef/browser/test.cc:10:5: warning: unused variable
../../cef/libcef/browser/test.cc:20:5: error: actual error
""")
        analyzer = BuildOutputAnalyzer('test.txt')
        analyzer.parse(output)

        # Should only count the error, not the warning
        self.assertEqual(analyzer.analysis.total_errors, 1)

    def test_multiline_error_context(self):
        """Test that error context lines don't create duplicate errors"""
        output = make_build_output("""
../../cef/libcef/browser/test.cc:10:5: error: test error
   10 |     Foo bar;
      |     ^
      |     suggestion here
gen/third_party/libc++/include/string.h:100:10: note: candidate constructor
  100 |   string();
      |   ^
""")
        analyzer = BuildOutputAnalyzer('test.txt')
        analyzer.parse(output)

        # Should only count the actual error, not the notes
        self.assertEqual(analyzer.analysis.total_errors, 1)

    def test_very_long_line_numbers(self):
        """Test handling of large line numbers"""
        output = make_build_output("""
../../cef/libcef/browser/test.cc:123456:78: error: test error
""")
        analyzer = BuildOutputAnalyzer('test.txt')
        analyzer.parse(output)

        self.assertEqual(analyzer.analysis.total_errors, 1)
        file_errors = analyzer.analysis.files['cef/libcef/browser/test.cc']
        self.assertEqual(file_errors.errors[0].line_number, 123456)

    def test_file_with_spaces_in_path(self):
        """Test handling of file paths (though Chromium shouldn't have these)"""
        output = make_build_output("""
../../cef/libcef/browser/my file.cc:10:5: error: test error
""")
        analyzer = BuildOutputAnalyzer('test.txt')
        analyzer.parse(output)

        # Should handle it (even though it's unusual)
        self.assertEqual(analyzer.analysis.total_errors, 1)

    def test_build_target_with_quotes(self):
        """Test build target extraction with various quote styles"""
        output = make_build_output("""
FAILED: abc "./obj/cef/libcef_static/test.o" CXX obj/cef/libcef_static/test.o
../../cef/libcef/browser/test.cc:10:5: error: test error
""")
        analyzer = BuildOutputAnalyzer('test.txt')
        analyzer.parse(output)

        file_errors = analyzer.analysis.files['cef/libcef/browser/test.cc']
        # Should strip the ./ prefix
        self.assertEqual(file_errors.build_target, 'obj/cef/libcef_static/test.o')


if __name__ == '__main__':
    unittest.main()
