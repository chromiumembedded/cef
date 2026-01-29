# CEF Client Development Guide for Claude Code

This guide covers using Claude Code to implement features in CEF applications using binary distributions.

***
[TOC]
***

## Overview

This workflow is for implementing features in CEF applications (like cefsimple, cefclient) using pre-built CEF binary distributions. This is **not** for updating CEF to new Chromium versions (see [CHROMIUM_UPDATE.md](CHROMIUM_UPDATE.md) for that).

**Use this guide when you want to:**

- Add custom URL schemes to cefsimple
- Implement JavaScript-to-C++ communication
- Customize popup windows
- Intercept network requests
- Add any feature to a CEF-based application

## Prerequisites

- CEF binary distribution ([download here](https://cef-builds.spotifycdn.com/index.html))
- C++ compiler:
    - Windows: Visual Studio 2022
    - macOS: Xcode with command-line tools
    - Linux: GCC/Clang, build-essential, and libgtk-3-dev packages
- CMake 3.19+
- **Optional:** CEF wiki repository cloned locally

## Setup

**1. Extract binary distribution:**

```bash
tar -xjf cef_binary_<version>_<platform>.tar.bz2
cd cef_binary_<version>_<platform>
```

**2. (Optional) Clone CEF wiki documentation:**

```bash
# Wiki content is now in docs/ directory
```

This gives Claude direct access to documentation via the Read tool.

## Quick Start

**Example task:** "Add a custom 'myapp' URL scheme to cefsimple"

**Prompt Claude:**

```
I have a CEF binary distribution at /path/to/cef_binary_<version>_<platform>.

Please add a custom "myapp://" URL scheme to cefsimple that serves
HTML content from C++.

Use the instructions in @cef/tools/claude/CLAUDE_CLIENT_INSTRUCTIONS.md

The CEF wiki is available at /path/to/cef_wiki (if cloned locally)
```

**Claude will:**

1. Read [CLAUDE_CLIENT_INSTRUCTIONS.md](CLAUDE_CLIENT_INSTRUCTIONS.md) to understand the environment
2. Consult GeneralUsage.md (via Read tool or WebFetch) for scheme handler documentation
3. Identify which files to modify (`simple_app.h/cc`)
4. Implement `OnRegisterCustomSchemes()`, `CefResourceHandler`, and `CefSchemeHandlerFactory`
5. Build the project
6. Ask you to test the application with `--url=myapp://test`
7. Wait for your feedback before proceeding

## Example Prompts

### Add JavaScript-to-C++ Communication

```
I have a CEF binary distribution at /path/to/cef_binary_<version>_macosarm64.

Please add a JavaScript function window.sendToApp(message) to cefsimple that
sends messages to C++ and displays them via LOG().

Use @cef/tools/claude/CLAUDE_CLIENT_INSTRUCTIONS.md for guidance.
```

### Customize Popup Windows

```
I have a CEF binary distribution at /path/to/cef_binary_<version>_windows64.

Please make cefsimple popup windows open at 600x400 instead of the default size.

Follow the instructions in @cef/tools/claude/CLAUDE_CLIENT_INSTRUCTIONS.md
```

### Intercept Network Requests

```
I have a CEF binary distribution at /path/to/cef_binary_<version>_linux64.

Please modify cefsimple to intercept all image requests and log their URLs to
the console.

Use @cef/tools/claude/CLAUDE_CLIENT_INSTRUCTIONS.md for guidance.
```

### Implement Custom Scheme Handler

```
I have a CEF binary distribution at /path/to/cef_binary_<version>_<platform>.

Please add support for a custom "myscheme://" protocol to cefsimple that
serves static HTML pages from an in-memory map.

Use @cef/tools/claude/CLAUDE_CLIENT_INSTRUCTIONS.md for guidance.
```

## Workflow

1. **Claude reads instructions** - [CLAUDE_CLIENT_INSTRUCTIONS.md](CLAUDE_CLIENT_INSTRUCTIONS.md) provides environment context
2. **Claude researches** - Consults wiki documentation for implementation details
3. **Claude implements** - Modifies cefsimple source files incrementally
4. **Claude builds** - Verifies compilation succeeds
5. **You test** - Claude cannot run GUI applications, asks you to test
6. **Iterate** - Based on your feedback, Claude fixes issues and continues

## Important Notes

- **Claude cannot test:** GUI applications require human testing. Claude will ask you to run the app and report results.
- **Build verification only:** Claude verifies code compiles but cannot verify functionality.
- **Documentation access:** Providing local wiki clone (`cef_wiki/`) improves efficiency vs. WebFetch.
- **Platform-specific:** Specify your platform (Windows/Linux/macOS) and architecture (x64/arm64) for accurate build commands.

## Common Build Commands

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

## Getting Help

### For Issues with Implementation

- Check [CLAUDE_CLIENT_INSTRUCTIONS.md](CLAUDE_CLIENT_INSTRUCTIONS.md) for environment details
- Consult CEF wiki (GeneralUsage.md, Tutorial.md, HandsOnTutorial.md)
- Look at cefclient source code in `tests/cefclient/` for examples

### For Issues with CEF API

- CEF wiki: https://github.com/chromiumembedded/cef/blob/master/docs/
- CEF forums: https://magpcss.org/ceforum/
- CEF issue tracker: https://github.com/chromiumembedded/cef/issues

### For Issues with Claude Code

- Claude Code documentation: https://docs.claude.com/claude-code
- Improve prompts with more context and specific examples
- Break large tasks into smaller, more manageable pieces
