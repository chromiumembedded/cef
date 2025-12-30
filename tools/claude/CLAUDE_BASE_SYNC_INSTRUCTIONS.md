# CEF Base Headers Chromium Sync Instructions

This document provides instructions for Claude agents to synchronize CEF base headers with Chromium.

## Overview

CEF maintains standalone versions of Chromium's `//base` headers in `cef/include/base/`. These headers allow CEF client applications to use Chromium utilities (callbacks, ref-counting, weak pointers, etc.) without linking to Chromium.

Periodically, these headers need to be synchronized with upstream Chromium to pick up bug fixes, performance improvements, and language modernization.

## Source of Truth

**The Chromium header is the source of truth.**

The goal is to make CEF headers match Chromium headers as closely as possible, with only the minimum required CEF-specific modifications.

**DO NOT:**

- Make interpretive changes or "improvements"
- Refactor code beyond what's in Chromium
- Add features not present in Chromium
- Change coding style or formatting beyond what's required

**DO:**

- Copy Chromium code exactly
- Apply only necessary CEF-specific changes (documented below)
- Apply only necessary reverts for unsupported language features

## Reference Document

After completing a sync, update `cef/include/base/CHROMIUM_SYNC.md` with:

- Sync date and Chromium commit hash
- List of synchronized files
- Any reverts applied
- CEF-specific changes made

## CEF-Specific Changes (Required Modifications)

When converting a Chromium header to a CEF header, apply ONLY these modifications:

### 1. Header Structure

All CEF headers follow this pattern:

```cpp
// Copyright (c) [year] Marshall A. Greenblatt. Portions copyright (c) [year]
// Google Inc. All rights reserved.
//
// [CEF BSD license text]

#ifndef CEF_INCLUDE_BASE_CEF_FILENAME_H_
#define CEF_INCLUDE_BASE_CEF_FILENAME_H_
#pragma once

#if defined(USING_CHROMIUM_INCLUDES)
// When building CEF include the Chromium header directly.
#include "base/path/to/chromium_header.h"
#else  // !USING_CHROMIUM_INCLUDES
// CEF standalone implementation
// [Copy Chromium header content here with modifications]
#endif  // !USING_CHROMIUM_INCLUDES

#endif  // CEF_INCLUDE_BASE_CEF_FILENAME_H_
```

### 2. Namespace Renames

CEF uses different namespaces to prevent linker symbol conflicts:

| Chromium Namespace | CEF Namespace |
|--------------------|---------------|
| `base::internal` | `cef_internal` |
| `base::subtle` | `cef_subtle` |

**Note:** The `base::` namespace itself is kept (e.g., `base::OnceCallback`).

### 3. Include Path Changes

- Chromium: `#include "base/foo.h"`
- CEF: `#include "include/base/cef_foo.h"`

### 4. Platform Macro Changes

- `BUILDFLAG(IS_WIN)` -> `defined(OS_WIN)`
- `BUILDFLAG(IS_APPLE)` -> `defined(OS_MAC)`
- `BUILDFLAG(IS_LINUX)` -> `defined(OS_LINUX)`
- `BUILDFLAG(IS_POSIX)` -> `defined(OS_POSIX)`

### 5. Remove Chromium-Specific Dependencies

These Chromium features are not available in CEF standalone mode:

| Feature | CEF Approach |
|---------|--------------|
| `raw_ptr<T>` / `raw_ref<T>` | Use raw pointers/references |
| `BASE_EXPORT` | Remove (not needed for header-only) |
| `SafeRef` | Remove |
| `FunctionRef` | Remove (not ported) |
| `SEQUENCE_CHECKER` macro | Use CEF's `ThreadChecker` |
| `base::check.h` / `base::notreached.h` | Use `cef_logging.h` |

### 6. Documentation Format (Optional)

CEF uses Doxygen-style comments (`///`) instead of plain `//` for public API documentation.

## Files That Are NOT Synchronized

These files have CEF-specific implementations and should not be replaced:

