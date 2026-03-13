# Claude Coverage Analysis Instructions

This document provides instructions for Claude to run and analyze code coverage for CEF and Chromium components.

## Human Prerequisites

Before Claude can run coverage analysis, a human must complete these one-time setup steps:

### 1. Enable Coverage Tools in .gclient

Add to your `.gclient` file's `custom_vars`:

```python
"checkout_clang_coverage_tools": True,
```

Then run:
```bash
gclient sync
```

### 2. Create a Coverage Build Directory

Copy an existing build configuration and modify it for coverage:

```bash
# Create the output directory
mkdir out/coverage

# Copy existing args.gn as a starting point (e.g., Debug_GN_x64, Release_GN_x64)
cp out/<existing_build_dir>/args.gn out/coverage/args.gn
```

Edit `out/coverage/args.gn` and add/modify these settings:

```gn
# Required for coverage
use_clang_coverage = true

# Required for accurate coverage (component builds are much slower with coverage)
is_component_build = false

# Release build recommended for reasonable performance
is_debug = false
```

Then generate the build files:

```bash
gn gen out/coverage
```

**Note:** Keep other settings from your existing args.gn (e.g., `target_cpu`, platform-specific flags) to ensure compatibility with your environment.

### 3. Verify Tools Are Available

```bash
# Check that llvm-profdata and llvm-cov are available
third_party/llvm-build/Release+Asserts/bin/llvm-profdata --version
third_party/llvm-build/Release+Asserts/bin/llvm-cov --version
```

---

## Claude Instructions

Once prerequisites are complete, Claude can perform coverage analysis using the following workflow.

### Building and Running Coverage

#### Step 1: Build the Test Target

```bash
autoninja -C out/coverage <test_target>
```

Example:
```bash
autoninja -C out/coverage cef_installer_unittests
```

#### Step 2: Generate Coverage Report

Use the Chromium coverage script to run tests and generate reports:

```bash
python3 tools/code_coverage/coverage.py <test_target> \
    -b out/coverage \
    -o out/report \
    -c out/coverage/<test_target> \
    -f <source_filter> \
    --coverage-tools-dir=third_party/llvm-build/Release+Asserts/bin \
    --no-component-view
```

**Parameters:**
- `-b out/coverage` - Build directory with coverage-instrumented binaries
- `-o out/report` - Output directory for coverage reports
- `-c out/coverage/<test_target>` - Command to run (can specify multiple with additional `-c` flags). **On Windows:** use backslashes and include `.exe` (e.g., `out\coverage\test.exe`)
- `-f <source_filter>` - Filter coverage to specific directories (e.g., `cef/libcef_dll/bootstrap/installer`)
- `--coverage-tools-dir` - Path to LLVM coverage tools
- `--no-component-view` - Skip component view generation (faster)

**Example for CEF installer (Linux/macOS):**
```bash
python3 tools/code_coverage/coverage.py cef_installer_unittests \
    -b out/coverage \
    -o out/report \
    -c out/coverage/cef_installer_unittests \
    -f cef/libcef_dll/bootstrap/installer \
    --coverage-tools-dir=third_party/llvm-build/Release+Asserts/bin \
    --no-component-view
```

**Example for CEF installer (Windows):**
```cmd
python3 tools/code_coverage/coverage.py cef_installer_unittests ^
    -b out/coverage ^
    -o out/report ^
    -c out\coverage\cef_installer_unittests.exe ^
    -f cef/libcef_dll/bootstrap/installer ^
    --coverage-tools-dir=third_party\llvm-build\Release+Asserts\bin ^
    --no-component-view
```

**Note:** On Windows, the `-c` command path must use backslashes and include `.exe`.

#### Step 3: Analyze Coverage Results

Use the coverage analyzer to parse and summarize results:

```bash
python3 cef/tools/claude/analyze_coverage_output.py \
    out/report/<platform>/summary.json \
    --filter <source_filter>
```

**Platform directories:**

- `out/report/linux/summary.json` - Linux
- `out/report/mac/summary.json` - macOS
- `out/report/win/summary.json` - Windows

**Detailed analysis with specific uncovered function names (recommended):**

```bash
python3 cef/tools/claude/analyze_coverage_output.py \
    out/report/<platform>/summary.json \
    --filter <source_filter> \
    --profdata out/report/<platform>/coverage.profdata \
    --binary out/coverage/<test_target> \
    --llvm-cov third_party/llvm-build/Release+Asserts/bin/llvm-cov
```

