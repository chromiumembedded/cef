#!/usr/bin/env python3
# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.
"""Analyze LLVM coverage reports.

This tool parses LLVM coverage summary.json files and reports on function
and line coverage, highlighting files that have less than 100% function
coverage.

Usage:
    python analyze_coverage_output.py <summary.json>
    python analyze_coverage_output.py summary.json --filter cef/libcef_dll
    python analyze_coverage_output.py summary.json --json
    python analyze_coverage_output.py summary.json --strip-prefix /path/to/src/

Options:
    --filter PATH       Only include files matching this path prefix
    --strip-prefix PATH Path prefix to strip from filenames for display
                        (auto-detected if not specified)
    --json              Output results as JSON instead of human-readable text
    --help              Show this help message
"""

import argparse
import glob as glob_module
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional, Tuple


@dataclass
class UncoveredFunction:
    """An uncovered function with location information."""
    name: str  # Demangled function name
    mangled_name: str  # Original mangled name
    filename: str  # Source file
    line: int  # Starting line number

    def __str__(self) -> str:
        return f"{self.filename}:{self.line}: {self.name}"


@dataclass
class FileCoverage:
    """Coverage data for a single file."""
    filename: str
    functions_covered: int
    functions_total: int
    functions_percent: float
    lines_covered: int
    lines_total: int
    lines_percent: float
    uncovered_functions: List[UncoveredFunction] = field(default_factory=list)

    @property
    def functions_missing(self) -> int:
        return self.functions_total - self.functions_covered

    @property
    def lines_missing(self) -> int:
        return self.lines_total - self.lines_covered


@dataclass
class CoverageAnalysis:
    """Complete coverage analysis results."""
    # Overall totals
    total_lines_covered: int = 0
    total_lines_count: int = 0
    total_lines_percent: float = 0.0
    total_functions_covered: int = 0
    total_functions_count: int = 0
    total_functions_percent: float = 0.0
    total_branches_covered: int = 0
    total_branches_count: int = 0
    total_branches_percent: float = 0.0
    total_regions_covered: int = 0
    total_regions_count: int = 0
    total_regions_percent: float = 0.0

    # Files with incomplete function coverage
    incomplete_function_files: List[FileCoverage] = field(default_factory=list)

    # All implementation files (non-test)
    implementation_files: List[FileCoverage] = field(default_factory=list)


# Cache for demangler availability checks
_cxxfilt_available: Optional[bool] = None
_undname_path: Optional[str] = None
_undname_checked: bool = False


def _find_undname() -> Optional[str]:
    """Find undname.exe in common Visual Studio installation directories."""
    global _undname_path, _undname_checked

    if _undname_checked:
        return _undname_path

    _undname_checked = True

    # Common VS installation paths
    vs_roots = [
        "C:/Program Files/Microsoft Visual Studio",
        "C:/Program Files (x86)/Microsoft Visual Studio",
    ]
    vs_editions = ["2022", "2019", "2017"]
    vs_skus = ["Enterprise", "Professional", "Community", "BuildTools"]

    for root in vs_roots:
        for edition in vs_editions:
            for sku in vs_skus:
                pattern = f"{root}/{edition}/{sku}/VC/Tools/MSVC/*/bin/Hostx64/x64/undname.exe"
                matches = glob_module.glob(pattern)
                if matches:
                    # Sort to get latest version
                    matches.sort(reverse=True)
                    _undname_path = matches[0]
                    return _undname_path

    return None


def _demangle_msvc_with_undname(name: str) -> str:
    """Demangle an MSVC symbol using undname.exe.

    Uses UNDNAME_NAME_ONLY (0x1000) flag to get clean output like
    "InstallerConfig::ReadConfigFromResource" instead of the full signature.
    """
    undname = _find_undname()
    if not undname:
        return name

    try:
        # 0x1000 = UNDNAME_NAME_ONLY - just get the name without signature
        result = subprocess.run(
            [undname, '0x1000', name],
            capture_output=True,
            text=True,
            timeout=5
        )
        if result.returncode == 0 and result.stdout.strip():
            # undname.exe output format:
            # Microsoft (R) C++ Name Undecorator
            # Copyright (C) Microsoft Corporation. All rights reserved.
            #
            # Undecoration of :- "?mangled@@"
            # is :- "ClassName::FunctionName"
            output = result.stdout
            # Look for the "is :-" line
            for line in output.split('\n'):
                if line.startswith('is :-'):
                    # Extract the quoted demangled name
                    match = re.search(r'is :- "(.+)"', line)
                    if match:
                        return match.group(1)
                    # Or unquoted
                    demangled = line[5:].strip().strip('"')
                    if demangled:
                        return demangled
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    return name