| File | Reason |
|------|--------|
| `cef_build.h` | CEF platform detection |
| `cef_macros.h` | Kept for backwards compatibility |
| `cef_logging.h/cc` | CEF-specific logging implementation |
| `cef_dump_without_crashing.h/cc` | CEF C API wrapper |
| `cef_trace_event.h` | CEF C API macro wrapper |
| Threading primitives | CEF C API delegation (see below) |

### Threading Primitives (CEF-Specific Implementations)

These files provide API compatibility with Chromium but have CEF-specific implementations that delegate to C API functions:

- `cef_atomic_flag.h` - uses CEF ThreadChecker
- `cef_atomic_ref_count.h` - CEF standalone implementation
- `cef_lock.h` / `cef_lock_impl.h` - CEF Lock classes
- `cef_platform_thread.h` - delegates to `cef_get_current_platform_thread_id()`
- `cef_thread_checker.h` / `cef_thread_checker_impl.h` - CEF ThreadChecker

**Do NOT replace these with Chromium implementations.** Only update comments or minor fixes if needed.

## Identifying Changes to Revert

Before syncing, check if Chromium has adopted any C++23 or compiler-specific features that CEF cannot support yet.

### How to Check

```bash
# Search for C++23-related changes in Chromium
git log --oneline --since="[last sync date]" \
  --grep="C++23\|c++23\|static_assert(false" \
  -- base/functional/ base/memory/

# Check Chromium bug tracker for:
# - Bug 388070065 (C++23 adoption)
# - Bug 1501952 (C++20 migration)
```

### Known Reverts

#### `static_assert(false)` in Templates

Chromium may use bare `static_assert(false, ...)` in template contexts. This requires C++23 support (P2593R1). CEF should use the `AlwaysFalse<T>` pattern instead:

```cpp
// Chromium (C++23):
template <typename T>
void Foo() {
  static_assert(false, "Not allowed");
}

// CEF (C++20 compatible):
#include "include/base/internal/cef_always_false.h"

template <typename T>
void Foo() {
  static_assert(AlwaysFalse<T>, "Not allowed");
}
```

The `cef_always_false.h` header provides this utility.

## Sync Process

### Step 1: Preparation

```bash
# Create working directories
mkdir -p cef/tmp/backup
mkdir -p cef/tmp/diffs

# Note the current Chromium commit
git log -1 --format='%H' HEAD
cat chrome/VERSION
```

### Step 2: For Each File to Sync

Use the helper script (see "Helper Script" section below) or manually:

```bash
./cef/tmp/update_cef_header.sh base/functional/callback.h cef/include/base/cef_callback.h
```

This backs up both files and generates a diff showing existing CEF changes.

### Step 3: Replace and Modify

1. Copy the Chromium header content
2. Apply CEF header structure (guards, copyright, `USING_CHROMIUM_INCLUDES`)
3. Apply namespace renames
4. Apply include path changes
5. Apply platform macro changes
6. Remove unsupported Chromium dependencies
7. Apply any necessary reverts (e.g., `AlwaysFalse`)

### Step 4: Verify

```bash
# Diff against original to see all changes
git diff --no-index cef/tmp/backup/cef_foo.h.original cef/include/base/cef_foo.h

# Diff against Chromium to verify only expected differences
git diff --no-index cef/tmp/backup/cef_foo.h.chromium cef/include/base/cef_foo.h
```

### Step 5: Build and Test

See "Build Commands" section below for full commands. At minimum:

```bash
autoninja -C out/Debug_GN_x64 cef
```

## Important Notes

### Headers AND Implementation Files

When syncing a header, also sync its corresponding `.cc` file if one exists:

| Header | Implementation |
|--------|----------------|
| `cef_callback_helpers.h` | `libcef_dll/base/cef_callback_helpers.cc` |
| `internal/cef_callback_internal.h` | `libcef_dll/base/cef_callback_internal.cc` |
| `cef_ref_counted.h` | `libcef_dll/base/cef_ref_counted.cc` |
| `cef_weak_ptr.h` | `libcef_dll/base/cef_weak_ptr.cc` |

### Atomic Updates

Phase 6 files (Callback/Bind) must be updated together because they have circular dependencies. Updating only some files will cause compile errors - the internal types and templates reference each other.

### The cef_subtle Namespace

