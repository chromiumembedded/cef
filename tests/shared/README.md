# CEF Shared Test Code

This directory contains common code and utilities shared across CEF test applications (cefclient and ceftests).

## Overview

The shared directory provides reusable components that handle common CEF integration patterns, reducing code duplication across test applications and demonstrating best practices for CEF application development.

**Used by:**

- [cefclient](../cefclient/) - Comprehensive feature demonstration and testing application
- [ceftests](../ceftests/) - Automated unit test suite

**Not used by:**

- [cefsimple](../cefsimple/) - Minimal C++ example (self-contained)
- [cefsimple_capi](../cefsimple_capi/) - Minimal C API example (self-contained)

## Directory Structure

### browser/

Browser process utilities including:

- Application framework with delegate pattern (`client_app_browser`)
- Message loop implementations (standard, external pump, multi-threaded)
- Cross-platform resource loading (`resource_util`)
- File system operations and geometry utilities

### common/

Code shared across all process types:

- Base application handler (`client_app`)
- Process type detection and command-line parsing
- Custom scheme registration
- String and binary data utilities

### renderer/

Renderer process handler with delegate pattern for V8 context management and JavaScript extension registration.

### resources/

Shared test resources (HTML pages, icons, sample files).

## Key Concepts

**Delegate Pattern:** Applications (cefclient, ceftests) extend shared base classes by implementing delegates rather than modifying shared code.

**Message Loop Abstraction:** Supports multiple threading models - standard CEF loop, external pump (integrates with platform event loops), and multi-threaded mode.

**Platform Abstraction:** Common interfaces with platform-specific implementations for resource loading, file operations, and window management.

## Usage

Applications use shared handlers for process-specific behavior:

```cpp
CefRefPtr<ClientApp> app;
switch (process_type) {
  case ClientApp::BrowserProcess:
    app = new ClientAppBrowser();   // Browser process handler
    break;
  case ClientApp::RendererProcess:
    app = new ClientAppRenderer();  // Renderer process handler
    break;
}
```

Applications customize behavior by implementing delegates without modifying shared code.

## References

- [CEF General Usage](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage.md) - CEF API documentation
- [CEF Tutorial](https://bitbucket.org/chromiumembedded/cef/wiki/Tutorial.md) - Application architecture overview
- [cefclient](../cefclient/) - Reference implementation using shared code
- [ceftests](../ceftests/) - Test suite using shared code
