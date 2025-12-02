# CEF libcef Development Instructions for Claude Code

You are assisting with CEF libcef development - the implementation layer between Chromium and the CEF public API.

## Quick Start

When working with libcef code, you typically need to:

- **Add new APIs** - Expose new functionality to CEF clients
- **Implement handlers** - Create WebContentsDelegate and other Chromium delegate implementations
- **Fix bugs** - Debug and fix issues in the CEF implementation layer
- **Update for Chromium changes** - Adapt CEF to upstream Chromium API changes

## Build Configuration

**Determining the build output directory:**

The user will typically provide the output directory, or you can identify it:

- **Common directories:** `out/Debug_GN_x64` (Windows/Linux), `out/Debug_GN_arm64` (Mac ARM)
- **Check existing builds:** `ls out/` to see available build directories
- **Look for .gn args:** The directory usually contains an `args.gn` file

**Throughout this document, replace `out/Debug_GN_x64` with the actual output directory for your platform.**

## CEF API Versioning

CEF uses a strict API versioning system to maintain ABI compatibility across different CEF/Chromium versions. This allows client applications to update CEF binaries without recompilation.

### Overview

**Three API Levels:**

1. **Legacy API** - Pre-2025 API grandfathered in as stable (no explicit version)
2. **Versioned API** - Stable API introduced in 2025+ with explicit version numbers
3. **Experimental API** - New API marked as experimental (compiled out when stable version selected)

**Version Number Format:** `XXXYY`

- `XXX` = CEF/Chromium major version (e.g., `133`)
- `YY` = Incremental number reset per major version (e.g., `01`, `02`)

**Placeholder Values for Pull Requests:**

- `CEF_EXPERIMENTAL` - Experimental API not approved for stable
- `CEF_NEXT` - API approved by CEF admin for next stable version

**Which Placeholder Should You Use?**

When adding new APIs in a pull request, choose the appropriate placeholder:

```
Is this API intended for stable release?
├─ Yes, or user explicitly requested stable API
│  └─ Use CEF_NEXT (will be converted to version number by admin)
│
└─ No, or experimental/testing feature
   └─ Use CEF_EXPERIMENTAL (will be compiled out when stable version selected)
```

**Decision Guidelines:**

- **Use `CEF_NEXT`** when:
    - Adding core functionality that client apps will depend on
    - User says "add stable support for X"
    - Implementing standard browser features
    - Exposing existing Chromium functionality to CEF clients

- **Use `CEF_EXPERIMENTAL`** when:
    - Adding experimental or testing features
    - User explicitly says "add experimental X"
    - Prototyping new functionality
    - Uncertain if API design is final

**When in doubt, use `CEF_NEXT`.** The CEF admin will review and can request changes if experimental is more appropriate.

### Adding New API Methods

**Scope:** All new public APIs in `include/*.h` must be versioned. Internal implementation code in `libcef/` does not need API versioning (but may use runtime version checks).

When adding a new method to a CEF interface (e.g., `CefDownloadItem::IsPaused()`):

**1. Public API Header (`include/cef_*.h`):**

Add the method declaration with **both** `CEF_API_ADDED` macro and `cef(added=...)` metadata:

```cpp
#include "include/cef_api_hash.h"

class CefDownloadItem : public virtual CefBaseRefCounted {
 public:
  // Existing methods...

  ///
  /// Returns true if the download has been interrupted.
  ///
  /*--cef()--*/
  virtual bool IsInterrupted() = 0;

#if CEF_API_ADDED(CEF_NEXT)
  ///
  /// Returns true if the download has been paused.
  ///
  /*--cef(added=next)--*/
  virtual bool IsPaused() = 0;
#endif
};
```

**CRITICAL:** Use lowercase `next` in the metadata comment, not `CEF_NEXT`.

**2. Implementation Header (`libcef/browser/*_impl.h`):**

Add the override declaration **WITHOUT** `CEF_API_ADDED` protection:

```cpp
class CefDownloadItemImpl : public CefDownloadItem {
 public:
  // CefDownloadItem methods.
  bool IsValid() override;
  bool IsInProgress() override;
  bool IsComplete() override;
  bool IsCanceled() override;
  bool IsInterrupted() override;
  bool IsPaused() override;  // No CEF_API_ADDED here
  // ...
};
```

**3. Implementation File (`libcef/browser/*_impl.cc`):**

Add the implementation **WITHOUT** `CEF_API_ADDED` protection:

```cpp
bool CefDownloadItemImpl::IsInterrupted() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().GetState() == download::DownloadItem::INTERRUPTED;
}

bool CefDownloadItemImpl::IsPaused() {  // No CEF_API_ADDED here
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().IsPaused();
}
```

**Why This Pattern?**

This pattern supports CEF's versioning architecture:

- **Client-side:** Applications compile against **ONE** specific API version (selected at build time via `cef_api_version` define)
    - `CEF_API_ADDED(version)` in public headers excludes newer APIs the client hasn't opted into
    - Allows clients to update libcef binaries without recompilation

