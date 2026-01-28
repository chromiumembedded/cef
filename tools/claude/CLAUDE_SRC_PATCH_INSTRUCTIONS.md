# Applying Chromium Commits to CEF Patches

Instructions for applying commits from Chromium (often from a newer version) to CEF as new or existing patch files.

## Overview

CEF uses a patch system where changes to Chromium source files are stored as `.patch` files in `//cef/patch/patches/`. The patch configuration is defined in `//cef/patch/patch.cfg`.

When you need to backport or apply a Chromium commit to CEF, you either:

1. **Create a new patch** - For changes that don't fit an existing patch category
2. **Add to an existing patch** - For changes related to an existing patch's purpose

## Prerequisites

Before modifying patches, ensure the specific patch you're working with is in the correct state.

**For modifying an existing patch (Option B):**

```bash
cd /path/to/chromium/src

# Check if the patch is applied by examining one of its files
git diff path/to/file/in/patch.cc  # Empty output = file matches HEAD

# If needed, reapply only the specific patch you're working with
cd cef
python3 tools/patch_updater.py --reapply --patch <patch_name>
```

**For creating a new patch (Option A):**

Ensure target files are in their current patched state (other patches that touch those files should be applied). Check with `git status`.

> **Important:** Only work with the specific patch in question. Avoid `--reapply` without `--patch <name>` as it touches all patched files and unnecessarily invalidates the build.

**Useful commands:**

```bash
cd /path/to/chromium/src/cef

# List all existing patches
ls patch/patches/

# See what files a patch modifies
grep "^diff --git" patch/patches/<patch_name>.patch

# See all patch_updater.py options
python3 tools/patch_updater.py --help
```

## Getting the Chromium Commit Diff

Since the Chromium source is available locally, use git to extract the commit:

```bash
cd /path/to/chromium/src

# View commit details
git show <commit-sha> --stat

# View the full diff
git show <commit-sha>

# Save the diff to a file
git show <commit-sha> --format="" > /tmp/chromium_commit.patch
```

If the commit is from a newer Chromium version not in your local history:

```bash
# Option 1: Fetch from upstream (may take a while)
git fetch origin

# Then view/extract as above
git show <commit-sha>
```

```bash
# Option 2: Fetch directly from Gitiles (no local fetch required)
# The ^! suffix means "this commit only" and format=TEXT returns base64-encoded diff
curl -s "https://chromium.googlesource.com/chromium/src/+/<commit-sha>^!/?format=TEXT" \
  | base64 -d > /tmp/chromium_commit.patch

# View the decoded diff
cat /tmp/chromium_commit.patch
```

The Gitiles URL format is:
```
https://chromium.googlesource.com/chromium/src/+/<commit-sha>^!/?format=TEXT
```

Note: The `^!` works as-is in curl from bash. In a browser URL bar or some scripts, encode it as `%5E%21`.

## Option A: Creating a New Patch File

Use this when the change doesn't fit an existing patch category.

### Step 1: Choose a descriptive patch name

Follow existing naming conventions in `patch.cfg`:

- Use lowercase with underscores
- Include issue numbers when relevant (e.g., `win_app_icon_3606`)
- Be descriptive of the change's purpose

### Step 2: Apply the Chromium changes to source files

```bash
cd /path/to/chromium/src

# Apply the commit diff
git apply /tmp/chromium_commit.patch

# Or apply specific files from the diff
git show <commit-sha> -- path/to/file.cc | git apply
```

If the patch doesn't apply cleanly (due to version differences):

```bash
# Try with 3-way merge
git apply --3way /tmp/chromium_commit.patch

# Or apply what you can and manually fix the rest
git apply --reject /tmp/chromium_commit.patch
# Review .rej files and manually apply failed hunks
```

For significant version differences, you may need to manually edit the source files to achieve the same result as the commit.

### Step 3: Create the patch file

