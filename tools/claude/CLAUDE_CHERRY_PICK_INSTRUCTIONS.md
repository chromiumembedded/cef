# CEF Cherry-Pick Instructions

Instructions for cherry-picking commits from upstream CEF that may include patch files.

## Overview

CEF uses a patch system where changes to Chromium source files are stored as `.patch` files in `//cef/patch/patches/`. When cherry-picking commits that modify these patch files, you must:

1. Cherry-pick the commit to get the CEF-specific file changes
2. Apply the corresponding source file changes to the Chromium source
3. Resave the patch to update index hashes
4. Commit with the full original message

**Critical Understanding**: Patch files contain diffs against Chromium source files. When a commit modifies a patch file, it means the corresponding Chromium source file needs to be modified. You cannot just copy the patch file - you must apply the actual changes to the source files, then resave.

## Prerequisites

```bash
cd /path/to/chromium/src/cef
git fetch upstream master
```

Ensure Chromium source files have all existing CEF patches applied before starting.

## Basic Workflow

### Step 1: Identify what the commit changes

```bash
# See which files are modified
git show <commit-sha> --name-only --oneline

# For commits that modify patch files, see what changed in the patch
git diff <commit-sha>^..<commit-sha> -- patch/patches/<name>.patch
```

### Step 2: Cherry-pick the commit

```bash
git cherry-pick <commit-sha> --no-commit
```

### Step 3: Check if patch files were modified

```bash
git diff --cached --name-only | grep '\.patch$'
```

If no patch files were modified, simply commit:
```bash
git commit -m "$(git log -1 --format='%B' <commit-sha>)"
```

### Step 4: For modified patch files - Apply source changes

**Important**: Do NOT just copy the patch file from upstream. You must apply the incremental source changes.

#### Method A: Extract and apply diff from patch (Recommended)

```bash
# Get the patch file content from the commit
git show <commit-sha>:patch/patches/<name>.patch > /tmp/upstream.patch

# Extract diff for a specific source file
# Note: Escape forward slashes in paths (e.g., ui\/views\/widget\.cc)
sed -n '/^diff --git <source-path>/,/^diff --git /p' /tmp/upstream.patch | head -n -1 > /tmp/source.patch

# IMPORTANT: Verify source file is in expected state before applying
# Check what the patch expects as the "before" state (lines starting with -)
grep "^-" /tmp/source.patch | head -10

# Compare with current source file state
grep -n "<expected content>" /path/to/chromium/src/<source-path>

# If the source file doesn't match the patch's expected "before" state:
# - The file may already have the changes (patch will say "Reversed or previously applied")
# - The file may be missing prerequisite patches (apply base patch first)
# - The file may have changes from a later commit (revert and re-apply base patches)

# Dry-run first to verify patch applies cleanly
cd /path/to/chromium/src
patch -p0 --dry-run < /tmp/source.patch

# If dry-run succeeds, apply for real
patch -p0 < /tmp/source.patch
```

If `patch` reports "Reversed (or previously applied) patch detected", the changes are already present - skip applying and proceed to resave.

If the patch only partially applies, use `--reject` to see what failed:
```bash
patch -p0 --reject-file=/tmp/rejections.rej < /tmp/source.patch
cat /tmp/rejections.rej  # Review failed hunks and apply manually
```

#### Method B: Manual edit based on commit diff

For small changes, read the commit diff and manually apply the changes:

```bash
# See exactly what changed in the source file section of the patch
git diff <commit-sha>^..<commit-sha> -- patch/patches/<name>.patch
```

Then edit the Chromium source file to match.

#### When upstream targets a different Chromium version

If the upstream patch is based on a different Chromium version, the diff may not apply cleanly. In this case:

1. Review the upstream commit to understand the intent of the change
2. Manually edit the source file to achieve the same result
3. Resave the patch

The goal is to replicate what the change accomplishes, not necessarily the exact diff.

### Step 5: Resave the patch

```bash
cd /path/to/chromium/src/cef
python3 tools/patch_updater.py --resave --patch <name_without_extension>
```

### Step 6: Verify the resaved patch

**Critical**: Inspect the patch to ensure it has the expected contents.