- **libcef-side:** The libcef binary must support **ALL** API versions simultaneously at runtime
    - Implementation code has no `CEF_API_ADDED` protection - all methods always exist
    - Uses `CEF_API_IS_ADDED(version)` runtime checks when calling client callbacks
    - Single binary serves clients compiled against different API versions

### Calling Versioned APIs from libcef

When calling versioned client APIs from libcef implementation code:

```cpp
#include "cef/libcef/common/api_version_util.h"

void CefClientImpl::SomethingHappened() {
  if (CEF_API_IS_ADDED(13305)) {
    handler->OnSomething();  // Only call if version supports it
  } else {
    // Default handling for older API versions
  }
}
```

**Available runtime macros (in libcef code):**

- `CEF_API_IS_ADDED(version)` - True if runtime version >= version
- `CEF_API_IS_REMOVED(version)` - True if runtime version >= version
- `CEF_API_IS_RANGE(from, to)` - True if runtime version in range

### Removing or Replacing APIs

When an API needs to be removed or replaced:

**1. Simple Removal** - Method removed entirely in a specific version:

```cpp
// include/cef_example.h
class CefExample : public virtual CefBaseRefCounted {
 public:
#if CEF_API_REMOVED(14000)
  ///
  /// Old method removed in version 14000.
  ///
  /*--cef(removed=14000)--*/
  virtual void OldMethod() = 0;
#endif
};
```

**IMPORTANT:** `CEF_API_REMOVED(14000)` is TRUE for versions < 14000 (old versions that still have the API). It compiles OUT the method for versions >= 14000.

**2. Method Replacement** - API changes over time with versioned names:

```cpp
// include/test/cef_api_version_test.h (example)
class CefApiVersionTestRefPtrLibrary : public CefBaseRefCounted {
 public:
#if CEF_API_REMOVED(13301)
  ///
  /// Return a value. This is replaced by GetValueV1 in version 13301.
  ///
  /*--cef(removed=13301)--*/
  virtual int GetValue() = 0;
#endif

#if CEF_API_RANGE(13301, 13302)
  ///
  /// Return a value (V1). Replaces GetValue in 13301, replaced by GetValueV2 in 13302.
  ///
  /*--cef(added=13301,removed=13302)--*/
  virtual int GetValueV1() = 0;
#endif

#if CEF_API_ADDED(13302)
  ///
  /// Return a value (V2). Replaces GetValueV1 in version 13302.
  ///
  /*--cef(added=13302)--*/
  virtual int GetValueV2() = 0;
#endif
};
```

**3. Class Replacement** - Entire class replaced with versioned variant:

```cpp
// include/test/cef_api_version_test.h (example)
#if CEF_API_REMOVED(13302)
///
/// Client-side child test object. Replaced by CefApiVersionTestRefPtrClientChildV2 in 13302.
///
/*--cef(source=client,removed=13302)--*/
class CefApiVersionTestRefPtrClientChild : public CefApiVersionTestRefPtrClient {
  // ...
};
#endif

#if CEF_API_ADDED(13302)
///
/// Client-side child test object V2. Replaces CefApiVersionTestRefPtrClientChild in 13302.
///
/*--cef(source=client,added=13302)--*/
class CefApiVersionTestRefPtrClientChildV2 : public CefApiVersionTestRefPtrClient {
  // ...
};
#endif
```

**Implementation Side (libcef):**

```cpp
// libcef/browser/example_impl.cc
void CefExampleImpl::CallHandler() {
#if CEF_API_IS_REMOVED(13301)
  if (handler_) {
    handler_->GetValue();  // Call removed API for old versions (< 13301)
  }
#elif CEF_API_IS_RANGE(13301, 13302)
  if (handler_) {
    handler_->GetValueV1();  // Call V1 for intermediate versions
  }
#elif CEF_API_IS_ADDED(13302)
  if (handler_) {
    handler_->GetValueV2();  // Call V2 for current versions (>= 13302)
  }
#endif
}
```

**Key Points:**

- `CEF_API_REMOVED(version)` includes the API for versions < version (before removal)
- `CEF_API_RANGE(from, to)` includes API for versions >= from AND < to
- C++ method names only need to change when signature changes would break overloading (add V1, V2 suffixes)
- C API can use explicit naming when needed: `/*--cef(added=13302,capi_name=get_value_v2)--*/`

**Examples:** See `include/test/cef_api_version_test.h` for comprehensive removal patterns

### After Modifying API Headers

When you add/remove/change versioned APIs in `include/*.h` files, you must regenerate the C API wrappers:

**1. Regenerate the C API wrappers:**

```bash
# From chromium/src/cef directory:

# Windows
create_debug.bat

# Linux/Mac
./create_debug.sh
```

**2. Rebuild:**

```bash
autoninja -C out/Debug_GN_x64 ceftests
```

