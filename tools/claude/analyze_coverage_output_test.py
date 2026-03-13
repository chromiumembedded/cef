#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
"""
Unit tests for analyze_coverage_output.py

Running the tests:
  # Run all tests with verbose output
  python3 -m unittest analyze_coverage_output_test.py -v

  # Run all tests (simple)
  python3 analyze_coverage_output_test.py

  # Run a specific test class
  python3 -m unittest analyze_coverage_output_test.TestCoverageAnalyzer -v

  # Run a specific test
  python3 -m unittest analyze_coverage_output_test.TestCoverageAnalyzer.test_parse_basic_coverage -v
"""

import json
import unittest
from unittest.mock import patch, MagicMock
from analyze_coverage_output import (
    CoverageAnalyzer, FileCoverage, CoverageAnalysis,
    UncoveredFunction, demangle_name, _check_cxxfilt_available,
    _demangle_with_cxxfilt, _find_undname, _demangle_msvc_with_undname
)
import analyze_coverage_output


def make_coverage_data(files=None, totals=None):
    """Helper to create coverage data structure.

    Args:
        files: List of file coverage dicts, or None for empty.
        totals: Override for totals dict, or None for defaults.

    Returns:
        Complete coverage data structure matching LLVM format.
    """
    if files is None:
        files = []

    default_totals = {
        "lines": {"covered": 100, "count": 200, "percent": 50.0},
        "functions": {"covered": 10, "count": 20, "percent": 50.0},
        "branches": {"covered": 50, "count": 100, "percent": 50.0},
        "regions": {"covered": 150, "count": 300, "percent": 50.0},
    }

    if totals:
        default_totals.update(totals)

    return {
        "data": [{
            "files": files,
            "totals": default_totals,
        }]
    }


def make_file_entry(filename, func_covered=5, func_total=5,
                    lines_covered=100, lines_total=100):
    """Helper to create a file entry in coverage data.

    Args:
        filename: The file path.
        func_covered: Number of functions covered.
        func_total: Total number of functions.
        lines_covered: Number of lines covered.
        lines_total: Total number of lines.

    Returns:
        File entry dict matching LLVM format.
    """
    func_percent = (func_covered / func_total * 100) if func_total > 0 else 0
    lines_percent = (lines_covered / lines_total * 100) if lines_total > 0 else 0

    return {
        "filename": filename,
        "summary": {
            "functions": {
                "covered": func_covered,
                "count": func_total,
                "percent": func_percent,
            },
            "lines": {
                "covered": lines_covered,
                "count": lines_total,
                "percent": lines_percent,
            },
        }
    }


class TestFileCoverage(unittest.TestCase):
    """Test cases for FileCoverage dataclass"""

    def test_functions_missing_calculation(self):
        """Test that functions_missing is calculated correctly"""
        fc = FileCoverage(
            filename="test.cc",
            functions_covered=3,
            functions_total=5,
            functions_percent=60.0,
            lines_covered=80,
            lines_total=100,
            lines_percent=80.0,
        )
        self.assertEqual(fc.functions_missing, 2)

    def test_lines_missing_calculation(self):
        """Test that lines_missing is calculated correctly"""
        fc = FileCoverage(
            filename="test.cc",
            functions_covered=5,
            functions_total=5,
            functions_percent=100.0,
            lines_covered=75,
            lines_total=100,
            lines_percent=75.0,
        )
        self.assertEqual(fc.lines_missing, 25)

    def test_zero_missing(self):
        """Test with full coverage"""
        fc = FileCoverage(
            filename="test.cc",
            functions_covered=10,
            functions_total=10,
            functions_percent=100.0,
            lines_covered=200,
            lines_total=200,
            lines_percent=100.0,
        )
        self.assertEqual(fc.functions_missing, 0)
        self.assertEqual(fc.lines_missing, 0)


class TestPathNormalization(unittest.TestCase):
    """Test cases for path normalization"""

    def test_normalize_with_explicit_prefix(self):
        """Test normalization with explicitly provided prefix"""
        analyzer = CoverageAnalyzer(strip_prefix="/home/user/src/")
        path = "/home/user/src/cef/test.cc"
        normalized = analyzer.normalize_path(path)
        self.assertEqual(normalized, "cef/test.cc")

    def test_normalize_windows_path_with_prefix(self):
        """Test normalization of Windows path with prefix"""
        analyzer = CoverageAnalyzer(strip_prefix="D:\\code\\src\\")
        path = "D:\\code\\src\\cef\\libcef\\browser\\test.cc"
        normalized = analyzer.normalize_path(path)
        self.assertEqual(normalized, "cef/libcef/browser/test.cc")

    def test_normalize_path_no_prefix(self):
        """Test normalization without prefix"""
        analyzer = CoverageAnalyzer()
        path = "cef/test.cc"
        normalized = analyzer.normalize_path(path)
        self.assertEqual(normalized, "cef/test.cc")

    def test_backslash_conversion(self):
        """Test that backslashes are converted to forward slashes"""
        analyzer = CoverageAnalyzer()
        path = "cef\\libcef\\browser\\test.cc"
        normalized = analyzer.normalize_path(path)
        self.assertEqual(normalized, "cef/libcef/browser/test.cc")

    def test_remove_leading_slash(self):
        """Test that leading slash is removed"""
        analyzer = CoverageAnalyzer(strip_prefix="/home/user/")
        path = "/home/user/test.cc"
        normalized = analyzer.normalize_path(path)
        self.assertEqual(normalized, "test.cc")


class TestPrefixAutoDetection(unittest.TestCase):
    """Test cases for automatic prefix detection"""

    def test_detect_common_prefix(self):
        """Test that common prefix is detected from multiple files"""
        data = make_coverage_data(
            files=[
                make_file_entry("/home/user/chromium/src/cef/file1.cc"),
                make_file_entry("/home/user/chromium/src/cef/file2.cc"),
                make_file_entry("/home/user/chromium/src/base/file3.cc"),
            ]
        )

        analyzer = CoverageAnalyzer()
        detected = analyzer._detect_prefix(data)

        # Normalize for comparison (Windows returns backslashes)
        detected_normalized = detected.replace('\\', '/')

        # Should detect /home/user/chromium/src/ or similar as common
        self.assertIn("home/user/chromium", detected_normalized)

    def test_detect_prefix_applied_when_parsing(self):
        """Test that detected prefix is applied when parsing"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/file1.cc", func_covered=3, func_total=5),
                make_file_entry("/src/cef/file2.cc", func_covered=3, func_total=5),
            ]
        )

        analyzer = CoverageAnalyzer()
        analyzer.parse(data)

        # Files should have /src/ stripped
        filenames = [f.filename for f in analyzer.analysis.implementation_files]
        self.assertTrue(all(not f.startswith('/src/') for f in filenames))

    def test_detect_empty_data(self):
        """Test detection with no files"""
        data = make_coverage_data(files=[])

        analyzer = CoverageAnalyzer()
        prefix = analyzer._detect_prefix(data)
        self.assertEqual(prefix, '')


class TestTestFileDetection(unittest.TestCase):
    """Test cases for test file detection"""

    def test_unittest_file(self):
        """Test detection of _unittest.cc files"""
        analyzer = CoverageAnalyzer()
        self.assertTrue(analyzer.is_test_file("cef/libcef/browser/test_unittest.cc"))

    def test_test_cc_file(self):
        """Test detection of _test.cc files"""
        analyzer = CoverageAnalyzer()
        self.assertTrue(analyzer.is_test_file("cef/libcef/browser/my_test.cc"))

    def test_regular_file(self):
        """Test that regular files are not detected as tests"""
        analyzer = CoverageAnalyzer()
        self.assertFalse(analyzer.is_test_file("cef/libcef/browser/browser.cc"))

    def test_file_with_test_in_name(self):
        """Test file with 'test' in name but not as suffix"""
        analyzer = CoverageAnalyzer()
        self.assertFalse(analyzer.is_test_file("cef/libcef/browser/test_helper.cc"))


class TestPathFiltering(unittest.TestCase):
    """Test cases for path filtering"""

    def test_filter_matches(self):
        """Test that filter correctly matches files"""
        analyzer = CoverageAnalyzer(path_filter="cef/libcef")
        self.assertTrue(analyzer.matches_filter("cef/libcef/browser/test.cc"))
        self.assertTrue(analyzer.matches_filter("cef/libcef_dll/test.cc"))
        self.assertFalse(analyzer.matches_filter("base/test.cc"))
        self.assertFalse(analyzer.matches_filter("chrome/browser/test.cc"))

    def test_no_filter(self):
        """Test that no filter matches all files"""
        analyzer = CoverageAnalyzer()
        self.assertTrue(analyzer.matches_filter("anything.cc"))

    def test_filter_applied_to_files(self):
        """Test that filter is applied when parsing"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/file1.cc", func_covered=3, func_total=5),
                make_file_entry("/src/base/file2.cc", func_covered=3, func_total=5),
                make_file_entry("/src/cef/file3.cc", func_covered=3, func_total=5),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/", path_filter="cef/")
        analyzer.parse(data)

        # Only cef files should be included
        self.assertEqual(len(analyzer.analysis.incomplete_function_files), 2)
        filenames = [f.filename for f in analyzer.analysis.incomplete_function_files]
        self.assertTrue(all(f.startswith("cef/") for f in filenames))


