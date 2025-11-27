# CEF Binary Distribution Development Instructions for Claude Agents

This document provides guidance for Claude agents working with CEF binary distributions to implement changes to CEF applications.

**Primary focus:** cefsimple and cefsimple_capi examples (simplest applications for learning)

**Also applicable to:** cefclient, ceftests, and custom CEF applications (see "Adapting These Instructions for Other Applications" section)

## Environment Context

**What you have:**

- CEF binary distribution (e.g., `cef_binary_<version>_<platform>.tar.bz2`)
- A task to implement changes (e.g., "add custom scheme support to cefsimple")

**What you're working with:**

- Pre-built CEF binaries (no Chromium source code)
- CMake build system
- Example applications: cefsimple (C++ API) or cefsimple_capi (C API)
- Platform: Windows, macOS, or Linux

## Accessing CEF Documentation

CEF wiki documentation is referenced throughout this guide. You can access it in two ways:

**Option 1: Read wiki pages directly via WebFetch (if available)**

The WebFetch tool can read Bitbucket wiki URLs:

```
Use WebFetch to read: https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage
```

**Option 2: Clone the wiki repository**

If WebFetch is unavailable or you need to search across multiple pages, clone the wiki repository:

```bash
# Clone CEF wiki documentation (markdown files)
git clone https://bitbucket.org/chromiumembedded/cef.git/wiki cef_wiki

# Wiki pages are markdown files:
# cef_wiki/GeneralUsage.md
# cef_wiki/Tutorial.md
# cef_wiki/HandsOnTutorial.md
# cef_wiki/JavaScriptIntegration.md
# etc.
```

Then use the Read tool to access documentation files directly.

## Adapting These Instructions for Other Applications

These instructions use **cefsimple** as the reference. For other CEF applications (cefclient, custom apps):

**Discover app structure first:**

```bash
Glob: "tests/*/CMakeLists.txt"           # Find available apps
Glob: "tests/app_name/*_app.cc"          # Find CefApp implementation
Grep: "class.*: public CefClient"        # Find handler files
Read: tests/app_name/CMakeLists.txt      # Understand build configuration
```

**Translation rules:**

- `simple_app.cc` → your app's CefApp implementation (e.g., `client_app.cc`)
- `simple_handler.cc` → your app's CefClient implementation (e.g., `client_handler.cc`)
- `--target cefsimple` → `--target your_app_name`
- Output: `build/tests/your_app_name/...`

**What stays the same:**

All CEF APIs (CefSchemeHandlerFactory, CefV8Handler, etc.), handler patterns, threading model (TID_UI, TID_RENDERER), reference counting (CefRefPtr), documentation resources, CMake patterns, platform-specific naming (`*_win.cc`, `*_mac.mm`, `*_linux.cc`).

**For complex apps (cefclient):**

