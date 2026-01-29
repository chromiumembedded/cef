This page provides a hands-on tutorial for building a CEF application by progressively modifying the cefsimple example.

**Contents**

- [Introduction](#introduction)
  - [CEF Quick Concepts](#cef-quick-concepts)
- [Step 0: Setup and Verify](#step-0-setup-and-verify)
  - [Download and Build cefsimple](#download-and-build-cefsimple)
  - [Understanding cefsimple Structure](#understanding-cefsimple-structure)
- [Step 1: Simplify to Absolute Minimum](#step-1-simplify-to-absolute-minimum)
  - [What We're Removing](#what-were-removing)
  - [Architecture Overview](#architecture-overview)
  - [Modified Files](#modified-files)
    - [tests/cefsimple/simple_handler.h](#testscefsimplesimple_handlerh)
    - [tests/cefsimple/simple_handler.cc](#testscefsimplesimple_handlercc)
    - [tests/cefsimple/simple_app.h](#testscefsimplesimple_apph)
    - [tests/cefsimple/simple_app.cc](#testscefsimplesimple_appcc)
    - [Delete Platform-Specific Handler Files](#delete-platform-specific-handler-files)
    - [Update CMakeLists.txt](#update-cmakeliststxt)
    - [Keep Unchanged](#keep-unchanged)
  - [Limitations of This Simplified Version](#limitations-of-this-simplified-version)
  - [Build and Run](#build-and-run)
  - [What's Happening](#whats-happening)
  - [Common Issues](#common-issues)
- [Step 2: Custom URL from Code](#step-2-custom-url-from-code)
  - [What We're Adding](#what-were-adding)
  - [Modified Files](#modified-files)
    - [tests/cefsimple/simple_app.cc](#testscefsimplesimple_appcc)
  - [Build and Run](#build-and-run)
  - [What's Happening](#whats-happening)
- [Step 3: Command-Line URL Support](#step-3-command-line-url-support)
  - [What We're Adding](#what-were-adding)
  - [Modified Files](#modified-files)
    - [tests/cefsimple/simple_app.cc](#testscefsimplesimple_appcc)
  - [Build and Run](#build-and-run)
  - [What's Happening](#whats-happening)
- [Step 4: Window Title Updates](#step-4-window-title-updates)
  - [What We're Adding](#what-were-adding)
  - [Modified Files](#modified-files)
    - [tests/cefsimple/simple_handler.h](#testscefsimplesimple_handlerh)
    - [tests/cefsimple/simple_handler.cc](#testscefsimplesimple_handlercc)
  - [Build and Run](#build-and-run)
  - [What's Happening](#whats-happening)
- [Step 5: JavaScript Execution](#step-5-javascript-execution)
  - [What We're Adding](#what-were-adding)
  - [Modified Files](#modified-files)
    - [tests/cefsimple/simple_handler.h](#testscefsimplesimple_handlerh)
    - [tests/cefsimple/simple_handler.cc](#testscefsimplesimple_handlercc)
  - [Build and Run](#build-and-run)
  - [What's Happening](#whats-happening)
- [Step 6: JavaScript-to-C++ Communication](#step-6-javascript-to-c-communication)
  - [What We're Adding](#what-were-adding)
  - [Modified Files](#modified-files)
    - [tests/cefsimple/simple_app.h](#testscefsimplesimple_apph)
    - [tests/cefsimple/simple_app.cc](#testscefsimplesimple_appcc)
    - [Platform Entry Points](#platform-entry-points)
      - [tests/cefsimple/cefsimple_linux.cc](#testscefsimplecefsimple_linuxcc)
      - [tests/cefsimple/cefsimple_win.cc](#testscefsimplecefsimple_wincc)
      - [tests/cefsimple/process_helper_mac.cc](#testscefsimpleprocess_helper_maccc)
      - [tests/cefsimple/CMakeLists.txt (macOS only)](#testscefsimplecmakeliststxt-macos-only)
    - [tests/cefsimple/simple_handler.h](#testscefsimplesimple_handlerh)
    - [tests/cefsimple/simple_handler.cc](#testscefsimplesimple_handlercc)
  - [Build and Run](#build-and-run)
  - [What's Happening](#whats-happening)
- [Step 7: Multiple Browser Support](#step-7-multiple-browser-support)
  - [What We're Adding](#what-were-adding)
  - [Why This Matters](#why-this-matters)
  - [Modified Files](#modified-files)
    - [tests/cefsimple/simple_handler.h](#testscefsimplesimple_handlerh)
    - [tests/cefsimple/simple_handler.cc](#testscefsimplesimple_handlercc)
  - [Build and Run](#build-and-run)
  - [What's Happening](#whats-happening)
- [Step 8: Views Styling of Popup Browsers](#step-8-views-styling-of-popup-browsers)
  - [What We're Adding](#what-were-adding)
  - [Why This Matters](#why-this-matters)
  - [Modified Files](#modified-files)
    - [tests/cefsimple/simple_app.cc](#testscefsimplesimple_appcc)
  - [Build and Run](#build-and-run)
  - [What's Happening](#whats-happening)
- [Next Steps](#next-steps)
  - [Explore More Features](#explore-more-features)
  - [Learn Advanced Topics](#learn-advanced-topics)
  - [Using the C API](#using-the-c-api)
  - [Using Claude Code for Implementation](#using-claude-code-for-implementation)

---

# Introduction

This tutorial teaches CEF development by **modifying the cefsimple example** step-by-step. You'll start with a working application and progressively add features, learning CEF concepts along the way.

**Who is this for?**

- Developers new to CEF who want hands-on learning
- C++ developers with basic knowledge (classes, pointers, inheritance)
- Those who prefer learning by doing rather than reading reference documentation
- Anyone who wants to understand CEF architecture through practice

**Why start from cefsimple?**

- ✅ CMake already configured correctly
- ✅ Resources (`.pak` files, frameworks) already in the right place
- ✅ macOS app bundle structure already set up
- ✅ Platform-specific entry points already working
- ✅ You can verify your build environment works before making changes

**Prerequisites:**

- CEF binary distribution for your platform ([download here](https://cef-builds.spotifycdn.com/index.html))
- C++ compiler:
    - Windows: Visual Studio 2022
    - macOS: Xcode with command-line tools
    - Linux: GCC/Clang, build-essential, and libgtk-3-dev packages
- CMake 3.19+

**What you'll build:**

Starting from cefsimple, you'll progressively add features to create a custom browser with JavaScript integration and popup window customization.

**Using the C API?** This tutorial uses the C++ API. For pure C implementation, see [cefsimple_capi](https://github.com/chromiumembedded/cef/blob/master/tests/cefsimple_capi/) and [Using The CAPI](using_the_capi.md).

## CEF Quick Concepts

Before diving into code, here are the key CEF concepts you'll encounter:

**Multi-process architecture:** CEF runs multiple processes for security and stability:

- **Browser process** - Main process, manages UI and coordinates other processes
- **Render processes** - Isolated processes for web content (one per site)
- **GPU process** - Hardware-accelerated graphics
- **Other processes** - Network service, audio service, etc.

**Reference counting:** CEF objects use reference counting for memory management:

- `CefRefPtr<T>` - Smart pointer that automatically manages reference counts
- `IMPLEMENT_REFCOUNTING` macro - Adds thread-safe reference counting to your classes
- Objects are deleted when their reference count reaches zero

**Threading:** CEF callbacks run on specific threads:

- **UI thread** - Browser process main thread (most callbacks run here)
- **IO thread** - Network operations
- **Renderer thread** - JavaScript execution (in render process)
- Use `CEF_REQUIRE_UI_THREAD()`, `CEF_REQUIRE_RENDERER_THREAD()`, etc. to assert correct thread usage

**Browser styles:**

- **Chrome style** - Full Chrome UI with tabs, address bar, etc. (default in unmodified cefsimple)
- **Alloy style** - Lightweight mode, you control all UI (we switch to this in Step 1)
- Unmodified cefsimple supports both styles: use `--use-alloy-style` command-line flag for Alloy
- Set via `GetBrowserRuntimeStyle()` and `GetWindowRuntimeStyle()`

**Views framework:** Cross-platform UI system:

- `CefWindow` - Top-level window
- `CefBrowserView` - Embeds the browser in a view
- Delegates (`CefWindowDelegate`, `CefBrowserViewDelegate`) - Control window/browser behavior
- Alternative: Platform-native UI or off-screen rendering (OSR)

**For deeper coverage** of CEF architecture and concepts, see [Tutorial.md](tutorial.md).

# Step 0: Setup and Verify

## Download and Build cefsimple

1. **Download CEF binary distribution:**

    Visit [https://cef-builds.spotifycdn.com/index.html](https://cef-builds.spotifycdn.com/index.html) and download the appropriate binary distribution for your platform (e.g., `cef_binary_<version>_windows64.tar.bz2`).

2. **Extract and enter the directory:**

    ```bash
    tar -xjf cef_binary_<version>_<platform>.tar.bz2
    cd cef_binary_<version>_<platform>
```

3. **Build cefsimple as-is:**

    **Windows:**
    ```bash
    # Build with sandbox disabled for simplicity
    cmake -B build -G "Visual Studio 17 2022" -A x64 -DUSE_SANDBOX=OFF
    cmake --build build --config Release --target cefsimple
    build\tests\cefsimple\Release\cefsimple.exe
```

    **Note:** This tutorial builds with sandbox disabled (`-DUSE_SANDBOX=OFF`) for simplicity. For production applications, you should enable the sandbox using the [bootstrap.exe approach](sandbox_setup.md#windows). The sandbox provides important security isolation for renderer processes.

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

    **Using an IDE (optional):**

    Instead of building from the command line, you can open the generated project in your IDE:

    - **Windows**: Open `build/cef.sln` in Visual Studio 2022
    - **macOS**: Open `build/cef.xcodeproj` in Xcode
    - **Linux**: Open the `build` directory in CLion or another CMake-supporting IDE

4. **Verify it works:**

    - You should see a window displaying google.com
    - Window title should update with the page title
    - Closing the window should exit the app cleanly

**If this works, your environment is set up correctly!** Now we can start learning CEF by modifying the code.

## Understanding cefsimple Structure

```text
tests/cefsimple/
├── simple_app.h                # Application handler (process-level)
├── simple_app.cc               # Browser creation logic
├── simple_handler.h            # Browser handler interface
├── simple_handler.cc           # Browser-level event handling
├── simple_handler_win.cc       # Windows-specific (window title)
├── simple_handler_linux.cc     # Linux-specific (window title)
├── simple_handler_mac.mm       # macOS-specific (window title)
├── cefsimple_win.cc            # Windows entry point
├── cefsimple_linux.cc          # Linux entry point
├── cefsimple_mac.mm            # macOS browser process entry point
└── process_helper_mac.cc       # macOS helper process entry point
```

**Cross-platform code:** `simple_app.*`, `simple_handler.h/cc` work identically on all platforms.

**Platform-specific code:** Entry points and window title updates for platform-specific implementations.

# Step 1: Simplify to Absolute Minimum

**Goal:** Strip cefsimple down to the bare minimum (~150 lines total) to understand the essential parts.

## What We're Removing

cefsimple has features we don't need yet:

- ❌ Command-line URL argument handling
- ❌ Window title updates
- ❌ Error page handling
- ❌ Multiple browser tracking

Let's remove these to see the core clearly.

## Architecture Overview

Before we dive into the code, here's what each piece does:

**SimpleApp** - Application-level handler

- Creates the browser and window when CEF is initialized
- Uses the Views framework (cross-platform UI)

**SimpleHandler** - Browser-level event handler

- Handles browser lifecycle events (created, closed)
- Implements singleton pattern (global instance accessed by platform entry points)
- Quits the app when the browser closes

**Views Framework Delegates** (in simple_app.cc)

- `SimpleWindowDelegate` - Controls window size, close behavior, runtime style
- `SimpleBrowserViewDelegate` - Specifies Alloy browser style

**Platform Entry Point Methods**

- `GetInstance()` - Returns the singleton instance (fully functional)
- `CloseAllBrowsers()`, `IsClosing()` - Empty stubs for now (implemented in Step 7 for multi-browser support)
- `ShowMainWindow()` - Called on macOS when user clicks dock icon. Empty stub in this tutorial (a proper implementation would show/activate the main window)

Now let's see the actual code:

## Modified Files

### tests/cefsimple/simple_handler.h

This is the browser-level event handler. Note the singleton pattern (`GetInstance()`) and stub methods for platform entry points.

**Replace the entire file with:**

```cpp
#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_

#include "include/cef_client.h"

// Simple browser event handler
// Inherits from CefClient (main handler interface) and CefLifeSpanHandler
// (lifecycle events). This allows one class to handle multiple callback types.
class SimpleHandler : public CefClient, public CefLifeSpanHandler {
 public:
  SimpleHandler();
  ~SimpleHandler() override;

  // Provide access to the single global instance of this object.
  // Used by platform entry points (cefsimple_mac.mm, etc.)
  static SimpleHandler* GetInstance();

  // CefClient methods:
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }

  // CefLifeSpanHandler methods:
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // Request that all existing browser windows close.
  // Used by platform entry points (cefsimple_mac.mm, etc.)
  void CloseAllBrowsers(bool force_close);

  // Always returns false - stub for platform entry points
  bool IsClosing() const { return false; }

  void ShowMainWindow();

 private:
  // Provides thread-safe reference counting (CefRefPtr).
  IMPLEMENT_REFCOUNTING(SimpleHandler);
  // Prevents copying (CEF objects shouldn't be copied).
  DISALLOW_COPY_AND_ASSIGN(SimpleHandler);
};

#endif  // CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
```

### tests/cefsimple/simple_handler.cc

Implementation of SimpleHandler. The key method is `OnBeforeClose()` which quits the app when the browser closes.

**Replace the entire file with:**

```cpp
#include "tests/cefsimple/simple_handler.h"

#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"

namespace {
// Anonymous namespace - variables here are only visible in this file
SimpleHandler* g_instance = nullptr;

}  // namespace

SimpleHandler::SimpleHandler() {
  DCHECK(!g_instance);  // Debug assertion - crashes if instance already exists
  g_instance = this;
}

SimpleHandler::~SimpleHandler() {
  g_instance = nullptr;
}

// static
SimpleHandler* SimpleHandler::GetInstance() {
  return g_instance;
}

void SimpleHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Browser closed - quit the application
  // This simplified version only supports a single browser
  CefQuitMessageLoop();
}

void SimpleHandler::CloseAllBrowsers(bool force_close) {
  // Empty stub - required by platform entry points but not used
  // in this simplified Views-based version
}

void SimpleHandler::ShowMainWindow() {
  // Empty stub. A proper implementation would show/activate the main window.
  // Called on macOS when user clicks the dock icon while app is running.
}
```

### tests/cefsimple/simple_app.h

Application-level handler that receives callbacks when CEF is initialized. Declares `OnContextInitialized()` where we create the browser.

**Replace the entire file with:**

```cpp
#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_APP_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_APP_H_

#include "include/cef_app.h"

// Implement application-level callbacks for the browser process.
class SimpleApp : public CefApp, public CefBrowserProcessHandler {
 public:
  SimpleApp();

  // CefApp methods:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  // CefBrowserProcessHandler methods:
  void OnContextInitialized() override;

 private:
  IMPLEMENT_REFCOUNTING(SimpleApp);
  DISALLOW_COPY_AND_ASSIGN(SimpleApp);
};

#endif  // CEF_TESTS_CEFSIMPLE_SIMPLE_APP_H_
```

### tests/cefsimple/simple_app.cc

Implementation of SimpleApp. Creates the browser and window using Views framework. Contains inline delegate classes that control window and browser behavior.

**Replace the entire file with:**

```cpp
#include "tests/cefsimple/simple_app.h"

#include "tests/cefsimple/simple_handler.h"

#include "include/cef_browser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

namespace {

// Window delegate - controls window behavior
class SimpleWindowDelegate : public CefWindowDelegate {
 public:
  explicit SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
      : browser_view_(browser_view) {}

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    // Add the browser view and show the window
    window->AddChildView(browser_view_);
    window->Show();
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    // Allow the window to close if the browser says it's OK
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
      return browser->GetHost()->TryCloseBrowser();
    }
    return true;
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(800, 600);
  }

  cef_runtime_style_t GetWindowRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;

  IMPLEMENT_REFCOUNTING(SimpleWindowDelegate);
  DISALLOW_COPY_AND_ASSIGN(SimpleWindowDelegate);
};

// Browser view delegate
class SimpleBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  SimpleBrowserViewDelegate() = default;

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

 private:
  IMPLEMENT_REFCOUNTING(SimpleBrowserViewDelegate);
  DISALLOW_COPY_AND_ASSIGN(SimpleBrowserViewDelegate);
};

}  // namespace

SimpleApp::SimpleApp() = default;

void SimpleApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  CefRefPtr<SimpleHandler> handler(new SimpleHandler());
  CefBrowserSettings browser_settings;

  // Create the browser view (Views framework - cross-platform)
  CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
      handler, "https://www.google.com", browser_settings, nullptr, nullptr,
      new SimpleBrowserViewDelegate());

  // Create the window (Views framework - cross-platform)
  CefWindow::CreateTopLevelWindow(new SimpleWindowDelegate(browser_view));
}
```

### Delete Platform-Specific Handler Files

Since we removed window title updates, delete these files:

- `tests/cefsimple/simple_handler_win.cc`
- `tests/cefsimple/simple_handler_linux.cc`
- `tests/cefsimple/simple_handler_mac.mm`

### Update CMakeLists.txt

`CMakeLists.txt` tells CMake which source files to compile. Since we deleted the platform-specific handler files, we need to remove them from the build configuration.

Update `tests/cefsimple/CMakeLists.txt` to remove references to the deleted files:

**Find these lines:**

```cmake
set(CEFSIMPLE_SRCS_LINUX
  cefsimple_linux.cc
  simple_handler_linux.cc
  )
set(CEFSIMPLE_SRCS_MAC
  cefsimple_mac.mm
  simple_handler_mac.mm
  )
set(CEFSIMPLE_SRCS_WINDOWS
  cefsimple_win.cc
  resource.h
  simple_handler_win.cc
  )
```

**Replace with:**

```cmake
set(CEFSIMPLE_SRCS_LINUX
  cefsimple_linux.cc
  )
set(CEFSIMPLE_SRCS_MAC
  cefsimple_mac.mm
  )
set(CEFSIMPLE_SRCS_WINDOWS
  cefsimple_win.cc
  resource.h
  )
```

This removes the references to `simple_handler_linux.cc`, `simple_handler_mac.mm`, and `simple_handler_win.cc` that we deleted.

### Keep Unchanged

**Don't modify these files** - they contain platform-specific entry point code that's already correct:

- [cefsimple_win.cc](https://github.com/chromiumembedded/cef/blob/master/tests/cefsimple/cefsimple_win.cc) - Windows entry point (modified in Step 6)
- [cefsimple_linux.cc](https://github.com/chromiumembedded/cef/blob/master/tests/cefsimple/cefsimple_linux.cc) - Linux entry point (modified in Step 6)
- [cefsimple_mac.mm](https://github.com/chromiumembedded/cef/blob/master/tests/cefsimple/cefsimple_mac.mm) - macOS browser process entry point
- [process_helper_mac.cc](https://github.com/chromiumembedded/cef/blob/master/tests/cefsimple/process_helper_mac.cc) - macOS helper process entry point (modified in Step 6)

## Limitations of This Simplified Version

This simplified code has the following limitations:

- **Alloy style only**: Hardcoded to `CEF_RUNTIME_STYLE_ALLOY` in both `SimpleWindowDelegate` and `SimpleBrowserViewDelegate`. The original cefsimple supports both Chrome and Alloy styles via command-line flag.
- **Single browser only**: The app exits immediately when the browser window closes. Multi-browser support requires tracking browser instances (added in Step 7).
- **No window title updates**: We removed the handlers that update window titles based on page titles (added back in Step 4).
- **No command-line URL support**: The URL is hardcoded to "https://www.google.com" (added in Step 3).
- **Quit menu doesn't work**: On macOS, the Quit option in the application menu doesn't work (the stub `CloseAllBrowsers()` implementation does nothing). You must close the window to quit the app. (Fixed in Step 7)
- **Dock icon doesn't restore window**: On macOS, clicking the dock icon when the app is running doesn't restore the window (the stub `ShowMainWindow()` implementation does nothing). This remains a stub throughout the tutorial.
- **Empty stub methods**: `CloseAllBrowsers()` and `IsClosing()` are empty stubs (implemented in Step 7 for multi-browser support). `ShowMainWindow()` remains a stub throughout (a proper implementation would show/activate the main window on macOS).

These limitations are intentional to keep the code minimal for learning. We'll add features back in subsequent steps.

## Build and Run

```bash
# Rebuild using the same commands as Step 0

# Windows
cmake --build build --config Release --target cefsimple

# Linux
cmake --build build --target cefsimple

# macOS
cmake --build build --config Release --target cefsimple
```

**Run:** See Step 0 for platform-specific run instructions.

You should see a window open with google.com. **Note:** The window title won't update with the page title anymore (we removed that feature to simplify), but otherwise the app works the same as the original. Now you understand every line of code!

## What's Happening

**Three key pieces:**

1. **SimpleApp** (application-level handler)
    - Inherits from `CefApp` and `CefBrowserProcessHandler`
    - `OnContextInitialized()` - Called when CEF is ready to create browsers
    - Creates the browser and window using Views framework

2. **SimpleHandler** (browser-level event handler)
    - Inherits from `CefClient` and `CefLifeSpanHandler`
    - `GetLifeSpanHandler()` - Returns this handler for lifecycle events
    - `OnBeforeClose()` - Called when browser is closing, quits the app

3. **Views Framework Delegates**
    - `SimpleWindowDelegate` - Controls window size, close behavior, runtime style
    - `SimpleBrowserViewDelegate` - Specifies Alloy browser style
    - Both are **cross-platform** - same code works on Windows, Linux, macOS

**Why Alloy style?** Simpler than Chrome style - you control the UI completely. We explicitly set `CEF_RUNTIME_STYLE_ALLOY` in both delegates.

**Entry points:** Platform-specific files (`cefsimple_*.cc/mm`) handle:

- CEF initialization (`CefInitialize`)
- Message loop (`CefRunMessageLoop`)
- Shutdown (`CefShutdown`)
- macOS-specific: Framework loading, NSApplication setup

**Program Flow:** Here's the complete execution sequence:

1. **main()** (in entry point file) - Program starts
2. **CefInitialize()** - Initializes CEF with SimpleApp instance
3. **SimpleApp::OnContextInitialized()** - CEF ready, creates browser:
    - Creates SimpleHandler instance
    - Creates CefBrowserView with URL
    - Creates CefWindow with SimpleWindowDelegate
4. **CefRunMessageLoop()** - Enters message loop, processes events
5. **User closes window** → SimpleWindowDelegate::CanClose() → TryCloseBrowser()
6. **SimpleHandler::OnBeforeClose()** - Called when browser is closing
    - Calls CefQuitMessageLoop() to exit the message loop
7. **CefShutdown()** - Cleanup and shutdown CEF
8. **main() exits** - Program terminates

## Common Issues

- **Build errors about missing files** - Make sure you:
    1. Deleted the three platform handler files (`simple_handler_*.cc/mm`)
    2. Updated `CMakeLists.txt` to remove references to those files (see instructions above)

# Step 2: Custom URL from Code

**Goal:** Load different URLs by modifying the code (not command-line yet).

## What We're Adding

Just change one line to demonstrate how browser creation works.

## Modified Files

### tests/cefsimple/simple_app.cc

**In `OnContextInitialized()`, replace the URL:**

From: `"https://www.google.com"`
To: `"https://www.wikipedia.org"`

## Build and Run

```bash
# Rebuild using the same commands as Step 0

# Windows
cmake --build build --config Release --target cefsimple

# Linux
cmake --build build --target cefsimple

# macOS
cmake --build build --config Release --target cefsimple
```

**Run:** See Step 0 for platform-specific run instructions.

Now it loads Wikipedia instead! This shows that the URL is set when creating the `CefBrowserView`.

**Try these:**

- `file:///path/to/local.html` - Local files (use absolute path)
- `data:text/html,<h1>Hello CEF!</h1>` - Inline HTML
- Any HTTPS URL

## What's Happening

The second parameter to `CefBrowserView::CreateBrowserView` is the initial URL. The browser navigates to this URL immediately after creation.

# Step 3: Command-Line URL Support

**Goal:** Pass custom URLs via command-line: `cefsimple --url=https://example.com`

## What We're Adding

Read the command-line and use it to set the URL.

## Modified Files

### tests/cefsimple/simple_app.cc

**Add at the top:**

```cpp
#include "include/cef_command_line.h"
```

**Replace `OnContextInitialized` with:**

```cpp
void SimpleApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  // Get command-line arguments
  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  // Check for --url= parameter
  std::string url = command_line->GetSwitchValue("url");
  if (url.empty()) {
    url = "https://www.google.com";  // Default URL
  }

  CefRefPtr<SimpleHandler> handler(new SimpleHandler());
  CefBrowserSettings browser_settings;

  CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
      handler, url, browser_settings, nullptr, nullptr,
      new SimpleBrowserViewDelegate());

  CefWindow::CreateTopLevelWindow(new SimpleWindowDelegate(browser_view));
}
```

## Build and Run

```bash
# Rebuild using the same commands as Step 0

# Windows
cmake --build build --config Release --target cefsimple

# Linux
cmake --build build --target cefsimple

# macOS
cmake --build build --config Release --target cefsimple
```

**Run with custom URL:**

```bash
# Windows
build\tests\cefsimple\Release\cefsimple.exe --url=https://github.com

# Linux
build/tests/cefsimple/cefsimple --url=https://github.com

# macOS
open build/tests/cefsimple/Release/cefsimple.app --args --url=https://github.com
```

## What's Happening

**New concepts:**

- `CefCommandLine::GetGlobalCommandLine()` - Access command-line arguments parsed by CEF
- `GetSwitchValue("url")` - Read the value of `--url=` parameter
- CEF automatically parses command-line in the entry point before calling `OnContextInitialized`

**Try it:**

```bash
# Load local file
cefsimple --url=file:///home/user/test.html

# Load data URL (note the quotes to escape shell characters)
cefsimple --url="data:text/html,<h1>Hello!</h1>"
```

# Step 4: Window Title Updates

**Goal:** Update window title to match the page title as it loads.

## What We're Adding

- Display handler to receive title change events
- Get window from browser to update its title

## Modified Files

### tests/cefsimple/simple_handler.h

**Update to add display handler:**

```cpp
#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_

#include "include/cef_client.h"

class SimpleHandler : public CefClient,
                      // NEW: Add display handler for title change events
                      public CefDisplayHandler,
                      public CefLifeSpanHandler {
 public:
  SimpleHandler();
  ~SimpleHandler() override;

  static SimpleHandler* GetInstance();

  // CefClient methods:
  // NEW: Return display handler
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }

  // CefDisplayHandler methods:
  // NEW: Handle page title changes
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

  // CefLifeSpanHandler methods:
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  void CloseAllBrowsers(bool force_close);
  bool IsClosing() const { return false; }
  void ShowMainWindow();

 private:
  IMPLEMENT_REFCOUNTING(SimpleHandler);
  DISALLOW_COPY_AND_ASSIGN(SimpleHandler);
};

#endif  // CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
```

### tests/cefsimple/simple_handler.cc

**Add new includes at the top:**

```cpp
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
```

**Add the OnTitleChange implementation** (after the existing methods, before `OnBeforeClose()`):

```cpp
void SimpleHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                   const CefString& title) {
  CEF_REQUIRE_UI_THREAD();

  // Get the browser view from the browser
  CefRefPtr<CefBrowserView> browser_view =
      CefBrowserView::GetForBrowser(browser);
  if (browser_view) {
    // Get the window and update its title
    CefRefPtr<CefWindow> window = browser_view->GetWindow();
    if (window) {
      window->SetTitle(title);
    }
  }
}
```

**Note:** `OnBeforeClose()` and other methods from Step 1 remain unchanged.

## Build and Run

```bash
# Rebuild using the same commands as Step 0

# Windows
cmake --build build --config Release --target cefsimple

# Linux
cmake --build build --target cefsimple

# macOS
cmake --build build --config Release --target cefsimple
```

**Run:** See Step 0 for platform-specific run instructions.

Now the window title updates to match the page title as you navigate!

## What's Happening

**New concepts:**

- `CefDisplayHandler::OnTitleChange()` - Called when the page title changes
- `CefBrowserView::GetForBrowser()` - Get the browser view from a browser instance
- `CefBrowserView::GetWindow()` - Get the window containing the browser view
- `CefWindow::SetTitle()` - Update the window title

**Flow:**

1. Page loads or navigates → `OnTitleChange()` called with new title
2. We get the browser view from the browser parameter
3. We get the window from the browser view and update its title

# Step 5: JavaScript Execution

**Goal:** Execute JavaScript code in the page from C++.

## What We're Adding

- Load handler to know when page is ready
- Execute JavaScript after page loads

## Modified Files

### tests/cefsimple/simple_handler.h

**Add load handler:**

```cpp
#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_

#include "include/cef_client.h"

class SimpleHandler : public CefClient,
                      public CefDisplayHandler,
                      public CefLifeSpanHandler,
                      // NEW: Add load handler for page load events
                      public CefLoadHandler {
 public:
  SimpleHandler();
  ~SimpleHandler() override;

  static SimpleHandler* GetInstance();

  // CefClient methods:
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  // NEW: Return load handler
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  // CefDisplayHandler methods:
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

  // CefLifeSpanHandler methods:
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefLoadHandler methods:
  // NEW: Handle page load completion
  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override;

  void CloseAllBrowsers(bool force_close);
  bool IsClosing() const { return false; }
  void ShowMainWindow();

 private:
  IMPLEMENT_REFCOUNTING(SimpleHandler);
  DISALLOW_COPY_AND_ASSIGN(SimpleHandler);
};

#endif  // CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
```

### tests/cefsimple/simple_handler.cc

**Add the OnLoadEnd implementation** (after `OnTitleChange()`, before `OnBeforeClose()`):

```cpp
void SimpleHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               int httpStatusCode) {
  CEF_REQUIRE_UI_THREAD();

  // Only execute on the main frame
  if (!frame->IsMain()) {
    return;
  }

  // Execute JavaScript to change the page background color
  frame->ExecuteJavaScript(
      "document.body.style.backgroundColor = 'lightblue';",
      frame->GetURL(), 0);
}
```

## Build and Run

```bash
# Rebuild using the same commands as Step 0

# Windows
cmake --build build --config Release --target cefsimple

# Linux
cmake --build build --target cefsimple

# macOS
cmake --build build --config Release --target cefsimple
```

**Run:** See Step 0 for platform-specific run instructions.

Now when pages load, the background turns light blue!

## What's Happening

**New concepts:**

- `CefLoadHandler::OnLoadEnd()` - Called when a frame finishes loading
- `CefFrame::IsMain()` - Check if this is the main frame (not an iframe)
- `CefFrame::ExecuteJavaScript()` - Execute arbitrary JavaScript code
- Parameters: JavaScript code, source URL (for debugging), start line number

**Try modifying the JavaScript:**

```cpp
// Alert box
frame->ExecuteJavaScript("alert('Hello from C++!');", frame->GetURL(), 0);

// DOM manipulation
frame->ExecuteJavaScript(
    "var h1 = document.createElement('h1');"
    "h1.textContent = 'Added by CEF!';"
    "document.body.insertBefore(h1, document.body.firstChild);",
    frame->GetURL(), 0);

// Console log
frame->ExecuteJavaScript("console.log('CEF executed this');",
                         frame->GetURL(), 0);
```

**Important notes:**

- JavaScript executes asynchronously
- `ExecuteJavaScript()` doesn't return the result directly
- To get results, use `EvaluateJavaScript()` (not shown) or process messages (next step)

# Step 6: JavaScript-to-C++ Communication

**Goal:** Call C++ functions from JavaScript code in the page.

## What We're Adding

- Process message handler to receive messages from the renderer process
- V8 context handler to register JavaScript functions
- Bidirectional communication between C++ and JavaScript

This requires implementing both browser process and renderer process handlers.

## Modified Files

### tests/cefsimple/simple_app.h

**Update to add renderer process handler:**

```cpp
#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_APP_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_APP_H_

#include "include/cef_app.h"

class SimpleApp : public CefApp,
                  public CefBrowserProcessHandler,
                  // NEW: Add renderer process handler for JavaScript integration
                  public CefRenderProcessHandler {
 public:
  SimpleApp();

  // CefApp methods:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }
  // NEW: Return renderer process handler
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
    return this;
  }

  // CefBrowserProcessHandler methods:
  void OnContextInitialized() override;

  // NEW: CefRenderProcessHandler methods for JavaScript integration
  // NEW: Called when the JavaScript context is created in the renderer process
  void OnContextCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override;

 private:
  IMPLEMENT_REFCOUNTING(SimpleApp);
  DISALLOW_COPY_AND_ASSIGN(SimpleApp);
};

#endif  // CEF_TESTS_CEFSIMPLE_SIMPLE_APP_H_
```

### tests/cefsimple/simple_app.cc

**Add new includes at the top** (after the existing includes):

```cpp
#include "include/cef_v8.h"
```

**Add V8 handler class in the anonymous namespace** (add after the includes, before the existing SimpleWindowDelegate and SimpleBrowserViewDelegate classes which remain unchanged):

```cpp
// V8 handler to handle JavaScript function calls in the renderer process
class MyV8Handler : public CefV8Handler {
 public:
  MyV8Handler() = default;

  bool Execute(const CefString& name,
               CefRefPtr<CefV8Value> object,
               const CefV8ValueList& arguments,
               CefRefPtr<CefV8Value>& retval,
               CefString& exception) override {
    if (name == "sayHello") {
      // Get the argument
      if (arguments.size() == 1 && arguments[0]->IsString()) {
        CefString message = arguments[0]->GetStringValue();

        // Send message to browser process
        CefRefPtr<CefProcessMessage> msg =
            CefProcessMessage::Create("greeting");
        msg->GetArgumentList()->SetString(0, message);

        CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
        CefRefPtr<CefFrame> frame = context->GetFrame();

        // Results in an asynchronous call to
        // SimpleHandler::OnProcessMessageReceived in the browser process
        frame->SendProcessMessage(PID_BROWSER, msg);

        // Return value to JavaScript
        retval = CefV8Value::CreateString("Message sent to C++!");
        return true;
      }
    }

    return false;
  }

 private:
  IMPLEMENT_REFCOUNTING(MyV8Handler);
  DISALLOW_COPY_AND_ASSIGN(MyV8Handler);
};
```

**Add the OnContextCreated implementation** (after OnContextInitialized, before the end of the file):

```cpp
// Called when the JavaScript context is created in the renderer process
void SimpleApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefV8Context> context) {
  // Debug assertion - crashes if called on wrong thread
  // V8 APIs must only be called from the renderer thread
  CEF_REQUIRE_RENDERER_THREAD();

  // Register JavaScript function 'sayHello' on the global object (window.sayHello)
  CefRefPtr<CefV8Value> object = context->GetGlobal();
  CefRefPtr<CefV8Handler> handler = new MyV8Handler();

  CefRefPtr<CefV8Value> func =
      CefV8Value::CreateFunction("sayHello", handler);
  object->SetValue("sayHello", func, V8_PROPERTY_ATTRIBUTE_NONE);
}
```

**Note:** The SimpleWindowDelegate, SimpleBrowserViewDelegate, and OnContextInitialized implementations remain unchanged from previous steps.

### Platform Entry Points

**Important:** For JavaScript-to-C++ communication to work, the renderer process needs access to `SimpleApp::OnContextCreated` to register the JavaScript function.

- **macOS:** Uses separate helper executables for sub-processes (renderer, GPU, etc.) - update `process_helper_mac.cc`
- **Windows and Linux:** Reuse the main executable for sub-processes - update `cefsimple_win.cc` and `cefsimple_linux.cc`

#### tests/cefsimple/cefsimple_linux.cc

**Update the main function to create SimpleApp before CefExecuteProcess:**

Find:
```cpp
  // CEF applications have multiple sub-processes (render, GPU, etc) that share
  // the same executable. This function checks the command-line and, if this is
  // a sub-process, executes the appropriate logic.
  int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    return exit_code;
  }
```

Replace with:
```cpp
  // SimpleApp implements application-level callbacks. It will create the first
  // browser instance in OnContextInitialized() after CEF has initialized.
  CefRefPtr<SimpleApp> app(new SimpleApp);

  // CEF applications have multiple sub-processes (render, GPU, etc) that share
  // the same executable. This function checks the command-line and, if this is
  // a sub-process, executes the appropriate logic.
  int exit_code = CefExecuteProcess(main_args, app, nullptr);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    return exit_code;
  }
```

**Then find the later SimpleApp creation:**

```cpp
  // SimpleApp implements application-level callbacks for the browser process.
  // It will create the first browser instance in OnContextInitialized() after
  // CEF has initialized.
  CefRefPtr<SimpleApp> app(new SimpleApp);
```

**Delete these lines** (since we now create app earlier).

#### tests/cefsimple/cefsimple_win.cc

**Update the RunMain function to create SimpleApp before CefExecuteProcess:**

Find:
```cpp
  // CEF applications have multiple sub-processes (render, GPU, etc) that share
  // the same executable. This function checks the command-line and, if this is
  // a sub-process, executes the appropriate logic.
  exit_code = CefExecuteProcess(main_args, nullptr, sandbox_info);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    return exit_code;
  }
```

Replace with:
```cpp
  // SimpleApp implements application-level callbacks. It will create the first
  // browser instance in OnContextInitialized() after CEF has initialized.
  CefRefPtr<SimpleApp> app(new SimpleApp);

  // CEF applications have multiple sub-processes (render, GPU, etc) that share
  // the same executable. This function checks the command-line and, if this is
  // a sub-process, executes the appropriate logic.
  exit_code = CefExecuteProcess(main_args, app, sandbox_info);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    return exit_code;
  }
```

**Then find the later SimpleApp creation:**

```cpp
  // SimpleApp implements application-level callbacks for the browser process.
  // It will create the first browser instance in OnContextInitialized() after
  // CEF has initialized.
  CefRefPtr<SimpleApp> app(new SimpleApp);
```

**Delete these lines** (since we now create app earlier).

#### tests/cefsimple/process_helper_mac.cc

**Add include at the top:**

```cpp
#include "tests/cefsimple/simple_app.h"
```

**Update the main function to create SimpleApp before CefExecuteProcess:**

Find:
```cpp
return CefExecuteProcess(main_args, nullptr, nullptr);
```

Replace with:
```cpp
// SimpleApp implements application-level callbacks for the sub-process.
// This allows SimpleApp::OnContextCreated to be called in the renderer process.
CefRefPtr<SimpleApp> app(new SimpleApp);

// Execute the sub-process.
return CefExecuteProcess(main_args, app, nullptr);
```

**Note:** Unlike the main executable, the macOS helper doesn't have a separate browser process initialization section, so there's no duplicate SimpleApp creation to remove.

#### tests/cefsimple/CMakeLists.txt (macOS only)

**Update the helper sources to include simple_app:**

Find:
```cmake
# cefsimple helper sources.
set(CEFSIMPLE_HELPER_SRCS_MAC
  process_helper_mac.cc
  )
```

Replace with:
```cmake
# cefsimple helper sources.
set(CEFSIMPLE_HELPER_SRCS_MAC
  process_helper_mac.cc
  simple_app.cc
  simple_app.h
  simple_handler.cc
  simple_handler.h
  )
```

This ensures the helper process can compile and link the SimpleApp code.

**Note:** We're including simple_handler.cc/h in the helper sources even though SimpleHandler is never used in renderer processes (only SimpleApp::OnContextCreated is called). For a production application, you could create a separate CefApp-derived class for the renderer process (e.g., `SimpleRendererApp`) to avoid linking browser-process-only dependencies like SimpleHandler into the helper executables.

**Reconfigure CMake (macOS only)** to pick up the CMakeLists.txt changes:

```bash
cmake -B build -G Xcode
```

### tests/cefsimple/simple_handler.h

**Add process message handler:**

```cpp
#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_

#include "include/cef_client.h"

class SimpleHandler : public CefClient,
                      public CefDisplayHandler,
                      public CefLifeSpanHandler,
                      public CefLoadHandler {
 public:
  SimpleHandler();
  ~SimpleHandler() override;

  static SimpleHandler* GetInstance();

  // CefClient methods:
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  // NEW: Handle messages from renderer process
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefProcessId source_process,
                                 CefRefPtr<CefProcessMessage> message) override;

  // CefDisplayHandler methods:
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

  // CefLifeSpanHandler methods:
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefLoadHandler methods:
  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override;

  void CloseAllBrowsers(bool force_close);
  bool IsClosing() const { return false; }
  void ShowMainWindow();

 private:
  IMPLEMENT_REFCOUNTING(SimpleHandler);
  DISALLOW_COPY_AND_ASSIGN(SimpleHandler);
};

#endif  // CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
```

### tests/cefsimple/simple_handler.cc

**Add at the top** (after the existing includes):

```cpp
#include <sstream>
```

**Update OnLoadEnd** to inject a test button (replace the background color change with button injection):

```cpp
// Frame load finished in the browser process
void SimpleHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               int httpStatusCode) {
  CEF_REQUIRE_UI_THREAD();

  if (!frame->IsMain()) {
    return;
  }

  // Inject a test button to call our C++ function
  std::stringstream ss;
  ss << "var button = document.createElement('button');"
     << "button.textContent = 'Call C++ Function';"
     << "button.style.position = 'fixed';"
     << "button.style.top = '10px';"
     << "button.style.left = '10px';"
     << "button.style.zIndex = '10000';"
     << "button.style.backgroundColor = '#4CAF50';"
     << "button.style.color = 'white';"
     << "button.style.padding = '10px 15px';"
     << "button.style.border = 'none';"
     << "button.style.borderRadius = '4px';"
     << "button.style.cursor = 'pointer';"
     << "button.onclick = function() {"
     << "  var result = sayHello('Hello from JavaScript!');"
     << "  alert(result);"
     << "};"
     << "document.body.appendChild(button);";

  frame->ExecuteJavaScript(ss.str(), frame->GetURL(), 0);
}
```

**Add OnProcessMessageReceived** (after OnLoadEnd, before OnBeforeClose):

```cpp
// Process message received in the browser process
bool SimpleHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_UI_THREAD();

  // Handle message from MyHandler::Execute() in the renderer process
  if (message->GetName() == "greeting") {
    CefString msg = message->GetArgumentList()->GetString(0);

    // Show the message in a JavaScript alert
    std::stringstream ss;
    ss << "alert('C++ received: " << msg.ToString() << "');";
    frame->ExecuteJavaScript(ss.str(), frame->GetURL(), 0);

    return true;
  }

  return false;
}
```

**Note:** OnTitleChange and OnBeforeClose remain unchanged from previous steps.

## Build and Run

```bash
# Rebuild using the same commands as Step 0

# Windows
cmake --build build --config Release --target cefsimple

# Linux
cmake --build build --target cefsimple

# macOS
cmake --build build --config Release --target cefsimple
```

**Run:** See Step 0 for platform-specific run instructions.

Now you'll see a button in the top-left corner of every page. Click it to:

1. Call JavaScript `sayHello()` function
2. JavaScript sends message to C++ via process message
3. C++ receives message in browser process
4. C++ sends alert back to JavaScript

## What's Happening

**New concepts:**

1. **Multi-process architecture:**
    - Browser process (UI, main logic)
    - Renderer process (usually one per origin, runs web content)
    - Processes communicate via messages

2. **Renderer process (SimpleApp):**
    - `OnContextCreated()` - Called when JavaScript context is created
    - `CefV8Value::CreateFunction()` - Register a JavaScript function
    - `CefV8Handler::Execute()` - Handle JavaScript function calls

3. **Process messages:**
    - `CefProcessMessage::Create()` - Create a message
    - `CefFrame::SendProcessMessage()` - Send from renderer to browser
    - `CefClient::OnProcessMessageReceived()` - Receive in browser process

**Flow:**

1. Renderer process creates → `OnContextCreated()` registers `sayHello()` function
2. User clicks button → JavaScript calls `sayHello('Hello from JavaScript!')`
3. `MyV8Handler::Execute()` receives call, creates process message
4. Message sent to browser process
5. `SimpleHandler::OnProcessMessageReceived()` handles message in browser process
6. C++ executes JavaScript to show alert

**Important notes:**

- JavaScript function runs in renderer process (sandboxed)
- C++ handler runs in browser process (has full access)
- Messages are the bridge between processes
- Messages can contain simple data types (strings, ints, lists, dictionaries), binary data, and shared memory regions

**Learn more:**

- **[JavaScriptIntegration.md](javascript_integration.md)** - Complete guide to V8 JavaScript integration:
    - Window binding and extensions
    - V8 value creation and type conversion
    - Working with V8 contexts
    - JavaScript objects and arrays
    - Function handlers (CefV8Handler)

- **[GeneralUsage.md](general_usage.md#inter-process-communication-ipc)** - Inter-Process Communication section:
    - Process startup messages
    - Process runtime messages (CefProcessMessage)
    - Asynchronous JavaScript bindings
    - Generic message router for request/response patterns
    - Custom IPC implementations

# Step 7: Multiple Browser Support

**Goal:** Track multiple browser instances and only quit the application when all browsers are closed.

## What We're Adding

In Steps 1-6, the application only supported a single browser. When that browser closed, the app quit immediately. Now we'll add:

- Browser instance tracking (map using browser ID as key)
- Proper lifecycle management (add on create, remove on close)
- Only quit when the last browser closes
- Working `CloseAllBrowsers()` implementation
- A second button to create popup windows

## Why This Matters

The single-browser approach from earlier steps has limitations:

- **Quits too early**: If user creates a popup, closing the main window quits immediately (popup also closes)
- **No browser tracking**: Can't iterate over browsers to close them all
- **CloseAllBrowsers() is a stub**: Doesn't actually do anything

Multi-browser tracking is essential for real applications that support popups, new windows, or multiple browser instances.

## Modified Files

### tests/cefsimple/simple_handler.h

**Add browser tracking and lifecycle methods:**

```cpp
#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_

#include <map>

#include "include/cef_client.h"

class SimpleHandler : public CefClient,
                      public CefDisplayHandler,
                      public CefLifeSpanHandler,
                      public CefLoadHandler {
 public:
  SimpleHandler();
  ~SimpleHandler() override;

  static SimpleHandler* GetInstance();

  // CefClient methods:
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefProcessId source_process,
                                 CefRefPtr<CefProcessMessage> message) override;

  // CefDisplayHandler methods:
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

  // CefLifeSpanHandler methods:
  // NEW: Track browser creation
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefLoadHandler methods:
  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override;

  void CloseAllBrowsers(bool force_close);
  bool IsClosing() const { return is_closing_; }
  void ShowMainWindow();

 private:
  // NEW: Track all browser instances (keyed by browser ID)
  // These members are only accessed on the UI thread (OnAfterCreated,
  // OnBeforeClose, and CloseAllBrowsers all run on TID_UI), so no
  // additional synchronization is needed.
  using BrowserMap = std::map<int, CefRefPtr<CefBrowser>>;
  BrowserMap browser_map_;

  // NEW: Track whether we're closing
  bool is_closing_ = false;

  IMPLEMENT_REFCOUNTING(SimpleHandler);
  DISALLOW_COPY_AND_ASSIGN(SimpleHandler);
};

#endif  // CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
```

**Changes:**

- Added `#include <map>` for browser tracking container
- Added `OnAfterCreated()` to track browser creation
- Added `browser_map_` member to store all browser instances (keyed by browser ID)
- Added `is_closing_` flag to prevent new browsers during shutdown
- Changed `IsClosing()` to return the actual flag instead of hardcoded `false`

### tests/cefsimple/simple_handler.cc

**Add includes for callbacks and closure tasks** (after the existing includes):

```cpp
#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
```

**Add OnAfterCreated** (new method, add after OnTitleChange):

```cpp
void SimpleHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Add browser to our tracking map
  browser_map_[browser->GetIdentifier()] = browser;
}
```

**Update OnLoadEnd** to add second button (replace the existing OnLoadEnd):

```cpp
void SimpleHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               int httpStatusCode) {
  CEF_REQUIRE_UI_THREAD();

  if (!frame->IsMain()) {
    return;
  }

  // Inject test buttons
  std::stringstream ss;
  // "Call C++ Function" button
  ss << "var button1 = document.createElement('button');"
     << "button1.textContent = 'Call C++ Function';"
     << "button1.style.position = 'fixed';"
     << "button1.style.top = '10px';"
     << "button1.style.left = '10px';"
     << "button1.style.zIndex = '10000';"
     << "button1.style.backgroundColor = '#4CAF50';"
     << "button1.style.color = 'white';"
     << "button1.style.padding = '10px 15px';"
     << "button1.style.border = 'none';"
     << "button1.style.borderRadius = '4px';"
     << "button1.style.cursor = 'pointer';"
     << "button1.onclick = function() {"
     << "  var result = sayHello('Hello from JavaScript!');"
     << "  alert(result);"
     << "};"
     << "document.body.appendChild(button1);"
     // "Create Popup Window" button
     << "var button2 = document.createElement('button');"
     << "button2.textContent = 'Create Popup Window';"
     << "button2.style.position = 'fixed';"
     << "button2.style.top = '50px';"
     << "button2.style.left = '10px';"
     << "button2.style.zIndex = '10000';"
     << "button2.style.backgroundColor = '#2196F3';"
     << "button2.style.color = 'white';"
     << "button2.style.padding = '10px 15px';"
     << "button2.style.border = 'none';"
     << "button2.style.borderRadius = '4px';"
     << "button2.style.cursor = 'pointer';"
     << "button2.onclick = function() {"
     << "  window.open('https://www.wikipedia.org', '_blank', 'width=800,height=600');"
     << "};"
     << "document.body.appendChild(button2);";

  frame->ExecuteJavaScript(ss.str(), frame->GetURL(), 0);
}
```

**Update OnBeforeClose** (replace the existing OnBeforeClose):

```cpp
void SimpleHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Remove browser from our tracking map
  browser_map_.erase(browser->GetIdentifier());

  // Only quit when all browsers are closed
  if (browser_map_.empty()) {
    CefQuitMessageLoop();
  }
}
```

**Update CloseAllBrowsers** (replace the existing stub implementation):

```cpp
void SimpleHandler::CloseAllBrowsers(bool force_close) {
  // CloseAllBrowsers can be called from anywhere (e.g., macOS menu's Quit command),
  // but CEF browser operations must run on the UI thread. Check if we're already
  // on the UI thread; if not, post a task to run this method on the UI thread.
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&SimpleHandler::CloseAllBrowsers, this,
                                        force_close));
    return;
  }

  is_closing_ = true;

  if (browser_map_.empty()) {
    return;
  }

  // Iterate over browsers and request close
  // Use a copy because browser_map_ will be modified during iteration (OnBeforeClose)
  BrowserMap browser_map_copy = browser_map_;
  for (auto& pair : browser_map_copy) {
    pair.second->GetHost()->CloseBrowser(force_close);
  }
}
```

**Note:** Constructor, OnTitleChange, OnProcessMessageReceived, and ShowMainWindow remain unchanged from previous steps.

**Changes:**

- Added `OnAfterCreated()`: Adds browser to `browser_map_` when created (keyed by browser ID)
- Updated `OnLoadEnd()`: Added second button "Create Popup Window" (blue, positioned below first button)
- Updated `OnBeforeClose()`: Removes browser from map by ID, only quits when map is empty
- Implemented `CloseAllBrowsers()`: Sets `is_closing_` flag, iterates over browser_map_ copy, closes each browser
- Uses `CefPostTask()` to ensure execution on UI thread

## Build and Run

```bash
# Rebuild using the same commands as Step 0

# Windows
cmake --build build --config Release --target cefsimple

# Linux
cmake --build build --target cefsimple

# macOS
cmake --build build --config Release --target cefsimple
```

**Run:** See Step 0 for platform-specific run instructions.

## What's Happening

**New concepts:**

1. **Browser lifecycle tracking with map:**
    - Use `std::map<int, CefRefPtr<CefBrowser>>` keyed by browser ID
    - Browser ID obtained via `browser->GetIdentifier()` - unique integer for each browser
    - `OnAfterCreated()` - Called when browser is created, add to map
    - `OnBeforeClose()` - Called when browser is closing, remove from map by ID
    - Only quit when map is empty
    - Why map instead of set? `CefRefPtr` comparison doesn't work as expected for `erase()`, so we key by integer ID

2. **Thread-safe closing:**
    - `CloseAllBrowsers()` uses `CefPostTask()` to execute on UI thread
    - CEF requires most operations on the UI thread
    - `base::BindOnce()` creates a callback that captures `this` and arguments
    - Requires two includes:
        - `include/base/cef_callback.h` - provides `base::BindOnce`
        - `include/wrapper/cef_closure_task.h` - converts callbacks to `CefTask` objects for `CefPostTask`

3. **Container iteration safety:**
    - Create a copy of `browser_map_` before iterating
    - `OnBeforeClose()` modifies the map during iteration (would cause crashes)
    - Copy ensures safe iteration even as browsers close
    - Iterate over `pair.second` to get browser from map entry

**Testing:**

1. Launch the app - main window appears
2. Click "Create Popup Window" - popup opens
3. Close main window - app stays running (popup still open)
4. Close popup - now app quits (all browsers closed)
5. Try creating multiple popups and closing them in different orders

**macOS note:** The Quit menu now works! `CloseAllBrowsers()` is properly implemented, so the macOS application menu's Quit option (which calls `CloseAllBrowsers()`) will close all browsers and quit the app.

**Learn more:**

- **[GeneralUsage.md](general_usage.md#posting-tasks)** - Posting Tasks section:
    - Complete guide to CefPostTask family of functions
    - base::BindOnce and base::Callback usage
    - Thread-safe task posting patterns
    - CefTaskRunner for accessing specific thread message loops
    - Bound methods vs bound functions

# Step 8: Views Styling of Popup Browsers

**Goal:** Customize the appearance and behavior of popup browser windows using the Views framework.

## What We're Adding

In Step 7, clicking "Create Popup Window" opened a new browser, but it used default styling (same size as main window). Now we'll add custom styling for popup windows:

- `OnPopupBrowserViewCreated()` callback in SimpleBrowserViewDelegate
- Custom window creation for popups (smaller size, different from main window)
- Understanding of when to use `OnBeforePopup` vs `OnPopupBrowserViewCreated`

## Why This Matters

CEF provides two callbacks for popup customization with different purposes:

**OnBeforePopup (CefLifeSpanHandler)** - Low-level popup control:

- Called **before** the popup browser is created
- Runs in the browser process
- Can cancel popup creation (return true to cancel)
- Can change the popup's `CefWindowInfo`, `CefClient` and `CefBrowserSettings`
- Use for: Blocking popups, popup policy enforcement, native window parenting

**OnPopupBrowserViewCreated (CefBrowserViewDelegate)** - Views-specific window creation:

- Called **after** the popup browser view is created
- Only called when using the Views framework
- Gives you control over window creation and styling
- Create custom CefWindow with different size, position, title bar, etc.
- Use for: Custom popup window appearance and behavior in Views-based apps

In this step, we use `OnPopupBrowserViewCreated` to create smaller popup windows (600x400) compared to the main window (800x600).

## Modified Files

### tests/cefsimple/simple_app.cc

**Update SimpleBrowserViewDelegate** to add OnPopupBrowserViewCreated (add after GetBrowserRuntimeStyle, before the private section):

```cpp
// Browser view delegate
class SimpleBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  SimpleBrowserViewDelegate() = default;

  // NEW: Called when a popup browser view is created
  bool OnPopupBrowserViewCreated(
      CefRefPtr<CefBrowserView> browser_view,
      CefRefPtr<CefBrowserView> popup_browser_view,
      bool is_devtools) override {
    // Create a smaller window for popups (600x400 instead of 800x600)
    // We use a custom SimpleWindowDelegate that returns a different size
    CefWindow::CreateTopLevelWindow(
        new SimpleWindowDelegate(popup_browser_view));

    // Returning true indicates we created the window
    return true;
  }

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

 private:
  IMPLEMENT_REFCOUNTING(SimpleBrowserViewDelegate);
  DISALLOW_COPY_AND_ASSIGN(SimpleBrowserViewDelegate);
};
```

**Update SimpleWindowDelegate** to support custom popup sizes (replace the GetPreferredSize method):

```cpp
  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    // Check if this is a popup by seeing if we have a browser yet
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser && browser->IsPopup()) {
      // Popup windows are smaller
      return CefSize(600, 400);
    }
    // Main window is larger
    return CefSize(800, 600);
  }
```

**Note:** The SimpleWindowDelegate, MyV8Handler, and OnContextInitialized/OnContextCreated implementations remain unchanged from Step 7.

## Build and Run

```bash
# Rebuild using the same commands as Step 0

# Windows
cmake --build build --config Release --target cefsimple

# Linux
cmake --build build --target cefsimple

# macOS
cmake --build build --config Release --target cefsimple
```

**Run:** See Step 0 for platform-specific run instructions.

## What's Happening

**New concepts:**

1. **OnPopupBrowserViewCreated callback:**
    - Called by CEF after a popup browser view is created
    - Only invoked when using Views framework (not for native windows)
    - Parameters:
        - `browser_view` - The parent browser view that initiated the popup
        - `popup_browser_view` - The newly created popup browser view
        - `is_devtools` - True if this is a DevTools popup
    - Return value:
        - `true` - You created the window (CEF won't create a default window)
        - `false` - CEF will create a default window

2. **Custom popup window creation:**
    - We create a new `CefWindow` for the popup using `CefWindow::CreateTopLevelWindow()`
    - Pass the `popup_browser_view` to `SimpleWindowDelegate` (same pattern as main window)
    - The delegate's `GetPreferredSize()` returns different sizes for popups vs main windows
    - `browser->IsPopup()` identifies popup browsers

3. **Popup detection:**
    - `CefBrowser::IsPopup()` returns true for browsers created via `window.open()` or similar
    - Main browser (created in `OnContextInitialized`) returns false
    - We use this to return different window sizes

4. **Alternative approach - GetDelegateForPopupBrowserView:**
    - Instead of using the same delegate for all browser views, you can override `CefBrowserViewDelegate::GetDelegateForPopupBrowserView()` to return a different delegate for popups
    - This allows you to completely separate popup behavior from main window behavior
    - Example: Create a `PopupBrowserViewDelegate` class with different implementations of `OnPopupBrowserViewCreated()`, etc.
    - The approach in this tutorial (same delegate, detect popups with `IsPopup()`) is simpler and works well for most cases
    - Use `GetDelegateForPopupBrowserView` when you need significantly different delegate behavior for popups

**Testing:**

1. Launch the app - main window is 800x600
2. Click "Create Popup Window" - popup opens at 600x400 (smaller)
3. Click "Create Popup Window" from the popup - another 600x400 popup opens
4. All popups use the custom smaller size
5. Close popups - app continues running until all browsers closed

**When to use each callback:**

| Scenario | Use OnBeforePopup | Use OnPopupBrowserViewCreated |
|----------|------------------|-------------------------------|
| Block all popups | ✅ Return true to cancel | ❌ Too late (browser already created) |
| Custom popup window size (Views) | ❌ Limited control | ✅ Full window control |
| Custom popup window styling (Views) | ❌ Limited control | ✅ Full window control |
| Popup policy enforcement | ✅ Can examine URL, features | ❌ Too late |
| Native window popups (non-Views) | ✅ Can modify CefWindowInfo | ❌ Not called for native windows |

**Important notes:**

- `OnPopupBrowserViewCreated` is **only called for Views-based browsers** (our tutorial uses Views)
- If using native platform windows (non-Views), use `OnBeforePopup` and `CefWindowInfo` instead
- Both callbacks can be used together (OnBeforePopup for policy, OnPopupBrowserViewCreated for styling)
- DevTools windows also trigger these callbacks (check `is_devtools` parameter)

# Next Steps

Congratulations! You've built a CEF application from scratch and learned:

- ✅ CEF initialization and lifecycle
- ✅ Views framework for cross-platform UI
- ✅ Browser and window delegates
- ✅ Command-line argument handling
- ✅ Display and load handlers
- ✅ JavaScript execution from C++
- ✅ JavaScript-to-C++ communication via process messages
- ✅ Multiple browser tracking and lifecycle management
- ✅ Custom popup window styling with OnPopupBrowserViewCreated

## Explore More Features

**[cefclient](https://github.com/chromiumembedded/cef/blob/master/tests/cefclient/)** - A comprehensive sample application demonstrating:

- Chrome style browsers (full Chrome UI)
- Platform-specific implementations (WinAPI, GTK, Cocoa)
- Off-screen rendering (OSR)
- Custom scheme handlers (`client://` URLs)
- Request interception and modification
- Cookie management
- Dialog handlers
- Context menus
- Drag and drop
- And much more...

**[cef-project](https://github.com/chromiumembedded/cef-project)** - Focused example applications demonstrating specific features:

- **[message_router](https://github.com/chromiumembedded/cef-project/blob/master/examples/message_router)** - JavaScript bindings using CefMessageRouter for request/response patterns
- **[resource_manager](https://github.com/chromiumembedded/cef-project/blob/master/examples/resource_manager)** - Resource request handling using CefResourceManager
- **[scheme_handler](https://github.com/chromiumembedded/cef-project/blob/master/examples/scheme_handler)** - Custom protocol handlers using CefSchemeHandlerFactory

## Learn Advanced Topics

**[General Usage](general_usage.md)** - Complete CEF documentation covering:

- Multi-process architecture in depth
- Threading model and task posting
- Request handling and network interception
- JavaScript integration patterns
- Custom protocol handlers
- Performance optimization
- Security considerations

## Using the C API

**[Using The CAPI](using_the_capi.md)** - Complete guide to the CEF C API:

- Manual reference counting patterns
- Structure allocation and ownership
- String handling
- Thread safety and atomic operations
- Complete working examples in [cefsimple_capi](https://github.com/chromiumembedded/cef/blob/master/tests/cefsimple_capi/)

## Using Claude Code for Implementation

Want help implementing additional features in your CEF application? The CEF project includes instructions for using Claude Code to assist with tasks like adding custom URL schemes, JavaScript-to-C++ communication, request interception, and more.

See the [CEF Client Development Guide](https://github.com/chromiumembedded/cef/blob/master/tools/claude/CLIENT_DEVELOPMENT.md) for step-by-step instructions.