class TestCoverageAnalyzer(unittest.TestCase):
    """Test cases for CoverageAnalyzer"""

    def test_parse_basic_coverage(self):
        """Test parsing basic coverage data"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/test.cc",
                                func_covered=5, func_total=5,
                                lines_covered=100, lines_total=100),
            ],
            totals={
                "lines": {"covered": 100, "count": 100, "percent": 100.0},
                "functions": {"covered": 5, "count": 5, "percent": 100.0},
                "branches": {"covered": 20, "count": 20, "percent": 100.0},
                "regions": {"covered": 50, "count": 50, "percent": 100.0},
            }
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        self.assertEqual(analyzer.analysis.total_lines_covered, 100)
        self.assertEqual(analyzer.analysis.total_lines_count, 100)
        self.assertEqual(analyzer.analysis.total_functions_covered, 5)
        self.assertEqual(analyzer.analysis.total_functions_count, 5)

    def test_parse_incomplete_function_coverage(self):
        """Test parsing files with incomplete function coverage"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/full.cc", func_covered=5, func_total=5),
                make_file_entry("/src/cef/partial.cc", func_covered=3, func_total=5),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        self.assertEqual(len(analyzer.analysis.incomplete_function_files), 1)
        self.assertEqual(
            analyzer.analysis.incomplete_function_files[0].filename,
            "cef/partial.cc")
        self.assertEqual(
            analyzer.analysis.incomplete_function_files[0].functions_missing, 2)

    def test_skip_test_files(self):
        """Test that test files are skipped"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/test.cc", func_covered=3, func_total=5),
                make_file_entry("/src/cef/test_unittest.cc", func_covered=2, func_total=5),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        # Only test.cc should be in incomplete files, not the unittest
        self.assertEqual(len(analyzer.analysis.incomplete_function_files), 1)
        self.assertEqual(
            analyzer.analysis.incomplete_function_files[0].filename,
            "cef/test.cc")

    def test_skip_files_with_no_functions(self):
        """Test that files with no functions are skipped"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/header.h",
                                func_covered=0, func_total=0,
                                lines_covered=0, lines_total=10),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        self.assertEqual(len(analyzer.analysis.incomplete_function_files), 0)
        self.assertEqual(len(analyzer.analysis.implementation_files), 0)

    def test_sorting_by_missing_functions(self):
        """Test that incomplete files are sorted by missing functions descending"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/few_missing.cc",
                                func_covered=4, func_total=5),  # 1 missing
                make_file_entry("/src/cef/many_missing.cc",
                                func_covered=2, func_total=10),  # 8 missing
                make_file_entry("/src/cef/some_missing.cc",
                                func_covered=5, func_total=8),  # 3 missing
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        files = analyzer.analysis.incomplete_function_files
        self.assertEqual(len(files), 3)
        self.assertEqual(files[0].functions_missing, 8)  # many_missing.cc
        self.assertEqual(files[1].functions_missing, 3)  # some_missing.cc
        self.assertEqual(files[2].functions_missing, 1)  # few_missing.cc

    def test_sorting_implementation_files_by_coverage(self):
        """Test that implementation files are sorted by line coverage ascending"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/high.cc",
                                lines_covered=90, lines_total=100),  # 90%
                make_file_entry("/src/cef/low.cc",
                                lines_covered=50, lines_total=100),  # 50%
                make_file_entry("/src/cef/medium.cc",
                                lines_covered=75, lines_total=100),  # 75%
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        files = analyzer.analysis.implementation_files
        self.assertEqual(len(files), 3)
        self.assertEqual(files[0].lines_percent, 50.0)  # low.cc first
        self.assertEqual(files[1].lines_percent, 75.0)  # medium.cc
        self.assertEqual(files[2].lines_percent, 90.0)  # high.cc last

    def test_empty_coverage_data(self):
        """Test parsing empty coverage data"""
        data = make_coverage_data(files=[])

        analyzer = CoverageAnalyzer()
        analyzer.parse(data)

        self.assertEqual(len(analyzer.analysis.incomplete_function_files), 0)
        self.assertEqual(len(analyzer.analysis.implementation_files), 0)


class TestJsonReport(unittest.TestCase):
    """Test cases for JSON report generation"""

    def test_json_report_structure(self):
        """Test that JSON report has correct structure"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/test.cc", func_covered=3, func_total=5),
            ],
            totals={
                "lines": {"covered": 80, "count": 100, "percent": 80.0},
                "functions": {"covered": 3, "count": 5, "percent": 60.0},
                "branches": {"covered": 40, "count": 80, "percent": 50.0},
                "regions": {"covered": 100, "count": 150, "percent": 66.7},
            }
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        json_output = analyzer.generate_json_report()
        report = json.loads(json_output)

        # Check totals structure
        self.assertIn("totals", report)
        self.assertIn("lines", report["totals"])
        self.assertIn("functions", report["totals"])
        self.assertIn("branches", report["totals"])
        self.assertIn("regions", report["totals"])

        # Check totals values
        self.assertEqual(report["totals"]["lines"]["covered"], 80)
        self.assertEqual(report["totals"]["functions"]["percent"], 60.0)

        # Check incomplete files
        self.assertIn("incomplete_function_files", report)
        self.assertEqual(len(report["incomplete_function_files"]), 1)

        file_data = report["incomplete_function_files"][0]
        self.assertEqual(file_data["filename"], "cef/test.cc")
        self.assertEqual(file_data["functions_missing"], 2)

        # Check statistics
        self.assertIn("statistics", report)
        self.assertEqual(report["statistics"]["total_incomplete_files"], 1)
        self.assertEqual(report["statistics"]["total_missing_functions"], 2)

    def test_json_report_empty(self):
        """Test JSON report with no incomplete files"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/full.cc", func_covered=5, func_total=5),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        json_output = analyzer.generate_json_report()
        report = json.loads(json_output)

        self.assertEqual(len(report["incomplete_function_files"]), 0)
        self.assertEqual(report["statistics"]["total_incomplete_files"], 0)
        self.assertEqual(report["statistics"]["total_missing_functions"], 0)

    def test_json_report_includes_filter(self):
        """Test that JSON report includes filter when set"""
        data = make_coverage_data(files=[])

        analyzer = CoverageAnalyzer(path_filter="cef/libcef")
        analyzer.parse(data)

        json_output = analyzer.generate_json_report()
        report = json.loads(json_output)

        self.assertIn("filter", report)
        self.assertEqual(report["filter"], "cef/libcef")