- May have subdirectories: `browser/`, `renderer/`, `common/`
- Use Grep to find where specific handlers are implemented (e.g., search for `GetLifeSpanHandler`)
- Follow existing organizational patterns (don't add everything to one file)
- Check CMakeLists.txt when adding new files

## Binary Distribution Structure

```
cef_binary_<version>_<platform>/
├── CMakeLists.txt                      # Root CMake configuration
├── cmake/                              # CMake helper files
├── include/                            # CEF header files
│   ├── cef_*.h                         # C++ API headers
│   └── capi/                           # C API headers
├── libcef_dll/                         # C++ wrapper implementation
├── Release/                            # Release build artifacts
│   ├── libcef.so/.dylib/.dll           # Main CEF library
│   ├── *.bin                           # Binary data files
│   └── ...                             # Other required files
├── Debug/                              # Debug build artifacts (similar to Release)
├── Resources/                          # Resource files (*.pak, etc.)
└── tests/                              # Example applications
    ├── cefsimple/                      # Simple C++ example
    ├── cefsimple_capi/                 # Simple C example
    ├── cefclient/                      # Comprehensive example application
    └── ceftests/                       # Unit tests
```

## Key CEF Concepts

### Multi-Process Architecture

CEF runs multiple processes:

- **Browser process** - Main process (UI, navigation, coordination)
- **Renderer process** - Web content (isolated, sandboxed, usually one per site)
- **GPU process** - Graphics acceleration
- **Other processes** - Network service, audio service, etc.

**For cefsimple modifications:**

- Browser process code: `simple_app.cc` (OnContextInitialized), `simple_handler.cc`
- Renderer process code: `simple_app.cc` (OnContextCreated, V8 handlers)
- Communication: Process messages (CefProcessMessage)

### Threading Model

Most CEF callbacks run on specific threads:

- **TID_UI** - Browser process main thread (most cefsimple code runs here)
- **TID_IO** - Network operations
- **TID_RENDERER** - Renderer process main thread (JavaScript execution)

**Use thread assertions:**

```cpp
CEF_REQUIRE_UI_THREAD();        // Browser process UI thread
CEF_REQUIRE_RENDERER_THREAD();  // Renderer process main thread
```

**Cross-thread task posting:**

```cpp
#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"

CefPostTask(TID_UI, base::BindOnce(&MyClass::MyMethod, this, arg));
```

### Reference Counting

CEF objects use reference counting for memory management:

```cpp
CefRefPtr<CefBrowser> browser = ...;  // Smart pointer
IMPLEMENT_REFCOUNTING(MyHandler);     // In your class
```

### Handler Patterns

CEF uses handler interfaces for callbacks:

```cpp
class SimpleHandler : public CefClient,
                      public CefLifeSpanHandler,
                      public CefLoadHandler {
 public:
  // CefClient methods:
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  // CefLifeSpanHandler methods:
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override { ... }
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override { ... }

  // CefLoadHandler methods:
  void OnLoadEnd(CefRefPtr<CefBrowser> browser, ...) override { ... }

 private:
  IMPLEMENT_REFCOUNTING(SimpleHandler);
  DISALLOW_COPY_AND_ASSIGN(SimpleHandler);
};
```

## Build and Test Workflow

### Initial Setup and Verification

**1. Extract binary distribution:**

```bash
tar -xjf cef_binary_<version>_<platform>.tar.bz2
cd cef_binary_<version>_<platform>
```

**2. Build cefsimple as-is to verify environment:**

**Windows:**
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64 -DUSE_SANDBOX=OFF
cmake --build build --config Release --target cefsimple
build\tests\cefsimple\Release\cefsimple.exe
```

**Linux:**
```bash
cmake -B build
cmake --build build --target cefsimple
build/tests/cefsimple/cefsimple
```

**macOS:**
```bash
cmake -B build -G Xcode
cmake --build build --config Release --target cefsimple
open build/tests/cefsimple/Release/cefsimple.app
```

### Build System Notes

**CMakeLists.txt changes:**

- If you add new source files, update `tests/cefsimple/CMakeLists.txt`
- After CMakeLists.txt changes, reconfigure: `cmake -B build -G <generator>`

**Platform-specific files:**

- Windows: `cefsimple_win.cc`, `simple_handler_win.cc` (if exists)
- Linux: `cefsimple_linux.cc`, `simple_handler_linux.cc` (if exists)
- macOS: `cefsimple_mac.mm`, `simple_handler_mac.mm` (if exists), `process_helper_mac.cc`

**macOS helper processes:**

- macOS uses separate helper executables for sub-processes
- If you modify renderer process code, update `CEFSIMPLE_HELPER_SRCS_MAC` in CMakeLists.txt
- Include both `simple_app.cc/h` and `simple_handler.cc/h` in helper sources

## Common Implementation Patterns

This section shows you **how to approach** common tasks. For complete implementation details, consult [GeneralUsage.md](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage) and the header file documentation.

### Custom Scheme Handler

**Task:** "Add custom 'foobar' scheme to cefsimple"

**Implementation steps:**

1. Override `CefApp::OnRegisterCustomSchemes()` to register your scheme
2. Create `CefResourceHandler` subclass to handle requests
3. Create `CefSchemeHandlerFactory` subclass to create handlers
4. Call `CefRegisterSchemeHandlerFactory()` in `OnContextInitialized()`

**Files to modify:** `simple_app.h/cc` (or create separate handler files)

**Documentation:**

- [GeneralUsage.md - Request Handling](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage#markdown-header-request-handling) - Complete guide
- `include/cef_scheme.h`, `include/cef_resource_handler.h` - API documentation
- [cef-project/examples/scheme_handler](https://bitbucket.org/chromiumembedded/cef-project/src/master/examples/scheme_handler/) - Working example

**Test:** `build/tests/cefsimple/cefsimple --url=foobar://test`

### JavaScript-to-C++ Communication

**Task:** "Add JavaScript function that calls C++"

**Implementation steps:**

1. Create `CefV8Handler` subclass in renderer process
2. Register JavaScript functions in `OnContextCreated()`
3. Send process messages from V8 handler to browser process
4. Handle messages in `OnProcessMessageReceived()` in browser process

**Files to modify:** `simple_app.h/cc`, `simple_handler.h/cc`, and on **macOS**: `CMakeLists.txt`, `process_helper_mac.cc`

**Documentation:**

- [HandsOnTutorial.md Step 6](https://bitbucket.org/chromiumembedded/cef/wiki/HandsOnTutorial#markdown-header-step-6-javascript-to-c-communication) - Complete example
- [JavaScriptIntegration.md](https://bitbucket.org/chromiumembedded/cef/wiki/JavaScriptIntegration) - V8 integration guide
- [GeneralUsage.md - IPC](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage#markdown-header-inter-process-communication-ipc) - Process messages
- [cef-project/examples/message_router](https://bitbucket.org/chromiumembedded/cef-project/src/master/examples/message_router/) - CefMessageRouter for request/response patterns
- `include/cef_v8.h`, `include/cef_process_message.h` - API documentation

### Request Interception

**Task:** "Intercept and modify network requests"

**Implementation steps:**

1. Add `CefRequestHandler` to SimpleHandler inheritance
2. Implement `GetRequestHandler()` to return `this`
3. Implement `GetResourceRequestHandler()` to return custom handler
4. Override callbacks like `OnBeforeResourceLoad()`, `OnResourceResponse()`, etc.

**Files to modify:** `simple_handler.h/cc`

**Documentation:**

- [GeneralUsage.md - Request Handling](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage#markdown-header-request-handling) - Complete guide
- `include/cef_request_handler.h`, `include/cef_resource_request_handler.h` - API documentation
- [cef-project/examples/resource_manager](https://bitbucket.org/chromiumembedded/cef-project/src/master/examples/resource_manager/) - Working example

### Popup Window Customization

**Task:** "Customize popup window appearance"

**Implementation steps:**

1. **For popup policy:** Override `OnBeforePopup()` in SimpleHandler
2. **For Views styling:** Override `OnPopupBrowserViewCreated()` in SimpleBrowserViewDelegate
3. Create custom `CefWindow` with different size, position, etc.

**Files to modify:** `simple_handler.h/cc` (policy), `simple_app.cc` (Views styling)

**Documentation:**

- [HandsOnTutorial.md Step 8](https://bitbucket.org/chromiumembedded/cef/wiki/HandsOnTutorial#markdown-header-step-8-views-styling-of-popup-browsers) - Complete example
- `include/cef_life_span_handler.h`, `include/views/cef_browser_view_delegate.h` - API documentation

## Documentation Resources

### Essential Wiki Pages

**Start here:**

- [Tutorial.md](https://bitbucket.org/chromiumembedded/cef/wiki/Tutorial) - Architectural overview
- [HandsOnTutorial.md](https://bitbucket.org/chromiumembedded/cef/wiki/HandsOnTutorial) - Step-by-step practical guide
- [GeneralUsage.md](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage) - Comprehensive reference

**Specific topics:**

- [JavaScriptIntegration.md](https://bitbucket.org/chromiumembedded/cef/wiki/JavaScriptIntegration) - V8 integration, JavaScript bindings
- [GeneralUsage.md#IPC](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage#markdown-header-inter-process-communication-ipc) - Process messages
- [GeneralUsage.md#RequestHandling](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage#markdown-header-request-handling) - Network interception, custom schemes

**C API:**

- [UsingTheCAPI.md](https://bitbucket.org/chromiumembedded/cef/wiki/UsingTheCAPI) - C API guide

### Header File Documentation

CEF headers are well-documented with inline comments:

```cpp
// Read the headers directly for detailed API documentation
include/cef_app.h              // CefApp, process handlers
include/cef_browser.h          // CefBrowser, browser operations
include/cef_client.h           // CefClient, handler registration
include/cef_frame.h            // CefFrame, JavaScript execution
include/cef_request.h          // CefRequest, network requests
include/cef_resource_handler.h // CefResourceHandler, custom responses
include/cef_scheme.h           // CefSchemeHandlerFactory, custom schemes
include/cef_v8.h               // V8 integration
include/views/                 // Views framework headers
```

### Example Applications

**In binary distribution:**

- `tests/cefsimple/` - Minimal C++ example (your working directory)
- `tests/cefsimple_capi/` - Minimal C example
- `tests/cefclient/` - Comprehensive example (Chrome style, OSR, request handlers, etc.)
- `tests/ceftests/` - Unit tests

**Additional examples (online):**

- [cef-project examples](https://bitbucket.org/chromiumembedded/cef-project/src/master/examples/) - Focused feature examples (message_router, resource_manager, scheme_handler)

## Task Execution Strategy

### Understanding the Task

**1. Identify what the task requires:**

- New CEF handlers? (CefSchemeHandlerFactory, CefResourceHandler, etc.)
- New callbacks? (OnBeforePopup, OnLoadEnd, etc.)
- JavaScript integration? (V8 handlers, process messages)
- Network operations? (Request interception, custom responses)

**2. Find relevant documentation:**

- Search wiki pages for the feature
- Read header file documentation for the handlers
- Look for example code in `tests/cefclient/` (included in binary distribution)

**3. Identify files to modify:**

- `simple_app.h/cc` - Application-level handlers, browser creation
- `simple_handler.h/cc` - Browser-level handlers, lifecycle
- Platform-specific files - Only if adding platform-specific code
- `CMakeLists.txt` - Only if adding new files

### Implementation Approach

**1. Start minimal:**

- Add the minimal code to verify the concept works
- Test early and often

**2. Follow existing patterns:**

- cefsimple uses specific patterns (singleton handler, Views framework)
- Don't fight the existing architecture
- Add to existing classes rather than creating new hierarchies

**3. Build incrementally:**

```
Step 1: Add handler interface inheritance
        → Build (should succeed, just adds empty overrides)

Step 2: Add minimal implementation
        → Build → Request user testing

Step 3: Add complete implementation
        → Build → Request user testing

Step 4: Add error handling, edge cases
        → Build → Request user testing
```

**Important:** You typically cannot run cefsimple directly (GUI application). After verifying the build succeeds, **ask the user to test** the application and report results. Wait for their feedback before proceeding to the next step.

**4. Debug with CEF logging:**

```cpp
#include "include/base/cef_logging.h"

LOG(INFO) << "Browser created: " << browser->GetIdentifier();
LOG(WARNING) << "Unexpected state: " << state;
LOG(ERROR) << "Failed to load: " << url.ToString();
```

CEF logs to stderr by default. For more information about logging configuration, command-line flags, and log file locations, see [Chromium Logging Documentation](https://www.chromium.org/for-testers/enable-logging/).

## Common Pitfalls

### Thread Safety

**Problem:** Accessing CEF objects from wrong thread

**Solution:** Use thread assertions and CefPostTask

```cpp
void MyHandler::MyMethod() {
  CEF_REQUIRE_UI_THREAD();  // Crashes if called from wrong thread

  // Safe to use UI-thread objects here
}

// From another thread:
CefPostTask(TID_UI, base::BindOnce(&MyHandler::MyMethod, this));
```

### Reference Counting

**Problem:** Object lifetime issues, crashes

**Solution:** Always use CefRefPtr, never raw pointers

```cpp
// Good:
CefRefPtr<CefBrowser> browser = ...;
browser->GetMainFrame()->LoadURL(url);

// Bad:
CefBrowser* browser = ...;  // Don't do this!
```

### Platform-Specific Code

**Problem:** Feature only works on one platform

**Solution:** Check existing platform patterns

- macOS uses `.mm` files for Objective-C++
- macOS helper processes need separate configuration
- Windows/Linux share more code patterns

## macOS-Specific Considerations

### Helper Process Configuration

macOS uses separate executables for sub-processes (renderer, GPU, etc.). If your changes affect renderer process code:

**1. Update CMakeLists.txt helper sources:**

```cmake
set(CEFSIMPLE_HELPER_SRCS_MAC
  process_helper_mac.cc
  simple_app.cc
  simple_app.h
  simple_handler.cc  # Include if SimpleApp references it
  simple_handler.h
  )
```

**2. Update process_helper_mac.cc:**

```cpp
#include "tests/cefsimple/simple_app.h"

// In main():
CefRefPtr<SimpleApp> app(new SimpleApp);
return CefExecuteProcess(main_args, app, nullptr);
```

**3. Reconfigure CMake after CMakeLists.txt changes:**

```bash
cmake -B build -G Xcode
```

**Note:** CMake automatically creates the correct app bundle structure (Frameworks, Resources, Helper executables).

## Quick Reference

### Build Commands

```bash
# Windows (Visual Studio 2022, x64, sandbox disabled)
cmake -B build -G "Visual Studio 17 2022" -A x64 -DUSE_SANDBOX=OFF
cmake --build build --config Release --target cefsimple
build\tests\cefsimple\Release\cefsimple.exe

# Linux
cmake -B build
cmake --build build --target cefsimple
build/tests/cefsimple/cefsimple

# macOS (Xcode)
cmake -B build -G Xcode
cmake --build build --config Release --target cefsimple
open build/tests/cefsimple/Release/cefsimple.app

# Rebuild after changes
# Windows/macOS:
cmake --build build --config Release --target cefsimple
# Linux:
cmake --build build --target cefsimple
```

### Common Modifications

**Add new source files:**

1. Create `tests/cefsimple/my_file.h/cc`
2. Update `tests/cefsimple/CMakeLists.txt`:
    ```cmake
    set(CEFSIMPLE_SRCS
     ...
     my_file.cc
     my_file.h
    )
    ```
3. Reconfigure: `cmake -B build -G <generator>`

**Add handler to SimpleHandler:**

```cpp
// simple_handler.h
class SimpleHandler : public CefClient,
                      public CefNewHandler {  // Add inheritance
  CefRefPtr<CefNewHandler> GetNewHandler() override { return this; }
  void NewMethod() override { /* implement */ }
};
```

**Register custom scheme:**

```cpp
// simple_app.h
void OnRegisterCustomSchemes(
    CefRawPtr<CefSchemeRegistrar> registrar) override;

// simple_app.cc
void SimpleApp::OnRegisterCustomSchemes(...) {
  registrar->AddCustomScheme("myscheme",
      CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_CORS_ENABLED);
}
```

See [GeneralUsage.md - Request Handling](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage#markdown-header-request-handling) for complete scheme handler implementation.

**JavaScript execution:**

```cpp
frame->ExecuteJavaScript("alert('Hello!');", frame->GetURL(), 0);
```

See [HandsOnTutorial.md Step 5](https://bitbucket.org/chromiumembedded/cef/wiki/HandsOnTutorial#markdown-header-step-5-javascript-execution) for examples.

**Process messages:**

```cpp
// Renderer → Browser
CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("greeting");
msg->GetArgumentList()->SetString(0, "Hello");
frame->SendProcessMessage(PID_BROWSER, msg);

// Receive in browser
bool SimpleHandler::OnProcessMessageReceived(...) {
  if (message->GetName() == "greeting") {
    CefString msg = message->GetArgumentList()->GetString(0);
    // Handle message
    return true;
  }
  return false;
}
```

See [HandsOnTutorial.md Step 6](https://bitbucket.org/chromiumembedded/cef/wiki/HandsOnTutorial#markdown-header-step-6-javascript-to-c-communication) and [GeneralUsage.md - IPC](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage#markdown-header-inter-process-communication-ipc) for complete examples.
