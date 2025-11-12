# CEF Simple C API Example

This directory contains a pure C implementation of the cefsimple sample application using the CEF C API (CAPI).

## Overview

This is a conversion of `cef/tests/cefsimple` from C++ to pure C, demonstrating:

- **Pure C implementation** - No C++ code, uses C11 standard
- **Manual reference counting** - Using atomic operations for thread safety
- **CEF C API usage** - All CEF calls use the C API (`include/capi/*.h`)
- **Cross-platform** - Supports Windows, Linux, and macOS

### Maintenance Pattern

This project serves as a reference C API implementation that mirrors the C++ `cefsimple` example. When changes are made to `cef/tests/cefsimple` (the C++ version), **equivalent changes should be applied to this project** using the C API patterns.

**Intended workflow:**

1. Changes are made to C++ `cefsimple` (new features, API updates, bug fixes)
2. Claude Code analyzes the C++ changes
3. Claude Code applies equivalent changes to `cefsimple_capi` using CEF C API
4. Result: Both examples stay in sync, demonstrating the same functionality in C++ and C

**Example Claude prompt:**
```
Apply the equivalent changes from the latest commits in cef/tests/cefsimple
to cef/tests/cefsimple_capi, converting the C++ code to C API patterns.
Use the reference counting and handler patterns from UsingTheCAPI.md.
```

This ensures the C API example remains current and demonstrates the same CEF features as the C++ version.

## Architecture

### Core Files

- **ref_counted.h** - Helper macros for implementing reference counting
- **simple_app.h/c** - Application handler (`cef_app_t`)
- **simple_handler.h/c** - Client handler with display, life span, and load handlers

### Platform-Specific Files

#### Windows
- **cefsimple_win.c** - Windows entry point (wWinMain)
- **simple_handler_win.c** - Windows-specific handler implementation

#### Linux
- **cefsimple_linux.c** - Linux entry point (main)
- **simple_handler_linux.c** - Linux/X11-specific handler implementation

#### macOS
- **cefsimple_mac.m** - macOS entry point with NSApplication integration
- **simple_handler_mac.m** - macOS-specific handler implementation

## Key Differences from C++ Version

The C API requires significantly different patterns than C++:

- **Manual reference counting** - Implement atomic `add_ref`/`release` functions instead of `IMPLEMENT_REFCOUNTING` macro
- **Explicit parameter release** - Must call `release()` on all object parameters in callbacks (memory leak if forgotten)
- **Function pointers** - Use structure fields with `CEF_CALLBACK` functions instead of virtual methods
- **Manual string conversion** - Use `cef_string_from_ascii()` / `cef_string_clear()` instead of `CefString` class
- **Structure initialization** - Use `calloc()` and set function pointers instead of C++ constructors

**See [UsingTheCAPI.md](https://bitbucket.org/chromiumembedded/cef/wiki/UsingTheCAPI.md) for complete C API patterns and examples.**

## Critical C API Rules

When working with the C API, you **must** follow these rules to avoid crashes and memory leaks:

1. **CEF structure MUST be first member** - Allows safe casting between base and wrapper types
2. **Set `size` to CEF type size** - Use `sizeof(cef_*_t)`, not `sizeof(your_wrapper_t)`
3. **Release all callback parameters** - Except `self`; failure causes memory leaks
4. **Add reference when returning to CEF** - CEF will release when done
5. **Use atomic operations** - Reference counting functions called from any thread

**See [UsingTheCAPI.md](https://bitbucket.org/chromiumembedded/cef/wiki/UsingTheCAPI.md) for detailed explanations, complete code examples, and reference counting patterns.**

## References

- [CEF C API Documentation](https://bitbucket.org/chromiumembedded/cef/wiki/UsingTheCAPI.md)
- [CEF C API Headers](../../../include/capi/)
- [Original C++ cefsimple](../cefsimple/)