**Why this is necessary:**

The CEF translator tool auto-generates C API wrappers from your C++ headers. The C API provides the ABI boundary between libcef and client applications. If you see build errors about:

- Missing C API symbols (e.g., `cef_download_item_t::is_paused`)
- Undefined references in `libcef_dll_wrapper`
- API hash mismatches

You likely forgot to regenerate after modifying headers.

**When to regenerate:**

- After adding new methods with `CEF_API_ADDED`
- After removing methods with `CEF_API_REMOVED`
- After modifying method signatures in `include/*.h`
- After changing struct definitions in `include/internal/cef_types*.h`

**Troubleshooting build errors:**

If you see **API hash mismatch errors**:
```
ERROR: API hash mismatch for version 13305
```

This means you changed the exported C API for an existing stable version. This is a fatal error - existing stable APIs must never change. Either:

- Fix your changes to maintain ABI compatibility (rename deprecated members, add new members at end)
- Contact CEF admin if this was unintentional

The `version_manager.py` tool runs during `cef_create_projects` and verifies API hashes. Any mismatch indicates ABI breakage.

### ABI Compatibility Guidelines

**Enums:**

- Add `_NUM_VALUES` at the end of all mutable enums
- New values go immediately before `_NUM_VALUES`
- Deprecated values renamed to `_DEPRECATED` or `_REMOVED` (never deleted)

Example from `include/internal/cef_types_content_settings.h`:

```cpp
typedef enum {
  CEF_CONTENT_SETTING_TYPE_COOKIES,
  CEF_CONTENT_SETTING_TYPE_IMAGES,
  // ... existing values ...

  // Old value deprecated but kept for ABI compatibility
  CEF_CONTENT_SETTING_TYPE_DEPRECATED_PPAPI_BROKER,

  // Conditional rename - old versions see original name, new see _DEPRECATED
#if CEF_API_ADDED(14000)
  CEF_CONTENT_SETTING_TYPE_PRIVATE_NETWORK_GUARD_DEPRECATED,
  CEF_CONTENT_SETTING_TYPE_PRIVATE_NETWORK_CHOOSER_DATA_DEPRECATED,
#else
  CEF_CONTENT_SETTING_TYPE_PRIVATE_NETWORK_GUARD,
  CEF_CONTENT_SETTING_TYPE_PRIVATE_NETWORK_CHOOSER_DATA,
#endif

#if CEF_API_ADDED(14200)
  // New value added in version 14200
  CEF_CONTENT_SETTING_TYPE_PERMISSION_ACTIONS_HISTORY,
#endif

  // Always last - used for iteration/validation
  CEF_CONTENT_SETTING_TYPE_NUM_VALUES,
} cef_content_setting_types_t;
```

Key points:

- Deprecated values stay at their original position in the enum
- Use conditional compilation for version-specific naming
- Values added in newer versions go before `_NUM_VALUES`
- The `_NUM_VALUES` sentinel enables safe iteration

**Structs:**

- All structs have `size_t size` member at beginning
- New members added at end only
- Deprecated members renamed to `_deprecated` or `_removed` (never deleted)
- Existing members never change type

Example from `include/internal/cef_types.h`:

```cpp
typedef struct _cef_browser_settings_t {
  ///
  /// Size of this structure.
  ///
  size_t size;  // Always first member

  // Existing members stay in their original position
  int windowless_frame_rate;
  cef_string_t standard_font_family;
  cef_state_t javascript;
  cef_state_t local_storage;

  // Deprecated member - renamed but not removed, stays at same position
#if CEF_API_ADDED(13800)
  cef_state_t databases_deprecated;  // New versions see this name
#else
  cef_state_t databases;  // Old versions see this name
#endif

  cef_state_t webgl;

  // New members always added at end
  cef_color_t background_color;
  cef_state_t chrome_status_bubble;
  cef_state_t chrome_zoom_bubble;
} cef_browser_settings_t;
```

Key points:

- `size` member allows runtime detection of struct version
- Deprecated members stay at same memory offset for ABI compatibility
- Conditional naming allows old code to compile unchanged
- Type never changes - if you need a different type, add a new member

**Methods:**

- Return value and parameters never change for existing methods
- New methods added at end of class
- Deprecated methods may be replaced with numbered/versioned variants
    - C++: `OnEvent()` → can use polymorphism with different params
    - C API: `on_event` → `on_event2`, `on_event3`, or `on_event_13305`

### Testing API Versions

**Build with specific API version:**

**WARNING:** Only do this with explicit user permission, as it will regenerate build files and may clobber the existing build configuration.

```bash
# Building from source:

# 1. Read the existing GN_DEFINES from cef/create_debug.bat or cef/create_debug.sh
# 2. Combine with the new cef_api_version arg
# Example:
export GN_DEFINES="is_official_build=false cef_api_version=13305"
export GN_OUT_CONFIGS=Debug_GN_x64

# 3. Regenerate build files:
./cef/cef_create_projects.sh

# 4. Build normally:
autoninja -C out/Debug_GN_x64 ceftests

# Building with binary distribution (cmake):
cmake ... -D api_version=13305
```