class TestSummaryReport(unittest.TestCase):
    """Test cases for summary report generation"""

    def test_summary_report_contains_totals(self):
        """Test that summary report contains overall totals"""
        data = make_coverage_data(
            totals={
                "lines": {"covered": 7980, "count": 9275, "percent": 86.0},
                "functions": {"covered": 778, "count": 813, "percent": 95.7},
                "branches": {"covered": 5182, "count": 9746, "percent": 53.2},
                "regions": {"covered": 23028, "count": 26263, "percent": 87.7},
            }
        )

        analyzer = CoverageAnalyzer()
        analyzer.parse(data)

        summary = analyzer.generate_summary_report(colorize=False)

        self.assertIn("OVERALL COVERAGE SUMMARY", summary)
        self.assertIn("7980", summary)
        self.assertIn("9275", summary)
        self.assertIn("86.0%", summary)
        self.assertIn("778", summary)
        self.assertIn("813", summary)

    def test_summary_report_contains_incomplete_files(self):
        """Test that summary report lists incomplete files"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/partial.cc", func_covered=3, func_total=5),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        summary = analyzer.generate_summary_report(colorize=False)

        self.assertIn("FILES WITH LESS THAN 100% FUNCTION COVERAGE", summary)
        self.assertIn("cef/partial.cc", summary)
        self.assertIn("3/5", summary)
        self.assertIn("Missing: 2 function(s)", summary)

    def test_summary_report_contains_line_coverage(self):
        """Test that summary report contains line coverage section"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/test.cc",
                                lines_covered=80, lines_total=100),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        summary = analyzer.generate_summary_report(colorize=False)

        self.assertIn("LINE COVERAGE SUMMARY", summary)
        self.assertIn("test.cc", summary)
        self.assertIn("80/100", summary)
        self.assertIn("80.0%", summary)

    def test_summary_report_shows_filter(self):
        """Test that summary report shows filter when set"""
        data = make_coverage_data(files=[])

        analyzer = CoverageAnalyzer(path_filter="cef/libcef")
        analyzer.parse(data)

        summary = analyzer.generate_summary_report(colorize=False)

        self.assertIn("filtered to: cef/libcef", summary)


class TestStatistics(unittest.TestCase):
    """Test cases for get_statistics method"""

    def test_statistics_values(self):
        """Test that statistics returns correct values"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/file1.cc",
                                func_covered=3, func_total=5),  # 2 missing
                make_file_entry("/src/cef/file2.cc",
                                func_covered=7, func_total=10),  # 3 missing
            ],
            totals={
                "lines": {"covered": 150, "count": 200, "percent": 75.0},
                "functions": {"covered": 10, "count": 15, "percent": 66.7},
                "branches": {"covered": 50, "count": 100, "percent": 50.0},
                "regions": {"covered": 100, "count": 150, "percent": 66.7},
            }
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        stats = analyzer.get_statistics()

        self.assertEqual(stats["total_lines_covered"], 150)
        self.assertEqual(stats["total_lines_count"], 200)
        self.assertEqual(stats["total_lines_percent"], 75.0)
        self.assertEqual(stats["total_functions_covered"], 10)
        self.assertEqual(stats["total_functions_count"], 15)
        self.assertEqual(stats["incomplete_files_count"], 2)
        self.assertEqual(stats["total_missing_functions"], 5)  # 2 + 3


class TestRealWorldSamples(unittest.TestCase):
    """Test cases based on real coverage data samples"""

    def test_chromium_style_paths(self):
        """Test parsing with Chromium-style paths"""
        data = make_coverage_data(
            files=[
                make_file_entry(
                    "/home/user/chromium/src/cef/libcef_dll/bootstrap/installer/installer_config.cc",
                    func_covered=3, func_total=5,
                    lines_covered=85, lines_total=131),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/home/user/chromium/src/")
        analyzer.parse(data)

        self.assertEqual(len(analyzer.analysis.incomplete_function_files), 1)
        file = analyzer.analysis.incomplete_function_files[0]
        self.assertEqual(file.filename,
                         "cef/libcef_dll/bootstrap/installer/installer_config.cc")
        self.assertEqual(file.functions_missing, 2)

    def test_multiple_component_files(self):
        """Test parsing multiple files from different components"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/libcef/browser/file1.cc",
                                func_covered=3, func_total=5),
                make_file_entry("/src/base/memory/file2.cc",
                                func_covered=6, func_total=7),
                make_file_entry("/src/chrome/browser/file3.cc",
                                func_covered=29, func_total=39),
                make_file_entry("/src/content/browser/file4.cc",
                                func_covered=27, func_total=36),
            ],
            totals={
                "lines": {"covered": 7980, "count": 9275, "percent": 86.0},
                "functions": {"covered": 778, "count": 813, "percent": 95.7},
                "branches": {"covered": 5182, "count": 9746, "percent": 53.2},
                "regions": {"covered": 23028, "count": 26263, "percent": 87.7},
            }
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        # All 4 files have incomplete coverage
        self.assertEqual(len(analyzer.analysis.incomplete_function_files), 4)

        # Should be sorted by missing functions
        files = analyzer.analysis.incomplete_function_files
        self.assertEqual(files[0].functions_missing, 10)  # chrome (39-29)
        self.assertEqual(files[1].functions_missing, 9)   # content (36-27)
        self.assertEqual(files[2].functions_missing, 2)   # cef (5-3)
        self.assertEqual(files[3].functions_missing, 1)   # base (7-6)