def _check_cxxfilt_available() -> bool:
    """Check if c++filt is available on this system."""
    global _cxxfilt_available
    if _cxxfilt_available is None:
        try:
            result = subprocess.run(
                ['c++filt', '--version'],
                capture_output=True,
                timeout=5
            )
            _cxxfilt_available = (result.returncode == 0)
        except (FileNotFoundError, subprocess.TimeoutExpired):
            _cxxfilt_available = False
    return _cxxfilt_available


def _demangle_with_cxxfilt(names: List[str]) -> List[str]:
    """Demangle a list of names using c++filt.

    Args:
        names: List of mangled symbol names.

    Returns:
        List of demangled names (same order as input).
    """
    if not names:
        return []

    # On macOS (Darwin), c++filt expects symbols with __Z prefix (Mach-O format)
    # but Itanium ABI mangling uses _Z. Add underscore prefix for Darwin.
    is_darwin = sys.platform == 'darwin'
    if is_darwin:
        # Add underscore to _Z symbols to make __Z (Darwin format)
        names_to_demangle = ['_' + n if n.startswith('_Z') else n for n in names]
    else:
        names_to_demangle = names

    try:
        result = subprocess.run(
            ['c++filt'],
            input='\n'.join(names_to_demangle),
            capture_output=True,
            text=True,
            timeout=30
        )
        if result.returncode == 0:
            return result.stdout.strip().split('\n')
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    return names  # Return original on failure


def demangle_name(mangled: str) -> str:
    """Demangle a C++ symbol name to human-readable form.

    On Windows, uses undname.exe if available, otherwise heuristics.
    On Linux/macOS, uses c++filt if available.
    """
    if not mangled:
        return mangled

    # MSVC mangled names: ?FunctionName@Namespace@@...
    match = re.search(r'\?(\w+)@', mangled)
    if match:
        # Try undname.exe first for accurate demangling
        if _find_undname():
            demangled = _demangle_msvc_with_undname(mangled)
            if demangled != mangled:
                return demangled

        # Fallback to heuristics
        func_name = match.group(1)
        # Skip internal/compiler-generated names
        if func_name.startswith('_') or func_name.startswith('$'):
            return mangled
        # Try to extract namespace
        parts = re.findall(r'\?(\w+)@', mangled)
        if len(parts) >= 2:
            # Reverse to get namespace::function order
            parts = [p for p in parts if not p.startswith('_') and not p.startswith('$')]
            if len(parts) >= 2:
                return '::'.join(reversed(parts[:3]))
            elif parts:
                return parts[0]
        return func_name

    # GCC/Clang mangling (Itanium ABI): _Z...
    if mangled.startswith('_Z'):
        if _check_cxxfilt_available():
            demangled = _demangle_with_cxxfilt([mangled])
            if demangled and demangled[0] != mangled:
                return demangled[0]
        return mangled

    return mangled