**Run API version tests:**
```bash
out/Debug_GN_x64/ceftests --gtest_filter=ApiVersionTest.*
```

**What to verify:**

When testing versioned APIs, verify that your API works correctly across version boundaries:

1. **With experimental API** (default build) - Your new API should be available
2. **With older stable version** (e.g., `cef_api_version=13305`) - Your new API should be compiled out, code using it should handle its absence gracefully
3. **With newer stable version** (once assigned) - Your API should be available and functional

Run the test suite with different API versions to ensure:

- Code compiles successfully with all targeted versions
- Runtime behavior adapts correctly to the configured version
- No ABI breakage occurs across version boundaries

**Example API version tests:** See `include/test/cef_api_version_test.h` for comprehensive test patterns

### Common Mistakes

❌ **WRONG** - Missing `CEF_API_ADDED` protection:
```cpp
// In include/cef_download_item.h
virtual bool IsPaused() = 0;  // Missing version protection!
```

❌ **WRONG** - Using `CEF_NEXT` in metadata (should be lowercase):
```cpp
/*--cef(added=CEF_NEXT)--*/  // Wrong! Should be lowercase "next"
virtual bool IsPaused() = 0;
```

❌ **WRONG** - Adding protection to implementation:
```cpp
// In libcef/browser/download_item_impl.cc
#if CEF_API_ADDED(CEF_NEXT)  // Wrong! Implementation should not be protected
bool CefDownloadItemImpl::IsPaused() {
  return const_value().IsPaused();
}
#endif
```

✓ **CORRECT** - Public header only protected:
```cpp
// include/cef_download_item.h
#if CEF_API_ADDED(CEF_NEXT)
/*--cef(added=next)--*/
virtual bool IsPaused() = 0;
#endif

// libcef/browser/download_item_impl.h
bool IsPaused() override;  // No protection

// libcef/browser/download_item_impl.cc
bool CefDownloadItemImpl::IsPaused() {  // No protection
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().IsPaused();
}
```

## Documentation References

- **API Versioning Wiki:** `//cef_wiki/ApiVersioning.md`
- **API Version Utilities:** `libcef/common/api_version_util.h`
- **Client-side Macros:** `include/cef_api_hash.h`
- **Example Tests:** `include/test/cef_api_version_test.h`

## WebContentsDelegate Implementation

WebContentsDelegate is Chromium's primary interface for browser embedders to respond to events from WebContents (tabs). CEF implements this interface to route callbacks to CEF client handlers.

### Architecture Overview

CEF has two browser implementations (Chrome style and Alloy style) that both need to handle WebContentsDelegate callbacks:

1. **CefBrowserContentsDelegate** (`libcef/browser/browser_contents_delegate.{cc,h}`)
    - Main implementation of WebContentsDelegate for both styles
    - Routes callbacks to CEF client handlers (e.g., CefFocusHandler, CefDisplayHandler)

