# C++ Rewriter Tool

A clang-based AST rewriter for C++20 modernization. This tool performs
type-aware code transformations that cannot be done with simple regex,
such as replacing `map.find(x) != map.end()` with `map.contains(x)`.

## Distribution Contents

This is a self-contained distribution that includes everything needed to run:

```
cpp_rewriter/
├── bin/
│   └── cpp_rewriter           # The clang-based rewriter tool
├── lib/
│   └── clang/<version>/       # Bundled clang headers (stdarg.h, etc.)
│       └── include/
├── scripts/
│   ├── run_rewriter.py        # Main wrapper script
│   ├── apply_edits.py         # Applies generated edits to files
│   └── process_edits.py       # Deduplicates and formats edits
└── README.md
```

The bundled `lib/clang/` headers are automatically detected and used by the
wrapper script, so the tool works without requiring a matching system clang.

## Prerequisites

- A C++ project with a `compile_commands.json` file
- Python 3.6+

## Generating compile_commands.json

The tool requires a compilation database to understand your project's
build configuration. Here's how to generate one for common build systems:

### CMake

**Important:** `CMAKE_EXPORT_COMPILE_COMMANDS` only works with **Makefile**
and **Ninja** generators. It does NOT work with Xcode or Visual Studio generators.

```bash
# Using Ninja (recommended)
cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build

# Using Unix Makefiles
cmake -G "Unix Makefiles" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build

# compile_commands.json is now in build/
```

If you normally use Xcode/Visual Studio, create a separate build directory
just for tooling:

```bash
# Keep your Xcode build, add a parallel Ninja build for tools
cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build-tools
# Use build-tools/compile_commands.json with the rewriter
```

### Meson

```bash
meson setup build
# compile_commands.json is automatically generated in build/
```

### Bear (for Make-based projects)

```bash
# Install Bear: https://github.com/rizsotto/Bear
bear -- make
# compile_commands.json is generated in current directory
```

### Ninja

```bash
ninja -C build -t compdb > build/compile_commands.json
```

## Usage

### Basic Usage

```bash
# Run on all files in compile_commands.json
./scripts/run_rewriter.py -p /path/to/build | ./scripts/apply_edits.py --base-dir /path/to/build

# Run on specific source directories only
./scripts/run_rewriter.py -p build src/ include/ | ./scripts/apply_edits.py --base-dir build
```

### Filter by Path Pattern

Use `--path-filter` with a substring to process only matching files:

```bash
# Only process files containing "src/core/" in their path
./scripts/run_rewriter.py -p build --path-filter "src/core/" | ./scripts/apply_edits.py --base-dir build
```

### Disable Specific Transformations

```bash
# Disable structured bindings transformation
./scripts/run_rewriter.py -p build --tool-args="--structured-bindings=false" | ./scripts/apply_edits.py --base-dir build

# Run only the .contains() transformation
./scripts/run_rewriter.py -p build --tool-args="--only=contains" | ./scripts/apply_edits.py --base-dir build
```

### Verbose Output

```bash
# Show progress
./scripts/run_rewriter.py -p build -v src/ 2>&1 | tee /tmp/edits.txt
./scripts/apply_edits.py --base-dir build < /tmp/edits.txt
```

## Available Transformations

- `.contains()` for associative containers
- Structured bindings for range-for loops
- Iterator loops to range-for
- DISALLOW_COPY_AND_ASSIGN replacement

All transformations are enabled by default. Use `--flag=false` to disable specific ones,
or `--only=<list>` to run only specific transformations.

See [TRANSFORMS.md](TRANSFORMS.md) for the complete list of command-line flags,
before/after examples, supported containers, and edge cases.

## Troubleshooting

### "compile_commands.json not found"

Make sure you've generated the compilation database. See the
"Generating compile_commands.json" section above.

### "No files found matching..."

- Check that the path filter matches your file paths
- Use `-v` for verbose output to see which files are being processed
- Try running without a path filter first

### Transformations not applying

- The tool only transforms code, not macro expansions
- Some patterns may not match due to unusual formatting
- Use `-v` to see error messages from the tool

## Compatibility

The transformed code uses C++20 features. See [TRANSFORMS.md](TRANSFORMS.md) for
minimum compiler requirements.

## Building from Source

This tool is built as part of the CEF (Chromium Embedded Framework) project.
See the CEF repository for build instructions.
