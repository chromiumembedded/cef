# CEF Patch Update Instructions for Claude Code

You are assisting with updating CEF (Chromium Embedded Framework) patches to work with a new Chromium version. This is a critical part of the CEF update process.

## Quick Start

When the user asks you to update patches, they will provide:

- Old Chromium version (e.g., `139.0.7258.0`)
- New Chromium version (e.g., `140.0.7339.0`)
- Output from `patch_updater.py` (either as a file or text)

Your job is to systematically fix all failed patches.

## Understanding the Task

### What are CEF Patches?

CEF maintains patch files in `cef/patch/patches/` that modify Chromium source code to add CEF-specific functionality. When Chromium updates, these patches must be updated because:

- Code at the patched locations has changed
- Files have been moved, renamed, or deleted
- APIs have changed requiring different patch approaches

### The Patch Update Process

1. `patch_updater.py` tries to apply all patches to the new Chromium version
2. Some patches fail when code has changed too much
3. You fix the failures by manually applying the changes
4. You resave the updated patches
5. Repeat until all patches apply cleanly

## CRITICAL: Working Directory Awareness

**BEFORE running ANY command with `cd` or relative paths:**

1. ✓ **Check current directory:** `pwd`
2. ✓ **Determine the correct path** from your current location
3. ✓ **Execute the command** with the correct path

**Never assume your current directory. Always verify first.**

Relative paths like `cd cef/tools/claude` behave differently depending on where you are:
- From `chromium/src`: navigates to `chromium/src/cef/tools/claude` ✓
- From `chromium/src/cef/tools`: tries to navigate to `chromium/src/cef/tools/cef/tools/claude` ✗

## Step-by-Step Workflow

### Step 1: Analyze the Patch Output

**If the user provides pre-analyzed output (patch_analysis.txt), use it directly.**

**If provided with raw `patch_updater.py` output (patch_output.txt):**

Run the analyzer to get a clear summary:

```bash
# From chromium/src/cef/tools/claude directory:
python3 analyze_patch_output.py patch_output.txt \
  --old-version {old_version} \
  --new-version {new_version} \
  --no-color > patch_analysis.txt
```

**If the user hasn't provided either file:**

Ask the user to provide either the raw patch output or run patch_updater.py yourself:

```bash
# From chromium/src/cef/tools directory:
python3 patch_updater.py > claude/patch_output.txt 2>&1

# Then analyze it:
cd claude
python3 analyze_patch_output.py patch_output.txt \
  --old-version {old_version} \
  --new-version {new_version} \
  --no-color > patch_analysis.txt
```

The summary shows:

- Total patches and success rate
- List of failed patches
- **Original file list** for each patch (showing which files had problems ✗ vs. applied successfully ✓)
- **Automatic file movement detection** - when a file is missing, it tries to detect if it was moved/renamed
- Specific files and line numbers that failed
- Ready-to-run commands for investigation

The original file list is especially useful when you need to regenerate a patch - you'll know exactly which files should be included.

**File Movement Detection:**
When a file is missing, the analyzer automatically detects simple renames and directory moves. If detected:
```
✗ chrome/browser/ui/views/frame/browser_view_layout.cc (FILE MISSING)
   → Detected new location: chrome/browser/ui/views/frame/layout/browser_view_layout.cc
```

**Note:** Detection only works for simple renames. File deletions, splits, merges, and complex refactorings will show as missing. Use `git log --full-history` to investigate. See README.md for complete details on detection limitations.

### Step 2: Create a Fix Plan

Create a TODO list with all failed patches:

```
1. Fix patch: runhooks
   - File: build/toolchain/win/setup_toolchain.py (1 hunk failed)
2. Fix patch: views_widget
   - File: ui/views/widget/widget.cc (1 hunk failed)
...
```

Work through patches **one at a time** to avoid confusion.

### Step 3: Fix Each Patch

For each failed patch, follow this process:

#### ⚠️ CRITICAL WARNING: Understanding .cefbak Files

You will see `.cefbak` files in the working directory. **These contain OLD code from BEFORE the Chromium update!**

**DO NOT restore from .cefbak files** - they will corrupt your patch with code from the previous Chromium version.

```bash
# ❌ WRONG - This will break everything!
cp chrome/browser/ui/browser.cc.cefbak chrome/browser/ui/browser.cc

# ✅ CORRECT - Only use .rej files to see what changes to apply
cat chrome/browser/ui/browser.cc.rej
```

