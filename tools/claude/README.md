# Claude Code Tools for CEF Development

This directory contains tools and instructions for using Claude Code to assist with CEF (Chromium Embedded Framework) development.

***
[TOC]
***

## Quick Navigation

**Are you updating CEF to a new Chromium version?**

→ See [CHROMIUM_UPDATE.md](CHROMIUM_UPDATE.md) for the complete workflow:

- Fixing patches that fail to apply
- Fixing build errors from API changes
- Using analyzer scripts and verification tools

**Are you developing a CEF application using a binary distribution?**

→ See [CLIENT_DEVELOPMENT.md](CLIENT_DEVELOPMENT.md) for implementing features:

- Adding custom URL schemes
- JavaScript-to-C++ communication
- Network request interception
- Popup window customization

**Are you writing or debugging CEF unit tests?**

→ See [CLAUDE_TEST_INSTRUCTIONS.md](CLAUDE_TEST_INSTRUCTIONS.md) for working with ceftests:

- Running existing unit tests
- Creating new tests for CEF features
- Debugging test failures and flakiness
- Using TestHandler, RoutingTestHandler, and TestServer

## Files in This Directory

### Documentation

- **[CHROMIUM_UPDATE.md](CHROMIUM_UPDATE.md)** - Complete guide for updating CEF to new Chromium versions
- **[CLIENT_DEVELOPMENT.md](CLIENT_DEVELOPMENT.md)** - Complete guide for CEF client application development
- **[CLAUDE_PATCH_INSTRUCTIONS.md](CLAUDE_PATCH_INSTRUCTIONS.md)** - Detailed instructions for Claude agents fixing patch failures
- **[CLAUDE_BUILD_INSTRUCTIONS.md](CLAUDE_BUILD_INSTRUCTIONS.md)** - Detailed instructions for Claude agents fixing build errors
- **[CLAUDE_CLIENT_INSTRUCTIONS.md](CLAUDE_CLIENT_INSTRUCTIONS.md)** - Detailed instructions for Claude agents implementing features in CEF applications
- **[CLAUDE_TEST_INSTRUCTIONS.md](CLAUDE_TEST_INSTRUCTIONS.md)** - Detailed instructions for Claude agents creating and running CEF unit tests
- **[CLAUDE_WIKI_INSTRUCTIONS.md](CLAUDE_WIKI_INSTRUCTIONS.md)** - Bitbucket wiki formatting guidelines for Claude agents
- **[CLAUDE.md](CLAUDE.md)** - General Claude Code instructions for Chromium/CEF codebase
- **[README.md](README.md)** - This file

### Python Tools

- **[analyze_build_output.py](analyze_build_output.py)** - Analyze and format ninja build output with error index
- **[analyze_patch_output.py](analyze_patch_output.py)** - Analyze and format `patch_updater.py` output with file movement detection
- **[patch_utils.py](patch_utils.py)** - Shared utilities for patch analysis tools (file movement detection)
- **[verify_patch.py](verify_patch.py)** - Verify that regenerated patches include all changes from reject files

### Test Files

- **[analyze_build_output_test.py](analyze_build_output_test.py)** - Unit tests for analyze_build_output.py
- **[analyze_patch_output_test.py](analyze_patch_output_test.py)** - Unit tests for analyze_patch_output.py
- **[patch_utils_test.py](patch_utils_test.py)** - Unit tests for patch_utils.py
- **[verify_patch_test.py](verify_patch_test.py)** - Unit tests for verify_patch.py

## Common Setup

For Chromium update workflows, run the setup script:

```bash
cd chromium/src/cef/tools
python3 setup_claude.py
```

This copies `CLAUDE.md` to the project root (`chromium/src/CLAUDE.md`), enabling Claude Code to understand the Chromium/CEF codebase structure.

## Tips for Working with Claude Code

These tips apply to both Chromium updates and client development:

### 1. Provide Context

Always include relevant version numbers or distribution details:

**For Chromium updates:**
```
Old version: 139.0.7258.0
New version: 140.0.7339.0
```

**For client development:**
```
CEF binary distribution: cef_binary_142.0.10+g29548e2+chromium-142.0.7444.135_macosarm64
```

### 2. Attach Relevant Files

Use `@filename` to attach files Claude should read:

