# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## About Chromium

Chromium is an open-source browser project that forms the foundation for Google Chrome and many other browsers. This codebase contains the full Chromium browser implementation, including the rendering engine (Blink), JavaScript engine (V8), networking stack, and platform-specific implementations across Windows, macOS, Linux, Android, iOS, and Chrome OS.

## Build System

Chromium uses **GN (Generate Ninja)** as its meta-build system and **Ninja** as the actual build system:

### Essential Build Commands

```bash
# Configure build (creates out/Default/ directory with build files)
gn gen out/Default

# Build Chrome browser
autoninja -C out/Default chrome

# Build with custom arguments inline
gn gen out/Default --args="is_debug=false is_component_build=true"

# Build specific targets
autoninja -C out/Default unit_tests browser_tests blink_tests

# Build with error tolerance (continue building other targets on failure)
autoninja -k 0 -C out/Default target_name
```

### Common Build Arguments (.gn args or --args)

For faster development builds:
```gn
is_debug = false                # Release build (faster linking)
is_component_build = true       # Many small libraries instead of one large binary
symbol_level = 0               # No debug symbols (fastest)
blink_symbol_level = 0         # Disable Blink debug symbols
v8_symbol_level = 0           # Disable V8 debug symbols
enable_nacl = false           # Disable Native Client
target_cpu = "x86"            # 32-bit build (may be faster)
```

For Google employees with remote execution:
```gn
use_remoteexec = true
use_siso = true
```

## Testing

### Running Tests

```bash
# Unit tests (gtest)
autoninja -C out/Default unit_tests
out/Default/unit_tests --gtest_filter="BrowserListUnitTest.*"

# Browser tests
autoninja -C out/Default browser_tests  
out/Default/browser_tests --gtest_filter="SomeTest.*"

# Web tests (formerly layout tests)
autoninja -C out/Default blink_tests
third_party/blink/tools/run_web_tests.py -t Default
third_party/blink/tools/run_web_tests.py -t Default fast/css/

# Find which test target contains a specific file
gn refs out/Default --testonly=true --type=executable --all chrome/browser/ui/browser_list_unittest.cc
```

### Test Execution Notes

- **Test filters**: Use `MyTestSuite.MyTest` for specific tests or `MyTestSuite.*` to run all tests in a suite
- **Platform considerations**: On Windows, add `.exe` to executable names (e.g., `browser_tests.exe`)
- **Error handling**: If tests fail, fix runtime errors iteratively and rebuild if necessary
- **Iteration limit**: After 3 failed attempts to fix the same error, stop and request more information

## Code Quality and Formatting

### Required Tools

```bash
# Format code (required before upload)
git cl format

# Static analysis (runs automatically on Gerrit CLs)
# Enable locally by adding to .gclient:
# custom_vars': { 'checkout_clang_tidy': True }
```

## Repository Management

### Getting and Updating Code

```bash
# Initial checkout
fetch chromium  # or fetch --no-history chromium
cd src
gclient runhooks

# Update existing checkout
git rebase-update    # Updates and rebases local branches
gclient sync         # Syncs dependencies and re-runs hooks

# Upload changes for review
git cl upload

# Submit changes
git cl land
```

## Architecture Overview

Chromium is organized into major components with clear separation of concerns:

### Core Browser Architecture

- **//chrome/**: Chrome browser application layer
    - `//chrome/browser/`: Browser process code (UI, downloads, profiles, extensions)
    - `//chrome/renderer/`: Renderer process code (ChromeRenderFrameObserver, etc.)
    - `//chrome/common/`: Shared code between browser and renderer processes

- **//content/**: Content API - cross-platform browser engine
    - `//content/browser/`: Browser process implementation (navigation, security, IPC)
    - `//content/renderer/`: Renderer process implementation (Blink integration)
    - `//content/public/`: Public APIs for embedders like Chrome

- **//base/**: Fundamental utilities and primitives
    - Cross-platform abstractions for threading, files, logging, memory management
    - Foundation for all other Chromium code