Only used in `cef_scoped_refptr.h` for the `StartRefCountFromOneTag` and `StartRefCountFromZeroTag` classes. Rename `base::subtle` to `cef_subtle` in that file only.

## Header Mapping Reference

| CEF Header | Chromium Source |
|------------|-----------------|
| `cef_callback.h` | `base/functional/callback.h` |
| `cef_callback_forward.h` | `base/functional/callback_forward.h` |
| `cef_callback_helpers.h` | `base/functional/callback_helpers.h` |
| `cef_bind.h` | `base/functional/bind.h` |
| `internal/cef_bind_internal.h` | `base/functional/bind_internal.h` |
| `internal/cef_callback_internal.h` | `base/functional/callback_internal.h` |
| `internal/cef_callback_tags.h` | `base/functional/callback_tags.h` |
| `cef_ref_counted.h` | `base/memory/ref_counted.h` |
| `cef_scoped_refptr.h` | `base/memory/scoped_refptr.h` |
| `cef_weak_ptr.h` | `base/memory/weak_ptr.h` |
| `cef_compiler_specific.h` | `base/compiler_specific.h` |
| `cef_immediate_crash.h` | `base/immediate_crash.h` |
| `cef_is_complete.h` | `base/types/is_complete.h` |
| `cef_is_instantiation.h` | `base/types/is_instantiation.h` |
| `cef_to_address.h` | `base/types/to_address.h` |
| `internal/cef_always_false.h` | `base/types/always_false.h` |

## Breaking Changes for Client Code

When syncing, be aware of Chromium changes that may break CEF client applications:

| Change Type | Example | Client Impact |
|-------------|---------|---------------|
| Removed classes | `SupportsWeakPtr<T>` removed | Clients must use `WeakPtrFactory` directly |
| Removed functions | `RetainBlock()` removed | Must enable ARC for Objective-C blocks |
| Removed headers | `cef_tuple.h` | Must use `std::tuple` |
| API changes | Callback signature changes | May require code updates |

Document any breaking changes in release notes.

## Cleanup

After successful sync and testing:

```bash
# Remove temporary files
rm -rf cef/tmp/backup/
rm -rf cef/tmp/diffs/
```

## Troubleshooting

### Compile Errors After Sync

1. Check for missing includes - Chromium may have added new dependencies
2. Check for namespace issues - ensure all `base::internal` -> `cef_internal` changes
3. Check for `raw_ptr` usage - must be removed for CEF standalone
4. Check for `BUILDFLAG` macros - must be converted to `defined()`

### Test Failures

1. Verify threading primitives still work (they use CEF-specific implementations)
2. Check callback/bind functionality with CEF test suite
3. Test sample applications (cefsimple, cefclient)

### Rollback

If a sync fails:

```bash
# Restore from backup
cp cef/tmp/backup/cef_foo.h.original cef/include/base/cef_foo.h
```

## Helper Script

Save this script to `cef/tmp/update_cef_header.sh` and use it to automate backup and diff generation:

```bash
#!/bin/bash
# Usage: ./update_cef_header.sh <chromium_path> <cef_path>
# Example: ./update_cef_header.sh base/functional/callback.h cef/include/base/cef_callback.h

set -e  # Exit on error

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <chromium_path> <cef_path>"
    exit 1
fi

CHROMIUM_FILE="$1"
CEF_FILE="$2"
BASENAME=$(basename "$CEF_FILE")

if [[ ! -f "$CHROMIUM_FILE" ]]; then
    echo "Error: Chromium file not found: $CHROMIUM_FILE"
    exit 1
fi

if [[ ! -f "$CEF_FILE" ]]; then
    echo "Error: CEF file not found: $CEF_FILE"
    exit 1
fi

# Safety check: fail if backup already exists
if [[ -f "cef/tmp/backup/${BASENAME}.original" ]]; then
    echo "Error: Backup already exists: cef/tmp/backup/${BASENAME}.original"
    echo "To re-process, remove backups first."
    exit 1
fi

mkdir -p cef/tmp/backup
mkdir -p cef/tmp/diffs

echo "=== Updating $BASENAME ==="

# Step 1: Backup both files
cp "$CEF_FILE" "cef/tmp/backup/${BASENAME}.original"
cp "$CHROMIUM_FILE" "cef/tmp/backup/${BASENAME}.chromium"
echo "Backed up to cef/tmp/backup/"

# Step 2: Generate diff showing existing CEF changes
git diff --no-index "cef/tmp/backup/${BASENAME}.chromium" "cef/tmp/backup/${BASENAME}.original" \
    > "cef/tmp/diffs/${BASENAME}.cef-changes.diff" 2>/dev/null || true
echo "Generated cef/tmp/diffs/${BASENAME}.cef-changes.diff"

# Step 3: Replace CEF file with Chromium file
cp "cef/tmp/backup/${BASENAME}.chromium" "$CEF_FILE"
echo "Replaced $CEF_FILE with Chromium version"

echo ""
echo "=== Manual steps required ==="
echo "1. Apply CEF-specific changes (namespaces, includes, header structure)"
echo "2. Apply any required reverts (AlwaysFalse, etc.)"
echo "3. Verify: git diff --no-index cef/tmp/backup/${BASENAME}.chromium $CEF_FILE"
echo "4. Build and test"
```