```bash
# Check what changed in the patch file
git diff patch/patches/<name>.patch

# Verify file count matches upstream
git show <commit-sha>:patch/patches/<name>.patch | grep "^diff --git" | wc -l
cat patch/patches/<name>.patch | grep "^diff --git" | wc -l

# Compare file lists - should have no differences
diff <(git show <commit-sha>:patch/patches/<name>.patch | grep "^diff --git" | sort) \
     <(cat patch/patches/<name>.patch | grep "^diff --git" | sort)
```

**What to check for**:

- **Missing files**: If file count decreased, the resave removed content (source file may have been reverted or is missing CEF changes)
- **Extra files**: If file count increased, unrelated source changes were included (revert those source files and resave)
- **Wrong diff content**: Compare the specific section that should have changed against the upstream version

If the patch is wrong, fix the source file state and resave again.

### Step 7: Stage and commit

```bash
git add patch/patches/<name>.patch <other-cef-files>
git commit -m "$(git log -1 --format='%B' <commit-sha>)"
```

## Cherry-Picking Multiple Commits

When cherry-picking a series of commits that modify the same patch file:

### Order Matters

Apply commits in the order that makes sense for dependencies. If commit B builds on commit A's changes, apply A first.

### Incremental Changes

Each commit should only add its incremental changes to the source files:

```bash
# For each commit in sequence:
# 1. See what THIS commit changed (not the final state)
git diff <commit-sha>^..<commit-sha> -- patch/patches/<name>.patch

# 2. Apply only those incremental changes to source files
# 3. Resave the patch
# 4. Commit
```

### Avoid State Contamination

**Problem**: If you apply source changes from a later commit, then try to resave an earlier commit's patch, the resave will include the later changes.

**Solution**:

- Apply source changes incrementally per commit
- Or, revert source files between commits if needed:
    ```bash
    git checkout HEAD -- <source-file>
    # Then re-apply the base patch
    patch -p0 < cef/patch/patches/<name>.patch
    # Then apply only this commit's changes
    ```

## Commits with Multiple Patch Files

When a single commit modifies multiple `.patch` files:

1. Cherry-pick the commit with `--no-commit`
2. For each modified patch file:
    - Extract and apply the source file changes
    - Resave that specific patch
3. Stage all modified patch files together
4. Create a single commit with the original message

```bash
# List all patch files modified by the commit
git diff <commit-sha>^..<commit-sha> --name-only | grep '\.patch$'

# Process each patch file, then resave all
python3 tools/patch_updater.py --resave --patch <name1> --patch <name2>
```

## Handling Merge Conflicts

### Conflict in patch file headers

Common conflict pattern - index hashes differ:
```
<<<<<<< HEAD
index abc123..def456 100644
=======
index abc123..xyz789 100644
>>>>>>> commit-sha
```

Resolution:

1. Keep either version (the hash will be regenerated when you resave)
2. Ensure the `@@ ... @@` line numbers match what makes sense for the changes
3. Apply the source file changes
4. Resave the patch (fixes the hashes automatically)

### Conflict in patch content

If the actual diff content conflicts:

1. Resolve based on understanding what both sides intended
2. Apply the resolved changes to the source file
3. Resave the patch

## Extracting Diffs from Patch Files

### Single file (with trailing diff)
```bash
sed -n '/^diff --git path\/to\/file\.cc/,/^diff --git /p' patch.patch | head -n -1
```

### Single file (last in patch, no trailing diff)
```bash
sed -n '/^diff --git path\/to\/file\.cc/,$p' patch.patch
```

### Applying extracted patches
```bash
# From chromium/src directory
patch -p0 < /tmp/extracted.patch
```

Note: CEF patches use `path/to/file` format (no `a/` `b/` prefix that standard git patches use), so use `-p0` instead of the usual `-p1`.

## Common Pitfalls

### 1. Copying patch files directly
**Wrong**: `git show <sha>:patch/patches/foo.patch > patch/patches/foo.patch`

This overwrites the entire patch with the upstream version, which may:

- Have different index hashes (causing apply failures)
- Be based on different Chromium source state

**Right**: Apply source changes, then resave.

### 2. Resaving with wrong source state
If source files have changes from commits you haven't reached yet, the resave will be wrong.

