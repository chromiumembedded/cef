# CEF Build Error Fixing Instructions for Claude Code

You are assisting with fixing CEF build errors after updating to a new Chromium version. This phase comes **after** all patches have been successfully fixed and applied.

**Prerequisites:** All patches must be fixed first. If patches are not yet fixed, see `CLAUDE_PATCH_INSTRUCTIONS.md` to complete the patch fixing phase before proceeding with build errors.

## Quick Start

When the user asks you to fix build errors, they will provide:

- Old Chromium version (e.g., `139.0.7258.0`)
- New Chromium version (e.g., `140.0.7339.0`)
- Build target (usually `cef`, `cefclient`, `cefsimple`, or `ceftests`)
- Output directory (usually `Debug_GN_x64` or `Release_GN_x64`)

Your job is to fix all compile errors in the `cef/` directory so the build succeeds.

## Understanding the Task

### What Phase Is This?

The CEF update process has distinct phases:

1. ✓ **Patch fixing** (COMPLETED) - Updated Chromium source files to apply CEF patches
    - See `CLAUDE_PATCH_INSTRUCTIONS.md` if patches are not yet fixed
2. → **Build error fixing** (CURRENT) - Update CEF source code to work with new Chromium APIs
3. **Testing** (NEXT) - Run tests to verify functionality
4. **Submission** (FINAL) - Submit the changes

### What Changed?

During the Chromium update, APIs changed:

- Function signatures (new/removed parameters)
- Class names and namespaces
- Header file locations
- Enum values
- Method visibility (static → instance, public → private)
- Entire subsystems refactored

Your task is to update CEF code in the `cef/` directory to work with these changes.

## CRITICAL: Working Directory Awareness

**BEFORE running ANY command with `cd` or relative paths:**

1. ✓ **Check current directory:** `pwd`
2. ✓ **Determine the correct path** from your current location
3. ✓ **Execute the command** with the correct path

**Never assume your current directory. Always verify first.**

Relative paths like `cd cef/tools/claude` behave differently depending on where you are:

- From `chromium/src`: navigates to `chromium/src/cef/tools/claude` ✓
- From `chromium/src/cef/tools`: tries to navigate to `chromium/src/cef/tools/cef/tools/claude` ✗

## Important Constraints

### Files You Can Modify

**✓ YES - Modify these:**

- Any file in the `cef/` directory
- This includes `cef/libcef/`, `cef/tests/`, `cef/include/`, etc.

**✗ NO - Never modify these:**

- Any Chromium file outside the `cef/` directory
- Files in `chrome/`, `content/`, `ui/`, `base/`, etc.
- Patch files (those were fixed in the previous phase)

### Build Configuration

**Project Root:** The Chromium source directory (referred to as `chromium/src` in this document)

**Output Directory:** Usually `out/Debug_GN_x64` (Windows), `out/Debug_GN_arm64` (Mac ARM), or equivalent for other platforms

**Build Target:** Usually `cef` (the main CEF library)

## Step-by-Step Workflow

### Step 1: Set Up Build Environment

**If .gn files changed during the update, regenerate build files:**

```bash
# From chromium/src directory:

# Windows
cef\create_debug.bat

# Linux/Mac
./cef/create_debug.sh
```

**Do NOT run `gn clean` or similar commands** - this causes unnecessary rebuilds. During the fixing phase, rebuild individual files (`.o` targets) to verify fixes quickly, not the entire `cef` target until all files compile.

### Step 2: Run Initial Build

Establish a baseline by running the build and capturing output:

```bash
# From chromium/src directory:
autoninja -k 0 -C out/Debug_GN_x64 cef 2>&1 | tee cef/tools/claude/build_output.txt
```

**Command explanation:**

