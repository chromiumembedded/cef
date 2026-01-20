# CEF Clang Tools - Claude Code Instructions

This file provides guidance to Claude Code when working with CEF clang tools.

## Overview

The `cef_cpp_rewriter` tool is a clang AST-based rewriter for C++20 modernization.
It uses Clang's LibTooling and AST matchers to perform type-aware transformations
that cannot be done with simple regex.

See [TRANSFORMS.md](TRANSFORMS.md) for complete documentation of all supported
transformations, command-line flags, and examples.

**Important:** CEF is a separate git repository within chromium/src. Chromium's
clang tool scripts use git commands that don't work across repository boundaries.
Use the CEF-specific scripts in `cef/tools/clang/scripts/` instead.

## Key Files

- `cef/tools/clang/cef_cpp_rewriter/CefCppRewriter.cpp` - Main tool implementation
- `cef/tools/clang/cef_cpp_rewriter/CMakeLists.txt` - Build configuration
- `cef/tools/clang/scripts/build_clang_rewriter.sh` - Bootstrap script
- `cef/tools/clang/cef_cpp_rewriter/tests/` - Test cases

## Prerequisites

**Python 3.10+ required.** Chromium's build scripts use type union syntax that requires Python 3.10+. On macOS:

```bash
brew install python@3.12
ln -s /opt/homebrew/opt/python@3.12/bin/python3.12 /opt/homebrew/opt/python@3.12/bin/python3
export PATH="/opt/homebrew/opt/python@3.12/bin:$PATH"
```

## Development Workflow

### Building the Tool

After modifying `CefCppRewriter.cpp`:

```bash
# Fast incremental rebuild (~seconds)
ninja -C third_party/llvm-build/Release+Asserts cef_cpp_rewriter
```

**Never re-run `build_clang_rewriter.sh`** unless the build directory was deleted.
It rebuilds clang from scratch (~30 minutes).

### Testing Matchers with clang-query

```bash
export PATH=$PATH:third_party/llvm-build/Release+Asserts/bin
clang-query /tmp/test.cc --
```

### Generating compile_commands.json for Manual Testing

The `test_tool.py` script generates this automatically, but for manual testing:

```bash
# macOS (run from chromium/src)
TEST_DIR=$(pwd)/cef/tools/clang/cef_cpp_rewriter/tests
cat > $TEST_DIR/compile_commands.json << EOF
[{"directory": "$TEST_DIR", "command": "clang++ -std=c++20 -isysroot $(xcrun --show-sdk-path) -c contains-original.cc", "file": "contains-original.cc"}]
EOF

# Linux (run from chromium/src)
TEST_DIR=$(pwd)/cef/tools/clang/cef_cpp_rewriter/tests
cat > $TEST_DIR/compile_commands.json << EOF
[{"directory": "$TEST_DIR", "command": "clang++ -std=c++20 -c contains-original.cc", "file": "contains-original.cc"}]
EOF

# Run the tool (--disable-path-filter needed for test files)
third_party/llvm-build/Release+Asserts/bin/cef_cpp_rewriter \
  --disable-path-filter -p $TEST_DIR $TEST_DIR/contains-original.cc
```

### Running Tests

```bash
python3 cef/tools/clang/scripts/test_tool.py cef_cpp_rewriter

# With verbose output
python3 cef/tools/clang/scripts/test_tool.py cef_cpp_rewriter --verbose
```

### Debugging

1. Add `dumpColor()` calls in `run()` to visualize matched AST nodes
2. Return 1 from `main()` to see tool output (run_tool.py hides output on success)
3. Dump AST: `clang++ -Xclang -ast-dump -std=c++20 file.cc`

## Important Constraints

1. **Symlink required**: The tool must be symlinked to `tools/clang/cef_cpp_rewriter`
2. **Do not transform std::string**: Only associative containers have C++20 `.contains()`
3. **Same container check**: Verify `find()` and `end()` are called on the same object
4. **Skip macros**: Don't transform code inside macro expansions
5. **CEF paths only**: Only transform files containing `/cef/` in their path

## Compatibility Requirements

CEF targets C++20. All transformations must work with these minimum toolchain versions:
- GCC 10 (CEF minimum)
- macOS 12.0 deployment target

Features to AVOID (incompatible):
- `std::string::contains()` - C++23, needs GCC 11+
- `std::format` - needs GCC 13+ and macOS 13.3+
- `using enum` - needs GCC 11+

## Adding New Transformations

1. Add a command-line flag in `CefCppRewriter.cpp`:
   ```cpp
   static llvm::cl::opt<bool> EnableNewFeature("new-feature",
       llvm::cl::desc("Enable new feature transformation"),
       llvm::cl::init(true));
   ```

2. Create a new `MatchCallback` class for the transformation

3. Add matcher to `match_finder` in `main()` guarded by the flag

4. Add test files: `tests/newfeature-original.cc` and `tests/newfeature-expected.cc`

5. Run tests: `python3 cef/tools/clang/scripts/test_tool.py cef_cpp_rewriter`
   (test_tool.py generates compile_commands.json automatically)

## Running the Tool on CEF Code

```bash
# Create build directory for clang tooling
# Start with the contents of your CEF build args (e.g., out/Debug_GN_arm64/args.gn)
# and make the following modifications:
#   - Add: enable_precompiled_headers=false (required for clang tools)
#   - Add: dcheck_always_on=true (to cover DCHECK code paths)
#   - Change: is_debug=false (faster builds)
#   - Change: is_component_build=true (faster linking)
mkdir -p out/ClangTool && cp out/Debug_GN_arm64/args.gn out/ClangTool/args.gn
# Edit out/ClangTool/args.gn to add/modify the above settings, then:
gn gen out/ClangTool

# Build only CEF-related gen targets (~9k vs ~39k for all of Chromium)
./cef/tools/clang/scripts/build_gen_targets.sh out/ClangTool

python3 cef/tools/clang/scripts/run_tool.py \
  --generate-compdb \
  -p out/ClangTool \
  cef/libcef cef/libcef_dll cef/tests > /tmp/contains-edits-raw.txt

# Note: --base-dir must match the -p argument since paths are relative to build dir
cat /tmp/contains-edits-raw.txt \
  | python3 cef/tools/clang/scripts/process_edits.py \
  | python3 cef/tools/clang/scripts/apply_edits.py --base-dir out/ClangTool
```

## Critical Reminders

1. **Symlink required**: `test_tool.py` looks for tools in `tools/clang/`, so the symlink must exist
2. **Test naming**: Files must be named `*-original.cc` and `*-expected.cc` (not `-test.cc`)
3. **Do not transform `std::string::find()`**: Only associative containers have C++20 `.contains()`
4. **Incremental builds**: Use `ninja -C third_party/llvm-build/Release+Asserts cef_cpp_rewriter`
5. **Do not re-run build.py**: It deletes everything and rebuilds from scratch (~30 min)
