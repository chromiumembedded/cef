# CEF Base Headers Chromium Sync

**For sync instructions, see `cef/tools/claude/CLAUDE_BASE_SYNC_INSTRUCTIONS.md`**

Last synchronized: 2025-12-30
Chromium commit: 70909488d8cc2cf4e9419d428f9bf77dc9e781b6
Chromium branch: 144.0.7559.0

## Files Synchronized

### Core Headers
- `cef_callback.h` <- `base/functional/callback.h`
- `cef_callback_forward.h` <- `base/functional/callback_forward.h`
- `cef_callback_helpers.h` <- `base/functional/callback_helpers.h`
- `cef_bind.h` <- `base/functional/bind.h`
- `internal/cef_bind_internal.h` <- `base/functional/bind_internal.h`
- `internal/cef_callback_internal.h` <- `base/functional/callback_internal.h`
- `internal/cef_callback_tags.h` <- `base/functional/callback_tags.h`

### Memory Management
- `cef_ref_counted.h` <- `base/memory/ref_counted.h`
- `cef_scoped_refptr.h` <- `base/memory/scoped_refptr.h`
- `cef_weak_ptr.h` <- `base/memory/weak_ptr.h`

### Threading Primitives (CEF-Specific Implementations)
These files provide the same API as Chromium but use CEF-specific implementations.
They delegate to C API functions (`cef_get_current_platform_thread_id()`,
`cef_get_current_platform_thread_handle()`) and use CEF's ThreadChecker instead
of Chromium's SequenceChecker:

- `cef_atomic_flag.h` - uses CEF ThreadChecker (Chromium uses SEQUENCE_CHECKER)
- `cef_atomic_ref_count.h` - CEF standalone implementation
- `cef_lock.h` - CEF Lock/AutoLock/AutoUnlock classes
- `internal/cef_lock_impl.h` - CEF lock implementation (CRITICAL_SECTION on Windows)
- `cef_platform_thread.h` - delegates to C API
- `cef_thread_checker.h` - CEF ThreadChecker (Chromium uses SequenceChecker)
- `internal/cef_thread_checker_impl.h` - CEF thread checker implementation

### Utilities
- `cef_compiler_specific.h` <- `base/compiler_specific.h`
- `cef_immediate_crash.h` <- `base/immediate_crash.h`
- `cef_is_complete.h` <- `base/types/is_complete.h`
- `cef_is_instantiation.h` <- `base/types/is_instantiation.h`
- `cef_to_address.h` <- `base/types/to_address.h`
- `internal/cef_always_false.h` <- `base/types/always_false.h`

### Implementation Files
- `libcef_dll/base/cef_callback_helpers.cc` <- `base/functional/callback_helpers.cc`
- `libcef_dll/base/cef_callback_internal.cc` <- `base/functional/callback_internal.cc`
- `libcef_dll/base/cef_ref_counted.cc` <- `base/memory/ref_counted.cc`
- `libcef_dll/base/cef_weak_ptr.cc` <- `base/memory/weak_ptr.cc`
- `libcef_dll/base/cef_atomic_flag.cc` <- (CEF implementation)
- `libcef_dll/base/cef_lock.cc` <- (CEF implementation)
- `libcef_dll/base/cef_lock_impl.cc` <- (CEF implementation)
- `libcef_dll/base/cef_thread_checker_impl.cc` <- (CEF implementation)

## Files NOT Synchronized (CEF-Specific)

- `cef_build.h` - CEF platform detection
- `cef_macros.h` - Kept for backwards compatibility
- `cef_logging.h` / `cef_logging.cc` - CEF-specific logging implementation
- `cef_dump_without_crashing.h/cc` - CEF C API wrapper
- `cef_trace_event.h` - CEF C API macro wrapper

## Reverts Applied

### 421425fe3324d: static_assert(false) -> AlwaysFalse pattern
- `internal/cef_bind_internal.h`: Changed `static_assert(false, ...)` back to
  `static_assert(AlwaysFalse<T>, ...)` in `ValidateReceiverType::ReceiverMustBePointerLike`

- `cef_callback.h`: Removed FunctionRef conversion operators entirely (dead code
  in standalone mode since FunctionRef is not ported)

## CEF-Specific Changes Applied

### Namespace Renames
- `base::internal` -> `cef_internal`
- `base::subtle` -> `cef_subtle` (in scoped_refptr)

### Include Path Changes
- `#include "base/..."` -> `#include "include/base/cef_..."`

### Header Structure
- CEF BSD copyright header
- `CEF_INCLUDE_BASE_CEF_*_H_` header guards
- `USING_CHROMIUM_INCLUDES` conditional structure

### Platform Macros
- `BUILDFLAG(IS_WIN)` -> `defined(OS_WIN)`
- `BUILDFLAG(IS_APPLE)` -> `defined(OS_MAC)`

### Removed Chromium Dependencies

- `raw_ptr` / `raw_ref` - CEF uses raw pointers
- `BASE_EXPORT` - Not needed for header-only standalone use
- `SafeRef` - Chromium-specific safety wrapper
- `FunctionRef` - Not ported to CEF standalone
