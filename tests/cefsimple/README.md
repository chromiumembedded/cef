# CEF Simple C++ Example

This directory contains a minimal CEF browser application written in C++ using the CEF C++ API.

## Overview

**cefsimple** is the canonical minimal CEF application demonstrating:

- **C++ implementation** - Uses CEF C++ API with automatic reference counting
- **Basic browser functionality** - Window creation, navigation, and lifecycle management
- **Cross-platform** - Supports Windows, Linux, and macOS
- **Minimal code** - Simple architecture suitable for learning CEF basics

### C API Version

A pure C implementation of this example is available at [../cefsimple_capi/](../cefsimple_capi/). The C version demonstrates the same functionality using the CEF C API with manual reference counting.

**When to use each:**

- **Use cefsimple (C++)** - For production applications and most use cases
- **Use cefsimple_capi (C)** - When C++ is not available or for embedding in C projects

## Learning Resources

**New to CEF?** Start with the [CEF Tutorial](https://bitbucket.org/chromiumembedded/cef/wiki/Tutorial.md), which provides a detailed walkthrough of this exact example:

- **Architecture overview** - CEF processes, threads, and callbacks
- **Code walkthrough** - Detailed explanation of each file and function
- **Program flow** - How cefsimple initializes and runs
- **Key concepts** - CefApp, CefClient, handlers, and browser lifecycle

The Tutorial is the recommended starting point for understanding how CEF applications work.

For complete API documentation, see [GeneralUsage](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage.md).

## Architecture

### Core Files

- **simple_app.h/cc** - Application handler (CefApp)
- **simple_handler.h/cc** - Client handler with display, life span, and load handlers

### Platform-Specific Files

#### Windows

- **cefsimple_win.cc** - Windows entry point (wWinMain)
- **simple_handler_win.cc** - Windows-specific handler implementation

#### Linux

- **cefsimple_linux.cc** - Linux entry point (main)
- **simple_handler_linux.cc** - Linux/X11-specific handler implementation

#### macOS

- **cefsimple_mac.mm** - macOS entry point with NSApplication integration
- **simple_handler_mac.mm** - macOS-specific handler implementation
- **process_helper_mac.cc** - Helper process entry point

## Building

From the Chromium `src` directory:

```bash
# Debug build (x64)
autoninja -C out/Debug_GN_x64 cefsimple

# Debug build (ARM64 on macOS)
autoninja -C out/Debug_GN_arm64 cefsimple

# Release build
autoninja -C out/Release_GN_x64 cefsimple
```

## Running

```bash
# macOS
out/Debug_GN_arm64/cefsimple.app/Contents/MacOS/cefsimple

# Linux
out/Debug_GN_x64/cefsimple

# Windows
out/Debug_GN_x64/cefsimple.exe
```

Expected behavior: Opens a window displaying https://www.google.com

## Key Features

- **Automatic reference counting** - CefRefPtr handles object lifetime
- **Helper window** - Creates a browser window with navigation controls
- **Error handling** - Displays error pages for failed navigations
- **Cross-platform UI** - Uses Views framework for consistent UI

## Maintenance Pattern

This is the primary CEF sample application. Changes to CEF features, API updates, or bug fixes should be applied here first, then ported to cefsimple_capi (the C version).

**Typical workflow:**

1. Update cefsimple (C++ version) with new features or API changes
2. Port equivalent changes to cefsimple_capi using C API patterns
3. Result: Both examples demonstrate the same functionality

## References

- [CEF Tutorial](https://bitbucket.org/chromiumembedded/cef/wiki/Tutorial.md) - Detailed walkthrough of this example
- [CEF General Usage](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage.md) - Complete API documentation
- [CEF C++ API Documentation](https://bitbucket.org/chromiumembedded/cef/wiki/)
- [C API Version](../cefsimple_capi/) - Pure C implementation of this example