**What are .cefbak files?**

- Created by `patch_updater.py` when it backs up files before reverting them
- Contain CEF-modified code from Chromium version **{old_version}** (the OLD version)
- Used internally by patch_updater.py for its own operations
- **NEVER manually restore these files**

**What should you use instead?**

- `.rej` files - show what CEF changes need to be applied
- `git diff` - compare Chromium versions to see what changed
- Current files in working tree (after patch_updater.py applies successful patches)

#### A. Understand What Failed

Read the **COMPLETE** reject file to see what changes couldn't be applied:

```bash
# Read the ENTIRE reject file, not just part of it
cat {file_path}.rej

# Count how many hunks failed - you must fix ALL of them
grep "^@@" {file_path}.rej | wc -l
```

**CRITICAL:** You must apply **ALL** failed hunks, not just some of them. Count the hunks and check them off as you apply each one.

**Reject File Format (Unified Diff):**
```
--- chrome/browser/ui/views/find_bar_host.cc
+++ chrome/browser/ui/views/find_bar_host.cc
@@ -614,6 +614,14 @@ gfx::Rect FindBarHost::GetDialogPosition(...)
   gfx::Rect find_bar_bounds = browser_view_->GetFindBarBoundingBox();
+
+#if BUILDFLAG(ENABLE_CEF)
+  if (browser_view_->browser() && browser_view_->browser()->cef_delegate()) {
+    browser_view_->browser()->cef_delegate()->UpdateFindBarBoundingBox(
+        &find_bar_bounds);
+  }
+#endif
+
   if (find_bar_bounds.IsEmpty()) {
```

**Interpretation:**

- Lines with no prefix = context (unchanged code)
- Lines with `+` = changes CEF wants to add
- Lines with `-` = changes CEF wants to remove
- `@@ -614,6 +614,14 @@` = hunk location and size

#### B. Understand What Changed in Chromium

See what changed in the Chromium file between versions:

```bash
# From chromium/src directory:
# Replace {old_version} and {new_version} with actual tags (e.g., 139.0.7258.0, 140.0.7339.0)
git diff --no-prefix \
  refs/tags/{old_version}...refs/tags/{new_version} \
  -- {file_path}
```

This shows you:

- Why the patch failed (code was moved/changed/removed)
- Where to apply the CEF changes in the new code
- If the approach needs to change

#### C. Apply the Fix

**For Failed Hunks (most common):**

1. Read the current Chromium file to see its state
2. Find the context from the reject file in the current file
3. Manually add the CEF changes (lines with `+`) at the correct location
4. Remove any lines marked with `-` if present

**Example:**

Reject file shows CEF wants to add code after this line:
```cpp
gfx::Rect find_bar_bounds = browser_view_->GetFindBarBoundingBox();
```

Find that line in the current file and add the CEF code:
```cpp
gfx::Rect find_bar_bounds = browser_view_->GetFindBarBoundingBox();

#if BUILDFLAG(ENABLE_CEF)
  if (browser_view_->browser() && browser_view_->browser()->cef_delegate()) {
    browser_view_->browser()->cef_delegate()->UpdateFindBarBoundingBox(
        &find_bar_bounds);
  }
#endif

if (find_bar_bounds.IsEmpty()) {
```

**For Missing Files:**

When a file is missing, investigate what happened:

```bash
# See what happened to the file (shows renames, deletions, etc.)
git log --full-history -1 --stat -- {file_path}
```

Then either:

- **File was moved/renamed:**

    **Complete workflow:**

    1. **Identify the new location:**

       - Check patch_analysis.txt - it may show "→ Detected new location: {new_path}"
       - Or run: `git log --follow --all --name-status -- {file_path}`

    2. **Read the reject file:** `cat {old_file_path}.rej`

    3. **Apply changes to the NEW location** (use Edit tool, apply ALL hunks)

    4. **Get the complete original file list:**

       - Look at patch_analysis.txt under "Original files in patch (X total)"
       - Shows which files were in the patch (✓ = applied, ✗ = failed)
       - You need ALL of these files in your regenerated patch

    5. **Manually regenerate the entire patch:**
       ```bash
       # From chromium/src directory:
       # Build git diff command with ALL files at their current locations
       git diff --no-prefix --relative -- \
         {successful_file1} \
         {successful_file2} \
         {manually_fixed_file} \
         {new_location_of_moved_file1} \
         > cef/patch/patches/{patch_name}.patch
       ```

    6. **Verify:**
       ```bash
       # File count should match original
       grep "^diff --git" cef/patch/patches/{patch_name}.patch | wc -l
       # Test it applies
       python3 cef/tools/patch_updater.py --patch {patch_name}
       ```

    **Important:**

    - ❌ **DO NOT** use `--add` for moved files (often fails)
    - ❌ **DO NOT** include old file paths in git diff
    - ✅ **DO** use patch_analysis.txt to find complete original file list