On Windows, add `.exe` to the binary and llvm-cov paths.

**Note:** Detailed analysis takes ~15-20 seconds as it processes the full llvm-cov export to extract function names.

Example output showing each uncovered function with its demangled name and line number:

```
  cef/libcef_dll/bootstrap/installer/installer_config.cc
    Functions: 3/5 (60.0%)
    Missing: 2 function(s)
    Uncovered:
      - cef_installer::ReadConfigFromResource (line 100)
      - cef_installer::ConfigToJson (line 113)
```

**JSON output for programmatic analysis:**
```bash
python3 cef/tools/claude/analyze_coverage_output.py \
    out/report/win/summary.json \
    --filter cef/libcef_dll/bootstrap/installer \
    --json
```

**Show uncovered line ranges for a specific file (for line coverage improvement):**

```bash
python3 cef/tools/claude/analyze_coverage_output.py \
    out/report/<platform>/summary.json \
    --profdata out/report/<platform>/coverage.profdata \
    --binary out/coverage/<test_target> \
    --show-uncovered-lines <source_file_or_glob_pattern>
```

Example (single file):
```bash
python3 cef/tools/claude/analyze_coverage_output.py \
    out/report/win/summary.json \
    --profdata out/report/win/coverage.profdata \
    --binary out/coverage/cef_installer_unittests.exe \
    --show-uncovered-lines cef/libcef_dll/bootstrap/installer/installer_archive.cc
```

**Glob pattern support:** You can analyze multiple files at once using glob patterns. This is much faster than running the command multiple times since the coverage data is loaded only once.

Example (all .cc files in a directory):
```bash
python3 cef/tools/claude/analyze_coverage_output.py \
    out/report/win/summary.json \
    --profdata out/report/win/coverage.profdata \
    --binary out/coverage/cef_installer_unittests.exe \
    --show-uncovered-lines "cef/libcef_dll/bootstrap/installer/*.cc"
```

**Note:** Quote the pattern to prevent shell expansion. Test files (`*_unittest.cc`, `*_test.cc`) are automatically filtered out.

Example output showing uncovered line ranges grouped by size:
```
cef/libcef_dll/bootstrap/installer/installer_archive.cc: 285 uncovered lines in 45 ranges
  Large (21+): 575-846 (272), 920-995 (76)
  Medium (6-20): 192-197 (6), 208-213 (6), 516-522 (7)
  Small (1-5): 44-48, 54-55, 127, 134, 145-146

cef/libcef_dll/bootstrap/installer/installer_config.cc: 58 uncovered lines in 21 ranges
  Medium (6-20): 149-165 (17)
  Small (1-5): 37-40, 46, 48-49, 59-60, ...
```

This helps prioritize which code paths to target for line coverage improvement. Large ranges often indicate entire code blocks (error handling, alternative paths) that need tests.

### Understanding Coverage Output

The analyzer reports:

1. **Overall Coverage Summary** - Lines, functions, branches, and regions covered
2. **Files with Incomplete Function Coverage** - Files sorted by number of missing functions
3. **Line Coverage Summary** - Per-file line coverage percentages

**Key metrics:**
- **Function coverage**: Percentage of functions that were executed at least once
- **Line coverage**: Percentage of executable lines that were executed
- **Branch coverage**: Percentage of branches (if/else, switch) that were taken

### How `--show-uncovered-lines` Works

The `--show-uncovered-lines` option uses **innermost-region-wins** logic to
determine which lines are genuinely uncovered:

- LLVM coverage tracks **regions** (code blocks), not lines. A single line can
  have multiple overlapping regions from nested control flow.
- For each line, the **smallest (most specific) region** determines coverage.
- **Single-line zero-count regions** (branch misses, e.g., the untaken side of
  `if (a && b)`) defer to enclosing multi-line regions -- the line was executed
  even if one branch wasn't taken.
- **Multi-line zero-count regions** (e.g., an if-body that was never entered)
  represent genuinely unexecuted code, even if an outer region was executed.

This avoids two failure modes:
1. Counting branch misses as uncovered lines (inflates counts on lines like
   `if (a && b)` where `a` was true but `b` was false).
