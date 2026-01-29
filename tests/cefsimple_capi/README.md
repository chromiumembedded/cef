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

## Learning Path

For best results, we recommend this learning path:

1. **[CEF Tutorial](https://github.com/chromiumembedded/cef/blob/master/docs/tutorial.md)** - Start here to understand CEF architecture and concepts using the C++ cefsimple example
2. **[Using the C API](https://github.com/chromiumembedded/cef/blob/master/docs/using_the_capi.md)** - Learn C-specific patterns and reference counting rules
3. **This example (cefsimple_capi)** - See the C API implementation in practice

**Note:** The Tutorial uses C++ code examples, but the architectural concepts (processes, threads, callbacks, program flow) apply equally to the C API version.

## Architecture

### Utility Modules

- **ref_counted.h** - Macros for implementing reference counting (`IMPLEMENT_REFCOUNTING_SIMPLE`, `IMPLEMENT_REFCOUNTING_MANUAL`, `INIT_CEF_BASE_REFCOUNTED`)
- **simple_utils.h** - Common utilities (`CHECK` macro for invariant validation)
- **simple_browser_list.h/c** - Dynamic list of browser instances with automatic reference counting

### Application Core

- **simple_app.h/c** - Application handler (`cef_app_t`) and browser process handler

### Client Handler Module

- **simple_handler.h** - Client handler interface and structure definitions
- **simple_handler.c** - Main client handler implementation, public API, and browser close operations
- **simple_display_handler.c** - Display handler (title change notifications)
- **simple_life_span_handler.c** - Life span handler (browser lifecycle management)
- **simple_load_handler.c** - Load handler (error page display)

### Views Framework

- **simple_views.h/c** - Browser view and window delegates for Views-based UI

### Platform-Specific Files

#### Windows

- **cefsimple_win.c** - Windows entry point (wWinMain)
- **simple_handler_win.c** - Windows-specific handler (window title updates)

#### Linux

- **cefsimple_linux.c** - Linux entry point (main)
- **simple_handler_linux.c** - Linux/X11-specific handler (window title updates)

#### macOS

- **cefsimple_mac.m** - macOS entry point with NSApplication integration
- **simple_handler_mac.m** - macOS-specific handler (window title updates)
- **process_helper_mac.c** - Helper process entry point

## Key Differences from C++ Version

The C API requires significantly different patterns than C++:

- **Manual reference counting** - Implement atomic `add_ref`/`release` functions instead of `IMPLEMENT_REFCOUNTING` macro
- **Explicit parameter release** - Must call `release()` on all object parameters in callbacks (memory leak if forgotten)
- **Function pointers** - Use structure fields with `CEF_CALLBACK` functions instead of virtual methods
- **Manual string conversion** - Use `cef_string_from_ascii()` / `cef_string_clear()` instead of `CefString` class
- **Structure initialization** - Use `calloc()` and set function pointers instead of C++ constructors

**See [UsingTheCAPI.md](https://github.com/chromiumembedded/cef/blob/master/docs/using_the_capi.md) for complete C API patterns and examples.**

## Critical C API Rules

When working with the C API, you **must** follow these rules to avoid crashes and memory leaks:

1. **CEF structure MUST be first member** - Allows safe casting between base and wrapper types
2. **Set `size` to CEF type size** - Use `sizeof(cef_*_t)`, not `sizeof(your_wrapper_t)`
3. **Release all callback parameters** - Except `self`; failure causes memory leaks
4. **Add reference when returning to CEF** - CEF will release when done
5. **Use atomic operations** - Reference counting functions called from any thread

**See [UsingTheCAPI.md](https://github.com/chromiumembedded/cef/blob/master/docs/using_the_capi.md) for detailed explanations, complete code examples, and reference counting patterns.**

## References

- [CEF Tutorial](https://github.com/chromiumembedded/cef/blob/master/docs/tutorial.md) - Architecture overview (uses C++ examples)
- [CEF C API Documentation](https://github.com/chromiumembedded/cef/blob/master/docs/using_the_capi.md) - C-specific patterns
- [Original C++ cefsimple](../cefsimple/)