class CoverageAnalyzer:
    """Analyzer for LLVM coverage summary.json files."""

    def __init__(self, strip_prefix: Optional[str] = None,
                 path_filter: Optional[str] = None):
        """Initialize the analyzer.

        Args:
            strip_prefix: Path prefix to strip from filenames for display.
                          If None, will be auto-detected from coverage data.
            path_filter: Optional path prefix to filter files (e.g., 'cef/libcef').
                         Only files matching this prefix will be included.
        """
        self.strip_prefix = strip_prefix
        self.path_filter = path_filter
        self.analysis = CoverageAnalysis()
        self._detected_prefix: Optional[str] = None

    def load_summary(self, path: str) -> dict:
        """Load the summary.json file.

        Args:
            path: Path to the summary.json file.

        Returns:
            Parsed JSON data.

        Raises:
            FileNotFoundError: If the file doesn't exist.
            json.JSONDecodeError: If the file isn't valid JSON.
        """
        with open(path, 'r') as f:
            return json.load(f)

    def load_detailed_export(self, profdata_path: str, binary_path: str,
                             llvm_cov_path: str,
                             source_filter: Optional[str] = None) -> dict:
        """Load detailed coverage data using llvm-cov export.

        This provides function-level details including uncovered functions.
        Note: This can be slow for large binaries as it exports all coverage data.

        Args:
            profdata_path: Path to the .profdata file.
            binary_path: Path to the instrumented binary.
            llvm_cov_path: Path to llvm-cov executable.
            source_filter: Optional source file/directory to filter (not used,
                          filtering is done in extract_uncovered_functions).

        Returns:
            Parsed JSON data from llvm-cov export.
        """
        # Use absolute paths for Windows compatibility
        llvm_cov_abs = os.path.abspath(llvm_cov_path)
        profdata_abs = os.path.abspath(profdata_path)
        binary_abs = os.path.abspath(binary_path)

        cmd = [
            llvm_cov_abs, 'export',
            '-instr-profile=' + profdata_abs,
            binary_abs,
        ]

        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(f"llvm-cov export failed: {result.stderr}")

        return json.loads(result.stdout)

    def extract_uncovered_functions(self, data: dict) -> List[UncoveredFunction]:
        """Extract uncovered functions from detailed export data.

        Args:
            data: Parsed JSON data from llvm-cov export.

        Returns:
            List of uncovered functions with location information.
        """
        uncovered = []

        functions = data.get('data', [{}])[0].get('functions', [])
        for func in functions:
            # Skip if function was executed
            if func.get('count', 0) > 0:
                continue

            name = func.get('name', '')
            filenames = func.get('filenames', [])
            regions = func.get('regions', [])

            # Skip test/gtest functions
            if 'gtest' in name.lower() or 'testing@@' in name:
                continue

            # Get primary source file (first non-test file)
            source_file = None
            for f in filenames:
                normalized = self.normalize_path(f)
                if self.matches_filter(normalized) and not self.is_test_file(normalized):
                    source_file = normalized
                    break

            if not source_file:
                continue

            # Get starting line from first region
            line = regions[0][0] if regions else 0

            uncovered.append(UncoveredFunction(
                name=demangle_name(name),
                mangled_name=name,
                filename=source_file,
                line=line,
            ))

        # Sort by filename then line number
        uncovered.sort(key=lambda f: (f.filename, f.line))
        return uncovered

    def add_uncovered_functions_to_files(self, uncovered: List[UncoveredFunction]) -> None:
        """Add uncovered function details to file coverage entries.

        Args:
            uncovered: List of uncovered functions from extract_uncovered_functions.
        """
        # Group by filename
        by_file = {}
        for func in uncovered:
            if func.filename not in by_file:
                by_file[func.filename] = []
            by_file[func.filename].append(func)

        # Add to existing FileCoverage entries
        for file_cov in self.analysis.incomplete_function_files:
            if file_cov.filename in by_file:
                file_cov.uncovered_functions = by_file[file_cov.filename]

    def _detect_prefix(self, data: dict) -> str:
        """Auto-detect the source root prefix from coverage data.

        Finds the Chromium source root (e.g., /path/to/chromium/src/) by
        looking for common top-level directories like 'cef/', 'base/', 'chrome/'.

        Args:
            data: Parsed JSON data from summary.json.

        Returns:
            Source root prefix string, or empty string if none found.
        """
        files = data.get('data', [{}])[0].get('files', [])
        if not files:
            return ''

        # Look for known top-level directories in the first file path
        sample_path = files[0].get('filename', '')
        if not sample_path:
            return ''

        # Normalize separators for consistent searching
        normalized = sample_path.replace('\\', '/')

        # Known Chromium top-level directories
        top_dirs = ['cef/', 'base/', 'chrome/', 'content/', 'components/',
                    'third_party/', 'ui/', 'net/', 'mojo/', 'services/']

        for top_dir in top_dirs:
            idx = normalized.find('/' + top_dir)
            if idx != -1:
                # Return everything up to and including the slash before the top dir
                # Convert back to original path separator style
                prefix = sample_path[:idx + 1]
                return prefix

        # Fallback: look for 'src/' in the path
        idx = normalized.find('/src/')
        if idx != -1:
            prefix = sample_path[:idx + 5]  # Include '/src/'
            return prefix

        return ''

    def normalize_path(self, filename: str) -> str:
        """Normalize a file path for display.

        Args:
            filename: Raw filename from coverage data.

        Returns:
            Normalized path with prefix stripped and backslashes converted.
        """
        result = filename

        # Handle relative paths like ..\..\cef\...
        result = result.replace('\\', '/')
        while result.startswith('../'):
            result = result[3:]

        # Strip explicit prefix if provided
        prefix = self.strip_prefix or self._detected_prefix or ''
        if prefix:
            prefix_normalized = prefix.replace('\\', '/')
            result = result.replace(prefix_normalized, '')

        # Remove leading slash if present
        if result.startswith('/'):
            result = result[1:]

        return result

    def is_test_file(self, filename: str) -> bool:
        """Check if a file is a test file or test-support file.

        Matches:
          - *_unittest.cc (unit test files)
          - *_test.cc (test files)
          - *_test_*.cc (test support/helper files, e.g. installer_test_helpers.cc)
          - *_integration_test.cc (integration test files)

        Args:
            filename: Normalized filename.

        Returns:
            True if this is a test or test-support file.
        """
        basename = filename.split('/')[-1]
        if '_unittest.cc' in basename or '_integration_test.cc' in basename:
            return True
        if basename.endswith('_test.cc'):
            return True
        # Match test support files like installer_test_helpers.cc,
        # installer_test_archive.cc, installer_test_catalog.cc
        if re.match(r'.*_test_\w+\.cc$', basename):
            return True
        return False

    def matches_filter(self, filename: str) -> bool:
        """Check if a file matches the path filter.

        Args:
            filename: Normalized filename.

        Returns:
            True if the file matches the filter (or no filter is set).
        """
        if not self.path_filter:
            return True
        return filename.startswith(self.path_filter)

    def parse(self, data: dict) -> None:
        """Parse coverage data and populate analysis results.

        Args:
            data: Parsed JSON data from summary.json.
        """
        # Auto-detect prefix if not provided
        if not self.strip_prefix:
            self._detected_prefix = self._detect_prefix(data)

        # Extract totals
        totals = data['data'][0]['totals']

        self.analysis.total_lines_covered = totals['lines']['covered']
        self.analysis.total_lines_count = totals['lines']['count']
        self.analysis.total_lines_percent = totals['lines']['percent']

        self.analysis.total_functions_covered = totals['functions']['covered']
        self.analysis.total_functions_count = totals['functions']['count']
        self.analysis.total_functions_percent = totals['functions']['percent']

        self.analysis.total_branches_covered = totals['branches']['covered']
        self.analysis.total_branches_count = totals['branches']['count']
        self.analysis.total_branches_percent = totals['branches']['percent']

        self.analysis.total_regions_covered = totals['regions']['covered']
        self.analysis.total_regions_count = totals['regions']['count']
        self.analysis.total_regions_percent = totals['regions']['percent']

        # Process individual files
        for file_info in data['data'][0]['files']:
            filename = self.normalize_path(file_info['filename'])

            # Apply path filter
            if not self.matches_filter(filename):
                continue

            func = file_info['summary']['functions']
            lines = file_info['summary']['lines']

            # Skip test files and files with no functions
            if self.is_test_file(filename) or func['count'] == 0:
                continue

            file_cov = FileCoverage(
                filename=filename,
                functions_covered=func['covered'],
                functions_total=func['count'],
                functions_percent=func['percent'],
                lines_covered=lines['covered'],
                lines_total=lines['count'],
                lines_percent=lines['percent'],
            )

            # Track files with incomplete function coverage
            if func['percent'] < 100:
                self.analysis.incomplete_function_files.append(file_cov)

            # Track all implementation files
            if lines['count'] > 0:
                self.analysis.implementation_files.append(file_cov)

        # Sort incomplete files by missing functions (descending)
        self.analysis.incomplete_function_files.sort(
            key=lambda x: x.functions_missing, reverse=True)

        # Sort implementation files by coverage percentage (ascending)
        self.analysis.implementation_files.sort(
            key=lambda x: x.lines_percent)

    def generate_summary_report(self, colorize: bool = True) -> str:
        """Generate a human-readable summary report.

        Args:
            colorize: Whether to include ANSI color codes.

        Returns:
            Formatted report string.
        """
        lines = []

        # Overall summary
        lines.append("=" * 70)
        lines.append("OVERALL COVERAGE SUMMARY")
        lines.append("=" * 70)
        lines.append("")

        a = self.analysis
        lines.append(f"  Lines:     {a.total_lines_covered:>5}/{a.total_lines_count:<5} "
                     f"({a.total_lines_percent:.1f}%)")
        lines.append(f"  Functions: {a.total_functions_covered:>5}/{a.total_functions_count:<5} "
                     f"({a.total_functions_percent:.1f}%)")
        lines.append(f"  Branches:  {a.total_branches_covered:>5}/{a.total_branches_count:<5} "
                     f"({a.total_branches_percent:.1f}%)")
        lines.append(f"  Regions:   {a.total_regions_covered:>5}/{a.total_regions_count:<5} "
                     f"({a.total_regions_percent:.1f}%)")
        lines.append("")

        # Files with incomplete function coverage
        lines.append("=" * 70)
        lines.append("FILES WITH LESS THAN 100% FUNCTION COVERAGE")
        if self.path_filter:
            lines.append(f"(filtered to: {self.path_filter})")
        lines.append("=" * 70)
        lines.append("")

        for f in a.incomplete_function_files:
            lines.append(f"  {f.filename}")
            lines.append(f"    Functions: {f.functions_covered}/{f.functions_total} "
                         f"({f.functions_percent:.1f}%)")
            lines.append(f"    Missing: {f.functions_missing} function(s)")
            if f.uncovered_functions:
                lines.append(f"    Uncovered:")
                for func in f.uncovered_functions:
                    lines.append(f"      - {func.name} (line {func.line})")
            lines.append("")

        total_missing = sum(f.functions_missing for f in a.incomplete_function_files)
        lines.append(f"Total files with incomplete function coverage: "
                     f"{len(a.incomplete_function_files)}")
        lines.append(f"Total missing functions: {total_missing}")
        lines.append("")

        # Line coverage summary
        lines.append("=" * 70)
        lines.append("LINE COVERAGE SUMMARY (implementation files only)")
        lines.append("=" * 70)
        lines.append("")

        lines.append(f"{'File':<55} {'Lines':<15} {'Coverage':<10}")
        lines.append("-" * 80)

        for f in a.implementation_files:
            short_name = f.filename.split('/')[-1]
            lines_str = f"{f.lines_covered}/{f.lines_total}"
            lines.append(f"  {short_name:<53} {lines_str:<15} {f.lines_percent:>6.1f}%")

        lines.append("")

        return "\n".join(lines)

    def generate_json_report(self) -> str:
        """Generate a JSON report of the analysis.

        Returns:
            JSON string with analysis results.
        """
        a = self.analysis

        report = {
            "totals": {
                "lines": {
                    "covered": a.total_lines_covered,
                    "count": a.total_lines_count,
                    "percent": a.total_lines_percent,
                },
                "functions": {
                    "covered": a.total_functions_covered,
                    "count": a.total_functions_count,
                    "percent": a.total_functions_percent,
                },
                "branches": {
                    "covered": a.total_branches_covered,
                    "count": a.total_branches_count,
                    "percent": a.total_branches_percent,
                },
                "regions": {
                    "covered": a.total_regions_covered,
                    "count": a.total_regions_count,
                    "percent": a.total_regions_percent,
                },
            },
            "incomplete_function_files": [
                {
                    "filename": f.filename,
                    "functions_covered": f.functions_covered,
                    "functions_total": f.functions_total,
                    "functions_percent": f.functions_percent,
                    "functions_missing": f.functions_missing,
                    "uncovered_functions": [
                        {
                            "name": func.name,
                            "line": func.line,
                            "mangled_name": func.mangled_name,
                        }
                        for func in f.uncovered_functions
                    ] if f.uncovered_functions else [],
                }
                for f in a.incomplete_function_files
            ],
            "statistics": {
                "total_incomplete_files": len(a.incomplete_function_files),
                "total_missing_functions": sum(
                    f.functions_missing for f in a.incomplete_function_files),
            },
        }

        if self.path_filter:
            report["filter"] = self.path_filter

        return json.dumps(report, indent=2)

    def get_statistics(self) -> dict:
        """Get summary statistics.

        Returns:
            Dictionary with key statistics.
        """
        a = self.analysis
        return {
            "total_lines_covered": a.total_lines_covered,
            "total_lines_count": a.total_lines_count,
            "total_lines_percent": a.total_lines_percent,
            "total_functions_covered": a.total_functions_covered,
            "total_functions_count": a.total_functions_count,
            "total_functions_percent": a.total_functions_percent,
            "incomplete_files_count": len(a.incomplete_function_files),
            "total_missing_functions": sum(
                f.functions_missing for f in a.incomplete_function_files),
        }

    def get_uncovered_lines_from_export(self, source_file: str,
                                         export_data: dict) -> List[Tuple[int, int, str]]:
        """Get uncovered line ranges from already-loaded export data.

        Uses region nesting to determine line coverage correctly:

        1. For each line, collect all regions (across all functions) that cover it.
        2. The innermost (smallest-span) region determines coverage — inner
           zero-count regions represent genuinely unexecuted code blocks.
        3. Exception: single-line zero-count regions (branch conditions like the
           `b` in `if (a && b)`) defer to enclosing multi-line regions, because
           the line itself was executed even if one branch wasn't taken.

        This avoids both failure modes:
        - OLD bug: any zero-count region marks line uncovered (inflates with branch misses)
        - Previous fix: any positive-count region marks line covered (ignores nesting)

        Args:
            source_file: Path to the source file to analyze (can be partial path).
            export_data: Parsed JSON data from llvm-cov export.

        Returns:
            List of tuples (start_line, end_line, description) for uncovered ranges.
        """
        # Normalize the source file path for matching
        source_normalized = source_file.replace('\\', '/').lower()
        if source_normalized.startswith('./'):
            source_normalized = source_normalized[2:]

        # Collect all regions covering each line: {line: [(span, exec_count), ...]}
        line_regions = {}  # type: dict[int, list[tuple[int, int]]]

        functions = export_data.get('data', [{}])[0].get('functions', [])
        for func in functions:
            filenames = func.get('filenames', [])
            regions = func.get('regions', [])

            # Check if this function is in our target file
            file_match = False
            for f in filenames:
                f_normalized = f.replace('\\', '/').lower()
                # Handle relative paths like ..\..\cef\...
                while '../' in f_normalized:
                    f_normalized = re.sub(r'[^/]+/\.\./', '', f_normalized)
                if source_normalized in f_normalized or f_normalized.endswith(source_normalized):
                    file_match = True
                    break

            if not file_match:
                continue

            # Regions format: [LineStart, ColStart, LineEnd, ColEnd, ExecutionCount, FileID, ExpandedFileID, Kind]
            for region in regions:
                if len(region) >= 5:
                    line_start = region[0]
                    line_end = region[2]
                    exec_count = region[4]
                    span = line_end - line_start + 1

                    for line in range(line_start, line_end + 1):
                        if line not in line_regions:
                            line_regions[line] = []
                        line_regions[line].append((span, exec_count))

        # Determine coverage per line using innermost-region-wins logic
        uncovered_lines = set()
        for line, regions_info in line_regions.items():
            min_span = min(s for s, _ in regions_info)
            smallest = [(s, e) for s, e in regions_info if s == min_span]

            if any(e > 0 for _, e in smallest):
                # Innermost region was executed → line is covered
                continue

            # All innermost regions have exec_count == 0.
            if min_span == 1:
                # Single-line zero-count regions are branch misses (e.g., the
                # untaken branch of `if (a && b)`). The line itself may have
                # been executed by an enclosing multi-line region.
                if any(e > 0 for s, e in regions_info if s > 1):
                    continue  # Enclosing region was executed → line is covered

            # Line is genuinely uncovered
            uncovered_lines.add(line)

        if not uncovered_lines:
            return []

        # Convert to sorted list and group into ranges
        sorted_lines = sorted(uncovered_lines)
        ranges = []
        range_start = sorted_lines[0]
        range_end = sorted_lines[0]

        for line in sorted_lines[1:]:
            if line == range_end + 1:
                range_end = line
            else:
                ranges.append((range_start, range_end, f"Lines {range_start}-{range_end}"))
                range_start = line
                range_end = line

        ranges.append((range_start, range_end, f"Lines {range_start}-{range_end}"))

        return ranges

    def get_uncovered_lines(self, source_file: str, profdata_path: str,
                            binary_path: str, llvm_cov_path: str) -> List[Tuple[int, int, str]]:
        """Get uncovered line ranges for a specific source file.

        Uses llvm-cov export (cached if already loaded) for speed.

        Args:
            source_file: Path to the source file to analyze.
            profdata_path: Path to the .profdata file.
            binary_path: Path to the instrumented binary.
            llvm_cov_path: Path to llvm-cov executable.

        Returns:
            List of tuples (start_line, end_line, description) for uncovered ranges.
        """
        # Load export data (this is the slow part, but we can cache it)
        export_data = self.load_detailed_export(profdata_path, binary_path, llvm_cov_path)
        return self.get_uncovered_lines_from_export(source_file, export_data)

    def generate_uncovered_lines_report(self, source_file: str,
                                        uncovered_ranges: List[Tuple[int, int, str]]) -> str:
        """Generate a concise report of uncovered line ranges.

        Args:
            source_file: Source file path.
            uncovered_ranges: List of (start, end, description) tuples.

        Returns:
            Formatted report string.
        """
        if not uncovered_ranges:
            return f"{source_file}: 100% line coverage"

        total_uncovered = sum(end - start + 1 for start, end, _ in uncovered_ranges)

        # Group ranges by size for easier prioritization
        small = []   # 1-5 lines
        medium = []  # 6-20 lines
        large = []   # 21+ lines

        for start, end, _ in uncovered_ranges:
            size = end - start + 1
            range_str = str(start) if start == end else f"{start}-{end}"
            if size <= 5:
                small.append(range_str)
            elif size <= 20:
                medium.append(f"{range_str} ({size})")
            else:
                large.append(f"{range_str} ({size})")

        lines = [f"{source_file}: {total_uncovered} uncovered lines in {len(uncovered_ranges)} ranges"]

        if large:
            lines.append(f"  Large (21+): {', '.join(large)}")
        if medium:
            lines.append(f"  Medium (6-20): {', '.join(medium)}")
        if small:
            lines.append(f"  Small (1-5): {', '.join(small)}")

        return '\n'.join(lines)


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description='Analyze LLVM coverage reports.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s out/coverage/summary.json
  %(prog)s summary.json --filter cef/libcef_dll/bootstrap/installer
  %(prog)s summary.json --json
  %(prog)s summary.json --strip-prefix /home/user/chromium/src/

