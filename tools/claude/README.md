# Claude Code Tools for CEF Development

This directory contains tools and instructions for using Claude Code to assist with CEF (Chromium Embedded Framework) development, particularly during Chromium version updates.

***
[TOC]
***

## Prerequisites

- Chromium checkout with CEF integration
- Python 3.x installed
- Basic familiarity with the [Chromium update workflow](https://bitbucket.org/chromiumembedded/cef/wiki/ChromiumUpdate.md)
- Claude Code installed and configured

## Setup

**Before using these tools, run the setup script:**

```bash
cd chromium/src/cef/tools
python3 setup_claude.py
```

This copies `CLAUDE.md` to the project root (`chromium/src/CLAUDE.md`), enabling Claude Code to understand the Chromium/CEF codebase structure. You only need to run this once per environment (or when updating to a new CEF version).

**Create build configuration helper scripts:**

These scripts simplify regenerating build files after .gn file changes.

**Linux/Mac** - Create `chromium/src/cef/create_debug.sh`:

```bash
#!/bin/bash
export GN_OUT_CONFIGS=Debug_GN_arm64
./cef_create_projects.sh
```

> **Note:** Replace `Debug_GN_arm64` with your platform's build config (e.g., `Debug_GN_x64` for x64 builds).

**Windows** - Create `chromium/src/cef/create_debug.bat`:

```bat
set GN_OUT_CONFIGS=Debug_GN_x64
call cef_create_projects.bat
```

Make the scripts executable (Linux/Mac):

```bash
chmod +x cef/create_debug.sh
```

> **Note:** All paths in this document are relative to `chromium/src` unless otherwise specified.

## Overview

Updating CEF to work with a new Chromium version involves two main phases:

1. **Patch Fixing** - Update CEF's patch files to apply cleanly to the new Chromium version
2. **Build Error Fixing** - Update CEF source code to work with new Chromium APIs

These tools help Claude Code systematically work through both phases.

## Files in This Directory

### Documentation
- **`CLAUDE_PATCH_INSTRUCTIONS.md`** - Complete guide for fixing patch failures
- **`CLAUDE_BUILD_INSTRUCTIONS.md`** - Complete guide for fixing build errors
- **`CLAUDE.md`** - General Claude Code instructions for Chromium/CEF
- **`README.md`** - This file

### Python Tools
- **`analyze_patch_output.py`** - Analyze and format `patch_updater.py` output with file movement detection
- **`verify_patch.py`** - Verify that regenerated patches include all changes from reject files
- **`patch_utils.py`** - Shared utilities for patch analysis tools (file movement detection)

### Test Files
- **`analyze_patch_output_test.py`** - Unit tests for analyze_patch_output.py
- **`verify_patch_test.py`** - Unit tests for verify_patch.py
- **`patch_utils_test.py`** - Unit tests for patch_utils.py

## Quick Start

### Phase 1: Fixing Patches

After running `patch_updater.py` and seeing failures:

1. **Capture the output:**
   ```bash
   # From chromium/src/cef/tools directory:
   python3 patch_updater.py > claude/patch_output.txt 2>&1
   ```

2. **Analyze the output and save it:**
   ```bash
   # From chromium/src/cef/tools/claude directory:
   python3 analyze_patch_output.py patch_output.txt \
     --old-version 139.0.7258.0 \
     --new-version 140.0.7339.0 \
     --no-color > patch_analysis.txt
   ```

   **Finding version numbers:**

   - See [Identifying the target Chromium version](https://bitbucket.org/chromiumembedded/cef/wiki/ChromiumUpdate.md#markdown-header-a-identify-the-target-chromium-version)
   - Old version: Check `chrome/VERSION` before the Chromium update
   - New version: Check `chrome/VERSION` after the Chromium update

3. **Prompt Claude Code:**
   ```
   Please update patches from 139.0.7258.0 to 140.0.7339.0
   using the instructions in @cef/tools/claude/CLAUDE_PATCH_INSTRUCTIONS.md

   Here's the patch output analysis: @cef/tools/claude/patch_analysis.txt
   ```

   > **Note:** Use full paths from `chromium/src` when attaching files with `@` syntax.

4. **Claude will:**

   - Analyze the failures
   - Create a TODO list of patches to fix
   - Systematically fix each patch by reading reject files
   - Use `git diff` to understand Chromium changes
   - Apply manual fixes to Chromium source files
   - Resave patches with `--resave --patch <name>`
   - Report progress regularly

### Phase 2: Fixing Build Errors

After all patches are fixed and you're ready to build:

1. **Run the build and capture output:**
   ```bash
   # From chromium/src directory:
   autoninja -k 0 -C out/Debug_GN_x64 cef > cef/tools/claude/build_output.txt 2>&1
   ```

   > **Note:** Replace `out/Debug_GN_x64` with your actual build output directory (common: `out/Debug_GN_x64`, `out/Debug_GN_arm64`)

2. **Prompt Claude Code:**
   ```
   Please fix build errors for the 139.0.7258.0 to 140.0.7339.0 update
   using the instructions in @cef/tools/claude/CLAUDE_BUILD_INSTRUCTIONS.md

   Here's the build output: @cef/tools/claude/build_output.txt
   ```

3. **Claude will:**

   - Parse build errors and create a TODO list
   - Systematically fix each error
   - Use `git diff` to understand Chromium API changes
   - Update CEF source code (files in `cef/` directory only)
   - Rebuild frequently to verify fixes
   - Report progress regularly

## Example Prompts

### Starting Patch Fixing

**With analysis output (see Quick Start):**
```
Please update patches from 139.0.7258.0 to 140.0.7339.0
using the instructions in @cef/tools/claude/CLAUDE_PATCH_INSTRUCTIONS.md

Here's the patch output analysis: @cef/tools/claude/patch_analysis.txt
```

**With raw output:**
```
Please update patches from 139.0.7258.0 to 140.0.7339.0
using the instructions in @cef/tools/claude/CLAUDE_PATCH_INSTRUCTIONS.md

Raw patch output: @cef/tools/claude/patch_output.txt

Please run analyze_patch_output.py first to understand what failed.
```

### Starting Build Error Fixing

**With build output (see Quick Start):**
```
The patches are all fixed. Now please fix the build errors using the
instructions in @cef/tools/claude/CLAUDE_BUILD_INSTRUCTIONS.md

Build output: @cef/tools/claude/build_output.txt
Target: cef
Out dir: Debug_GN_x64
```

**Basic:**
```
Please fix CEF build errors using @cef/tools/claude/CLAUDE_BUILD_INSTRUCTIONS.md

Build target: cef
Output directory: Debug_GN_x64
Chromium version: 139.0.7258.0 → 140.0.7339.0
```

### Mid-Stream Prompts

**Check status:**
```
What's the current status? How many patches/errors are left?
```

**Resume work:**
```
Please continue fixing the remaining patches/errors.
```

**Focus on specific area:**
```
Please focus on the chrome_runtime_views patch - it has the most failures.
```

**Ask for explanation:**
```
Can you explain why the views_widget patch failed and what changes you made?
```

## Analyzer Script Usage

The `analyze_patch_output.py` script transforms verbose patch output into useful formats and detects file movements:

### Required Arguments

- **`--old-version`**: Old Chromium version tag (e.g., `142.0.7444.0`)
- **`--new-version`**: New Chromium version tag (e.g., `143.0.7491.0`)

These enable **file movement detection** - when a file is missing, the script uses `git diff` to detect if it was renamed or moved to a subdirectory.

### Optional Arguments

- **`--project-root`**: Path to Chromium source root (auto-inferred from patch output if not specified)
- **`--no-color`**: Disable color output (use when saving to files)
- **`--fix-plan`**: Generate systematic fix plan instead of summary
- **`--json`**: Output machine-readable JSON format

### Human-Readable Summary (Default)

```bash
# For viewing in terminal (with colors):
python3 analyze_patch_output.py patch_output.txt \
  --old-version 142.0.7444.0 \
  --new-version 143.0.7491.0

# For saving to file (without ANSI colors):
python3 analyze_patch_output.py patch_output.txt \
  --old-version 142.0.7444.0 \
  --new-version 143.0.7491.0 \
  --no-color > patch_analysis.txt
```

**Output:**

- Statistics (success rate, number of failures)
- List of failed patches with specific files and line numbers
- **File movement detection** - Shows when files have moved to new locations
- Ready-to-run commands for investigation
- Clear next steps

### Systematic Fix Plan

```bash
# For viewing in terminal (with colors):
python3 analyze_patch_output.py patch_output.txt \
  --old-version 142.0.7444.0 \
  --new-version 143.0.7491.0 \
  --fix-plan

# For saving to file (without ANSI colors):
python3 analyze_patch_output.py patch_output.txt \
  --old-version 142.0.7444.0 \
  --new-version 143.0.7491.0 \
  --fix-plan \
  --no-color > patch_analysis.txt
```

**Output:**

- Numbered list of patches to fix
- Step-by-step actions for each file
- Commands to run for investigation
- Resave commands for each patch

### JSON Format (for tools/scripts)

```bash
python3 analyze_patch_output.py patch_output.txt \
  --old-version 142.0.7444.0 \
  --new-version 143.0.7491.0 \
  --json
```

**Output:**

- Machine-readable structured data
- Can be parsed by other tools
- Useful for CI/CD integration

### Pipe from patch_updater.py

```bash
# From chromium/src/cef/tools directory:
# Direct to terminal (with colors):
python3 patch_updater.py 2>&1 | python3 claude/analyze_patch_output.py \
  --old-version 142.0.7444.0 \
  --new-version 143.0.7491.0

# Save to file (without colors):
python3 patch_updater.py 2>&1 | python3 claude/analyze_patch_output.py \
  --old-version 142.0.7444.0 \
  --new-version 143.0.7491.0 \
  --no-color > claude/patch_analysis.txt
```

## Patch Verification Tool

The `verify_patch.py` script verifies that all files that failed during patch application are included in the regenerated patch. It uses the patch_output.txt to determine which files failed for the specific patch.

**Important**: This tool checks **file coverage** (did you remember to include all the files?), not **content correctness** (are the changes right?). Manual review of the patch content is still required.

### Usage

```bash
# Basic verification with file movement detection
python3 verify_patch.py patch_output.txt --patch chrome_runtime_views \
  --old-version 142.0.7444.0 \
  --new-version 143.0.7491.0

# With custom project root and patch directory
python3 verify_patch.py patch_output.txt --patch views_widget \
  --old-version 142.0.7444.0 \
  --new-version 143.0.7491.0 \
  --project-root /path/to/chromium/src \
  --patch-dir /path/to/patches
```

### Required Arguments

- **`patch_output_file`**: Path to patch_output.txt file from patch_updater.py (positional argument)
- **`--patch`**: Patch name (without .patch extension)
- **`--old-version`**: Old Chromium version tag (e.g., `142.0.7444.0`)
- **`--new-version`**: New Chromium version tag (e.g., `143.0.7491.0`)

### Optional Arguments

- **`--patch-dir`**: Directory containing patch files (default: `../../patch/patches` relative to script)
- **`--project-root`**: Chromium source root (default: auto-calculated from script location)

### What It Checks

The tool automatically:

- Lists all files in the regenerated patch
- Parses patch_output.txt to find which files failed for the specific patch
- Checks if each failed file path is present in the regenerated patch
- **Detects file movements** using git (when files were moved to new locations)
- Reports whether moved files are included in the patch at their new location
- Returns exit code 0 if all file paths are covered, 1 if any are missing

### What It Does NOT Check

This tool does **not** verify:

- Whether the actual changes from .rej files made it into the patch
- Whether the CEF modifications are correct in the new Chromium context
- Whether the patch will apply successfully

**Manual verification required**: Always review the regenerated patch with `git diff` to ensure the CEF changes are correct and make sense in the updated Chromium code.

### File Movement Detection Limitations

The tool can automatically detect:

- ✅ **Simple renames**: `old/path/file.cc` → `new/path/file.cc`
- ✅ **Directory moves**: File moved to different directory with same basename

The tool **cannot** detect:

- ❌ **File deletions**: If Chromium removed the file entirely, the tool will report it as missing (false positive - the patch is no longer needed)
- ❌ **File splits**: If one file was refactored into multiple files (1 → many), the tool will report it as missing
- ❌ **File merges**: If multiple files were consolidated into one (many → 1)
- ❌ **Complex refactorings**: Code moved between files with different names

**When you see "File NOT in patch" warnings**: Use `git log --full-history` to investigate what happened to the file. If it was deleted or refactored, you may need to:

1. Determine if the CEF changes are still needed
2. Find which new file(s) should contain the changes
3. Manually apply the changes and regenerate the patch

### Example Output

```
Verifying patch: chrome_runtime_views
================================================================================

Files in patch (18):
  ✓ chrome/browser/ui/views/frame/layout/browser_view_layout.cc
  ✓ chrome/browser/ui/views/frame/layout/browser_view_layout.h
  ...

Reject files found (5):
  chrome/browser/ui/views/frame/browser_view_layout.cc (7 hunks)
    ✓ File moved to chrome/browser/ui/views/frame/layout/browser_view_layout.cc and found in patch
      Hunk 1: @@ -150 +151 @@
      ...

================================================================================
✓ Verification PASSED: All reject files are covered by the patch
```

## Tips for Working with Claude Code

### 1. Provide Context

Always include the Chromium version numbers:
```
Old version: 139.0.7258.0
New version: 140.0.7339.0
```

This allows Claude to generate accurate `git diff` commands.

### 2. Attach Relevant Files

Use `@filename` to attach files Claude should read (use full paths from `chromium/src`):

- `@cef/tools/claude/patch_analysis.txt` - Analyzed patch output (recommended)
- `@cef/tools/claude/patch_output.txt` - Raw patch_updater.py output (alternative)
- `@cef/tools/claude/build_output.txt` - Raw build output
- `@chrome/browser/file.cc.rej` - Specific reject file for complex issues

### 3. Let Claude Work Systematically

Don't interrupt mid-stream unless necessary. Claude will:

- Create TODO lists
- Work through items one by one
- Report progress regularly
- Ask for help when stuck

### 4. Use Incremental Prompts

For large tasks, you can guide Claude incrementally:
```
1. "Start fixing patches using @CLAUDE_PATCH_INSTRUCTIONS.md"
2. [Claude works for a while]
3. "Good progress. Continue with the remaining patches."
4. [Claude finishes]
5. "Now let's fix build errors using @CLAUDE_BUILD_INSTRUCTIONS.md"
```

### 5. Ask for Summaries

After a session:
```
Please summarize all the changes you made, organized by category.
```

Claude can provide a detailed summary for documentation or commit messages.

### 6. Use the Analyzer Proactively

Run the analyzer yourself first to understand the scope:
```bash
# From chromium/src/cef/tools/claude directory:
# View in terminal:
python3 analyze_patch_output.py patch_output.txt \
  --old-version 139.0.7258.0 \
  --new-version 140.0.7339.0

# Or save for Claude:
python3 analyze_patch_output.py patch_output.txt \
  --old-version 139.0.7258.0 \
  --new-version 140.0.7339.0 \
  --no-color > patch_analysis.txt
```

Then share relevant sections with Claude if needed.

## Workflow Overview

### Complete Update Workflow

```
1. Update Chromium checkout to new version
   └─> automate-git.py or manual git commands

2. Run patch_updater.py
   └─> Captures failures in patch_output.txt

3. Fix patches with Claude
   ├─> Analyze output with analyze_patch_output.py → patch_analysis.txt
   ├─> Prompt: "@CLAUDE_PATCH_INSTRUCTIONS.md" with @patch_analysis.txt
   ├─> Claude fixes patches one by one
   └─> Verify: patch_updater.py runs clean

4. Build CEF
   └─> Captures errors in build_output.txt

5. Fix build errors with Claude
   ├─> Prompt: "@CLAUDE_BUILD_INSTRUCTIONS.md"
   ├─> Claude fixes errors one by one
   └─> Verify: Build succeeds

6. Test CEF
   └─> Run ceftests, cefclient, manual testing

7. Submit changes
   └─> Create PR with updated patches and fixes
```

### Time Estimates

Based on typical Chromium updates:

- **Patch fixing**: 1-3 hours (depending on number of failures)
    - Minor updates (X.Y.Z): Usually < 10 patches fail
    - Major updates (X.Y.0): Can be 10-20+ patches
- **Build error fixing**: 2-6 hours (depending on API changes)
    - Minor updates: 20-50 errors typical
    - Major updates: 100+ errors possible
- **Total with Claude**: 3-9 hours of active work
    - Claude works faster than manual fixing
    - Most time is reading/understanding Chromium changes

## Troubleshooting

### "Claude seems stuck on one error"

**After 3 attempts or 5 minutes on the same error, Claude should ask for help.**

If Claude doesn't, you can prompt:
```
You've tried this error several times. Can you explain what you've tried
and what's unclear about the Chromium changes?
```

### "Patch keeps failing even after Claude fixes it"

Check that:

1. Files were actually saved (use `Read` tool to verify)
2. Claude is editing the right files (full path check)
3. The fix matches the reject file exactly
4. You're using `--resave --patch <name>`, not `--resave` (resaves all)

### "Too many build errors at once"

Prompt Claude to focus:
```
There are many errors. Please focus on fixing all the "missing include"
errors first, then we'll tackle the API changes.
```

### "Claude modified the wrong files"

Remind Claude of constraints:
```
Remember: During patch fixing, only modify Chromium files (outside cef/).
During build fixing, only modify CEF files (inside cef/).
```

## Integration with Other Tools

### CI/CD Integration

The analyzer script returns appropriate exit codes:

```bash
python3 analyze_patch_output.py output.txt --json | \
  jq -e '.status == "success"' || exit 1
```

Use in CI to detect when patches need manual intervention.

### Pre-commit Hooks

Validate patches before committing:

```bash
# In .git/hooks/pre-commit
python3 cef/tools/patch_updater.py || {
  echo "Patches have failures - run Claude fix workflow"
  exit 1
}
```

### Git Aliases

Add helpful aliases to `.gitconfig`:

```ini
[alias]
  cef-patches = !python3 cef/tools/patch_updater.py
  cef-analyze = !python3 cef/tools/claude/analyze_patch_output.py
```

Usage: `git cef-patches` or `git cef-analyze output.txt`

## Best Practices

1. **Work in a clean branch** - Don't mix patch fixes with other changes
2. **Commit incrementally** - After patches are fixed, before build errors
3. **Document major changes** - Note significant API changes in commit messages
4. **Test thoroughly** - Run ceftests after build succeeds
5. **Save output** - Keep patch_output.txt, patch_analysis.txt, and build_output.txt for reference
6. **Review Claude's changes** - Understand what was changed and why

## Getting Help

### For Issues with Tools

- Check this README for usage examples
- Read the instruction files (CLAUDE_PATCH_INSTRUCTIONS.md, etc.)
- Check analyzer script help: `python3 analyze_patch_output.py --help`

### For Issues with CEF Update Process

- See [ChromiumUpdate.md](https://bitbucket.org/chromiumembedded/cef/wiki/ChromiumUpdate.md) for the overall process
- Check CEF forums: https://magpcss.org/ceforum/
- Check CEF issue tracker: https://github.com/chromiumembedded/cef/issues

### For Issues with Claude Code

- Claude Code documentation: https://docs.claude.com/claude-code
- Improve prompts with more context and specific examples
- Break large tasks into smaller, more manageable pieces

## Contributing

If you improve these tools or instructions:

1. Test with a real Chromium update
2. Document what you changed and why
3. Update this README if needed
4. Share improvements with the CEF community

## License

These tools are part of the CEF project and follow the same license as CEF (BSD license).
