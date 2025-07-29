# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## About CEFClient

CEFClient is a sample application demonstrating the Chromium Embedded Framework (CEF). It serves as both a reference implementation and testing platform for CEF integration patterns. This is part of the larger CEF project that provides embedded browser capabilities for native applications.

## Build System

CEFClient uses CMake as its build system with platform-specific configurations:

- **Build file**: `CMakeLists.txt.in` (template processed by CEF's build system)
- **Platform support**: Linux (GTK), macOS (Cocoa), Windows
- **Architecture**: Multi-process browser with separate browser, renderer, and helper processes

### Key Build Concepts

- **Process types**: CEFClient builds different executables for different process types on some platforms
- **Alloy vs Chrome style**: Supports both CEF Alloy style (legacy) and Chrome style (modern) implementations
- **OSR support**: Includes off-screen rendering capabilities with platform-specific backends (D3D11, OpenGL)

## Architecture Overview

### Core Components

1. **MainContext** (`browser/main_context.h`, `browser/main_context_impl.h`)
   - Global application state and configuration management
   - Command-line argument processing
   - CEF initialization and shutdown

2. **RootWindowManager** (`browser/root_window_manager.h`)
   - Creates and manages browser windows
   - Handles popup window creation and management
   - Tracks active windows and browser instances

3. **ClientHandler** (`browser/client_handler.h`)
   - Primary CEF client implementation
   - Handles browser events, callbacks, and lifecycle
   - Implements multiple CEF handler interfaces (display, download, keyboard, etc.)

4. **RootWindow** (`browser/root_window.h`)
   - Represents individual browser windows
   - Platform-specific implementations (GTK, Cocoa, Win32, Views)
   - Manages window lifecycle and UI integration

### Platform-Specific Code

- **Linux**: GTK-based implementation in `*_gtk.*` files
- **macOS**: Cocoa/Objective-C implementation in `*_mac.*` files  
- **Windows**: Win32 API implementation in `*_win.*` files
- **Views**: Cross-platform CEF Views framework in `*_views.*` files

### Test Framework

- **TestRunner** (`browser/test_runner.h`): Coordinates test execution
- **Test implementations**: Various `*_test.*` files implementing specific functionality tests
- **Resource files**: HTML/JS test pages in `resources/` directory

## Key File Locations

- **Entry points**: `cefclient_gtk.cc`, `cefclient_mac.mm`, `cefclient_win.cc`
- **Common code**: `common/` directory for shared functionality
- **Renderer process**: `renderer/` directory for renderer-side code
- **Platform resources**: `mac/`, `win/` directories for platform-specific resources
- **Test resources**: `resources/` directory containing HTML test pages

## Development Workflow

### Building
CEFClient is typically built as part of the larger CEF build process. The CMake configuration is generated from `CMakeLists.txt.in` during the CEF build.

### Testing
The application includes numerous built-in tests accessible through the Tests menu:
- Binding tests (JavaScript-C++ communication)
- Download and upload functionality
- Dialog handling
- Performance measurements
- Scheme handlers
- Window management

### Debugging
- OSR (Off-Screen Rendering) debugging tools for windowless scenarios
- DevTools integration for web content debugging
- Platform-specific debugging helpers

## Code Patterns

### Handler Implementation
Most functionality is implemented through CEF handler interfaces. The pattern is:
1. Inherit from appropriate CEF handler interface
2. Implement required methods
3. Register handler with CEF client

### Multi-Process Communication
- Uses CEF's process message system for browser/renderer communication
- Shared memory regions for efficient data transfer
- Process isolation for security and stability

### Resource Management
- RAII patterns with CefRefPtr for CEF objects
- Platform-specific resource cleanup
- Careful attention to object lifetime across process boundaries

## Important Notes

- **Thread safety**: CEF has strict threading requirements - many operations must occur on specific threads
- **Process boundaries**: Be aware of multi-process architecture when designing features
- **Platform differences**: Implementations vary significantly between platforms, especially for UI integration
- **CEF API versions**: Code uses conditional compilation for different CEF API versions

When modifying this codebase, always consider the multi-platform nature and ensure changes work across all supported platforms and CEF configurations.