class TestEdgeCases(unittest.TestCase):
    """Test edge cases and error handling"""

    def test_zero_percent_coverage(self):
        """Test file with 0% coverage"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/empty.cc",
                                func_covered=0, func_total=10,
                                lines_covered=0, lines_total=100),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        self.assertEqual(len(analyzer.analysis.incomplete_function_files), 1)
        file = analyzer.analysis.incomplete_function_files[0]
        self.assertEqual(file.functions_missing, 10)
        self.assertEqual(file.lines_missing, 100)

    def test_100_percent_coverage(self):
        """Test file with 100% coverage"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/full.cc",
                                func_covered=10, func_total=10,
                                lines_covered=100, lines_total=100),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        # Should not appear in incomplete files
        self.assertEqual(len(analyzer.analysis.incomplete_function_files), 0)
        # But should appear in implementation files
        self.assertEqual(len(analyzer.analysis.implementation_files), 1)

    def test_single_function_file(self):
        """Test file with single function"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/single.cc",
                                func_covered=0, func_total=1),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        self.assertEqual(len(analyzer.analysis.incomplete_function_files), 1)
        self.assertEqual(
            analyzer.analysis.incomplete_function_files[0].functions_missing, 1)

    def test_large_numbers(self):
        """Test with large coverage numbers"""
        data = make_coverage_data(
            totals={
                "lines": {"covered": 1000000, "count": 1500000, "percent": 66.67},
                "functions": {"covered": 50000, "count": 60000, "percent": 83.33},
                "branches": {"covered": 200000, "count": 400000, "percent": 50.0},
                "regions": {"covered": 800000, "count": 1000000, "percent": 80.0},
            }
        )

        analyzer = CoverageAnalyzer()
        analyzer.parse(data)

        self.assertEqual(analyzer.analysis.total_lines_covered, 1000000)
        self.assertEqual(analyzer.analysis.total_functions_count, 60000)


class TestDemangleName(unittest.TestCase):
    """Test cases for MSVC name demangling"""

    def setUp(self):
        """Reset undname cache before each test."""
        analyze_coverage_output._undname_path = None
        analyze_coverage_output._undname_checked = False

    def tearDown(self):
        """Reset undname cache after each test."""
        analyze_coverage_output._undname_path = None
        analyze_coverage_output._undname_checked = False

    def test_msvc_simple_function(self):
        """Test demangling simple MSVC function name"""
        mangled = "?MyFunction@MyClass@@QEAAXXZ"
        result = demangle_name(mangled)
        # Should extract MyFunction and MyClass
        self.assertIn("MyFunction", result)

    def test_msvc_namespace_function(self):
        """Test demangling MSVC function with namespace"""
        mangled = "?DoSomething@Helper@Utils@@QEAAXXZ"
        result = demangle_name(mangled)
        # Should extract namespace hierarchy
        self.assertIn("DoSomething", result)

    @patch('analyze_coverage_output._find_undname')
    def test_msvc_internal_name_skipped(self, mock_find):
        """Test that internal names starting with _ are skipped (heuristic fallback)"""
        mock_find.return_value = None  # Force heuristic path
        mangled = "?_InternalFunc@Class@@QEAAXXZ"
        result = demangle_name(mangled)
        # Internal names should return original
        self.assertEqual(result, mangled)

    @patch('analyze_coverage_output._find_undname')
    def test_msvc_compiler_generated_skipped(self, mock_find):
        """Test that compiler-generated names starting with $ are skipped (heuristic fallback)"""
        mock_find.return_value = None  # Force heuristic path
        mangled = "?$GeneratedFunc@Class@@QEAAXXZ"
        result = demangle_name(mangled)
        # Compiler-generated names should return original
        self.assertEqual(result, mangled)

    @patch('analyze_coverage_output._check_cxxfilt_available')
    def test_gcc_clang_mangled_returned_as_is(self, mock_check):
        """Test that GCC/Clang mangled names are returned as-is when c++filt unavailable"""
        mock_check.return_value = False  # Simulate c++filt not available
        mangled = "_ZN9MyClass10MyFunctionEv"
        result = demangle_name(mangled)
        # Without c++filt, return as-is
        self.assertEqual(result, mangled)

    def test_unmangled_name_returned_as_is(self):
        """Test that unmangled names are returned as-is"""
        name = "simple_c_function"
        result = demangle_name(name)
        self.assertEqual(result, name)

    def test_empty_string(self):
        """Test empty string handling"""
        result = demangle_name("")
        self.assertEqual(result, "")

    @patch('analyze_coverage_output._check_cxxfilt_available')
    @patch('analyze_coverage_output._demangle_with_cxxfilt')
    def test_gcc_mangled_with_cxxfilt(self, mock_demangle, mock_check):
        """Test GCC/Clang demangling when c++filt is available"""
        mock_check.return_value = True
        mock_demangle.return_value = ["MyNamespace::MyFunction()"]

        mangled = "_ZN11MyNamespace10MyFunctionEv"
        result = demangle_name(mangled)

        mock_demangle.assert_called_once_with([mangled])
        self.assertEqual(result, "MyNamespace::MyFunction()")

    @patch('analyze_coverage_output._check_cxxfilt_available')
    def test_gcc_mangled_without_cxxfilt(self, mock_check):
        """Test GCC/Clang demangling when c++filt is not available"""
        mock_check.return_value = False

        mangled = "_ZN11MyNamespace10MyFunctionEv"
        result = demangle_name(mangled)

        # Should return original when c++filt not available
        self.assertEqual(result, mangled)


class TestUndnameIntegration(unittest.TestCase):
    """Test cases for undname.exe integration (MSVC demangling)"""

    def setUp(self):
        """Reset cached values before each test."""
        analyze_coverage_output._undname_path = None
        analyze_coverage_output._undname_checked = False

    def tearDown(self):
        """Reset cached values after each test."""
        analyze_coverage_output._undname_path = None
        analyze_coverage_output._undname_checked = False

    @patch('analyze_coverage_output.glob_module.glob')
    def test_find_undname_success(self, mock_glob):
        """Test finding undname.exe in VS directory"""
        mock_glob.return_value = [
            "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/MSVC/14.38.33130/bin/Hostx64/x64/undname.exe"
        ]

        result = _find_undname()

        self.assertIsNotNone(result)
        self.assertIn("undname.exe", result)

    @patch('analyze_coverage_output.glob_module.glob')
    def test_find_undname_not_found(self, mock_glob):
        """Test when undname.exe is not found"""
        mock_glob.return_value = []

        result = _find_undname()

        self.assertIsNone(result)

    @patch('analyze_coverage_output.glob_module.glob')
    def test_find_undname_caches_result(self, mock_glob):
        """Test that undname search result is cached"""
        mock_glob.return_value = [
            "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/MSVC/14.38/bin/Hostx64/x64/undname.exe"
        ]

        result1 = _find_undname()
        result2 = _find_undname()

        self.assertEqual(result1, result2)
        # glob should only be called once due to caching
        # (it may be called multiple times for different patterns on first search,
        # but not on second search)
        first_call_count = mock_glob.call_count
        _find_undname()
        self.assertEqual(mock_glob.call_count, first_call_count)

    @patch('analyze_coverage_output.glob_module.glob')
    def test_find_undname_selects_latest_version(self, mock_glob):
        """Test that latest MSVC version is selected"""
        mock_glob.return_value = [
            "C:/VS/2022/Pro/VC/Tools/MSVC/14.35/bin/Hostx64/x64/undname.exe",
            "C:/VS/2022/Pro/VC/Tools/MSVC/14.38/bin/Hostx64/x64/undname.exe",
            "C:/VS/2022/Pro/VC/Tools/MSVC/14.36/bin/Hostx64/x64/undname.exe",
        ]

        result = _find_undname()

        # Should select 14.38 (latest when sorted in reverse)
        self.assertIn("14.38", result)

    @patch('analyze_coverage_output._find_undname')
    @patch('analyze_coverage_output.subprocess.run')
    def test_demangle_msvc_with_undname_success(self, mock_run, mock_find):
        """Test successful MSVC demangling with undname"""
        mock_find.return_value = "C:/path/to/undname.exe"
        # Real undname.exe output format with 0x1000 (NAME_ONLY) flag
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout='''Microsoft (R) C++ Name Undecorator
Copyright (C) Microsoft Corporation. All rights reserved.

Undecoration of :- "?MyFunc@MyClass@@QEAAXXZ"
is :- "MyClass::MyFunc"
'''
        )

        result = _demangle_msvc_with_undname("?MyFunc@MyClass@@QEAAXXZ")

        self.assertEqual(result, "MyClass::MyFunc")
        # Verify 0x1000 flag is passed
        call_args = mock_run.call_args[0][0]
        self.assertIn('0x1000', call_args)

    @patch('analyze_coverage_output._find_undname')
    @patch('analyze_coverage_output.subprocess.run')
    def test_demangle_msvc_with_undname_unquoted_output(self, mock_run, mock_find):
        """Test MSVC demangling with unquoted output format"""
        mock_find.return_value = "C:/path/to/undname.exe"
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout='''Undecoration of :- "?MyFunc@@"
is :- MyFunc
'''
        )

        result = _demangle_msvc_with_undname("?MyFunc@@")

        self.assertEqual(result, "MyFunc")

    @patch('analyze_coverage_output._find_undname')
    def test_demangle_msvc_without_undname(self, mock_find):
        """Test MSVC demangling when undname is not available"""
        mock_find.return_value = None

        mangled = "?MyFunc@MyClass@@QEAAXXZ"
        result = _demangle_msvc_with_undname(mangled)

        # Should return original when undname not available
        self.assertEqual(result, mangled)

    @patch('analyze_coverage_output._find_undname')
    @patch('analyze_coverage_output.subprocess.run')
    def test_demangle_msvc_undname_failure(self, mock_run, mock_find):
        """Test MSVC demangling when undname fails"""
        mock_find.return_value = "C:/path/to/undname.exe"
        mock_run.return_value = MagicMock(returncode=1, stdout="")

        mangled = "?MyFunc@MyClass@@QEAAXXZ"
        result = _demangle_msvc_with_undname(mangled)

        # Should return original on failure
        self.assertEqual(result, mangled)

    @patch('analyze_coverage_output._find_undname')
    @patch('analyze_coverage_output._demangle_msvc_with_undname')
    def test_demangle_name_uses_undname(self, mock_demangle, mock_find):
        """Test that demangle_name uses undname when available"""
        mock_find.return_value = "C:/path/to/undname.exe"
        mock_demangle.return_value = "void MyClass::MyFunc(void)"

        result = demangle_name("?MyFunc@MyClass@@QEAAXXZ")

        mock_demangle.assert_called_once()
        self.assertEqual(result, "void MyClass::MyFunc(void)")


class TestUndnameIntegrationReal(unittest.TestCase):
    """Integration tests for undname.exe on Windows - tests actual behavior without mocking.

    These tests verify that MSVC symbol demangling works correctly on Windows.
    They will be skipped if undname.exe is not available or not on Windows.
    """

    def setUp(self):
        """Reset cached undname values before each test."""
        analyze_coverage_output._undname_path = None
        analyze_coverage_output._undname_checked = False

    def tearDown(self):
        """Reset cached undname values after each test."""
        analyze_coverage_output._undname_path = None
        analyze_coverage_output._undname_checked = False

    def test_real_undname_demangles_msvc_names_on_windows(self):
        """Test that MSVC mangled names are demangled correctly on Windows."""
        import sys
        if sys.platform != 'win32':
            self.skipTest("This test is specific to Windows")

        if not _find_undname():
            self.skipTest("undname.exe not available")

        # Test MSVC mangled names
        test_cases = [
            # Simple class method
            ("?MyFunction@MyClass@@QEAAXXZ", "MyClass::MyFunction"),
            # Nested namespace
            ("?MyFunction@MyClass@MyNamespace@@QEAAXXZ",
             "MyNamespace::MyClass::MyFunction"),
        ]

        for mangled, expected in test_cases:
            with self.subTest(mangled=mangled):
                result = _demangle_msvc_with_undname(mangled)
                self.assertEqual(result, expected,
                    f"Failed to demangle {mangled}: got {result}")

    def test_real_undname_via_demangle_name_on_windows(self):
        """Test that demangle_name uses undname correctly on Windows."""
        import sys
        if sys.platform != 'win32':
            self.skipTest("This test is specific to Windows")

        if not _find_undname():
            self.skipTest("undname.exe not available")

        # Test via the main demangle_name function
        mangled = "?MyFunction@MyClass@@QEAAXXZ"
        result = demangle_name(mangled)

        # Should be demangled, not the heuristic fallback
        self.assertIn("MyClass", result)
        self.assertIn("MyFunction", result)


class TestCxxfiltIntegration(unittest.TestCase):
    """Test cases for c++filt integration"""

    @patch('analyze_coverage_output.subprocess.run')
    def test_check_cxxfilt_available_success(self, mock_run):
        """Test c++filt availability check when present"""
        # Reset cached value
        analyze_coverage_output._cxxfilt_available = None

        mock_run.return_value = MagicMock(returncode=0)
        result = _check_cxxfilt_available()

        self.assertTrue(result)

    @patch('analyze_coverage_output.subprocess.run')
    def test_check_cxxfilt_available_not_found(self, mock_run):
        """Test c++filt availability check when not present"""
        # Reset cached value
        analyze_coverage_output._cxxfilt_available = None

        mock_run.side_effect = FileNotFoundError()
        result = _check_cxxfilt_available()

        self.assertFalse(result)

    @patch('analyze_coverage_output.subprocess.run')
    def test_check_cxxfilt_caches_result(self, mock_run):
        """Test that c++filt check result is cached"""
        # Reset cached value
        analyze_coverage_output._cxxfilt_available = None

        mock_run.return_value = MagicMock(returncode=0)

        # First call
        result1 = _check_cxxfilt_available()
        # Second call should use cache
        result2 = _check_cxxfilt_available()

        self.assertTrue(result1)
        self.assertTrue(result2)
        # Should only call subprocess once
        self.assertEqual(mock_run.call_count, 1)

        # Reset for other tests
        analyze_coverage_output._cxxfilt_available = None

    @patch('analyze_coverage_output.subprocess.run')
    def test_demangle_with_cxxfilt_success(self, mock_run):
        """Test demangling with c++filt"""
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout="MyClass::MyFunction(int)\n"
        )

        result = _demangle_with_cxxfilt(["_ZN7MyClass10MyFunctionEi"])

        self.assertEqual(result, ["MyClass::MyFunction(int)"])

    @patch('analyze_coverage_output.subprocess.run')
    def test_demangle_with_cxxfilt_multiple(self, mock_run):
        """Test demangling multiple names with c++filt"""
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout="Func1()\nFunc2(int)\nFunc3(double)\n"
        )

        result = _demangle_with_cxxfilt(["_Z5Func1v", "_Z5Func2i", "_Z5Func3d"])

        self.assertEqual(result, ["Func1()", "Func2(int)", "Func3(double)"])

    @patch('analyze_coverage_output.subprocess.run')
    def test_demangle_with_cxxfilt_failure(self, mock_run):
        """Test demangling when c++filt fails"""
        mock_run.return_value = MagicMock(returncode=1)

        names = ["_ZN7MyClass10MyFunctionEi"]
        result = _demangle_with_cxxfilt(names)

        # Should return original names on failure
        self.assertEqual(result, names)

    def test_demangle_with_cxxfilt_empty_list(self):
        """Test demangling empty list"""
        result = _demangle_with_cxxfilt([])
        self.assertEqual(result, [])


class TestUncoveredFunction(unittest.TestCase):
    """Test cases for UncoveredFunction dataclass"""

    def test_str_representation(self):
        """Test string representation"""
        func = UncoveredFunction(
            name="MyFunction",
            mangled_name="?MyFunction@Class@@QEAAXXZ",
            filename="cef/test.cc",
            line=42,
        )
        self.assertEqual(str(func), "cef/test.cc:42: MyFunction")

    def test_attributes(self):
        """Test all attributes are set correctly"""
        func = UncoveredFunction(
            name="DemangleTest",
            mangled_name="mangled",
            filename="test/file.cc",
            line=100,
        )
        self.assertEqual(func.name, "DemangleTest")
        self.assertEqual(func.mangled_name, "mangled")
        self.assertEqual(func.filename, "test/file.cc")
        self.assertEqual(func.line, 100)


class TestExtractUncoveredFunctions(unittest.TestCase):
    """Test cases for extracting uncovered functions from detailed export"""

    def make_detailed_export(self, functions=None):
        """Helper to create llvm-cov export data structure."""
        if functions is None:
            functions = []
        return {
            "data": [{
                "functions": functions,
            }]
        }

    def make_function_entry(self, name, count=0, filenames=None, regions=None):
        """Helper to create a function entry."""
        if filenames is None:
            filenames = ["cef/test.cc"]
        if regions is None:
            regions = [[10, 1, 20, 1, 0, 0, 0, 0]]  # Line 10 start
        return {
            "name": name,
            "count": count,
            "filenames": filenames,
            "regions": regions,
        }

    def test_extract_uncovered_function(self):
        """Test extracting a single uncovered function"""
        data = self.make_detailed_export([
            self.make_function_entry("?MyFunc@Class@@", count=0,
                                     filenames=["/src/cef/test.cc"],
                                     regions=[[42, 1, 50, 1, 0, 0, 0, 0]]),
        ])

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        uncovered = analyzer.extract_uncovered_functions(data)

        self.assertEqual(len(uncovered), 1)
        self.assertEqual(uncovered[0].filename, "cef/test.cc")
        self.assertEqual(uncovered[0].line, 42)

    def test_skip_covered_functions(self):
        """Test that covered functions are skipped"""
        data = self.make_detailed_export([
            self.make_function_entry("?CoveredFunc@@", count=5,
                                     filenames=["/src/cef/test.cc"]),
            self.make_function_entry("?UncoveredFunc@@", count=0,
                                     filenames=["/src/cef/test.cc"]),
        ])

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        uncovered = analyzer.extract_uncovered_functions(data)

        self.assertEqual(len(uncovered), 1)
        self.assertIn("UncoveredFunc", uncovered[0].name)

    def test_skip_gtest_functions(self):
        """Test that gtest functions are skipped"""
        data = self.make_detailed_export([
            self.make_function_entry("?gtest_main@@", count=0,
                                     filenames=["/src/cef/test.cc"]),
            self.make_function_entry("?RealFunc@@", count=0,
                                     filenames=["/src/cef/test.cc"]),
        ])

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        uncovered = analyzer.extract_uncovered_functions(data)

        self.assertEqual(len(uncovered), 1)
        self.assertIn("RealFunc", uncovered[0].name)

    def test_skip_testing_namespace_functions(self):
        """Test that testing:: namespace functions are skipped"""
        data = self.make_detailed_export([
            self.make_function_entry("?TestBody@testing@@", count=0,
                                     filenames=["/src/cef/test.cc"]),
            self.make_function_entry("?RealFunc@@", count=0,
                                     filenames=["/src/cef/test.cc"]),
        ])

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        uncovered = analyzer.extract_uncovered_functions(data)

        self.assertEqual(len(uncovered), 1)

    def test_skip_test_files(self):
        """Test that functions in test files are skipped"""
        data = self.make_detailed_export([
            self.make_function_entry("?TestHelper@@", count=0,
                                     filenames=["/src/cef/test_unittest.cc"]),
            self.make_function_entry("?RealFunc@@", count=0,
                                     filenames=["/src/cef/impl.cc"]),
        ])

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        uncovered = analyzer.extract_uncovered_functions(data)

        self.assertEqual(len(uncovered), 1)
        self.assertEqual(uncovered[0].filename, "cef/impl.cc")

    def test_path_filter_applied(self):
        """Test that path filter is applied to uncovered functions"""
        data = self.make_detailed_export([
            self.make_function_entry("?BaseFunc@@", count=0,
                                     filenames=["/src/base/test.cc"]),
            self.make_function_entry("?CefFunc@@", count=0,
                                     filenames=["/src/cef/test.cc"]),
        ])

        analyzer = CoverageAnalyzer(strip_prefix="/src/", path_filter="cef/")
        uncovered = analyzer.extract_uncovered_functions(data)

        self.assertEqual(len(uncovered), 1)
        self.assertEqual(uncovered[0].filename, "cef/test.cc")

    def test_sorted_by_filename_then_line(self):
        """Test that results are sorted by filename then line number"""
        data = self.make_detailed_export([
            self.make_function_entry("?Func3@@", count=0,
                                     filenames=["/src/cef/b.cc"],
                                     regions=[[100, 1, 110, 1, 0, 0, 0, 0]]),
            self.make_function_entry("?Func1@@", count=0,
                                     filenames=["/src/cef/a.cc"],
                                     regions=[[50, 1, 60, 1, 0, 0, 0, 0]]),
            self.make_function_entry("?Func2@@", count=0,
                                     filenames=["/src/cef/a.cc"],
                                     regions=[[10, 1, 20, 1, 0, 0, 0, 0]]),
        ])

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        uncovered = analyzer.extract_uncovered_functions(data)

        self.assertEqual(len(uncovered), 3)
        self.assertEqual(uncovered[0].filename, "cef/a.cc")
        self.assertEqual(uncovered[0].line, 10)  # Func2
        self.assertEqual(uncovered[1].filename, "cef/a.cc")
        self.assertEqual(uncovered[1].line, 50)  # Func1
        self.assertEqual(uncovered[2].filename, "cef/b.cc")

    def test_handle_empty_regions(self):
        """Test handling of functions with empty regions"""
        data = self.make_detailed_export([
            self.make_function_entry("?EmptyRegions@@", count=0,
                                     filenames=["/src/cef/test.cc"],
                                     regions=[]),
        ])

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        uncovered = analyzer.extract_uncovered_functions(data)

        self.assertEqual(len(uncovered), 1)
        self.assertEqual(uncovered[0].line, 0)

    def test_handle_empty_data(self):
        """Test handling of empty export data"""
        data = self.make_detailed_export([])

        analyzer = CoverageAnalyzer()
        uncovered = analyzer.extract_uncovered_functions(data)

        self.assertEqual(len(uncovered), 0)


class TestAddUncoveredFunctionsToFiles(unittest.TestCase):
    """Test cases for linking uncovered functions to file coverage"""

    def test_add_to_matching_file(self):
        """Test adding uncovered functions to matching file"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/test.cc", func_covered=3, func_total=5),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        # Create uncovered functions
        uncovered = [
            UncoveredFunction(
                name="MissingFunc1",
                mangled_name="?MissingFunc1@@",
                filename="cef/test.cc",
                line=42,
            ),
            UncoveredFunction(
                name="MissingFunc2",
                mangled_name="?MissingFunc2@@",
                filename="cef/test.cc",
                line=100,
            ),
        ]

        analyzer.add_uncovered_functions_to_files(uncovered)

        # Check that functions were added
        file_cov = analyzer.analysis.incomplete_function_files[0]
        self.assertEqual(len(file_cov.uncovered_functions), 2)
        self.assertEqual(file_cov.uncovered_functions[0].name, "MissingFunc1")
        self.assertEqual(file_cov.uncovered_functions[1].line, 100)

    def test_multiple_files(self):
        """Test adding uncovered functions to multiple files"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/a.cc", func_covered=3, func_total=5),
                make_file_entry("/src/cef/b.cc", func_covered=2, func_total=4),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        uncovered = [
            UncoveredFunction("FuncA1", "m1", "cef/a.cc", 10),
            UncoveredFunction("FuncA2", "m2", "cef/a.cc", 20),
            UncoveredFunction("FuncB1", "m3", "cef/b.cc", 30),
        ]

        analyzer.add_uncovered_functions_to_files(uncovered)

        # Sort by filename to get consistent order
        files = sorted(analyzer.analysis.incomplete_function_files,
                       key=lambda f: f.filename)

        self.assertEqual(len(files[0].uncovered_functions), 2)  # a.cc
        self.assertEqual(len(files[1].uncovered_functions), 1)  # b.cc

    def test_no_match_for_file(self):
        """Test that unmatched files are not affected"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/test.cc", func_covered=3, func_total=5),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        # Uncovered functions for different file
        uncovered = [
            UncoveredFunction("OtherFunc", "m1", "cef/other.cc", 10),
        ]

        analyzer.add_uncovered_functions_to_files(uncovered)

        # test.cc should have no uncovered functions added
        file_cov = analyzer.analysis.incomplete_function_files[0]
        self.assertEqual(len(file_cov.uncovered_functions), 0)