For detailed function-level analysis (requires coverage build artifacts):
  %(prog)s summary.json --profdata out/report/coverage.profdata \\
      --binary out/coverage/my_unittests --llvm-cov third_party/llvm-build/Release+Asserts/bin/llvm-cov

To show uncovered lines in a specific file:
  %(prog)s summary.json --profdata out/report/win/coverage.profdata \\
      --binary out/coverage/my_unittests.exe \\
      --show-uncovered-lines cef/libcef_dll/bootstrap/installer/installer_archive.cc
""")

    parser.add_argument('summary_file', nargs='?',
                        help='Path to LLVM coverage summary.json file')
    parser.add_argument('--filter', '-f', dest='path_filter',
                        help='Only include files matching this path prefix')
    parser.add_argument('--strip-prefix', '-p', dest='strip_prefix',
                        help='Path prefix to strip from filenames (auto-detected if not specified)')
    parser.add_argument('--json', '-j', action='store_true',
                        help='Output results as JSON')

    # Detailed analysis options
    parser.add_argument('--profdata', dest='profdata_path',
                        help='Path to .profdata file for detailed function analysis')
    parser.add_argument('--binary', dest='binary_path',
                        help='Path to instrumented binary for detailed function analysis')
    parser.add_argument('--llvm-cov', dest='llvm_cov_path',
                        default='third_party/llvm-build/Release+Asserts/bin/llvm-cov',
                        help='Path to llvm-cov executable (default: third_party/llvm-build/Release+Asserts/bin/llvm-cov)')
    parser.add_argument('--show-uncovered-lines', dest='uncovered_lines_file',
                        help='Show uncovered line ranges for source file(s). Supports glob patterns like "cef/.../installer/*.cc" (requires --profdata and --binary)')

    args = parser.parse_args()

    if not args.summary_file:
        parser.print_help()
        sys.exit(1)

    summary_path = args.summary_file

    if not args.json:
        print(f"Loading coverage data from: {summary_path}")
        print()

    analyzer = CoverageAnalyzer(
        strip_prefix=args.strip_prefix,
        path_filter=args.path_filter
    )

    try:
        data = analyzer.load_summary(summary_path)
    except FileNotFoundError:
        print(f"Error: Could not find {summary_path}", file=sys.stderr)
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in {summary_path}: {e}", file=sys.stderr)
        sys.exit(1)

    analyzer.parse(data)

    # Load detailed export data if needed (for functions or uncovered lines)
    detailed_data = None
    if args.profdata_path and args.binary_path:
        if not args.json and not args.uncovered_lines_file:
            print("Loading detailed function coverage...")
            print()
        try:
            detailed_data = analyzer.load_detailed_export(
                args.profdata_path,
                args.binary_path,
                args.llvm_cov_path,
                source_filter=args.path_filter
            )
            if not args.uncovered_lines_file:
                uncovered = analyzer.extract_uncovered_functions(detailed_data)
                analyzer.add_uncovered_functions_to_files(uncovered)
        except Exception as e:
            print(f"Warning: Could not load detailed coverage: {e}", file=sys.stderr)

    # Handle --show-uncovered-lines option (supports glob patterns)
    if args.uncovered_lines_file:
        if not args.profdata_path or not args.binary_path:
            print("Error: --show-uncovered-lines requires --profdata and --binary",
                  file=sys.stderr)
            sys.exit(1)
        if detailed_data is None:
            print("Error: Could not load coverage data", file=sys.stderr)
            sys.exit(1)

        # Expand glob pattern if provided
        pattern = args.uncovered_lines_file
        if '*' in pattern or '?' in pattern:
            # Glob pattern - find matching files
            source_files = sorted(glob_module.glob(pattern, recursive=True))
            if not source_files:
                print(f"Error: No files match pattern: {pattern}", file=sys.stderr)
                sys.exit(1)
            # Filter to only .cc/.cpp/.h files (skip test files)
            source_files = [f for f in source_files
                           if (f.endswith('.cc') or f.endswith('.cpp') or f.endswith('.h'))
                           and '_unittest' not in f and '_test.cc' not in f]
        else:
            source_files = [pattern]

        try:
            for source_file in source_files:
                uncovered_ranges = analyzer.get_uncovered_lines_from_export(
                    source_file,
                    detailed_data
                )
                print(analyzer.generate_uncovered_lines_report(
                    source_file, uncovered_ranges))
                if len(source_files) > 1:
                    print()  # Blank line between files
        except Exception as e:
            print(f"Error: Could not analyze uncovered lines: {e}", file=sys.stderr)
            sys.exit(1)
    elif args.json:
        print(analyzer.generate_json_report())
    else:
        print(analyzer.generate_summary_report(colorize=False))


if __name__ == '__main__':
    main()