2. Counting all lines in an outer region as covered just because the function
   was entered (misses genuinely unexecuted inner blocks).

### Improving Coverage

When tasked with improving test coverage:

**For function coverage (getting to 100% function coverage):**
1. **Identify gaps**: Run the analyzer to find files with incomplete function coverage
2. **Prioritize**: Focus on files with the most missing functions first
3. **Read source**: Examine the uncovered functions to understand what they do
4. **Check existing tests**: Read the existing `*_unittest.cc` file to understand test patterns
5. **Add tests**: Write tests that exercise the uncovered functions
6. **Verify**: Build and run tests in the regular build directory first (faster iteration)
7. **Measure**: Re-run coverage to confirm improvement

**For line coverage (improving coverage within already-tested functions):**
1. **Find uncovered lines**: Use `--show-uncovered-lines <file>` to get line ranges
2. **Prioritize large ranges**: Focus on Large (21+) and Medium (6-20) ranges first
3. **Read the code**: Use the Read tool to examine the uncovered line ranges
4. **Identify the code path**: Determine what conditions trigger these lines (error handling, edge cases, alternative branches)
5. **Add meaningful tests**: Only add tests that verify real behavior, not just to hit lines
6. **Skip untestable code**: Some paths (e.g., OS errors, hardware failures) may not be practically testable

**Workflow loop:**
```bash
# 1. Build and test in regular build directory (faster iteration)
autoninja -C out/<build_dir> <test_target>
out/<build_dir>/<test_target> --gtest_filter="<TestSuite>*"

# 2. Once tests pass, build in coverage directory
autoninja -C out/coverage <test_target>

# 3. Generate coverage report
python3 tools/code_coverage/coverage.py <test_target> ...

# 4. Analyze results
python3 cef/tools/claude/analyze_coverage_output.py out/report/<platform>/summary.json --filter <path>

# 5. Repeat
```

**Note:** Always develop and verify tests using the regular build directory (e.g., `out/Debug_GN_x64`, `out/Release_GN_x64`, or other project-specific directory). Coverage builds are significantly slower due to instrumentation overhead. Only use the coverage build when you need to measure coverage.

### Common Test Patterns

When adding coverage tests, follow these patterns:

```cpp
// Test fixture for tests needing setup/teardown
class MyFeatureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Setup code
  }
  void TearDown() override {
    // Cleanup code
  }
};

// Simple test
TEST(MyFeatureTest, FunctionReturnsExpectedValue) {
  EXPECT_EQ(expected, MyFunction(input));
}

// Test with fixture
TEST_F(MyFeatureTest, ComplexScenario) {
  // Test using fixture state
}

// Test for error conditions
TEST(MyFeatureTest, HandlesInvalidInput) {
  EXPECT_EQ(ErrorCode::kInvalidArgument, MyFunction(nullptr));
}

// Test with temporary files
TEST(MyFeatureTest, WritesToFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath file_path = temp_dir.GetPath().Append("test.txt");
  ASSERT_TRUE(base::WriteFile(file_path, "content"));

  // Test file operations
}
```

### Troubleshooting

**"Could not find summary.json"**
- Ensure coverage report was generated successfully
- Check the correct platform subdirectory (linux/mac/win)

**"FileNotFoundError: The system cannot find the file specified" on Windows**
- The `-c` command path must use backslashes on Windows, not forward slashes
- Correct: `-c out\coverage\cef_installer_unittests.exe`
- Wrong: `-c out/coverage/cef_installer_unittests.exe`
- Also ensure the `.exe` extension is included

**Low coverage despite adding tests**
- Verify tests are actually running (check test output)
- Ensure the test binary was rebuilt after adding tests
- Check that the source filter includes the target files

**Build failures with coverage enabled**
- Some targets may not support coverage instrumentation
- Try building without `is_component_build=false` if linking fails

**Tests timeout or are slow**
- Coverage instrumentation adds overhead (2-10x slower)
- Use `--gtest_filter` to run specific tests during development

---

## Reference

- [Chromium Code Coverage Documentation](https://chromium.googlesource.com/chromium/src/+/main/docs/testing/code_coverage.md)
- [Coverage Analyzer](analyze_coverage_output.py) - General-purpose LLVM coverage analyzer
- [Coverage Analyzer Tests](analyze_coverage_output_test.py)