class TestLoadDetailedExport(unittest.TestCase):
    """Test cases for loading detailed export via llvm-cov"""

    @patch('analyze_coverage_output.subprocess.run')
    def test_successful_load(self, mock_run):
        """Test successful load of detailed export"""
        export_data = {
            "data": [{
                "functions": [
                    {"name": "test", "count": 0, "filenames": ["t.cc"], "regions": []}
                ]
            }]
        }
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout=json.dumps(export_data),
            stderr=""
        )

        analyzer = CoverageAnalyzer()
        result = analyzer.load_detailed_export(
            profdata_path="test.profdata",
            binary_path="test.exe",
            llvm_cov_path="llvm-cov"
        )

        self.assertEqual(result, export_data)
        mock_run.assert_called_once()

    @patch('analyze_coverage_output.subprocess.run')
    def test_llvm_cov_failure(self, mock_run):
        """Test handling of llvm-cov failure"""
        mock_run.return_value = MagicMock(
            returncode=1,
            stdout="",
            stderr="error: invalid binary"
        )

        analyzer = CoverageAnalyzer()
        with self.assertRaises(RuntimeError) as ctx:
            analyzer.load_detailed_export(
                profdata_path="test.profdata",
                binary_path="test.exe",
                llvm_cov_path="llvm-cov"
            )

        self.assertIn("llvm-cov export failed", str(ctx.exception))

    @patch('analyze_coverage_output.subprocess.run')
    def test_command_construction(self, mock_run):
        """Test that correct command is constructed"""
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout='{"data":[{"functions":[]}]}',
            stderr=""
        )

        analyzer = CoverageAnalyzer()
        analyzer.load_detailed_export(
            profdata_path="coverage.profdata",
            binary_path="my_test.exe",
            llvm_cov_path="llvm-cov"
        )

        # Verify command was called with correct arguments
        call_args = mock_run.call_args[0][0]
        self.assertIn('export', call_args)
        self.assertTrue(any('coverage.profdata' in arg for arg in call_args))