## Dependency Ordering

Files must be synced in dependency order. Earlier phases must complete before later ones.

### Phase 1: Foundation (No Dependencies)

- `cef_immediate_crash.h`
- `internal/cef_always_false.h`

### Phase 2: Compiler Utilities

- `cef_compiler_specific.h`

**Note:** `cef_logging.h` is NOT synced - it has a CEF-specific implementation.

### Phase 3: Threading Primitives

These are CEF-specific implementations - only update comments/minor fixes, not the implementation.

### Phase 4: Memory Management

- `cef_scoped_refptr.h`
- `cef_ref_counted.h`

### Phase 5: WeakPtr

- `cef_weak_ptr.h`

**CRITICAL:** WeakPtr MUST complete before Phase 6. `cef_bind_internal.h` includes `cef_weak_ptr.h`.

### Phase 6: Callback/Bind (Atomic Update)

ALL of these must be updated together in one commit:

- `cef_callback_forward.h`
- `internal/cef_callback_internal.h`
- `internal/cef_bind_internal.h`
- `cef_bind.h`
- `cef_callback.h`
- `cef_callback_helpers.h`
- `internal/cef_callback_tags.h`

### Phase 7: Utilities

- Type utilities (`cef_is_complete.h`, etc.)

## Handling New Chromium Includes

When Chromium headers add new `#include` directives:

### If the include is for a CEF-ported header

Update the path:

```cpp
// Chromium:
#include "base/types/is_complete.h"

// CEF:
#include "include/base/cef_is_complete.h"
```

### If the include is for a non-ported header

Check if the functionality is needed in standalone mode:

- **If not needed:** Omit the include entirely
- **If needed:** Port the required header (see "Porting a New Header" below)

### How to decide: Port vs Omit

**Omit** if the include is for:

- Chromium-specific safety features (`raw_ptr`, `SafeRef`, checks)
- Build system integration (`BASE_EXPORT`, `component_export.h`)
- Features not exposed in CEF API (`FunctionRef`, internal utilities)

**Port** if the include is for:

- Type traits used in template code (`is_complete.h`, `always_false.h`)
- Core functionality that callbacks/bind depend on
- Something that causes compile errors when omitted

### Porting a New Header

1. Copy the Chromium header to `cef/include/base/cef_<name>.h`
2. Apply all CEF-specific changes (header structure, namespaces, etc.)
3. Add to `cef/cef_paths2.gypi`
4. Update `CHROMIUM_SYNC.md` with the new mapping
5. If it has a `.cc` file, port that to `libcef_dll/base/`

### Common includes that can be omitted

- `base/check.h` / `base/check_op.h` - CEF uses `cef_logging.h`
- `base/notreached.h` - CEF uses `cef_logging.h`
- `base/memory/raw_ptr.h` - CEF uses raw pointers
- `base/memory/raw_ref.h` - CEF uses raw references
- `base/compiler_specific.h` - Use `cef_compiler_specific.h`

## Staged Approach for Complex Files

For complex files like `cef_bind_internal.h`, use a staged approach to avoid mistakes.

