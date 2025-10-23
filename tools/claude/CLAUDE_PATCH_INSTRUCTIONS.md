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

## Step-by-Step Workflow

### Step 1: Analyze the Patch Output

**If provided with raw `patch_updater.py` output:**

Run the analyzer to get a clear summary:

```bash
# From chromium/src/cef/tools/claude directory:
python3 analyze_patch_output.py patch_output.txt \
  --old-version {old_version} \
  --new-version {new_version}
```

**If the user provides pre-analyzed output, use it directly.**

The summary shows:

- Total patches and success rate
- List of failed patches
- Specific files and line numbers that failed
- Ready-to-run commands for investigation

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

#### A. Understand What Failed

Read the reject file to see what changes couldn't be applied:

```bash
cat {file_path}.rej
```

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
git log --full-history -1 -- {file_path}
```

Then either:

- **File was moved:** Update the patch file to reference the new path
- **File was deleted:** Remove that section from the patch, or find where functionality moved
- **File was split:** Apply changes to the appropriate new files

#### D. Resave the Patch

After fixing the Chromium source files, regenerate the patch:

```bash
# From chromium/src/cef/tools directory:
python patch_updater.py --resave --patch {patch_name_without_extension}
```

Example:
```bash
# From chromium/src/cef/tools directory:
python patch_updater.py --resave --patch views_widget
```

**This will:**

- Regenerate the patch file based on your manual changes
- Update line numbers and offsets
- Validate the patch can now be applied

**If the resave succeeds:** Move to the next patch.

**If the resave fails:** You have more failures to fix in this patch. Repeat step 3.

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

After all patches are fixed, verify everything applies cleanly:

```bash
# From chromium/src/cef/tools directory:
python patch_updater.py
```

This should complete with no `!!!! WARNING:` messages.

## Time Expectations

Based on typical Chromium updates:

- **Simple patches** (context shifts, minor refactoring): 5-15 minutes each
- **Complex patches** (file moves, major refactors, API changes): 30-60 minutes each
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

1. Use `git log --full-history -1 -- {old_path}` to find what happened
2. Visit `https://crrev.com/{commit_hash}` to see the change
3. Edit the patch file in a text editor to update the file paths
4. Resave the patch

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

1. **Only modify Chromium source files** (outside `cef/` directory) when applying patch changes
2. **Never skip patches** - every failed patch must be fixed
3. **One patch at a time** - don't try to fix multiple patches simultaneously
4. **Always resave** after manual fixes - never edit patch files directly (except for path changes)
5. **Verify context** - make sure the CEF changes make sense in the new Chromium code
6. **Preserve CEF intent** - understand what the patch is trying to accomplish, not just mechanically apply it

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
python3 analyze_patch_output.py {output_file} --fix-plan
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
git log --full-history -1 -- {file_path}
git show {commit_hash}
# Or visit: https://crrev.com/{commit_hash}
```

### Resave individual patch:
```bash
# From chromium/src/cef/tools directory:
python patch_updater.py --resave --patch {patch_name}
```

### Resave all patches:
```bash
# From chromium/src/cef/tools directory:
python patch_updater.py --resave
```

### Verify all patches apply:
```bash
# From chromium/src/cef/tools directory:
python patch_updater.py
```

### Add files to existing patch:
```bash
# From chromium/src/cef/tools directory:
python patch_updater.py --resave --patch {name} --add {path1} --add {path2}
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

1. ✓ `python patch_updater.py` completes without warnings
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

✓ Fixed runhooks successfully

Moving to patch 2/9: views_widget

[Continues systematically...]
```

## Tips for Efficiency

1. **Use the analyzer** - Don't parse raw output manually
2. **Read reject files carefully** - They tell you exactly what to add
3. **Check Chromium changes** - Understand why it failed before fixing
4. **Resave frequently** - After each patch, not all at once
5. **Keep notes** - Track what you did for each patch
6. **Pattern recognition** - Similar patches often fail for similar reasons

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

**"I'm not sure if my fix is correct"**

- Does it preserve the CEF functionality? (Look at what the patch is trying to do)
- Does it make sense in the context of the new Chromium code?
- You can optionally test compilation (see Testing Patches below)

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

## Reference: Reject File Format Details

A reject file shows hunks that couldn't be applied:

```
--- ui/views/widget/widget.cc
+++ ui/views/widget/widget.cc
@@ -480,7 +480,8 @@ void Widget::Init(InitParams params) {
   }

   params.child |= (params.type == InitParams::TYPE_CONTROL);
-  is_top_level_ = !params.child;
+  is_top_level_ = !params.child ||
+                  params.parent_widget != gfx::kNullAcceleratedWidget;
   is_headless_ = params.ShouldInitAsHeadless();
```

**How to read this:**

- `--- ui/views/widget/widget.cc` = File being patched
- `@@ -480,7 +480,8 @@` = Location (old line 480, new line 480)
- Lines with no prefix = Context for finding the right location
- `-  is_top_level_ = !params.child;` = Remove this line
- `+  is_top_level_ = !params.child ||` = Add this line (and the next)

**How to apply:**

1. Open `ui/views/widget/widget.cc`
2. Search for the context: `params.child |= (params.type == InitParams::TYPE_CONTROL);`
3. Find the line: `is_top_level_ = !params.child;`
4. Replace it with the two-line version from the reject file

---

**You are now ready to fix CEF patches! Work systematically, one patch at a time, and report progress as you go.**