class TestSummaryReportWithUncoveredFunctions(unittest.TestCase):
    """Test summary report generation with uncovered function details"""

    def test_report_includes_uncovered_function_names(self):
        """Test that summary report includes uncovered function names"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/test.cc", func_covered=3, func_total=5),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        # Add uncovered functions
        file_cov = analyzer.analysis.incomplete_function_files[0]
        file_cov.uncovered_functions = [
            UncoveredFunction("ReadConfig", "m1", "cef/test.cc", 42),
            UncoveredFunction("WriteConfig", "m2", "cef/test.cc", 100),
        ]

        report = analyzer.generate_summary_report(colorize=False)

        self.assertIn("Uncovered:", report)
        self.assertIn("ReadConfig", report)
        self.assertIn("line 42", report)
        self.assertIn("WriteConfig", report)
        self.assertIn("line 100", report)

    def test_report_without_uncovered_details(self):
        """Test report when no uncovered function details available"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/test.cc", func_covered=3, func_total=5),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        report = analyzer.generate_summary_report(colorize=False)

        # Should show missing count but not "Uncovered:" section
        self.assertIn("Missing: 2 function(s)", report)
        self.assertNotIn("Uncovered:", report)


class TestJsonReportWithUncoveredFunctions(unittest.TestCase):
    """Test JSON report generation with uncovered function details"""

    def test_json_includes_uncovered_functions(self):
        """Test that JSON report includes uncovered function details"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/test.cc", func_covered=3, func_total=5),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        # Add uncovered functions
        file_cov = analyzer.analysis.incomplete_function_files[0]
        file_cov.uncovered_functions = [
            UncoveredFunction("ReadConfig", "?ReadConfig@@", "cef/test.cc", 42),
        ]

        json_output = analyzer.generate_json_report()
        report = json.loads(json_output)

        file_data = report["incomplete_function_files"][0]
        self.assertIn("uncovered_functions", file_data)
        self.assertEqual(len(file_data["uncovered_functions"]), 1)

        func = file_data["uncovered_functions"][0]
        self.assertEqual(func["name"], "ReadConfig")
        self.assertEqual(func["line"], 42)
        self.assertEqual(func["mangled_name"], "?ReadConfig@@")

    def test_json_empty_uncovered_functions(self):
        """Test JSON report with no uncovered function details"""
        data = make_coverage_data(
            files=[
                make_file_entry("/src/cef/test.cc", func_covered=3, func_total=5),
            ]
        )

        analyzer = CoverageAnalyzer(strip_prefix="/src/")
        analyzer.parse(data)

        json_output = analyzer.generate_json_report()
        report = json.loads(json_output)

        file_data = report["incomplete_function_files"][0]
        self.assertEqual(file_data["uncovered_functions"], [])


class TestFileCoverageWithUncoveredFunctions(unittest.TestCase):
    """Test FileCoverage dataclass with uncovered functions"""

    def test_default_empty_uncovered_functions(self):
        """Test that uncovered_functions defaults to empty list"""
        fc = FileCoverage(
            filename="test.cc",
            functions_covered=3,
            functions_total=5,
            functions_percent=60.0,
            lines_covered=80,
            lines_total=100,
            lines_percent=80.0,
        )
        self.assertEqual(fc.uncovered_functions, [])

    def test_uncovered_functions_list(self):
        """Test setting uncovered_functions list"""
        funcs = [
            UncoveredFunction("Func1", "m1", "test.cc", 10),
            UncoveredFunction("Func2", "m2", "test.cc", 20),
        ]
        fc = FileCoverage(
            filename="test.cc",
            functions_covered=3,
            functions_total=5,
            functions_percent=60.0,
            lines_covered=80,
            lines_total=100,
            lines_percent=80.0,
            uncovered_functions=funcs,
        )
        self.assertEqual(len(fc.uncovered_functions), 2)
        self.assertEqual(fc.uncovered_functions[0].name, "Func1")


class TestCxxfiltIntegrationReal(unittest.TestCase):
    """Integration tests for c++filt on macOS - tests actual behavior without mocking.

    These tests verify that the Darwin underscore prefix handling works correctly.
    They will be skipped if c++filt is not available on the system.
    """

    def setUp(self):
        """Reset cached c++filt availability before each test."""
        analyze_coverage_output._cxxfilt_available = None

    def test_real_cxxfilt_demangles_itanium_names_on_darwin(self):
        """Test that Itanium ABI names are demangled correctly on macOS."""
        if not _check_cxxfilt_available():
            self.skipTest("c++filt not available")

        import sys
        if sys.platform != 'darwin':
            self.skipTest("This test is specific to macOS")

        # Test Itanium ABI mangled names (with single underscore _Z prefix)
        test_cases = [
            ("_ZN7MyClass10MyFunctionEi", "MyClass::MyFunction(int)"),
            ("_ZN11MyNamespace7MyClass10MyFunctionEi",
             "MyNamespace::MyClass::MyFunction(int)"),
        ]

        for mangled, expected in test_cases:
            with self.subTest(mangled=mangled):
                result = demangle_name(mangled)
                self.assertEqual(result, expected,
                    f"Failed to demangle {mangled}: got {result}")

    def test_real_cxxfilt_batch_demangle_on_darwin(self):
        """Test batch demangling with _demangle_with_cxxfilt on macOS."""
        if not _check_cxxfilt_available():
            self.skipTest("c++filt not available")

        import sys
        if sys.platform != 'darwin':
            self.skipTest("This test is specific to macOS")

        names = [
            "_ZN7MyClass10MyFunctionEi",
            "_ZN11MyNamespace7MyClass10MyFunctionEi",
        ]
        expected = [
            "MyClass::MyFunction(int)",
            "MyNamespace::MyClass::MyFunction(int)",
        ]

        results = _demangle_with_cxxfilt(names)
        self.assertEqual(results, expected)


class TestUncoveredLinesFromExport(unittest.TestCase):
    """Test get_uncovered_lines_from_export functionality"""

    def make_export_data(self, functions):
        """Helper to create export data structure."""
        return {"data": [{"functions": functions}]}

    def test_no_uncovered_lines(self):
        """Test file with 100% coverage returns empty list"""
        export_data = self.make_export_data([
            {
                "name": "MyFunc",
                "filenames": ["cef/test.cc"],
                "regions": [[10, 1, 20, 1, 5, 0, 0, 0]],  # exec_count=5
            }
        ])
        analyzer = CoverageAnalyzer()
        result = analyzer.get_uncovered_lines_from_export("test.cc", export_data)
        self.assertEqual(result, [])

    def test_single_uncovered_region(self):
        """Test single uncovered region"""
        export_data = self.make_export_data([
            {
                "name": "MyFunc",
                "filenames": ["cef/test.cc"],
                "regions": [[10, 1, 15, 1, 0, 0, 0, 0]],  # exec_count=0
            }
        ])
        analyzer = CoverageAnalyzer()
        result = analyzer.get_uncovered_lines_from_export("test.cc", export_data)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0][0], 10)  # start
        self.assertEqual(result[0][1], 15)  # end

    def test_multiple_uncovered_regions_merged(self):
        """Test adjacent uncovered regions are merged"""
        export_data = self.make_export_data([
            {
                "name": "MyFunc",
                "filenames": ["cef/test.cc"],
                "regions": [
                    [10, 1, 12, 1, 0, 0, 0, 0],
                    [13, 1, 15, 1, 0, 0, 0, 0],
                ],
            }
        ])
        analyzer = CoverageAnalyzer()
        result = analyzer.get_uncovered_lines_from_export("test.cc", export_data)
        self.assertEqual(len(result), 1)  # Should merge into one range
        self.assertEqual(result[0][0], 10)
        self.assertEqual(result[0][1], 15)

    def test_non_adjacent_regions_separate(self):
        """Test non-adjacent uncovered regions stay separate"""
        export_data = self.make_export_data([
            {
                "name": "MyFunc",
                "filenames": ["cef/test.cc"],
                "regions": [
                    [10, 1, 12, 1, 0, 0, 0, 0],
                    [20, 1, 22, 1, 0, 0, 0, 0],
                ],
            }
        ])
        analyzer = CoverageAnalyzer()
        result = analyzer.get_uncovered_lines_from_export("test.cc", export_data)
        self.assertEqual(len(result), 2)
        self.assertEqual(result[0], (10, 12, "Lines 10-12"))
        self.assertEqual(result[1], (20, 22, "Lines 20-22"))

    def test_file_path_matching_partial(self):
        """Test partial path matching works"""
        export_data = self.make_export_data([
            {
                "name": "MyFunc",
                "filenames": ["D:/code/chromium/src/cef/libcef/test.cc"],
                "regions": [[10, 1, 12, 1, 0, 0, 0, 0]],
            }
        ])
        analyzer = CoverageAnalyzer()
        result = analyzer.get_uncovered_lines_from_export("cef/libcef/test.cc", export_data)
        self.assertEqual(len(result), 1)

    def test_file_path_matching_with_relative(self):
        """Test matching with relative paths like ../.."""
        export_data = self.make_export_data([
            {
                "name": "MyFunc",
                "filenames": ["../../cef/libcef/test.cc"],
                "regions": [[10, 1, 12, 1, 0, 0, 0, 0]],
            }
        ])
        analyzer = CoverageAnalyzer()
        result = analyzer.get_uncovered_lines_from_export("cef/libcef/test.cc", export_data)
        self.assertEqual(len(result), 1)

    def test_branch_miss_does_not_mark_line_uncovered(self):
        """Test that a single-line branch miss is covered by enclosing region.

        A line like `if (a && b)` generates a single-line region for the
        untaken branch. The enclosing function body region (multi-line) was
        executed, so the line itself was reached — only the branch wasn't taken.
        """
        export_data = self.make_export_data([
            {
                "name": "MyFunc",
                "filenames": ["cef/test.cc"],
                "regions": [
                    [10, 1, 20, 1, 5, 0, 0, 0],   # Function body: executed 5x
                    [12, 10, 12, 20, 0, 0, 0, 0],  # Branch on line 12: never taken
                ],
            }
        ])
        analyzer = CoverageAnalyzer()
        result = analyzer.get_uncovered_lines_from_export("test.cc", export_data)
        # Line 12: single-line region (exec=0) defers to enclosing multi-line
        # region (exec=5). The line was executed.
        self.assertEqual(result, [])

    def test_inner_block_uncovered_despite_outer_executed(self):
        """Test that an unexecuted inner block is uncovered even if outer ran.

        If a function body (lines 10-20) was executed but an if-body inside it
        (lines 14-18) was never entered, lines 14-18 are genuinely uncovered.
        The inner multi-line region with exec=0 takes precedence over the
        outer region with exec>0.
        """
        export_data = self.make_export_data([
            {
                "name": "MyFunc",
                "filenames": ["cef/test.cc"],
                "regions": [
                    [10, 1, 20, 1, 5, 0, 0, 0],   # Function body: executed 5x
                    [14, 1, 18, 1, 0, 0, 0, 0],   # If-body: never entered
                ],
            }
        ])
        analyzer = CoverageAnalyzer()
        result = analyzer.get_uncovered_lines_from_export("test.cc", export_data)
        # Lines 14-18 are uncovered: the inner 5-line region (exec=0) is more
        # specific than the outer 11-line region (exec=5).
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0][0], 14)
        self.assertEqual(result[0][1], 18)

    def test_overlapping_regions_mixed_coverage(self):
        """Test inner block uncovered + separate function uncovered.

        Lines 10-15 are in a function body (executed). Lines 13-15 are also in
        an inner block that was never entered — these are uncovered despite the
        outer region being executed. Lines 20-25 are in a separate never-executed
        function — also uncovered.
        """
        export_data = self.make_export_data([
            {
                "name": "FuncA",
                "filenames": ["cef/test.cc"],
                "regions": [
                    [10, 1, 15, 1, 3, 0, 0, 0],   # Executed 3x
                    [13, 5, 15, 1, 0, 0, 0, 0],   # Inner block not entered
                ],
            },
            {
                "name": "FuncB",
                "filenames": ["cef/test.cc"],
                "regions": [
                    [20, 1, 25, 1, 0, 0, 0, 0],   # Never executed
                ],
            },
        ])
        analyzer = CoverageAnalyzer()
        result = analyzer.get_uncovered_lines_from_export("test.cc", export_data)
        # Lines 13-15: inner 3-line region (exec=0) overrides outer 6-line (exec=3)
        # Lines 20-25: only region has exec=0
        self.assertEqual(len(result), 2)
        self.assertEqual(result[0][0], 13)
        self.assertEqual(result[0][1], 15)
        self.assertEqual(result[1][0], 20)
        self.assertEqual(result[1][1], 25)

    def test_same_line_covered_by_different_functions(self):
        """Test line covered by one function but not another still counts as covered.

        An inlined header function might appear in multiple function entries.
        If any function's region covers the line, it's covered. Both regions
        have the same span, so the one with exec>0 wins.
        """
        export_data = self.make_export_data([
            {
                "name": "FuncA",
                "filenames": ["cef/test.cc"],
                "regions": [[10, 1, 12, 1, 0, 0, 0, 0]],  # Not executed
            },
            {
                "name": "FuncB",
                "filenames": ["cef/test.cc"],
                "regions": [[10, 1, 12, 1, 1, 0, 0, 0]],  # Executed 1x
            },
        ])
        analyzer = CoverageAnalyzer()
        result = analyzer.get_uncovered_lines_from_export("test.cc", export_data)
        # Lines 10-12: two regions with same span (3 lines), one has exec>0
        self.assertEqual(result, [])

    def test_standalone_single_line_uncovered(self):
        """Test that a single-line region with no enclosing region is uncovered.

        A single-line zero-count region only defers to enclosing multi-line
        regions. If there are none, the line is genuinely uncovered.
        """
        export_data = self.make_export_data([
            {
                "name": "MyFunc",
                "filenames": ["cef/test.cc"],
                "regions": [
                    [50, 1, 50, 30, 0, 0, 0, 0],  # Single line, never executed
                ],
            }
        ])
        analyzer = CoverageAnalyzer()
        result = analyzer.get_uncovered_lines_from_export("test.cc", export_data)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0][0], 50)
        self.assertEqual(result[0][1], 50)


class TestUncoveredLinesReport(unittest.TestCase):
    """Test generate_uncovered_lines_report formatting"""

    def test_empty_ranges(self):
        """Test report with no uncovered lines"""
        analyzer = CoverageAnalyzer()
        result = analyzer.generate_uncovered_lines_report("test.cc", [])
        self.assertIn("100% line coverage", result)

    def test_small_ranges(self):
        """Test report groups small ranges correctly"""
        ranges = [(10, 12, ""), (20, 20, "")]  # 3 lines, 1 line
        analyzer = CoverageAnalyzer()
        result = analyzer.generate_uncovered_lines_report("test.cc", ranges)
        self.assertIn("Small (1-5)", result)
        self.assertIn("10-12", result)
        self.assertIn("20", result)

    def test_medium_ranges(self):
        """Test report groups medium ranges correctly"""
        ranges = [(10, 20, "")]  # 11 lines
        analyzer = CoverageAnalyzer()
        result = analyzer.generate_uncovered_lines_report("test.cc", ranges)
        self.assertIn("Medium (6-20)", result)
        self.assertIn("10-20 (11)", result)

    def test_large_ranges(self):
        """Test report groups large ranges correctly"""
        ranges = [(100, 150, "")]  # 51 lines
        analyzer = CoverageAnalyzer()
        result = analyzer.generate_uncovered_lines_report("test.cc", ranges)
        self.assertIn("Large (21+)", result)
        self.assertIn("100-150 (51)", result)

    def test_mixed_ranges(self):
        """Test report with mix of small, medium, large"""
        ranges = [
            (10, 12, ""),    # small: 3
            (50, 60, ""),    # medium: 11
            (100, 200, ""),  # large: 101
        ]
        analyzer = CoverageAnalyzer()
        result = analyzer.generate_uncovered_lines_report("test.cc", ranges)
        self.assertIn("Large (21+)", result)
        self.assertIn("Medium (6-20)", result)
        self.assertIn("Small (1-5)", result)
        self.assertIn("115 uncovered lines", result)
        self.assertIn("3 ranges", result)


class TestGlobPatternExpansion(unittest.TestCase):
    """Test glob pattern expansion for --show-uncovered-lines"""

    def test_pattern_detection_asterisk(self):
        """Test that asterisk is detected as glob pattern"""
        pattern = "cef/libcef_dll/bootstrap/installer/*.cc"
        self.assertIn('*', pattern)

    def test_pattern_detection_question(self):
        """Test that question mark is detected as glob pattern"""
        pattern = "cef/libcef_dll/bootstrap/installer/installer_?.cc"
        self.assertIn('?', pattern)

    def test_filter_test_files(self):
        """Test that test files are filtered out from glob results"""
        files = [
            "cef/installer/installer_config.cc",
            "cef/installer/installer_config_unittest.cc",
            "cef/installer/installer_test.cc",
            "cef/installer/installer_archive.cc",
        ]
        # Filter logic from main()
        filtered = [f for f in files
                   if (f.endswith('.cc') or f.endswith('.cpp') or f.endswith('.h'))
                   and '_unittest' not in f and '_test.cc' not in f]
        self.assertEqual(len(filtered), 2)
        self.assertIn("cef/installer/installer_config.cc", filtered)
        self.assertIn("cef/installer/installer_archive.cc", filtered)
        self.assertNotIn("cef/installer/installer_config_unittest.cc", filtered)
        self.assertNotIn("cef/installer/installer_test.cc", filtered)

    def test_filter_keeps_header_files(self):
        """Test that header files are kept in glob results"""
        files = [
            "cef/installer/installer_config.h",
            "cef/installer/installer_config.cc",
        ]
        filtered = [f for f in files
                   if (f.endswith('.cc') or f.endswith('.cpp') or f.endswith('.h'))
                   and '_unittest' not in f and '_test.cc' not in f]
        self.assertEqual(len(filtered), 2)

    def test_filter_excludes_non_source_files(self):
        """Test that non-source files are excluded"""
        files = [
            "cef/installer/installer_config.cc",
            "cef/installer/README.md",
            "cef/installer/BUILD.gn",
        ]
        filtered = [f for f in files
                   if (f.endswith('.cc') or f.endswith('.cpp') or f.endswith('.h'))
                   and '_unittest' not in f and '_test.cc' not in f]
        self.assertEqual(len(filtered), 1)
        self.assertEqual(filtered[0], "cef/installer/installer_config.cc")


if __name__ == '__main__':
    unittest.main()