### Stage 1: CEF-Specific Changes

Apply standard CEF modifications:

- CEF BSD copyright header
- Header guard: `CEF_INCLUDE_BASE_*_H_`
- Add `#pragma once`
- Add `USING_CHROMIUM_INCLUDES` structure
- Change `#include "base/..."` to `#include "include/base/cef_..."`
- Change `namespace internal` to `namespace cef_internal`
- Change `BUILDFLAG(IS_WIN)` to `defined(OS_WIN)`
- Change `BUILDFLAG(IS_APPLE)` to `defined(OS_MAC)`

### Stage 2: raw_ptr/raw_ref Removal

CEF standalone doesn't support MiraclePtr. Remove raw_ptr functionality:

**Simplify `UnretainedWrapper<T>`:**

```cpp
// Chromium has multiple template params for raw_ptr traits
template <typename T, typename PtrTraits, ...>
class UnretainedWrapper { ... };

// CEF: single template param with raw pointer
template <typename T>
class UnretainedWrapper {
 public:
  explicit UnretainedWrapper(T* o) : ptr_(o) {}
  T* get() const { return ptr_; }
 private:
  T* ptr_;
};
```

**Remove these entirely:**

- `UnretainedRefWrapper`, `UnretainedRefWrapperReceiver`
- `unretained_traits::MayDangle`, `MayNotDangle`, `MayDangleUntriaged`
- `PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)` blocks

**Simplify validation checks (remove raw_ptr-specific):**

- `NotRawPtr`
- `MayBeDanglingPtrPassedCorrectly`
- `MayDangleAndMayBeDanglingHaveMatchingTraits`

**Keep all other validation logic:**

- `MoveOnlyTypeMustUseBasePassed`
- `NonConstRefParamMustBeWrapped`
- `CanBeForwardedToBoundFunctor`
- `FunctorTraits`, `BindHelper`, `Invoker`

### Stage 3: Apply Reverts

Apply any required reverts (e.g., `AlwaysFalse` pattern).

## Verification Commands

After syncing, run these verification commands:

### Check for remaining raw_ptr references

```bash
# Should only match comments, not code
grep -n "raw_ptr\|raw_ref\|MayDangle\|MayNotDangle" \
    cef/include/base/internal/cef_bind_internal.h
```

### Verify no bare static_assert(false)

```bash
# Should return NO matches
grep -n "static_assert(false" cef/include/base/*.h cef/include/base/internal/*.h
```

### Verify AlwaysFalse is used where needed

```bash
# Should return matches in cef_bind_internal.h
grep -n "AlwaysFalse" cef/include/base/internal/cef_bind_internal.h
```

### Check namespace renames

```bash
# Should return NO matches (should be cef_internal)
grep -n "namespace internal" cef/include/base/internal/cef_bind_internal.h | grep -v cef_internal

# Should return matches
grep -n "namespace cef_internal" cef/include/base/internal/cef_bind_internal.h
```

### Check for BUILDFLAG macros

```bash
# Should return NO matches (should be defined(OS_*))
grep -n "BUILDFLAG" cef/include/base/*.h cef/include/base/internal/*.h
```

### Check include paths

```bash
# Should return NO matches to bare base/ includes
grep -n '#include "base/' cef/include/base/*.h cef/include/base/internal/*.h
```

## Updating cef_paths2.gypi

After adding or removing files, update `cef/cef_paths2.gypi`:

```python
# In the 'includes_common' section, add/remove file entries:
'includes_common': [
  'include/base/cef_callback.h',
  'include/base/cef_new_file.h',  # Add new files
  # Remove deleted files
],
```

## Syncing Test Files

CEF maintains unit tests for the base headers in `cef/tests/ceftests/base/`. These tests are ported from Chromium's corresponding unit tests and should be updated when syncing headers.

### Source of Truth

**The Chromium test file is the source of truth.** Keep CEF tests as close to Chromium originals as possible. The goal is **minimal diff from Chromium** for easier maintenance.

**Only make these changes:**

