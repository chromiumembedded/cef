# CEF Clang Tools

This directory contains clang-based tools for CEF code modernization and refactoring.

## Tools

### cef_cpp_rewriter

A clang AST-based rewriter tool for automating C++20 modernization of CEF code.

**Supported transformations:**

- `.contains()` for associative containers
- Structured bindings for range-for loops
- Iterator loops to range-for
- DISALLOW_COPY_AND_ASSIGN replacement

See [TRANSFORMS.md](TRANSFORMS.md) for complete documentation of all transformations, command-line flags, and examples.

## Prerequisites

- Chromium checkout with CEF
- **Python 3.10+** (Chromium's build scripts require it)
- ~30 minutes for initial clang bootstrap

**Note:** CEF is a separate git repository within chromium/src. Chromium's clang
tool scripts (`run_tool.py`, `test_tool.py`, `apply_edits.py`) use git commands
that don't work across repository boundaries. This directory provides CEF-specific
replacements: `run_tool.py`, `test_tool.py`, and `apply_edits.py`.

### Python Version

macOS ships with Python 3.9 which is too old. Install Python 3.12 via Homebrew:

```bash
brew install python@3.12
ln -s /opt/homebrew/opt/python@3.12/bin/python3.12 /opt/homebrew/opt/python@3.12/bin/python3
export PATH="/opt/homebrew/opt/python@3.12/bin:$PATH"
python3 --version  # Should show 3.12.x
```

Add the export line to `~/.zshrc` to make it permanent.

## Quick Start

### Initial Setup (one-time, ~30 minutes)

```bash
# From chromium/src directory
./cef/tools/clang/scripts/build_clang_rewriter.sh
```

This will:

1. Create a symlink from `tools/clang/cef_cpp_rewriter` to the CEF tool
2. Bootstrap clang with the tool (~30 minutes)

**Warning:** The clang bootstrap updates `third_party/llvm-build/`, which will
invalidate all existing Chromium/CEF build directories and trigger a full
rebuild the next time you build. Plan accordingly.

### Running the Tool

```bash
# Create a build directory for clang tooling
# Start with the contents of your CEF build args (e.g., out/Debug_GN_arm64/args.gn)
# and make the following modifications:
#   - Add: enable_precompiled_headers=false (required for clang tools)
#   - Add: dcheck_always_on=true (to cover DCHECK code paths)
#   - Change: is_debug=false (faster builds)
#   - Change: is_component_build=true (faster linking)
mkdir -p out/ClangTool
cp out/Debug_GN_arm64/args.gn out/ClangTool/args.gn
# Edit out/ClangTool/args.gn to add/modify the above settings, then:
gn gen out/ClangTool

# Build only CEF-related gen targets (~9k vs ~39k for all of Chromium)
./cef/tools/clang/scripts/build_gen_targets.sh out/ClangTool

# Run the tool on CEF code (all transformations enabled by default)
python3 cef/tools/clang/scripts/run_tool.py \
  -p out/ClangTool \
  cef/libcef cef/libcef_dll cef/tests > /tmp/edits-raw.txt

# Run only specific transformations using --tool-args
python3 cef/tools/clang/scripts/run_tool.py \
  -p out/ClangTool \
  --tool-args="--structured-bindings --contains=false" \
  cef/libcef cef/libcef_dll cef/tests > /tmp/edits-raw.txt

# Or run on all CEF files (omit path arguments)
# python3 cef/tools/clang/scripts/run_tool.py -p out/ClangTool

# Process and apply the edits
# Note: --base-dir must match the -p argument since paths are relative to build dir
cat /tmp/edits-raw.txt \
  | python3 cef/tools/clang/scripts/process_edits.py \
  | python3 cef/tools/clang/scripts/apply_edits.py --base-dir out/ClangTool
```

### Tool Arguments

The `--tool-args` option passes arguments directly to `cef_cpp_rewriter`.
See [TRANSFORMS.md](TRANSFORMS.md) for the complete list of command-line flags.

**Examples:**
```bash
# Run only iterator-loops transformation
--tool-args="--only=iterator-loops"

# Run only contains and structured-bindings
--tool-args="--only=contains,structured-bindings"

# Disable specific transformation (all others still run)
--tool-args="--contains=false"
```

### Incremental Rebuilds

After modifying `CefCppRewriter.cpp`, rebuild quickly with:

```bash
ninja -C third_party/llvm-build/Release+Asserts cef_cpp_rewriter
```

**Do not re-run `build_clang_rewriter.sh`** - it rebuilds everything from scratch.

## Testing

```bash
# Run tool tests
python3 cef/tools/clang/scripts/test_tool.py cef_cpp_rewriter

# With verbose output
python3 cef/tools/clang/scripts/test_tool.py cef_cpp_rewriter --verbose
```

## Compatibility

CEF targets C++20. See [TRANSFORMS.md](TRANSFORMS.md) for minimum toolchain versions.

## Standalone Distribution

The tool can be packaged for use on any C++ project (not just CEF):

```bash
# Package to a target directory
./cef/tools/clang/scripts/package_standalone.sh /path/to/target

# This creates:
# /path/to/target/
# ├── bin/cpp_rewriter           # The tool binary
# ├── lib/clang/<version>/       # Bundled clang headers
# ├── scripts/
# │   ├── run_rewriter.py        # Standalone wrapper
# │   ├── apply_edits.py
# │   └── process_edits.py
# └── README.md
```

The standalone distribution is self-contained and includes bundled clang headers
so it works without requiring a matching system clang installation.

**Usage on any CMake project:**
```bash
cd my-project

# Generate compile_commands.json (requires Ninja or Makefiles generator)
cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build

# Run the rewriter
/path/to/target/scripts/run_rewriter.py -p build src/ \
  | /path/to/target/scripts/apply_edits.py --base-dir build
```

See `standalone/README.md` for complete standalone usage documentation.

## Directory Structure

```
cef/tools/clang/
├── cef_cpp_rewriter/
│   ├── CMakeLists.txt
│   ├── CefCppRewriter.cpp
│   ├── OutputHelper.h
│   ├── OutputHelper.cpp
│   └── tests/
│       ├── contains-original.cc
│       ├── contains-expected.cc
│       ├── contains-negative-original.cc
│       ├── contains-negative-expected.cc
│       ├── disallow-copy-original.cc
│       ├── disallow-copy-expected.cc
│       ├── disallow-copy-negative-original.cc
│       ├── disallow-copy-negative-expected.cc
│       ├── structured-bindings-original.cc
│       ├── structured-bindings-expected.cc
│       ├── structured-bindings-negative-original.cc
│       ├── structured-bindings-negative-expected.cc
│       ├── iterator-loops-original.cc
│       ├── iterator-loops-expected.cc
│       ├── iterator-loops-negative-original.cc
│       └── iterator-loops-negative-expected.cc
├── scripts/
│   ├── apply_edits.py            # Applies edits (no git dependency)
│   ├── build_gen_targets.sh      # Build CEF-related gen targets only
│   ├── build_clang_rewriter.sh   # Bootstrap clang with the tool
│   ├── package_standalone.sh     # Package for standalone use
│   ├── process_edits.py          # Adds markers, deduplicates edits
│   ├── run_tool.py               # Run tool on CEF files
│   └── test_tool.py              # CEF-specific test harness
├── standalone/
│   ├── README.md                 # Standalone usage documentation
│   └── run_rewriter.py           # Standalone wrapper script
├── CLAUDE.md
├── README.md
└── TRANSFORMS.md                 # Transformation documentation
```

## Maintenance

This tool is not regularly tested. It may need updates when:

| Trigger | Likely Fix |
|---------|------------|
| Chromium updates `tools/clang/` infrastructure | Update `CMakeLists.txt`, check `build.py` flags |
| LLVM/Clang API changes | Update AST matcher syntax, check deprecations in Clang doxygen |
| CEF adopts C++23 | Add `flat_map`/`flat_set` container types to matcher |
| Build fails after Chromium update | Compare with current `tools/clang/ast_rewriter/` example |

### Diagnosing Build Failures

If the tool fails to build after a Chromium update:

1. Check if `ast_rewriter` still exists and builds:
    ```bash
    python3 tools/clang/scripts/build.py --bootstrap --extra-tools ast_rewriter
    ```

2. Compare CMakeLists.txt with the current ast_rewriter version:
    ```bash
    diff cef/tools/clang/cef_cpp_rewriter/CMakeLists.txt tools/clang/ast_rewriter/CMakeLists.txt
    ```

3. Check for API changes in Clang's AST matchers (review Clang release notes)

### Cleanup

After running the tool, you may want to remove the symlink:

```bash
rm tools/clang/cef_cpp_rewriter  # Removes symlink only, not CEF source
```

## Resources

- Chromium clang tool documentation: `docs/clang_tool_refactoring.md`
- Chromium example: `tools/clang/ast_rewriter/`
- [AST Matcher Reference](https://clang.llvm.org/docs/LibASTMatchersReference.html)
- [LibTooling Tutorial](https://clang.llvm.org/docs/LibASTMatchersTutorial.html)
