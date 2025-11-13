# CEF Client Sample Application

This directory contains the comprehensive CEF sample application demonstrating advanced browser integration features and CEF capabilities.

## Overview

**cefclient** is the primary CEF sample application showcasing:

- **Complete feature set** - Demonstrates most CEF APIs and integration patterns
- **Test framework** - Built-in tests for various CEF functionality
- **Advanced features** - OSR (off-screen rendering), process messaging, scheme handlers, and more
- **Multi-window support** - Window management, popups, and DevTools
- **Cross-platform** - Full implementations for Windows, Linux, and macOS

### Comparison with cefsimple

| Feature | cefclient | cefsimple |
|---------|-----------|-----------|
| **Purpose** | Comprehensive feature demo & testing | Minimal learning example |
| **Complexity** | ~100+ source files | ~10 source files |
| **Features** | Full CEF API coverage | Basic browser only |
| **Best for** | Reference implementation, testing | Learning CEF basics |

**For learning CEF:** Start with [cefsimple](../cefsimple/) to understand the basics, then explore cefclient for advanced features.

## Learning Resources

**Understanding cefclient architecture:**

1. **[CEF Tutorial](https://bitbucket.org/chromiumembedded/cef/wiki/Tutorial.md)** - Covers cefsimple architecture, which provides the foundation
2. **[CEF General Usage](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage.md)** - Complete CEF API documentation
3. **This application** - Explore the built-in tests and advanced implementations

## Architecture

### Core Components

- **MainContext** (`browser/main_context*`) - Application state and CEF initialization
- **RootWindowManager** (`browser/root_window_manager*`) - Window creation and management
- **ClientHandler** (`browser/client_handler*`) - CEF client implementation with event handlers
- **RootWindow** (`browser/root_window*`) - Individual browser window management
- **TestRunner** (`browser/test_runner*`) - Test coordination and execution

### Directory Structure

- **browser/** - Browser process implementation (main application logic)
- **renderer/** - Renderer process code (runs in isolated renderer processes)
- **common/** - Shared code used by multiple processes
- **resources/** - HTML/JS/CSS test pages and resources
- **mac/**, **win/** - Platform-specific resources and helpers

### Platform Entry Points

- **cefclient_gtk.cc** - Linux (GTK) entry point
- **cefclient_mac.mm** - macOS (Cocoa) entry point
- **cefclient_win.cc** - Windows (Win32) entry point

## Key Features Demonstrated

### Basic Browser Features

- Window creation and management
- Navigation and lifecycle
- JavaScript execution
- Resource loading and scheme handlers

### Advanced Features

- **Off-Screen Rendering (OSR)** - Windowless rendering with custom painting
- **Process messaging** - Communication between browser and renderer processes
- **DevTools integration** - Chrome Developer Tools
- **Downloads** - Download handling and management
- **Dialogs** - File dialogs, JavaScript dialogs, context menus
- **Performance** - DOM access, frame rate control
- **Extensions** - Chrome extension loading (Chrome style)

### Testing

The application includes numerous tests accessible via the **Tests** menu:

- Binding tests (JavaScript ↔ C++ communication)
- Window management tests
- Request/response tests
- JavaScript execution tests
- Scheme handler tests
- And many more...

## UI Frameworks

cefclient supports multiple UI approaches:

- **Native** - Platform-specific UI (GTK, Cocoa, Win32)
- **Views** - Cross-platform CEF Views framework
- **OSR** - Custom rendering without native windows

## Browser Styles

cefclient uses the Chrome bootstrap and supports two browser styles:

- **Chrome style** (default) - Modern Chrome browser with advanced features (autofill, extensions, gamepad, etc.) and standard Chrome UI (toolbars, dialogs, etc.)
- **Alloy style** - Traditional CEF browser based on the Chromium Content API, provides more flexibility for custom UI

Use command-line flags to select the browser style and other options.

## Exploring Features

Try these different modes to explore various cefclient features:

```bash
# Chrome style (default):
cefclient

# Alloy style:
cefclient --use-alloy-style
cefclient --use-alloy-style --use-native
cefclient --off-screen-rendering-enabled

# Multi-threaded message loop (Windows/Linux only):
cefclient --multi-threaded-message-loop
cefclient --use-alloy-style --multi-threaded-message-loop

# Chrome style + native parent (Windows/Linux only):
cefclient --use-native
```

**What each mode demonstrates:**

- **Default** - Chrome style with Views framework
- **--use-alloy-style** - Traditional CEF browser
- **--use-native** - Platform-specific native UI instead of Views
- **--off-screen-rendering-enabled** - OSR mode (windowless rendering)
- **--multi-threaded-message-loop** - Alternative threading model

**Note:** On macOS, use `open cefclient.app --args <flags>` to pass command-line arguments.

Once running, explore the **Tests** menu to see demonstrations of various CEF features.

## C API Version

Unlike cefsimple, cefclient does **not** have a C API equivalent due to its complexity. For a C API example, see [cefsimple_capi](../cefsimple_capi/).

## References

- [CEF Tutorial](https://bitbucket.org/chromiumembedded/cef/wiki/Tutorial.md) - Start with cefsimple concepts
- [CEF General Usage](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage.md) - Complete API documentation
- [cefsimple](../cefsimple/) - Minimal example for learning basics
- [CEF Forum](https://magpcss.org/ceforum/) - Community support and discussions