### 3. Forgetting to apply source changes
Just cherry-picking the CEF files without updating Chromium source means the patch won't match reality.

### 4. Skipping verification after resave
Always verify the resaved patch (see Step 6) before committing. Catching issues early is much easier than fixing them after multiple commits.

## Recovery When Things Go Wrong

### Reset CEF cherry-pick state
```bash
cd /path/to/chromium/src/cef
git reset HEAD
git checkout -- .
```

### Restore a source file to its base patched state
```bash
cd /path/to/chromium/src
git checkout HEAD -- <source-file>
patch -p0 < cef/patch/patches/<name>.patch
```

### Abort a problematic cherry-pick entirely
```bash
cd /path/to/chromium/src/cef
git cherry-pick --abort  # If cherry-pick is still in progress
# Or manually reset
git reset --hard HEAD
cd /path/to/chromium/src
git checkout HEAD -- <affected-source-files>
```

## When Manual Reimplementation is Better

Sometimes cherry-picking is not the best approach:

- **Significantly different Chromium base**: The upstream change targets a Chromium version with substantial differences from yours
- **Heavily refactored patches**: The patch file structure has changed significantly between versions
- **Conflicting intermediate commits**: Multiple commits touch the same code in incompatible ways
- **Simple changes**: For small, straightforward changes, manually editing may be faster than the extract/apply/resave workflow

In these cases, understand what the upstream commit intended to accomplish and reimplement the changes directly.

## Build Verification (Optional)

For significant changes, verify the build still works before committing:

```bash
cd /path/to/chromium/src/cef

# If GN files (.gn, .gni, BUILD.gn) changed, regenerate build files first
./create_debug.sh    # Linux/Mac
create_debug.bat     # Windows

# Build (replace Debug_GN_x64 with your build directory)
cd /path/to/chromium/src
autoninja -C out/Debug_GN_x64 cef
```

See `//cef/tools/claude/CLAUDE_BUILD_INSTRUCTIONS.md` for more build details.

## Quick Reference

| Task | Command |
|------|---------|
| Fetch upstream | `git fetch upstream master` |
| Cherry-pick without commit | `git cherry-pick <sha> --no-commit` |
| View commit's patch changes | `git diff <sha>^..<sha> -- patch/patches/<name>.patch` |
| Get patch file from commit | `git show <sha>:patch/patches/<name>.patch` |
| Extract single file diff | `sed -n '/^diff --git <path>/,/^diff --git /p' patch \| head -n -1` |
| Apply patch to source | `patch -p0 < /tmp/extracted.patch` (from chromium/src) |
| Dry-run patch | `patch -p0 --dry-run < /tmp/extracted.patch` |
| Apply with reject file | `patch -p0 --reject-file=/tmp/rej.rej < /tmp/extracted.patch` |
| Resave a patch | `python3 tools/patch_updater.py --resave --patch <name>` |
| Resave multiple patches | `python3 tools/patch_updater.py --resave --patch <n1> --patch <n2>` |
| Get full commit message | `git log -1 --format='%B' <sha>` |
| Revert source file | `git checkout HEAD -- <path>` |
| Reset CEF staging | `git reset HEAD && git checkout -- .` |
| Restore source + patch | `git checkout HEAD -- <file> && patch -p0 < cef/patch/patches/<name>.patch` |

## Example: Full Cherry-Pick Workflow

```bash
# Setup
cd /path/to/chromium/src/cef
git fetch upstream master

# Cherry-pick commit that modifies views_widget.patch
git cherry-pick abc123 --no-commit

# Check what source file changes are needed
git diff abc123^..abc123 -- patch/patches/views_widget.patch

# Extract and apply the source changes
git show abc123:patch/patches/views_widget.patch > /tmp/vw.patch
sed -n '/^diff --git ui\/views\/widget\/widget\.cc/,/^diff --git /p' /tmp/vw.patch | head -n -1 > /tmp/widget.patch
cd /path/to/chromium/src
patch -p0 < /tmp/widget.patch

# Resave and commit
cd cef
python3 tools/patch_updater.py --resave --patch views_widget
git add patch/patches/views_widget.patch
git commit -m "$(git log -1 --format='%B' abc123)"
```