```bash
cd /path/to/chromium/src/cef

# Create a new patch with the modified files
# --patch: name without .patch extension
# --add: file paths relative to the patch's target directory
#        (chromium/src by default, or the 'path' value in patch.cfg)
python3 tools/patch_updater.py --resave --patch <new_patch_name> \
  --add path/to/modified/file1.cc \
  --add path/to/modified/file2.h
```

### Step 4: Add entry to patch.cfg

Edit `//cef/patch/patch.cfg` to add the new patch. Each entry should include:

- A comment describing the patch purpose with links to relevant issues
- The `'name'` field (patch filename without `.patch` extension)
- Optional `'path'` field if not targeting chromium/src root
- Optional `'condition'` field for conditional application

Example entry:

```python
  {
    # Brief description of what this patch does.
    # https://github.com/chromiumembedded/cef/issues/XXXX
    # https://chromium-review.googlesource.com/c/chromium/src/+/XXXXXXX
    'name': 'descriptive_name_issue_number',
  },
```

Place the entry in a logical location within the file (group with related patches). Each file should only appear in one patch—never have the same file in multiple patches.

### Step 5: Verify the patch

```bash
# Check the patch file was created correctly
cat patch/patches/<new_patch_name>.patch

# Verify it contains the expected files
grep "^diff --git" patch/patches/<new_patch_name>.patch
```

## Option B: Adding to an Existing Patch File

Use this when the change relates to an existing patch's purpose.

### Step 1: Identify the appropriate existing patch

Review `patch.cfg` comments to find a patch with related functionality. For example:

- Widget-related changes might go in `views_widget`
- Browser plugin changes might go in `chrome_plugins`
- Network service changes might go in `services_network_2622`

### Step 2: Apply the Chromium changes to source files

Same as Option A, Step 2.

### Step 3: Resave the existing patch

```bash
cd /path/to/chromium/src/cef

# If adding new files to the patch
python3 tools/patch_updater.py --resave --patch <existing_patch_name> \
  --add path/to/new/file.cc

# If only modifying files already in the patch
python3 tools/patch_updater.py --resave --patch <existing_patch_name>
```

### Step 4: Update the patch.cfg comment

Add a comment describing the new change to the existing patch entry:

```python
  {
    # Existing description.
    # https://github.com/chromiumembedded/cef/issues/XXXX
    #
    # New change description.
    # https://chromium-review.googlesource.com/c/chromium/src/+/XXXXXXX
    'name': 'existing_patch_name',
  },
```

### Step 5: Verify the updated patch

```bash
# Check what changed in the patch file
git diff patch/patches/<existing_patch_name>.patch

# Verify file count
grep "^diff --git" patch/patches/<existing_patch_name>.patch | wc -l
```

## Handling Version Differences

When the Chromium commit targets a different version than your checkout:

### The code context has changed

If surrounding code has changed, the patch won't apply directly:

1. Understand the intent of the original commit
2. Find the equivalent location in your version
3. Manually apply the changes
4. Resave the patch

### APIs have changed

If the commit uses APIs that don't exist in your version:

1. Check if equivalent functionality exists
2. Adapt the implementation to use available APIs
3. Document any differences in the patch.cfg comment

### The file has been moved or renamed

```bash
# Find where the file exists in your version
find . -name "original_filename.cc" 2>/dev/null

# Or search for unique content from the file
grep -r "unique_function_name" --include="*.cc"
```

## Verifying the Patch Works

### Build verification

```bash
cd /path/to/chromium/src/cef

# If GN files (.gn, .gni, BUILD.gn) changed, regenerate build files first
./create_debug.sh    # Linux/Mac
create_debug.bat     # Windows

# Build (replace Debug_GN_x64 with your build directory)
cd /path/to/chromium/src
autoninja -C out/Debug_GN_x64 cef
```

### Functional verification

After building, verify the change works as intended:

```bash
# Run CEF tests
out/Debug_GN_x64/ceftests

# Or test manually with the sample browser
out/Debug_GN_x64/cefclient
```

### Test the patch can be reverted and reapplied