- `autoninja` - Parallel build tool (faster than ninja)
- `-k 0` - Continue building other targets on error (don't stop at first failure)
- `-C out/Debug_GN_x64` - Build directory (replace with your actual output directory)
- `cef` - Target to build
- `2>&1 | tee` - Capture both stdout and stderr to file while showing in terminal

**Expected result:** Build will fail with compile errors. This is normal.

**Note:** If the user already provided `build_output.txt`, you can skip this step.

### Step 3: Analyze Build Errors

**RECOMMENDED: Use the analyzer script**

The user will typically provide a `build_analysis.txt` file created with `analyze_build_output.py`. This file contains:

- Concise error index sorted by file
- Line number references to `build_output.txt` for full error details
- Build targets for rebuilding individual files
- Clear workflow instructions

**If the user did NOT provide `build_analysis.txt`**, create it:
```bash
# From chromium/src/cef/tools/claude directory:
python3 analyze_build_output.py build_output.txt \
  --old-version {old_version} \
  --new-version {new_version} \
  --no-color > build_analysis.txt
```

**Example format:**
```
1. cef/libcef/common/parser_impl.cc - 8 error(s)
   Rebuild: autoninja -C out/Debug_GN_x64 obj/cef/libcef_static/parser_impl.o
   • Line 39 → build_output.txt:1045
   • Line 40 → build_output.txt:1057
```

Use this index to create your TODO list. Read the referenced lines in `build_output.txt` for complete error details.

**Alternative: Parse raw build output manually**

If not using the analyzer, focus on extracting file paths, line numbers, and error messages, then create a TODO list of all unique errors.

### Step 4: Fix Each Error

For each error, follow this process:

#### A. Understand the Error

Read the error message carefully:
```
cef/libcef/browser/browser_host_impl.cc:123:45: error: 'CreateParams' has no member named 'initial_size'
    params.initial_size = gfx::Size(800, 600);
                          ^
```

This tells you:

- **File:** `cef/libcef/browser/browser_host_impl.cc`
- **Line:** 123, column 45
- **Problem:** `CreateParams` struct no longer has `initial_size` member

#### B. Investigate What Changed in Chromium

See what changed in the relevant Chromium file:
```bash
# From chromium/src directory:
# Find which Chromium file defines CreateParams (usually from includes or namespace)

# See what changed between versions
# Replace {old_version} and {new_version} with actual tags (e.g., 139.0.7258.0, 140.0.7339.0)
git diff --no-prefix \
  refs/tags/{old_version}...refs/tags/{new_version} \
  -- {chromium_file_path}
```

Look for:

- Was the member renamed?
- Was it moved to a different struct/class?
- Was the API changed entirely?
- Is there a replacement?

#### C. Apply the Fix

Update the CEF code to work with the new API.

**Common fixes:**

1. **Member renamed:**
    ```cpp
    // Old
    params.initial_size = gfx::Size(800, 600);

    // New (if renamed to 'size')
    params.size = gfx::Size(800, 600);
    ```

2. **Member removed:**
    ```cpp
    // Old
    params.initial_size = gfx::Size(800, 600);

    // New (if size is set differently now)
    params.SetInitialSize(gfx::Size(800, 600));
    // Or remove if no longer needed
    ```

3. **API changed:**
    ```cpp
    // Old
    auto* browser = CreateBrowser(params);

    // New (if signature changed)
    auto* browser = CreateBrowser(params, browser_context);
    ```

See "Common Build Error Patterns" section for more examples.

#### D. Rebuild to Verify

After each fix (or batch of related fixes), **rebuild just that file** (not the entire `cef` target):

```bash
# From chromium/src directory:
# Use the build target from the error index or FAILED line
autoninja -C out/Debug_GN_x64 obj/cef/libcef_static/file.o
```

**Examples:**
```bash
# For cef/libcef/browser/browser_host_impl.cc:
autoninja -C out/Debug_GN_x64 obj/cef/libcef_static/browser_host_impl.o

# For cef/libcef/common/parser_impl.cc:
autoninja -C out/Debug_GN_x64 obj/cef/libcef_static/parser_impl.o
```

**If using build_analysis.txt**, the correct rebuild command is shown for each file.

**If the file compiles successfully:** Move to the next file.

**If the error persists or new errors appear:**

- Review your fix
- Check if you modified the right file/line
- Consider if there are related changes needed

**Special cases:**

- **No build target available:** If `build_analysis.txt` shows "(no build target available)", the error didn't have a FAILED line. Rebuild the entire `cef` target to verify: `autoninja -C out/Debug_GN_x64 cef`

- **Header file errors (.h):** If you fix a header file, you may need to rebuild all .cc files that include it. The safest approach is to rebuild the entire `cef` target after fixing header files, or rebuild multiple related .o files.

**IMPORTANT:** Do NOT rebuild the entire `cef` target until ALL individual files compile successfully. Rebuilding individual files is much faster and provides immediate feedback on your fixes.

### Step 5: Track Progress

Update your TODO list as you fix errors:

```
TODO:
✓ 1. Fix error in cef/libcef/browser/browser_host_impl.cc:123
✓ 2. Fix error in cef/libcef/renderer/render_frame_observer.cc:456
→ 3. Fix error in cef/libcef/common/request_impl.cc:789 (in progress)
  4. Fix error in cef/libcef/browser/navigation_handler.cc:234
...
```

Report progress to the user periodically (e.g., every 5-10 errors fixed, or every 10 minutes).

### Step 6: Handle Difficult Errors

**If you cannot fix an error after 3 attempts, or if it takes > 5 minutes:**

Stop and ask for help with these details:

```
⚠ Need assistance with build error

**File:** cef/libcef/browser/browser_host_impl.cc:123
**Line:** 123
**Error:** 'CreateParams' has no member named 'initial_size'

**Chromium change:** The CreateParams struct was refactored in commit abc123.
The 'initial_size' member was removed, and size is now set via SetBounds().

**Attempted fixes:**
1. Tried using params.size instead - error: no member named 'size'
2. Tried params.SetInitialSize() - error: no such method
3. Looked at Chromium diff - CreateParams significantly restructured

**Question:** How should CEF set the initial browser size in the new API?
```

### Step 7: Final Verification

Once all individual files compile without errors, rebuild the entire `cef` target to ensure success:

```bash
# From chromium/src directory:
autoninja -k 0 -C out/Debug_GN_x64 cef 2>&1 | tee cef/tools/claude/build_output.txt
```

**Success criteria:**

- Build completes without errors
- Output shows completion (e.g., "ninja: build stopped", "Done.", or similar platform-specific message)
- Exit code is 0
- Zero compile errors
- Warnings are acceptable (but note any new warnings)

**If the final rebuild fails with many errors:**

This can happen due to linker errors, template instantiation errors, or errors in files not in the original build output.

1. **Analyze the new errors:**
    ```bash
    # From chromium/src/cef/tools/claude directory:
    python3 analyze_build_output.py build_output.txt \
     --old-version {old_version} \
     --new-version {new_version} \
     --no-color > build_analysis.txt
    ```

2. **Return to Step 3** with the updated error index and repeat the process

3. **Report to user** if the errors are significantly different from the original set

## Build Error Expectations

The number of errors typically ranges from 20-50 for minor updates to 100+ for major updates. Work systematically through each error, and rebuild individual files frequently to track progress—some errors will disappear as root causes are fixed.

## Common Build Error Patterns

### Pattern 1: Missing Include Headers

**Symptoms:**
```
error: 'WebContents' was not declared in this scope
error: 'GURL' does not name a type
error: 'base::BindOnce' was not declared
```

**Cause:** Chromium moved header files or changed what headers are included transitively.

**Fix:** Add the missing `#include` at the top of the file:
```cpp
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "base/functional/bind.h"
```

**How to find the right header:**

- Error message often hints at the type name
- Search Chromium codebase for the type definition
- Use tools like `gn refs` or IDE "Go to Definition"

### Pattern 2: Method Signature Changes

**Symptoms:**
```
error: no matching function for call to 'CreateWebContents(const CreateParams&)'
error: too few arguments to function call, expected 4, have 3
```

**Cause:** Chromium added, removed, or changed parameters.

**Fix:** Update the call to match the new signature:
```cpp
// Old
auto* contents = CreateWebContents(params);

// New (if added browser_context parameter)
auto* contents = CreateWebContents(params, browser_context_);
```

**How to find the new signature:**

- Use `git diff` on the Chromium header file
- Look at how Chromium itself calls the function
- Check the function declaration in the current code

### Pattern 3: Namespace Changes

**Symptoms:**
```
error: 'mojom' is not a member of 'content'
error: 'switches' is not a member of 'content'
```

**Cause:** Types or constants moved to different namespaces.

**Fix:** Update the namespace:
```cpp
// Old
content::mojom::SomeType

// New (if moved)
content::SomeType
// or
blink::mojom::SomeType
```

Or add proper using declaration:
```cpp
using content::mojom::SomeType;
```

### Pattern 4: Static to Instance Method

**Symptoms:**
```
error: 'ScreenToDIPPoint' is not a member of 'display::win::ScreenWin'
error: cannot call member function without object
```

**Cause:** Chromium changed static methods to instance methods.

**Fix:** Call on an instance instead:
```cpp
// Old
auto point = display::win::ScreenWin::ScreenToDIPPoint(screen_point);

// New
auto point = display::win::GetScreenWin()->ScreenToDIPPoint(screen_point);
```

### Pattern 5: Deprecated API Replacements

**Symptoms:**
```
error: 'RunUntilIdle' is not a member of 'base::RunLoop'
warning: 'PostTask' is deprecated
```

**Cause:** Chromium deprecated old APIs in favor of new ones.

**Fix:** Use the replacement API:
```cpp
// Old
base::RunLoop().RunUntilIdle();

// New
task_environment_.RunUntilIdle();
```

```cpp
// Old
base::PostTask(FROM_HERE, task);

// New
base::ThreadPool::PostTask(FROM_HERE, task);
// or
GetUIThreadTaskRunner()->PostTask(FROM_HERE, task);
```

### Pattern 6: Enum Value Removed

**Symptoms:**
```
error: no member named 'PLUGIN' in 'task_manager::Task'
error: no member named 'NACL' in 'task_manager::Task'
```

**Cause:** Chromium removed enum values (often due to feature removal).

**Fix:** Remove references and mark CEF API as deprecated:
```cpp
// In CEF implementation - remove the case
switch (type) {
  case task_manager::Task::RENDERER:
    return CEF_TASK_TYPE_RENDERER;
  // Remove this case:
  // case task_manager::Task::PLUGIN:
  //   return CEF_TASK_TYPE_PLUGIN;
  ...
}

// In CEF public API header - mark as deprecated
enum cef_task_type_t {
  CEF_TASK_TYPE_RENDERER,
#if CEF_API_ADDED(CEF_NEXT)
  CEF_TASK_TYPE_PLUGIN_DEPRECATED,  // Marked as deprecated
#else
  /// A plugin process.
  CEF_TASK_TYPE_PLUGIN,
#endif
  ...
};
```

### Pattern 7: Method Signature with Additional Parameters

**Symptoms:**
```
error: no matching function for call to 'GoToEntryAtOffset(int, bool, std::optional<...>)'
error: too few arguments to function call, expected 4, have 3
```

**Cause:** Chromium added new required parameters.

**Fix:** Provide the new parameters:
```cpp
// Old
controller->GoToEntryAtOffset(offset, needs_reload);

// New (if added timestamp parameter)
controller->GoToEntryAtOffset(offset, needs_reload, base::TimeTicks::Now());
```

Common additions:

- Timestamps: `base::TimeTicks::Now()`
- User gesture flags: `true` or `false`
- Callback parameters: Create appropriate callback

### Pattern 8: API Evolution - Registry Pattern

**Symptoms:**
```
error: no matching function for call to 'CreateThrottlesForNavigation(NavigationHandle*, NavigationThrottleList&)'
error: 'throttles' was not declared in this scope
```

**Cause:** Chromium changed from container-based to registry-based API.

**Fix:** Use the new registry API:
```cpp
// Old
void CreateThrottlesForNavigation(
    NavigationHandle* handle,
    NavigationThrottleList& throttles) {
  throttles.push_back(CreateMyThrottle(handle));
}

// New
void CreateThrottlesForNavigation(
    NavigationThrottleRegistry& registry) {
  registry.AddThrottle(CreateMyThrottle(registry.GetNavigationHandle()));
}
```

### Pattern 9: Type Safety and Conversions

**Symptoms:**
```
error: cannot convert 'ContentSettingsType' to 'cef_content_setting_types_t'
error: invalid static_cast from 'int' to enum type
```

**Cause:** Chromium tightened type safety or changed enum types.

**Fix:** Use proper conversion functions:
```cpp
// Old
cef_type = static_cast<cef_content_setting_types_t>(chrome_type);

// New (create helper function)
cef_content_setting_types_t ToCefType(ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::COOKIES:
      return CEF_CONTENT_SETTING_TYPE_COOKIES;
    case ContentSettingsType::JAVASCRIPT:
      return CEF_CONTENT_SETTING_TYPE_JAVASCRIPT;
    // ... handle all cases
    default:
      NOTREACHED();
      return CEF_CONTENT_SETTING_TYPE_DEFAULT;
  }
}

cef_type = ToCefType(chrome_type);
```

### Pattern 10: Environment Variable API Changes

**Symptoms:**
```
error: 'GetVar' expects 2 arguments but only 1 provided
error: cannot initialize variable with GetVar result
```

**Cause:** Chromium changed from output parameters to return values (often `std::optional`).

**Fix:** Use the new return-value API:
```cpp
// Old
std::string value;
if (env->GetVar("MY_VAR", &value)) {
  // use value
}

// New
if (const auto& value = env->GetVar("MY_VAR")) {
  // use value.value() or *value
}
```

### Pattern 11: Collection Constructor Changes

**Symptoms:**
```
error: no matching constructor for 'CookiePartitionKeyCollection'
error: 'FromOptional' is not a member of class
```

**Cause:** Chromium changed static factory methods to direct constructors.

**Fix:** Use direct construction:
```cpp
// Old
auto collection = CookiePartitionKeyCollection::FromOptional(key);

// New
auto collection = CookiePartitionKeyCollection(key);
```

### Pattern 12: Build File Changes

**Symptoms:**
```
error: Target "//some/path:target" not found
error: Unknown variable "enable_some_feature" in BUILD.gn
```

**Cause:** Chromium renamed or reorganized build targets or flags.

**Fix for target not found:**
```python
# In BUILD.gn file
# Old
deps = [ "//chrome/browser:browser" ]

# New (if renamed)
deps = [ "//chrome/browser/ui:ui" ]
```

**Fix for unknown variable:**
```python
# In BUILD.gn or .gni file
# Old
if (enable_some_feature) { ... }

# New (if renamed)
if (use_some_feature) { ... }
```

Use `gn refs` to find target dependencies.

### Pattern 13: Conditional Compilation

**Symptoms:**
```
error: 'cef_framework_angle_binaries' target not found
error: undefined reference to ANGLE libraries
```

**Cause:** Build configuration changed (e.g., static vs dynamic linking).

**Fix:** Add proper conditionals:
```python
# In BUILD.gn
# Old
deps = [ "//third_party/angle:angle_binaries" ]

# New (if made conditional)
if (!use_static_angle) {
  deps = [ "//third_party/angle:angle_binaries" ]
}
```

## Working with Different File Types

### C++ Source Files (.cc, .cpp)

**Location:** `cef/libcef/browser/`, `cef/libcef/renderer/`, etc.

**Common errors:** Missing includes, API changes, signature mismatches

**Tips:**

- Read error message carefully - it usually tells you exactly what's wrong
- Use IDE or compiler's "fix suggestions" when available
- Format code after changes: Run `python3 tools/fix_style.py` from the `chromium/src/cef` directory

### C++ Header Files (.h)

**Location:** `cef/include/`, `cef/libcef/common/`, etc.

**Common errors:** Forward declaration issues, missing types

**Tips:**

- Public API headers (`cef/include/`) should maintain backward compatibility when possible
- Internal headers can be changed freely
- Update both declaration (.h) and definition (.cc) files

### Build Files (.gn, BUILD.gn)

**Location:** `cef/BUILD.gn`, `cef/libcef/BUILD.gn`, etc.

**Common errors:** Missing dependencies, wrong target names, unknown variables

**Tips:**

- Validate with: `gn check out/Debug_GN_x64` (from chromium/src)
- Find target: `gn ls out/Debug_GN_x64 //cef/...` (from chromium/src)
- Find dependencies: `gn refs out/Debug_GN_x64 //some/target` (from chromium/src)

### Python Scripts (.py)

**Location:** `cef/tools/`, build scripts

**Common errors:** Syntax errors, import changes

**Tips:**

- Less common to have errors during Chromium updates
- Test with: `python3 script.py --help`

## Iteration Strategy

### Single Error

Fix, rebuild that file, verify, move to next error.

### Multiple Related Errors

If many errors are from the same root cause (e.g., all missing the same include), fix them all at once, then rebuild those files.

### Cascading Errors

Some errors cause other errors. Fix the first error, rebuild that file, and many others may disappear.

### Efficient Workflow

1. **Scan all errors** - Identify patterns
2. **Group by pattern** - Missing includes, API changes, etc.
3. **Fix by group** - All missing includes, then all signature changes
4. **Rebuild files in that group** - Verify progress for those files
5. **Adjust strategy** - If a fix causes new errors, rethink approach
6. **Final rebuild** - Only after ALL files compile, rebuild entire `cef` target

## Communication Guidelines

### Report Progress Regularly

Every 5-10 errors fixed or every 10 minutes:

```
Progress Update:
✓ Fixed 15 errors (45 remaining)
✓ All missing include errors resolved
→ Currently fixing API signature changes in browser code

Recent fixes:
- Added #include for WebContents in 8 files
- Updated CreateParams usage to new API
- Fixed NavigationThrottle registry migration
```

### Report When Stuck

After 3 failed attempts on a single error:

```
⚠ Need assistance with error

**File:** cef/libcef/browser/browser_host_impl.cc
**Line:** 123
**Error:** [full error message]

**Chromium change:** [what you discovered]

**Attempted fixes:**
1. [what you tried]
2. [what you tried]
3. [what you tried]

**Question:** [specific question]
```

### Final Report

When build succeeds:

```
✓ BUILD SUCCESSFUL

Summary:
- Total errors fixed: 87
- Files modified: 34
- Build time: 4m 32s

Major changes:
- Updated all NavigationThrottle code to use registry API (12 files)
- Migrated to new CreateParams structure (8 files)
- Added missing includes throughout codebase (23 files)
- Updated deprecated API usage (15 instances)

Build command used:
autoninja -k 0 -C out/Debug_GN_x64 cef

Ready for testing phase.
```

## Success Criteria

The build is successful when:

1. ✓ Build completes without stopping
2. ✓ Final message shows success (e.g., "Build Succeeded:", "100% complete")
3. ✓ Zero compile errors
4. ✓ Warnings are acceptable but should be noted

## After Build Succeeds

Once the build is successful, the next steps are:

1. **Run CEF tests** - Verify functionality
    - `ceftests` - Unit tests
    - `cefclient` - Sample application
    - Manual testing of key features

2. **Build other configurations** - If only Debug was built
    - Release build
    - Other platforms (if applicable)

3. **Submit changes** - Create PR
    - Commit message format
    - Code review process

The user will guide you through these next steps.

## Tips for Efficiency

1. **Read errors carefully** - The compiler often tells you exactly what to do
2. **Use patterns** - Similar errors often have similar fixes
3. **Check Chromium first** - See how Chromium adapted to its own changes
4. **Group related fixes** - Fix all missing includes at once
5. **Rebuild individual files frequently** - Catch cascading issues early with fast rebuilds
6. **Use search** - Find all usages of an API to fix them together
7. **Keep notes** - Track what patterns you've seen

## Troubleshooting

**"Build is very slow"**

- Use `autoninja` instead of `ninja` (automatic parallelization)
- Build individual file targets (`.o` files) during fixing, not the entire `cef` target
- Only build the full `cef` target after all files compile successfully
- Check system resources (CPU, disk space)

**"Same error keeps coming back"**

- Make sure you saved the file
- Make sure you're editing the right file (check path carefully)
- Make sure you rebuilt that specific file after the change
- Clear build cache if needed (rare): `rm -rf out/Debug_GN_x64/obj/cef`

**"Too many errors to handle"**

- Focus on one pattern at a time
- Fix all instances of that pattern
- Rebuild those individual files and see how many remain
- Repeat with next pattern

**"Error in generated file"**

- Generated files are in `out/Debug_GN_x64/gen/`
- Don't edit generated files directly
- Find the source template or generator and fix that
- Regenerate by cleaning and rebuilding

**"Linker errors instead of compile errors"**

- These come after all compilation succeeds
- Usually missing dependencies in BUILD.gn
- Or unimplemented functions
- Check that all declared functions are defined

## Important Reminders

1. **ALWAYS verify your current directory with `pwd`** - Before using `cd` or relative paths (see "CRITICAL: Working Directory Awareness")
2. **Only modify CEF code** - Files in `cef/` directory only (see "Files You Can Modify")
3. **Patches already fixed** - This is the build error phase; patch phase was completed earlier (see "What Phase Is This?")
4. **Rebuild individual files frequently** - After every fix or small batch of fixes. Only rebuild the entire `cef` target after ALL individual files compile successfully (see Step 4D "Rebuild to Verify")
5. **Ask for help when stuck** - After 3 attempts or if unable to resolve within a reasonable time (see Step 6 "Handle Difficult Errors")
6. **Track progress** - Keep TODO list updated and report regularly to the user
7. **Stay organized** - Group similar errors, work systematically (see "Iteration Strategy")

---

**You are now ready to fix CEF build errors! Work systematically, rebuild individual files frequently, and report progress as you go.**
