# CEF Unit Tests

This directory contains the CEF unit test application (`ceftests`) - a comprehensive test suite for validating CEF functionality across all supported platforms.

## Overview

**ceftests** is a Google Test-based unit test application that validates CEF APIs and integration patterns:

- **Comprehensive coverage** - Tests most CEF APIs and features
- **Automated testing** - Runs headlessly for CI/CD integration
- **Cross-platform** - Validates behavior on Windows, Linux, and macOS
- **Multiple modes** - Supports Chrome style and Alloy style browser testing
- **Quality assurance** - Used to verify CEF releases and Chromium updates

### Comparison with cefclient

| Feature | ceftests | cefclient |
|---------|----------|-----------|
| **Purpose** | Automated unit testing | Manual feature demonstration |
| **Test Type** | Google Test (gtest) | Interactive manual tests |
| **Execution** | Headless/automated | GUI-based interactive |
| **Best for** | CI/CD, regression testing | Feature exploration, debugging |

**For testing:** Use ceftests for automated validation and cefclient for manual feature testing and exploration.

## Test Categories

ceftests includes unit tests for:

### Core Features

- **API Version** - CEF version and API hash validation
- **Command Line** - Command-line argument parsing
- **Life Span** - Browser lifecycle and window management
- **Navigation** - URL loading, redirects, and navigation events
- **Frame** - Multi-frame handling and frame lifecycle

### Web Content

- **DOM** - DOM manipulation and queries
- **JavaScript** - V8 integration and JavaScript execution
- **Display** - Rendering and display events
- **Dialogs** - JavaScript dialogs and file dialogs

### Network and Resources

- **URL Request** - Network requests and responses
- **Scheme Handler** - Custom protocol handlers
- **Cookie** - Cookie management and storage
- **CORS** - Cross-origin resource sharing
- **Resource Manager** - Resource loading and caching
- **Certificate Errors** - SSL/TLS certificate handling

### Advanced Features

- **Process Messages** - Inter-process communication
- **Message Router** - JavaScript-C++ messaging framework
- **DevTools** - Chrome DevTools protocol
- **Downloads** - Download handling and management
- **Printing** - Print and print-to-PDF functionality
- **Media Access** - Camera and microphone permissions
- **Audio Output** - Audio device enumeration

### UI and Rendering

- **Views Framework** - CEF Views UI tests (button, panel, textfield, window, scroll view)
- **OSR (Off-Screen Rendering)** - Windowless rendering and accessibility
- **Draggable Regions** - Custom window dragging areas
- **Permission Prompts** - Permission request UI

### Utilities

- **Parser** - URL and data parsing
- **String** - CEF string handling
- **Stream** - Stream operations and handlers
- **Time** - Time conversion utilities
- **Thread** - Threading and task posting
- **Tracing** - Performance tracing integration
- **Waitable Event** - Synchronization primitives
- **XML/ZIP** - XML parsing and ZIP archive handling

## Running Tests

### Basic Usage

Run all tests with default settings:

```bash
# Windows/Linux
ceftests

# macOS
./ceftests.app/Contents/MacOS/ceftests
```

### Common Test Options

Use Google Test flags to control test execution:

```bash
# Run specific test suite
ceftests --gtest_filter="NavigationTest.*"

# Run specific test
ceftests --gtest_filter="NavigationTest.LoadURL"

# Exclude specific tests
ceftests --gtest_filter="-AudioOutputTest.*"

# List all available tests
ceftests --gtest_list_tests

# Run with Chrome style Views framework
ceftests --use-views

# Run with Alloy style
ceftests --use-alloy-style

# Combine options
ceftests --use-views --gtest_filter="NavigationTest.*"
```

### Platform-Specific Notes

**macOS:** Use the path to the executable inside the `.app` bundle:
```bash
./ceftests.app/Contents/MacOS/ceftests --use-views
```

**Linux headless:** Use `xvfb-run` for headless testing:
```bash
xvfb-run ceftests --no-sandbox
```

### Filters for Known Issues

Some tests are known to be flaky or have platform-specific issues. For the recommended test filters that exclude these known issues, see [ChromiumUpdate.md § Run CEF tests](https://github.com/chromiumembedded/cef/blob/master/docs/chromium_update.md#5-run-cef-tests), section C.

## Test Infrastructure

### Core Test Framework

- **test_handler.h/cc** - Base class for browser-based tests
- **test_suite.h/cc** - Test suite initialization and configuration
- **test_util.h/cc** - Common test utilities and helpers
- **routing_test_handler.h/cc** - Base for tests using message routing
- **thread_helper.h/cc** - Threading utilities for async tests
- **track_callback.h** - Callback tracking for verifying execution order

### Test Server Support

- **test_server.h/cc** - HTTP test server implementation
- **test_server_manager.h/cc** - Server instance management
- **test_server_runner.h/cc** - Server execution framework
- **test_server_observer.h/cc** - Server event monitoring

### Platform-Specific Files

- **Windows**: `win/` directory with resources and manifest
- **macOS**: Helper process integration, OS rendering support (`os_rendering_unittest_mac.mm`)
- **Linux**: Resource utilities (`resource_util_linux.cc`)

## Test Development

### Writing New Tests

Tests use Google Test framework with CEF-specific patterns:

```cpp
#include "tests/ceftests/test_handler.h"

TEST(MyTestSuite, MyTest) {
  // Create test handler
  CefRefPtr<MyTestHandler> handler = new MyTestHandler();

  // Execute test
  handler->ExecuteTest();

  // Verify results
  EXPECT_TRUE(handler->succeeded());
}
```

For browser-based tests, extend `TestHandler`:

```cpp
class MyTestHandler : public TestHandler {
 public:
  void RunTest() override {
    // Create and load browser
    CreateBrowser("https://example.com");
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    // Test logic here
    DestroyTest();
  }
};
```

### Test Execution Modes

ceftests supports multiple execution modes:

- **Chrome style** - Tests using Chrome UI layer (default)
- **Alloy style** - Tests using Content API layer (`--use-alloy-style`)
- **Views framework** - Tests using CEF Views (`--use-views`)
- **Headless** - Non-interactive testing for CI/CD (Linux: use `xvfb-run`)

## Known Issues

Some tests may be flaky or have known limitations:

- **Flaky tests** - Tracked at: https://github.com/chromiumembedded/cef/issues?q=is%3Aissue+is%3Aopen+label%3Atests
- **Chrome style limitations** - Tracked at: https://github.com/chromiumembedded/cef/issues?q=is%3Aissue+is%3Aopen+label%3Achrome
- **macOS Views support** - Issue #3188: https://github.com/chromiumembedded/cef/issues/3188

For the latest test filters and workarounds, see the ChromiumUpdate.md Wiki page.

## Resources

Test resources are located in `resources/` directory:

- SSL certificates for certificate validation tests
- Test data files

## References

- [CEF General Usage](https://github.com/chromiumembedded/cef/blob/master/docs/general_usage.md) - Complete API documentation
- [ChromiumUpdate.md](https://github.com/chromiumembedded/cef/blob/master/docs/chromium_update.md) - Test execution details
- [Google Test Documentation](https://google.github.io/googletest/) - gtest framework reference
- [cefclient](../cefclient/) - Interactive test application for manual testing