```bash
cd /path/to/chromium/src/cef

# Revert the patch
python3 tools/patch_updater.py --revert --patch <patch_name>

# Verify source files are reverted
git status

# Reapply the patch
python3 tools/patch_updater.py --reapply --patch <patch_name>

# Verify source files have the changes again
git status
```

## patch.cfg Reference

### Entry format

```python
{
  # Comment describing the patch.
  # Links to issues and code reviews.
  'name': 'patch_name',           # Required: filename without .patch
  'path': 'relative/path',        # Optional: repo root (default: chromium/src)
  'condition': 'ENV_VAR_NAME',    # Optional: only apply if this env var is set
},
```

The `condition` field checks if an environment variable **exists** (is set). The patch is applied only if the variable is present in the environment, regardless of its value (even an empty string counts as set).

### Examples from existing patches

**Simple patch targeting chromium/src:**

```python
{
  # Support component builds (GN is_component_build=true).
  # https://github.com/chromiumembedded/cef/issues/1617
  'name': 'component_build',
},
```

**Patch with custom path (targeting a subdirectory):**

```python
{
  # Add v8_used_in_shared_library GN arg.
  # https://issues.chromium.org/issues/336738728
  'name': 'v8_build',
  'path': 'v8'
},
```

**Patch with multiple related changes:**

```python
{
  # Fix white flash during browser creation.
  # https://github.com/chromiumembedded/cef/issues/1984
  #
  # Windows: Fix crash during window creation.
  # https://bugs.chromium.org/p/chromium/issues/detail?id=761389
  #
  # Add RWHVAura::SetRootWindowBoundsCallback.
  # https://github.com/chromiumembedded/cef/issues/3920
  'name': 'renderer_host_aura',
},
```

## Quick Reference

| Task | Command |
|------|---------|
| View Chromium commit | `git show <sha>` |
| Save commit as patch | `git show <sha> --format="" > /tmp/commit.patch` |
| Fetch from Gitiles | `curl -s "https://chromium.googlesource.com/chromium/src/+/<sha>^!/?format=TEXT" \| base64 -d` |
| Apply patch to source | `git apply /tmp/commit.patch` |
| Apply with 3-way merge | `git apply --3way /tmp/commit.patch` |
| Apply with reject files | `git apply --reject /tmp/commit.patch` |
| Create new CEF patch | `python3 tools/patch_updater.py --resave --patch <name> --add <file>` |
| Resave existing patch | `python3 tools/patch_updater.py --resave --patch <name>` |
| Reset patch to original | `python3 tools/patch_updater.py --patch <name>` |
| Revert a patch | `python3 tools/patch_updater.py --revert --patch <name>` |
| Reapply a patch | `python3 tools/patch_updater.py --reapply --patch <name>` |

## Starting Over

If you make a mistake and need to reset:

```bash
cd /path/to/chromium/src

# Discard changes to specific source files
git checkout HEAD -- path/to/file.cc

# Or reset a patch to its original state (automatically reverts and reapplies)
cd cef
python3 tools/patch_updater.py --patch <patch_name>
```

## Common Issues

### Patch creates empty file

If the patch file is empty or missing expected content, the source file changes weren't present when resaving. Ensure:

1. You applied the changes to the source files
2. You're in the correct directory when running patch_updater.py
3. The `--add` paths are correct (relative to chromium/src)

### Patch includes unrelated changes

If the patch includes changes you didn't intend:

1. Revert the source files: `git checkout HEAD -- <file>`
2. Reapply only the base patch: `python3 tools/patch_updater.py --reapply --patch <name>`
3. Apply only your intended changes
4. Resave the patch

### File already exists in another patch

Each file must only appear in one patch. If the file you need to modify is already patched:

1. Check which patch modifies the file: `grep -l "path/to/file" patch/patches/*.patch`
2. Add your changes to that existing patch instead (Option B)

See `//cef/tools/claude/CLAUDE_BUILD_INSTRUCTIONS.md` for build details.