- `@cef/tools/claude/patch_analysis.txt` - Analyzed patch output
- `@cef/tools/claude/build_analysis.txt` - Analyzed build output
- `@cef/tools/claude/CLAUDE_CLIENT_INSTRUCTIONS.md` - Client development guide
- `@cef/tools/claude/CLAUDE_TEST_INSTRUCTIONS.md` - CEF unit testing guide

**Tip:** In large directory structures like `chromium/src`, the `@` syntax can be slow. Use `//` syntax for workspace-relative paths instead (e.g., `//cef/tools/claude/patch_analysis.txt`).

### 3. Let Claude Work Systematically

Don't interrupt mid-stream unless necessary. Claude will:

- Create TODO lists
- Work through items one by one
- Report progress regularly
- Ask for help when stuck

### 4. Use Incremental Prompts

For large tasks, you can guide Claude incrementally:

**For Chromium updates:**
```
1. "Start fixing patches using @CLAUDE_PATCH_INSTRUCTIONS.md"
2. [Claude works for a while]
3. "Good progress. Continue with the remaining patches."
4. [Claude finishes]
5. "Now let's fix build errors using @CLAUDE_BUILD_INSTRUCTIONS.md"
```

**For unit testing:**
```
1. "Create tests for the new feature using @CLAUDE_TEST_INSTRUCTIONS.md"
2. [Claude creates tests]
3. "Run the tests and fix any failures"
4. [Claude debugs and fixes]
5. "Good, now verify they work in both Chrome and Alloy styles"
```

### 5. Ask for Summaries

After a session:
```
Please summarize all the changes you made, organized by category.
```

Claude can provide a detailed summary for documentation or commit messages.

### 6. Use the Analyzers Proactively

For Chromium updates, run the analyzers yourself first to understand the scope:

**For patches:**
```bash
# From chromium/src/cef/tools/claude directory:
python3 analyze_patch_output.py patch_output.txt \
  --old-version 139.0.7258.0 \
  --new-version 140.0.7339.0 \
  --no-color > patch_analysis.txt
```

**For build errors:**
```bash
# From chromium/src/cef/tools/claude directory:
python3 analyze_build_output.py build_output.txt \
  --old-version 139.0.7258.0 \
  --new-version 140.0.7339.0 \
  --no-color > build_analysis.txt
```

Then share the analysis files with Claude.

## Getting Help

### For Issues with Tools

- Check [CHROMIUM_UPDATE.md](CHROMIUM_UPDATE.md), [CLIENT_DEVELOPMENT.md](CLIENT_DEVELOPMENT.md), or [CLAUDE_TEST_INSTRUCTIONS.md](CLAUDE_TEST_INSTRUCTIONS.md) for usage examples
- Read the instruction files ([CLAUDE_PATCH_INSTRUCTIONS.md](CLAUDE_PATCH_INSTRUCTIONS.md), [CLAUDE_BUILD_INSTRUCTIONS.md](CLAUDE_BUILD_INSTRUCTIONS.md), [CLAUDE_CLIENT_INSTRUCTIONS.md](CLAUDE_CLIENT_INSTRUCTIONS.md), [CLAUDE_TEST_INSTRUCTIONS.md](CLAUDE_TEST_INSTRUCTIONS.md))
- Check analyzer script help: `python3 analyze_patch_output.py --help`

### For Issues with CEF

- CEF wiki: https://bitbucket.org/chromiumembedded/cef/wiki/Home
- CEF forums: https://magpcss.org/ceforum/
- CEF issue tracker: https://github.com/chromiumembedded/cef/issues

### For Issues with Claude Code

- Claude Code documentation: https://docs.claude.com/claude-code
- Improve prompts with more context and specific examples
- Break large tasks into smaller, more manageable pieces

## Contributing

If you improve these tools or instructions:

1. Test with a real Chromium update, client development task, or unit testing workflow
2. Document what you changed and why
3. Update the appropriate guide ([CHROMIUM_UPDATE.md](CHROMIUM_UPDATE.md), [CLIENT_DEVELOPMENT.md](CLIENT_DEVELOPMENT.md), or [CLAUDE_TEST_INSTRUCTIONS.md](CLAUDE_TEST_INSTRUCTIONS.md))
4. Share improvements with the CEF community

## License

These tools are part of the CEF project and follow the same license as CEF (BSD license).