### UI and Platform Integration

- **//ui/**: Cross-platform UI toolkit and primitives
    - `//ui/views/`: Desktop UI framework (Windows/Linux/Mac desktop UI)
    - `//ui/gfx/`: Graphics primitives and rendering utilities
    - `//ui/base/`: Basic UI utilities and resource management

- **//ash/**: Chrome OS system UI (launcher, system tray, window management)

### Web Platform Implementation

- **//third_party/blink/**: Web rendering engine
    - `//third_party/blink/renderer/core/`: DOM, CSS, layout engine
    - `//third_party/blink/renderer/platform/`: Platform abstractions for Blink
    - Located in third_party but developed as part of Chromium

- **//v8/**: JavaScript engine (Git submodule)

### Networking and Services

- **//net/**: Cross-platform networking library
    - HTTP/HTTPS, cookies, DNS, proxy support
    - Can be used independently of the browser

- **//services/**: Mojo-based services architecture
    - Process isolation and security boundaries
    - Network service, storage service, device service, etc.

### Platform-Specific Code

- **Platform patterns**: `*_win.cc`, `*_mac.mm`, `*_linux.cc`, `*_android.cc`
- **//android_webview/**: WebView implementation for Android
- **//ios/**: iOS-specific browser implementation
- **//chrome/browser/ui/views/**: Desktop platform UI using Views framework

### Security and Sandboxing

- **//sandbox/**: Multi-platform sandboxing implementations
- **//content/browser/**: Security policy enforcement and process management
- **Site Isolation**: Security architecture separating web sites into different processes

### Build and Development Infrastructure

- **//build/**: Build system configuration and scripts
- **//tools/**: Development tools and utilities
- **//testing/**: Test infrastructure and utilities

## Multi-Process Architecture

Chromium uses a multi-process architecture for security and stability:

- **Browser Process**: Main process, manages UI, navigation, downloads, profiles
- **Renderer Processes**: Isolated processes for web content (one per site)
- **GPU Process**: Hardware-accelerated graphics operations
- **Network Service**: Network operations in isolated process
- **Utility Processes**: Various sandboxed helper processes

Communication between processes uses **Mojo IPC** (//mojo/) with strong type safety and security boundaries.

## Development Workflow

### Typical Development Process

1. **Set up build**: `gn gen out/Default --args="..."`
2. **Make changes**: Edit source files
3. **Build**: `autoninja -C out/Default chrome`
    - If encountering compile errors, fix them iteratively and rebuild
    - Use `autoninja -k 0 -C out/Default target_name` to continue building other targets on failure
4. **Test**: Run relevant tests (unit_tests, browser_tests, or web tests)
5. **Format**: `git cl format` (required)
6. **Upload**: `git cl upload` for code review
7. **Address feedback**: Make changes, re-upload
8. **Submit**: `git cl land` or submit via Gerrit

### Platform Considerations

- **Cross-platform compatibility**: Most core code should work across platforms
- **Platform-specific implementations**: Use `#if BUILDFLAG(IS_*)` for platform-specific code
- **Views framework**: Preferred for desktop UI across Windows, Linux, Mac
- **Material Design**: UI should follow Material Design principles where applicable

### Key Documentation

Refer to [docs/README.md](docs/README.md) for comprehensive documentation including:

- Platform-specific build instructions
- Design documents and architecture guides  
- Testing methodologies and best practices
- Development workflow and tools

## Important Notes

- **Thread safety**: Much Chromium code runs on specific threads (UI, IO, background threads)
- **Memory management**: Uses custom smart pointers (`scoped_refptr`, etc.) and careful lifetime management
- **Security**: All web-facing code must consider security implications and sandboxing
- **Performance**: Chromium is performance-critical; changes should consider memory usage, startup time, and runtime performance
- **Backwards compatibility**: Web platform changes must maintain compatibility with existing web content

When modifying this codebase, always consider the multi-platform nature, security implications, and performance impact of changes. Follow existing patterns and consult the extensive documentation in the [docs/](docs/) directory.