- Update include paths (`base/` → `include/base/cef_*`)
- Update copyright header (with Chromium commit hash)
- Remove non-portable tests (death tests, SequenceChecker, etc.)
- Convert `raw_ptr<T>` to `T*` and `raw_ref<T>` to `T&`
- Convert threading from `base::Thread` to `CefThread`
- Convert `base::internal::` to `cef_internal::`

**Do NOT:**

- Rename tests or test suites
- Change test logic or assertions
- Reformat code style (keep Chromium's formatting)
- Add new tests not present in Chromium
- "Improve" or refactor test code
- Change the order of tests within a file

### Test File Mapping

| CEF Test File | Chromium Source | Uses TEST_F |
|---------------|-----------------|-------------|
| `tests/ceftests/base/callback_helpers_unittest.cc` | `base/functional/callback_helpers_unittest.cc` | No |
| `tests/ceftests/base/callback_unittest.cc` | `base/functional/callback_unittest.cc` | Yes |
| `tests/ceftests/base/bind_unittest.cc` | `base/functional/bind_unittest.cc` | Yes |
| `tests/ceftests/base/ref_counted_unittest.cc` | `base/memory/ref_counted_unittest.cc` | No |
| `tests/ceftests/base/weak_ptr_unittest.cc` | `base/memory/weak_ptr_unittest.cc` | No |
| `tests/ceftests/base/lock_unittest.cc` | `base/synchronization/lock_unittest.cc` | No |
| `tests/ceftests/base/atomic_flag_unittest.cc` | `base/synchronization/atomic_flag_unittest.cc` | No |
| `tests/ceftests/base/is_complete_unittest.cc` | `base/types/is_complete_unittest.cc` | No |
| `tests/ceftests/base/is_instantiation_unittest.cc` | `base/types/is_instantiation_unittest.cc` | No |
| `tests/ceftests/base/to_address_unittest.cc` | `base/types/to_address_unittest.cc` | No |

**Notes:**

- The Chromium commit hash is recorded in each CEF test file header. Check the file to find which Chromium version it was ported from.
- Files marked "Uses TEST_F: Yes" require the TEST_F comment block after the header.

### Test File Structure

Each CEF test file must have:

**1. Header with commit hash:**

The copyright header must reference the original Chromium file's copyright year:

```cpp
// Copyright (c) 2025 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ---------------------------------------------------------------------------
//
// This file was ported from Chromium source file:
// base/functional/callback_helpers_unittest.cc
// Chromium commit: 70909488d8cc2
//
// ---------------------------------------------------------------------------
```

To find the Chromium copyright year, check the first line of the original Chromium file:
```bash
head -1 base/functional/callback_helpers_unittest.cc
# Output: // Copyright 2013 The Chromium Authors
```

**2. TEST_F comment (if applicable):**

If the test file uses `TEST_F` fixtures instead of `TEST()`, add this comment after the header block:

```cpp
// ---------------------------------------------------------------------------
//
// Note: These tests use TEST_F fixtures to match the Chromium source
// structure for easier maintenance, which differs from typical CEF test
// convention that uses TEST() with manual helper classes.
```

**3. Skipped tests block at end of file:**
```cpp
}  // namespace

// =============================================================================
// SKIPPED TESTS from Chromium's base/functional/callback_unittest.cc
// =============================================================================
// CallbackDeathTest.RunNullCallbackChecks - Death test
// CallbackTest.MaybeValidInvalidateWeakPtrsOnSameSequence - SequenceChecker
// CallbackTest.MaybeValidInvalidateWeakPtrsOnOtherSequence - base::Thread
```

Format: `// TestSuite.TestName - Reason`

If all tests are portable:
```cpp
// =============================================================================
// SKIPPED TESTS from Chromium's base/functional/callback_helpers_unittest.cc
// =============================================================================
// (none - all tests are portable)
```

### Include Path Transformations

| Chromium Include | CEF Include |
|------------------|-------------|
| `base/functional/callback.h` | `include/base/cef_callback.h` |
| `base/functional/bind.h` | `include/base/cef_bind.h` |
| `base/functional/callback_helpers.h` | `include/base/cef_callback_helpers.h` |
| `base/memory/ref_counted.h` | `include/base/cef_ref_counted.h` |
| `base/memory/weak_ptr.h` | `include/base/cef_weak_ptr.h` |
| `base/synchronization/lock.h` | `include/base/cef_lock.h` |
| `base/synchronization/atomic_flag.h` | `include/base/cef_atomic_flag.h` |
| `base/test/bind.h` | `tests/ceftests/base/test_bind.h` |
| `testing/gtest/include/gtest/gtest.h` | `tests/gtest/include/gtest/gtest.h` |
| `testing/gmock/include/gmock/gmock.h` | `tests/gmock/include/gmock/gmock.h` |

### Includes to Remove

Delete these includes (the code using them will be removed):

```cpp
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_checker.h"
#include "base/run_loop.h"
#include "base/task/..."  // Any base/task/ includes
```

### Tests to Skip

Delete any test that:

- Has `DeathTest` in its name
- Uses `EXPECT_DCHECK_DEATH`, `EXPECT_CHECK_DEATH`
- Uses `base::Thread` (unless converting to `CefThread`)
- Uses `PlatformThread::Delegate`
- Uses `SequenceChecker` or sequence-bound assertions
- Uses `raw_ptr`/`raw_ref` in ways that can't be trivially converted
- Uses `base::test::TaskEnvironment`
- Uses `ScopedFeatureList`

**Portable:** `BindLambdaForTesting()` and gmock features ARE portable - just update include paths.

### Converting Threading Tests

For tests using `base::Thread`, convert to `CefThread`:

```cpp
// BEFORE (Chromium):
#include "base/threading/thread.h"
base::Thread thread("test");
thread.Start();
thread.task_runner()->PostTask(FROM_HERE, ...);
thread.Stop();

// AFTER (CEF):
#include "include/cef_thread.h"
#include "include/wrapper/cef_closure_task.h"
CefRefPtr<CefThread> thread = CefThread::CreateThread("test");
thread->GetTaskRunner()->PostTask(CefCreateClosureTask(...));
thread->Stop();
```

For synchronization:

```cpp
// BEFORE (Chromium):
#include "base/synchronization/waitable_event.h"
base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::NOT_SIGNALED);

// AFTER (CEF):
#include "include/cef_waitable_event.h"
CefRefPtr<CefWaitableEvent> event =
    CefWaitableEvent::CreateWaitableEvent(false, false);
```

### Test Sync Helper Script

Save this script to `cef/tmp/update_cef_test.sh`:

```bash
#!/bin/bash
# Usage: ./update_cef_test.sh <chromium_path> <cef_path>
# Example: ./update_cef_test.sh base/functional/callback_helpers_unittest.cc cef/tests/ceftests/base/callback_helpers_unittest.cc

set -e

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <chromium_path> <cef_path>"
    exit 1
fi

CHROMIUM_FILE="$1"
CEF_FILE="$2"
BASENAME=$(basename "$CEF_FILE")
CEF_DIR=$(dirname "$CEF_FILE")

if [[ ! -f "$CHROMIUM_FILE" ]]; then
    echo "Error: Chromium file not found: $CHROMIUM_FILE"
    exit 1
fi

CEF_EXISTS=true
if [[ ! -f "$CEF_FILE" ]]; then
    CEF_EXISTS=false
    echo "Note: CEF file does not exist yet (initial port)"
fi

if [[ -f "cef/tmp/backup/${BASENAME}.original" ]]; then
    echo "Error: Backup already exists. Remove it first:"
    echo "  rm cef/tmp/backup/${BASENAME}.*"
    exit 1
fi

mkdir -p cef/tmp/backup cef/tmp/diffs cef/tmp/tests "$CEF_DIR"

CHROMIUM_COMMIT=$(git log -1 --format='%h' HEAD)
echo "=== Updating $BASENAME ==="
echo "Chromium commit: $CHROMIUM_COMMIT"

# Backup files
if [[ "$CEF_EXISTS" == true ]]; then
    cp "$CEF_FILE" "cef/tmp/backup/${BASENAME}.original"
fi
cp "$CHROMIUM_FILE" "cef/tmp/backup/${BASENAME}.chromium"

# Extract test lists
grep -E "^TEST\(|^TEST_F\(" "$CHROMIUM_FILE" | \
    sed 's/TEST(\([^,]*\), *\([^)]*\)).*/\1.\2/' | \
    sed 's/TEST_F(\([^,]*\), *\([^)]*\)).*/\1.\2/' | \
    sort > "cef/tmp/tests/${BASENAME}.chromium-tests.txt"
CHROMIUM_COUNT=$(wc -l < "cef/tmp/tests/${BASENAME}.chromium-tests.txt" | tr -d ' ')

if [[ "$CEF_EXISTS" == true ]]; then
    grep -E "^TEST\(|^TEST_F\(" "$CEF_FILE" | \
        sed 's/TEST(\([^,]*\), *\([^)]*\)).*/\1.\2/' | \
        sed 's/TEST_F(\([^,]*\), *\([^)]*\)).*/\1.\2/' | \
        sort > "cef/tmp/tests/${BASENAME}.cef-tests.txt"
    CEF_COUNT=$(wc -l < "cef/tmp/tests/${BASENAME}.cef-tests.txt" | tr -d ' ')

    # Find skipped tests
    comm -23 "cef/tmp/tests/${BASENAME}.chromium-tests.txt" \
             "cef/tmp/tests/${BASENAME}.cef-tests.txt" \
        > "cef/tmp/tests/${BASENAME}.skipped-tests.txt"
    SKIPPED=$(wc -l < "cef/tmp/tests/${BASENAME}.skipped-tests.txt" | tr -d ' ')

    # Generate diff
    git diff --no-index "cef/tmp/backup/${BASENAME}.chromium" \
        "cef/tmp/backup/${BASENAME}.original" \
        > "cef/tmp/diffs/${BASENAME}.cef-changes.diff" 2>/dev/null || true

    # Check existing commit
    OLD_COMMIT=$(grep -E "^// Chromium commit:" "$CEF_FILE" 2>/dev/null | \
        sed 's/.*: *//' || echo "(not found)")

    echo ""
    echo "Summary: Chromium=$CHROMIUM_COUNT CEF=$CEF_COUNT Skipped=$SKIPPED"
    echo "Previous commit: $OLD_COMMIT"
fi

# Copy Chromium file to CEF location
echo ""
echo "Copying Chromium file to CEF location..."
cp "$CHROMIUM_FILE" "$CEF_FILE"
echo "  -> $CEF_FILE"

echo ""
echo "Current Chromium commit for header: $CHROMIUM_COMMIT"
```

### Updating Tests During Header Sync

When syncing headers, also check if the corresponding tests need updates:

1. Run the test sync helper script
2. Check for new tests in Chromium (review `chromium-tests.txt`)
3. Check if any previously-skipped tests are now portable
4. Update CEF test file if needed
5. Update the Chromium commit hash in the test file header
6. Update the skipped tests block at the end of the file

### Test Validation Checklist

After updating a test file, verify:

- [ ] **Minimal diff**: Test bodies unchanged from Chromium
- [ ] Test count matches expected portable count
- [ ] All tests pass
- [ ] No Chromium-only includes remain
- [ ] No `raw_ptr` or `raw_ref` usage
- [ ] No death test macros
- [ ] Copyright header uses "Portions copyright" with Chromium's year
- [ ] Chromium commit hash recorded in file header
- [ ] TEST_F comment included (if file uses TEST_F fixtures)
- [ ] Skipped tests documented at end of file with reasons

## Build Commands

### Minimal per-file verification

```bash
# Compile just the .cc file to catch obvious errors
autoninja -C out/Debug_GN_x64 obj/cef/libcef_dll/base/cef_callback_internal.o
```

### Full CEF build

```bash
autoninja -C out/Debug_GN_x64 cef
```

### Continue despite errors (see all errors at once)

```bash
autoninja -k 0 -C out/Debug_GN_x64 cef
```

### Run tests

```bash
autoninja -C out/Debug_GN_x64 ceftests
out/Debug_GN_x64/ceftests
```

### Test sample applications

```bash
autoninja -C out/Debug_GN_x64 cefsimple cefclient
out/Debug_GN_x64/cefsimple
out/Debug_GN_x64/cefclient
```