- **File was deleted:** (automatic detection won't catch this) Remove that section from the patch, or find where functionality moved
- **File was split:** (automatic detection won't catch this) Apply changes to appropriate new files and manually regenerate

#### D. Resave and Verify the Patch

After fixing the Chromium source files, follow these verification steps:

**1. Resave the patch:**

```bash
# From chromium/src/cef/tools directory:
python3 patch_updater.py --resave --patch {patch_name_without_extension}
```

Example:
```bash
# From chromium/src/cef/tools directory:
python3 patch_updater.py --resave --patch views_widget
```

This will:

- Regenerate the patch file based on your manual changes
- Update line numbers and offsets
- Validate the patch can now be applied

**2. REQUIRED: Verify File Coverage**

Run verify_patch.py to ensure all failed files are included in the regenerated patch:

```bash
# From chromium/src/cef/tools/claude directory:
python3 verify_patch.py patch_output.txt --patch {patch_name} \
  --old-version {old_version} --new-version {new_version}
```

Example:
```bash
# From chromium/src/cef/tools/claude directory:
python3 verify_patch.py patch_output.txt --patch views_widget \
  --old-version 142.0.7444.0 --new-version 143.0.7491.0
```

**This checks:**

- All files that had failures are included in the regenerated patch
- Files that moved are included at their new locations
- Provides clear ✓/✗ status for each file

**What it does NOT check:** Whether the actual content changes from .rej files are correct (next step).

**If verification fails:**

- Check which files are missing from the output
- Verify you included all files in your git diff command
- For moved files, ensure you used the NEW file paths
- Re-run the git diff with the missing files

**Note:** "File NOT in patch" warnings may be expected for deletions, splits, or merges. See README.md for detection limitations.

**3. REQUIRED: Verify Content Changes**

Manually verify that ALL changes from the .rej file(s) are in the new patch:

```bash
# Review what's in the new patch file
cat cef/patch/patches/{patch_name}.patch

# OR review your git diff to see what changes were captured
git diff {file_path}
```

**Check that:**

1. Every `+` line from the `.rej` file is now in the new patch
2. Every `-` line from the `.rej` file is now in the new patch
3. The context matches (the surrounding unchanged lines)

**If ANY changes from the .rej file are missing:** You missed applying some hunks manually. Go back to step 3C, apply the missing changes, and resave again.

**If ALL checks pass (file coverage + content verification):** The patch is complete. Move to the next patch.

### Step 4: Track Progress

Update your TODO list as you complete each patch:

```
✓ 1. Fix patch: runhooks
✓ 2. Fix patch: views_widget
→ 3. Fix patch: chrome_runtime (in progress)
  4. Fix patch: chrome_runtime_views
...
```

Report progress to the user periodically, especially after completing each patch.

### Step 5: Final Verification

After you've fixed all patches from your TODO list, verify that ALL patches now apply cleanly by re-running patch_updater.py and regenerating the analysis:

```bash
# From chromium/src/cef/tools directory:
python3 patch_updater.py > claude/patch_output.txt 2>&1

# From chromium/src/cef/tools/claude directory:
python3 analyze_patch_output.py patch_output.txt \
  --old-version {old_version} \
  --new-version {new_version} \
  --no-color > patch_analysis.txt
```

**Review patch_analysis.txt:**

**Success criteria:**

- "Success rate: 100%" at the top of patch_analysis.txt
- No patches listed under "Failed patches" section
- Summary shows all patches applied successfully

**If you still see failures in patch_analysis.txt:**

- You missed a patch - check which one(s) failed
- Add the remaining failed patches to your TODO list
- Continue fixing them using Step 3

**If successful (100% success rate):**

- All patches now apply cleanly to Chromium {new_version}
- Ready to proceed to Phase 2: Build Error Fixing (see CLAUDE_BUILD_INSTRUCTIONS.md)
- Report completion to the user with summary statistics

## Time Expectations

Based on typical Chromium updates:

- **Simple patches** (context shifts, minor refactoring): few minutes each (mostly automatic)
- **Complex patches** (file moves, major refactors, API changes): 15-30 minutes each (may require user guidance/correction)
- **Total patch fixing**: 1-3 hours typically

  - Minor updates: Usually < 10 patches fail
  - Major updates: Can be 10-20+ patches

Work systematically and don't rush. Understanding the Chromium changes is more important than speed.

## Common Patch Failure Patterns

### Pattern 1: Code Refactoring

**Symptom:** Context lines from reject file don't match current file exactly

**Cause:** Chromium refactored the code (renamed variables, restructured logic)

**Fix:**

- Find the equivalent location in the refactored code
- Apply the CEF changes adapted to the new structure
- May need to update variable names or function calls

### Pattern 2: File Moved/Renamed

**Symptom:** `Skipping non-existing file` or `can't find file to patch`

**Cause:** Chromium moved or renamed the file

**Fix:**

1. Find what happened: `git log --full-history -1 --stat -- {old_path}`
2. Look for "Renamed" or new file location in the output
3. Read the changes that need to be applied: `cat {old_path}.rej`
4. Manually apply those changes to the file at its NEW location
5. Manually regenerate the entire patch (see section C under "For Missing Files" above)

**IMPORTANT:** When files are moved, the `--add` approach often fails. Manually regenerate the patch instead.

### Pattern 3: API Changes

**Symptom:** Hunks fail near method signatures or class definitions

**Cause:** Chromium changed an API (new parameters, return types, etc.)

**Fix:**

- Update CEF patch to match new API signature
- Check if calling code in CEF also needs updates
- May need to add/remove parameters

### Pattern 4: Functionality Removed

**Symptom:** Large sections of code or entire files are missing

**Cause:** Chromium removed or replaced the feature

**Fix:**

- Search for replacement functionality in new Chromium
- May need to completely redesign the patch approach
- Consult with user if CEF loses critical functionality

### Pattern 5: Context Shift

**Symptom:** Many hunks succeed with large offsets before some fail

**Cause:** Significant code was added/removed earlier in the file

**Fix:**

- The failed hunks likely conflict with the changes that caused the offset
- Manually apply those specific hunks
- Resave will automatically correct all offsets

## Working with Different File Types

### C++ Source Files (.cc, .cpp, .h)

- Most common
- Watch for: API changes, refactoring, moved code
- Use IDE or editor to ensure syntax is correct after manual edits

### Build Files (.gn, BUILD.gn)

- Patch failures often due to target reorganization
- Check if dependencies were renamed or moved
- Verify target still exists: `gn ls out/Default //path/to:target`

### Python Scripts (.py)

- Less common to fail
- Usually involve build tooling
- Test the script still runs if possible

### Mojo Files (.mojom)

- Interface definitions
- Changes often cascade to implementation files
- May need to update both .mojom and implementing .cc files

## Important Rules

1. **ALWAYS run `pwd` before using `cd` or relative paths** - Never assume your current directory location. Verify first, then navigate.
2. **Only modify Chromium source files** (outside `cef/` directory) when applying patch changes
3. **Never skip patches** - every failed patch must be fixed
4. **One patch at a time** - don't try to fix multiple patches simultaneously
5. **Always resave** after manual fixes - never manually edit patch files. **Exception:** When files are moved/renamed, manually regenerate the entire patch using `git diff` (see troubleshooting section below)
6. **Verify context** - make sure the CEF changes make sense in the new Chromium code
7. **Preserve CEF intent** - understand what the patch is trying to accomplish, not just mechanically apply it

## Helper Commands Reference

### Analyze patch output:
```bash
# From chromium/src/cef/tools/claude directory:
python3 analyze_patch_output.py {output_file} \
  --old-version {old_version} --new-version {new_version}
```

### Generate systematic fix plan:
```bash
# From chromium/src/cef/tools/claude directory:
python3 analyze_patch_output.py {output_file} --fix-plan \
  --old-version {old_version} --new-version {new_version}
```

### View reject file:
```bash
# From chromium/src directory:
cat {file_path}.rej
```

### See what changed in Chromium:
```bash
# From chromium/src directory:
git diff --no-prefix \
  refs/tags/{old_version}...refs/tags/{new_version} -- {file_path}
```

### Investigate missing file:
```bash
# From chromium/src directory:
# See what happened (shows renames, deletions, etc.)
git log --full-history -1 --stat -- {file_path}

# Or view full commit details
git show {commit_hash}
```

### Resave individual patch:
```bash
# From chromium/src/cef/tools directory:
python3 patch_updater.py --resave --patch {patch_name}
```

### Verify file coverage (REQUIRED after resave):
```bash
# From chromium/src/cef/tools/claude directory:
python3 verify_patch.py patch_output.txt --patch {patch_name} \
  --old-version {old_version} --new-version {new_version}
```

### Resave all patches:
```bash
# From chromium/src/cef/tools directory:
python3 patch_updater.py --resave
```

### Verify all patches apply:
```bash
# From chromium/src/cef/tools directory:
python3 patch_updater.py
```

### Add files to existing patch:
```bash
# From chromium/src/cef/tools directory:
python3 patch_updater.py --resave --patch {name} --add {path1} --add {path2}

# Note: This often fails when files were moved/renamed.
# If it fails, manually regenerate the patch instead (see troubleshooting section)
```

### Create new patch file:
```bash
# From chromium/src directory:
git diff --no-prefix --relative {path1} {path2} > {name}.patch
# Copy to cef/patch/patches/{name}.patch
# Add entry to cef/patch/patch.cfg
```

## Communication Guidelines

### Report Progress

After each patch is fixed, report:
```
✓ Fixed patch: views_widget
  - File: ui/views/widget/widget.cc
  - Issue: Hunk #2 failed due to code refactoring
  - Solution: Applied CEF changes to refactored Init() method
  - Status: Resaved successfully

Remaining: 8 patches
```

### Ask for Help When Needed

If you cannot fix a patch after 3 attempts, or a single issue takes > 5 minutes, ask for help:

```
⚠ Need assistance with patch: chrome_runtime_views

**File:** chrome/browser/ui/views/frame/browser_frame.cc
**Issue:** File is missing entirely
**Investigation:** git log shows file was deleted in commit abc123
**Chromium change:** Functionality moved to browser_widget.cc and browser_frame.h
**Question:** Should I:
  1. Update patch to modify browser_widget.cc instead?
  2. Remove this file from the patch?
  3. Something else?

Attempted fixes:
  1. Searched for equivalent code in browser_widget.cc - found partial match
  2. Tried applying changes there - uncertain if complete
  3. Checked browser_frame.h - some functionality there too
```

### Provide Error Details

When reporting errors, include:

- Patch name
- File path and line number
- Full error message
- What you tried
- Relevant context from Chromium changes

## Success Criteria

Patches are successfully updated when:

1. ✓ `python3 patch_updater.py` completes without warnings
2. ✓ All patches apply cleanly (no `.rej` files created)
3. ✓ Chromium source files contain all necessary CEF modifications
4. ✓ You can explain what each significant change was and why it was needed

## After Patches Are Fixed

Once all patches apply successfully, the next steps are:

1. **Build CEF** - Patches are fixed, but there may be compile errors

   - Use: `autoninja -k 0 -C out/Debug_GN_x64 cef` (from chromium/src)
   - If .gn files changed, run `cef/create_debug.bat` (Windows) or `./cef/create_debug.sh` (Linux/Mac) first

2. **Fix compile errors** - Update CEF code in `cef/` directory to work with new Chromium

   - This is a **separate task** with different instructions
   - See: `CLAUDE_BUILD_INSTRUCTIONS.md` for the build error fixing phase
   - Only modify files in the `cef/` directory during this phase
   - During patch fixing, you only modified Chromium files (outside `cef/`)

3. **Run tests** - Verify functionality still works
4. **Submit changes** - Create PR with the updated patches

The user will guide you through these next steps. Each phase has its own instructions.

## Next Phase

After all patches are successfully fixed, proceed to the build error fixing phase using the instructions in `CLAUDE_BUILD_INSTRUCTIONS.md`.

## Example Session

```
User: Please update patches from 139.0.7258.0 to 140.0.7339.0

Claude: I'll help update the CEF patches. First, let me analyze the patch_updater.py
output to understand what failed.

[Analyzes output]

I found 9 failed patches affecting 20 files. Here's my plan:

TODO:
1. Fix runhooks (1 file)
2. Fix views_widget (1 file)
3. Fix chrome_runtime (1 file)
... (6 more)

Starting with patch 1/9: runhooks

[Reads reject file]
[Checks Chromium changes]
[Applies manual fix]
[Resaves patch]
[Verifies file coverage with verify_patch.py]
[Verifies content changes]

✓ Fixed runhooks successfully - all files included, all changes applied

Moving to patch 2/9: views_widget

[Continues systematically...]
```

## Tips for Efficiency

1. **Use the analyzer** - Don't parse raw output manually
2. **Read reject files carefully** - They tell you exactly what to add
3. **Check Chromium changes** - Understand why it failed before fixing
4. **Resave frequently** - After each patch, not all at once
5. **Always verify** - Run verify_patch.py after every resave to catch missing files early
6. **Keep notes** - Track what you did for each patch
7. **Pattern recognition** - Similar patches often fail for similar reasons

## Troubleshooting

**"I can't find the context from the reject file"**

- The code was significantly refactored
- Search for key function/variable names
- Look at the Chromium diff to see where code moved

**"The patch keeps failing even after my fix"**

- You may have fixed the wrong location
- Check that you're editing the correct file
- Ensure your changes exactly match what the reject file specifies
- Make sure you're in the right branch/version of Chromium

**"Multiple hunks in one file are failing"**

- Fix them all before resaving
- Work top-to-bottom in the file
- Resave once after all hunks in that file are fixed
- **IMPORTANT:** After resaving, verify ALL hunks are in the new patch by comparing with the .rej file

**"The patch applies cleanly but I think I missed some changes"**

- The resaved patch only contains what you actually changed
- If you missed applying some hunks, they're gone from the patch
- Compare the new patch with the original .rej file to verify all changes are present
- Use: `cat cef/patch/patches/{patch_name}.patch` and visually check against the .rej file

**"I'm not sure if my fix is correct"**

- Does it preserve the CEF functionality? (Look at what the patch is trying to do)
- Does it make sense in the context of the new Chromium code?
- You can optionally test compilation (see Testing Patches below)

**"Failed to create patch file: no such path in the working tree"**

**Symptom:** `patch_updater.py --resave --patch {name} --add {new_path}` fails with "no such path in the working tree"

**Cause:** The patch still references old file paths that no longer exist (files were moved/renamed)

**Solution:** Manually regenerate the patch with correct file paths:

1. Check what files are currently in the patch:
   ```bash
   grep "^diff --git" cef/patch/patches/{patch_name}.patch
   ```

2. Check what files you've actually modified:
   ```bash
   git diff --name-only | grep {relevant_pattern}
   ```

3. Create new patch with correct paths:
   ```bash
   # From chromium/src directory:
   # List ALL files that should be in the patch
   git diff --no-prefix --relative {file1} {file2} {file3} ... > /tmp/temp.patch

   # Back up the old patch
   cp cef/patch/patches/{patch_name}.patch cef/patch/patches/{patch_name}.patch.bak

   # Replace with new patch
   cp /tmp/temp.patch cef/patch/patches/{patch_name}.patch
   ```

4. Verify it works:
   ```bash
   cd cef/tools
   python3 patch_updater.py --patch {patch_name}
   ```

## Testing Patches (Optional)

While not required during the patch update phase, you can verify your patches compile:

### Build Configuration

CEF uses a separate build directory. The default is `Debug_GN_x64` (Windows) or `Debug_GN_arm64` (Mac ARM) or equivalent for other platforms.

**If .gn files changed, regenerate build files first:**

```bash
# From chromium/src directory:

# Windows
cef\create_debug.bat

# Linux/Mac
./cef/create_debug.sh
```

**Do not** run `gn clean` or similar commands.

### Build Command

```bash
# From chromium/src directory:

# Build CEF target
autoninja -k 0 -C out/Debug_GN_x64 cef

# Or build a specific target
autoninja -k 0 -C out/Debug_GN_x64 cefclient
```

The `-k 0` flag continues building other targets even if some fail, which is useful during updates.

### When to Test Build

- **Not required** during patch fixing - patches can be fixed independently of compilation
- **Useful** if you're uncertain whether a manual change is correct
- **Required** after all patches are fixed (next phase of the update)

Note: Build errors are a **separate phase** from patch errors. Fix all patches first, then move to fixing build errors.

---

**You are now ready to fix CEF patches! Work systematically, one patch at a time, and report progress as you go.**