2. **ChromeBrowserDelegate** (`libcef/browser/chrome/chrome_browser_delegate.{cc,h}`)
    - Chrome-style specific delegate
    - Inherits from `cef::BrowserDelegate` (CEF's abstraction)
    - **1:1 relationship with Browser** - Each Browser has one ChromeBrowserDelegate
    - Calls into CefBrowserContentsDelegate for shared logic via `GetDelegateForWebContents(source)`
    - `cef::BrowserDelegate` extends WebContentsDelegate, only declares methods that differ from the base implementation (e.g., `RequestMediaAccessPermissionEx()` with modified return value)

3. **AlloyBrowserHostImpl** (`libcef/browser/alloy/alloy_browser_host_impl.{cc,h}`)
    - Alloy-style specific implementation
    - Extends WebContentsDelegate directly (unlike Chrome which goes through cef::BrowserDelegate)
    - Uses `contents_delegate_` (CefBrowserContentsDelegate) directly

4. **Chromium Patches** (`patch/patches/chrome_browser_browser.patch`)
    - Patches `chrome/browser/ui/browser.cc` to call CEF delegates
    - Uses `CALL_CEF_DELEGATE_RETURN` macro for routing

**Platform-specific behavior** is delegated to `CefBrowserPlatformDelegate` implementations (e.g., views-based vs native). Both Chrome and Alloy styles use platform delegates for window management, input handling, and OS-specific functionality.

### Deciding Which Pattern to Use

**Step 1: Check if already implemented**

Before implementing any callback, check if CEF already has it:

```bash
# Check CEF implementations
grep -n "MethodName" \
  libcef/browser/browser_contents_delegate.{h,cc} \
  libcef/browser/chrome/chrome_browser_delegate.{h,cc} \
  libcef/browser/alloy/alloy_browser_host_impl.{h,cc}

# Check if already patched in Browser
grep -n "MethodName" patch/patches/chrome_browser_browser.patch
```

The callback may be implemented for one browser style but not the other. If already implemented for both styles, you may just need to update the existing implementation. If implemented for only one style, add the missing implementation to the other style.

**If not yet implemented, continue to Step 2.**

**Step 2: Determine which interface provides the callback**

Identify which Chromium interface provides the callback:

```bash
# Search for the method in Chromium
grep -r "virtual.*MethodName" \
  content/public/browser/web_contents_delegate.h \
  content/public/browser/web_contents_observer.h
```

**If the callback is in WebContentsObserver:**

- These are purely observational callbacks (no behavior override)
- Content layer calls these directly on observers
- Don't require Browser class involvement or Chromium patches
- **Implementation:** Add to CefBrowserContentsDelegate for shared functionality, or AlloyBrowserHostImpl for Alloy-only (no patches needed)
- See "WebContentsObserver Methods" section below for details on where to implement
- **You're done** - no need for Step 3

**If the callback is in WebContentsDelegate:**

- These can override/handle browser behavior
- Browser class may need to coordinate the callback
- Chrome-style requires routing through browser.cc
- **Implementation:** Continue to Step 3

**Step 3: Choose WebContentsDelegate implementation pattern**

For new WebContentsDelegate implementations, choose the appropriate pattern:

```
Does it need state management? (dialog managers, file pickers, etc.)
│
├─ YES → Use CefBrowserHostBase pattern
│  └─ Modify: CefBrowserContentsDelegate, ChromeBrowserDelegate,
│     AlloyBrowserHostImpl, browser.cc patch, + CefBrowserHostBase
│     (state storage + cleanup)
│     See "WebContentsDelegate Methods (Stateful - CefBrowserHostBase)" section below
│
└─ NO → Use stateless WebContentsDelegate pattern
   └─ Modify: CefBrowserContentsDelegate, ChromeBrowserDelegate,
      AlloyBrowserHostImpl, browser.cc patch
      See "WebContentsDelegate Methods (Stateless)" section below
```

### Understanding browser.cc Patches

Before implementing WebContentsDelegate callbacks, understand how CEF integrates with Chrome's Browser class via patches.

#### Macros Defined in browser.cc

The `chrome_browser_browser.patch` defines two macros for routing callbacks to CEF:

**CALL_CEF_DELEGATE_RETURN** - For methods with return values:
```cpp
// Macro definition (in browser.cc):
#define CALL_CEF_DELEGATE_RETURN(name, ...)          \
  if (cef_browser_delegate_) {                       \
    return cef_browser_delegate_->name(__VA_ARGS__); \
  }

// Usage:
bool Browser::TakeFocus(content::WebContents* source, bool reverse) {
  CALL_CEF_DELEGATE_RETURN(TakeFocus, source, reverse);
  return false;  // Chrome default for non-CEF browsers
}
```

**CALL_CEF_DELEGATE** - For void methods (no return):
```cpp
// Macro definition (in browser.cc):
#define CALL_CEF_DELEGATE(name, ...)          \
  if (cef_browser_delegate_) {                \
    cef_browser_delegate_->name(__VA_ARGS__); \
  }

// Usage:
void Browser::LoadingStateChanged(WebContents* source, bool should_show_loading_ui) {
  ScheduleUIUpdate(source, content::INVALIDATE_TYPE_LOAD);
  CALL_CEF_DELEGATE(LoadingStateChanged, source, should_show_loading_ui);
}
```

#### Observer vs Handler Pattern

**CRITICAL:** Understand whether CEF is observing or handling the callback:

**Observer Pattern** - CEF just wants notification, doesn't override behavior:
```cpp
// Example: LoadingStateChanged (void return)
void Browser::LoadingStateChanged(WebContents* source, bool should_show_loading_ui) {
  ScheduleUIUpdate(source, content::INVALIDATE_TYPE_LOAD);
  UpdateWindowForLoadingStateChanged(source, should_show_loading_ui);
  CALL_CEF_DELEGATE(LoadingStateChanged, source, should_show_loading_ui);  // No fallback needed
}
```
- Use `CALL_CEF_DELEGATE` (no return value)
- CEF gets notified, Chrome behavior continues normally
- No fallback logic required

**Handler Pattern** - CEF can handle/override behavior:
```cpp
// Example: TakeFocus (bool return)
bool Browser::TakeFocus(content::WebContents* source, bool reverse) {
  CALL_CEF_DELEGATE_RETURN(TakeFocus, source, reverse);  // Returns delegate result for CEF browsers
  return false;  // Chrome default for non-CEF browsers
}

// Example: GetJavaScriptDialogManager (pointer return, with explicit fallback check)
content::JavaScriptDialogManager* Browser::GetJavaScriptDialogManager(
    WebContents* source) {
  #if BUILDFLAG(ENABLE_CEF)
    if (cef_browser_delegate_) {
      auto* cef_manager = cef_browser_delegate_->GetJavaScriptDialogManager(source);
      if (cef_manager) {  // CEF provided a manager
        return cef_manager;
      }
      // Fall through to Chrome default for this CEF browser
    }
  #endif
  return TabModalDialogManager::FromWebContents(source);  // Chrome default
}
```
- For explicit fallback (like GetJavaScriptDialogManager): CEF returns `nullptr`, empty `std::optional`, etc. to indicate "use Chrome default"
- For simple delegation (like TakeFocus): CEF always handles for CEF browsers, Chrome default only for non-CEF browsers
- Never bypass Chrome's default - CEF should integrate, not replace

**Rule of thumb:**

- Void methods → Observer pattern → Use `CALL_CEF_DELEGATE`
- Non-void methods → Handler pattern → Check CEF return value, fall back to Chrome default

### WebContentsDelegate Methods (Stateless)

For WebContentsDelegate methods without state management (simple delegation to CEF client handlers).

**Where to implement:**

- **Shared functionality** (both Chrome and Alloy styles need it): Add to CefBrowserContentsDelegate, with routing through AlloyBrowserHostImpl, ChromeBrowserDelegate and browser.cc patch (full pattern below)
- **Alloy-only functionality**: Add to AlloyBrowserHostImpl only
    - When Chrome-style also needs the functionality, move the implementation from AlloyBrowserHostImpl to CefBrowserContentsDelegate

Example from commit `8cf30843f9` - implementing `TakeFocus()` callback as shared functionality:

**Step 1: Add to CefBrowserContentsDelegate**

```cpp
// libcef/browser/browser_contents_delegate.h
class CefBrowserContentsDelegate : public content::WebContentsDelegate {
 public:
  // Add override declaration
  bool TakeFocus(content::WebContents* source, bool reverse) override;
};
```

```cpp
// libcef/browser/browser_contents_delegate.cc
bool CefBrowserContentsDelegate::TakeFocus(content::WebContents* source,
                                           bool reverse) {
  // Call the CEF client handler
  if (auto c = client()) {
    if (auto handler = c->GetFocusHandler()) {
      handler->OnTakeFocus(browser(), !reverse);
    }
  }

  return false;  // Return value as needed by Chromium
}
```

**Step 2: Add to ChromeBrowserDelegate (Chrome style)**

**When to add to cef::BrowserDelegate:**

- If the method signature matches WebContentsDelegate exactly → Just implement in ChromeBrowserDelegate (inherits through cef::BrowserDelegate)
- If the method signature differs (e.g., modified return value) → Add to `cef::BrowserDelegate` first (in `libcef/browser/browser_delegate.h`), then implement in ChromeBrowserDelegate

For this example, `TakeFocus()` signature matches WebContentsDelegate, so no cef::BrowserDelegate changes needed:

```cpp
// libcef/browser/chrome/chrome_browser_delegate.h
class ChromeBrowserDelegate : public cef::BrowserDelegate {
 public:
  bool TakeFocus(content::WebContents* source, bool reverse) override;
};
```

```cpp
// libcef/browser/chrome/chrome_browser_delegate.cc
bool ChromeBrowserDelegate::TakeFocus(content::WebContents* source,
                                      bool reverse) {
  // Delegate to CefBrowserContentsDelegate
  if (auto delegate = GetDelegateForWebContents(source)) {
    return delegate->TakeFocus(source, reverse);
  }
  return false;
}
```

**Step 3: Update AlloyBrowserHostImpl (Alloy style)**

**Check if Alloy already implements it:**
```bash
grep -n "TakeFocus" libcef/browser/alloy/alloy_browser_host_impl.{h,cc}
```

If found and it already delegates to `contents_delegate_.TakeFocus()`, **skip this step** (already correct).

Otherwise, add the delegation:

```cpp
// libcef/browser/alloy/alloy_browser_host_impl.h
class AlloyBrowserHostImpl : public CefBrowserHostBase,
                             public content::WebContentsDelegate {
 public:
  bool TakeFocus(content::WebContents* source, bool reverse) override;
};
```

```cpp
// libcef/browser/alloy/alloy_browser_host_impl.cc
bool AlloyBrowserHostImpl::TakeFocus(content::WebContents* source,
                                     bool reverse) {
  // Delegate to CefBrowserContentsDelegate
  return contents_delegate_.TakeFocus(source, reverse);
}
```

**Step 4: Patch Chromium's Browser class**

Add the CEF callback routing to `chrome/browser/ui/browser.cc`. See "Understanding browser.cc Patches" section above for guidance on choosing the correct macro (CALL_CEF_DELEGATE vs CALL_CEF_DELEGATE_RETURN) and pattern (Observer vs Handler).

**Note:** The WebContentsDelegate method may not already have an implementation in `chrome/browser/ui/browser.{h,cc}`. If it doesn't exist, you'll need to add the entire method implementation to Browser, not just modify an existing one.

```diff
// patch/patches/chrome_browser_browser.patch
 bool Browser::TakeFocus(content::WebContents* source, bool reverse) {
+  CALL_CEF_DELEGATE_RETURN(TakeFocus, source, reverse);
   return false;
 }
```

**Step 5: Resave the Patch**

After manually editing `chrome/browser/ui/browser.cc`, you must resave the patch file:

```bash
# From chromium/src directory:
# Format the modified file first
git cl format chrome/browser/ui/browser.cc

# From chromium/src/cef/tools directory:
# Resave the chrome_browser_browser patch
python3 patch_updater.py --resave --patch chrome_browser_browser
```

**What this does:**

- Regenerates `patch/patches/chrome_browser_browser.patch` based on your manual changes
- Updates line numbers and offsets automatically

**Verification (recommended):**
```bash
# Verify the patch applies cleanly
python3 patch_updater.py --patch chrome_browser_browser
```

**IMPORTANT:** Always resave the patch after manually editing Chromium files. The patch file must stay in sync with the actual code changes.

**Step 6: Verify the Implementation**

After implementing all files, verify it works:

```bash
# Build and check for compilation errors
autoninja -C out/Debug_GN_x64 cef
```

**Add or update test coverage:**

If implementing new functionality, add tests to `//cef/tests/ceftests/`:

```bash
# Check if tests already exist
ls cef/tests/ceftests/*_unittest.cc | grep -i "find\|download\|focus"

# If tests exist for Alloy-only, update them to work with both styles
# Remove UseAlloyStyleBrowserGlobal() guards if functionality is now shared
```

**Test both browser styles:**

```bash
# Chrome style (default)
out/Debug_GN_x64/ceftests --use-views --gtest_filter="*TakeFocus*"

# Alloy style
out/Debug_GN_x64/ceftests --use-views --use-alloy-style --gtest_filter="*TakeFocus*"
```

**See `CLAUDE_TEST_INSTRUCTIONS.md` for:**

- Creating new browser-based tests with TestHandler
- Testing handler interfaces (CefFindHandler, CefDownloadHandler, etc.)
- Async operations and callback tracking
- Platform-specific test execution

### WebContentsDelegate Methods (Stateful - CefBrowserHostBase)

When functionality needs **state management** (dialog managers, file pickers, etc.)

**Where to implement:**

- **Shared functionality** (both Chrome and Alloy styles need it): Add to CefBrowserHostBase
- **Alloy-only functionality**: Add to AlloyBrowserHostImpl
    - When Chrome-style also needs the functionality, move the implementation from AlloyBrowserHostImpl to CefBrowserHostBase

**When to Use CefBrowserHostBase Directly vs Platform Delegate:**

**Use CefBrowserHostBase directly when:**

- Functionality is **cross-platform** (just calls Chromium components)
- Implementation only needs `GetWebContents()` - no platform-specific behavior
- Example: Find-in-page (just calls `find_in_page::FindTabHelper`)

**Use platform delegate when:**

- Functionality has **platform-specific implementations** (e.g., different behavior on Windows vs Mac)
- Needs platform-specific state or resources
- Example: Input handling, window management, IME support

**Don't unnecessarily route through platform_delegate_** when the functionality is purely cross-platform.

Example from commit `e4bb51f6f` - implementing `GetJavaScriptDialogManager()`.

**This follows the full pattern from above (Steps 1-6), plus additional state management:**

**Additional Step: Add State to CefBrowserHostBase**

```cpp
// libcef/browser/browser_host_base.h
#include "cef/libcef/browser/javascript_dialog_manager.h"

class CefBrowserHostBase : public CefBrowserHost {
 public:
  // Called from AlloyBrowserHostImpl::GetJavaScriptDialogManager and
  // ChromeBrowserDelegate::GetJavaScriptDialogManager.
  content::JavaScriptDialogManager* GetJavaScriptDialogManager();

 private:
  // Used for creating and managing JavaScript dialogs.
  std::unique_ptr<CefJavaScriptDialogManager> javascript_dialog_manager_;
};
```

```cpp
// libcef/browser/browser_host_base.cc
content::JavaScriptDialogManager*
CefBrowserHostBase::GetJavaScriptDialogManager() {
  if (!javascript_dialog_manager_) {
    javascript_dialog_manager_ =
        std::make_unique<CefJavaScriptDialogManager>(this);
  }
  return javascript_dialog_manager_.get();
}

void CefBrowserHostBase::DestroyWebContents(...) {
  // Clean up shared state
  if (javascript_dialog_manager_) {
    javascript_dialog_manager_->Destroy();
    javascript_dialog_manager_.reset();
  }
}
```

**Modified Step 1: CefBrowserContentsDelegate calls CefBrowserHostBase**

Instead of implementing logic directly, delegate to the base class:

```cpp
// libcef/browser/browser_contents_delegate.cc
content::JavaScriptDialogManager*
CefBrowserContentsDelegate::GetJavaScriptDialogManager(
    content::WebContents* source) {
  // Delegate to browser host (which has the shared state)
  return browser_host_->GetJavaScriptDialogManager();
}
```

**Modified Step 2: AlloyBrowserHostImpl delegates to base class**

```cpp
// libcef/browser/alloy/alloy_browser_host_impl.cc
content::JavaScriptDialogManager*
AlloyBrowserHostImpl::GetJavaScriptDialogManager(content::WebContents* source) {
  return CefBrowserHostBase::GetJavaScriptDialogManager();  // Call base class
}
```

**Modified Step 3: ChromeBrowserDelegate gets browser_host first**

Key difference: Must get `browser_host` via `GetBrowserForContents()` to access base class:

```cpp
// libcef/browser/chrome/chrome_browser_delegate.cc
content::JavaScriptDialogManager*
ChromeBrowserDelegate::GetJavaScriptDialogManager(
    content::WebContents* source) {
  auto browser_host = ChromeBrowserHostImpl::GetBrowserForContents(source);
  if (browser_host) {
    return browser_host->GetJavaScriptDialogManager();  // Call base class
  }
  return nullptr;
}
```

**Step 4 and Step 5:** Same as standard pattern (patch browser.cc, resave).

**Key Differences from Standard Pattern:**

1. **State Storage**: Shared state lives in CefBrowserHostBase (not CefBrowserContentsDelegate)
2. **Cleanup**: Add cleanup in `CefBrowserHostBase::DestroyWebContents()`
3. **Access Pattern**: All implementations delegate to `CefBrowserHostBase::MethodName()`
4. **Chrome Style**: Must get `browser_host` first via `GetBrowserForContents()`

**When to Use This Pattern:**

- Need state management (dialog managers, file pickers, etc.)
- Complex initialization logic
- Cleanup coordinated with browser lifecycle
- Functionality shared by both Chrome and Alloy styles

**Finding Incomplete Implementations:**

ChromeBrowserHostImpl may have `NOTIMPLEMENTED()` stubs for API methods that need to be implemented:

```cpp
// libcef/browser/chrome/chrome_browser_host_impl.cc
void ChromeBrowserHostImpl::SomeApiMethod() {
  NOTIMPLEMENTED();  // Stub indicates this needs implementation
}
```

**When you find NOTIMPLEMENTED() stubs:**

1. Check if AlloyBrowserHostImpl has a working implementation
2. If yes, combine both implementations and move to CefBrowserHostBase
3. Update both Chrome and Alloy to delegate to the base class
4. Remove the NOTIMPLEMENTED() stub

**Search for stubs:**
```bash
# Find NOTIMPLEMENTED() stubs in Chrome implementation
grep -n "NOTIMPLEMENTED" libcef/browser/chrome/chrome_browser_host_impl.cc

# Check if Alloy has a real implementation
grep -n "MethodName" libcef/browser/alloy/alloy_browser_host_impl.{h,cc}
```

This pattern consolidates functionality and ensures consistent behavior across both browser styles.

### WebContentsObserver Methods (No Patch Required)

CefBrowserContentsDelegate inherits from **both** `content::WebContentsDelegate` **and** `content::WebContentsObserver`.

WebContentsObserver methods are called directly by the content layer, **not routed through browser.cc**.

**Where to implement:**

- **Shared functionality** (both Chrome and Alloy styles need it): Add to CefBrowserContentsDelegate
- **Alloy-only functionality**: Add to AlloyBrowserHostImpl only
    - When Chrome-style also needs the functionality, move the implementation from AlloyBrowserHostImpl to CefBrowserContentsDelegate

**Pattern for WebContentsObserver methods:**

```cpp
// libcef/browser/browser_contents_delegate.h
class CefBrowserContentsDelegate : public content::WebContentsDelegate,
                                   public content::WebContentsObserver {
 public:
  // Override WebContentsObserver method directly
  void DidFinishNavigation(content::NavigationHandle* navigation_handle) override;
};
```

```cpp
// libcef/browser/browser_contents_delegate.cc
void CefBrowserContentsDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Call CEF client handler
  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      handler->OnLoadEnd(browser(), ...);
    }
  }
}
```

### Additional Considerations

**Parameter Transformations:**

- CEF handler signatures may differ from Chromium (e.g., `!reverse` → `reverse` in TakeFocus)
- Always check existing CEF handler signatures before implementing
- Adapt parameters as needed to match CEF's API conventions

**CEF API Default Return Values:**

- CEF API methods (in `include/*.h`) should return `false`, `nullptr`, or other zero-type values by default
- This signals "use default behavior" to libcef, allowing Chrome's default implementation to run
- Only return non-zero values when the client explicitly handles the callback
- Example: `CefLifeSpanHandler::OnBeforePopup()` returns `false` by default (allow popup), client returns `true` to cancel

## Additional Development Patterns

*(Additional sections to be added as patterns are discovered)*
