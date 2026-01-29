This page describes sandbox usage and requirements for CEF.

**Contents**

- [Overview](#overview)
- [Usage](#usage)
  - [Windows](#windows)
  - [macOS](#macos)
  - [Linux](#linux)
- [Building cef_sandbox (Windows/macOS)](#building-cef_sandbox-windowsmacos)
- [Building bootstrap (Windows)](#building-bootstrap-windows)

---

# Overview

The Chromium/CEF [multi-process model](general_usage.md#processes) uses separate processes for the renderer, network stack, GPU, etc. This model allows Chromium to restrict access to system resources based on the sub-process type. For example, a renderer sub-process that executes JavaScript code does not need direct access to the file system or network stack. Allowing unnecessary access can be a security concern if a sub-process is compromised while executing content (e.g. JavaScript code) from a hostile website.

The [Chromium sandbox](https://chromium.googlesource.com/chromium/src/+/master/docs/design/sandbox.md) is used to apply per-process access restrictions based on the process type. With the sandbox enabled a compromised sub-process will often be terminated before it can do any harm. CEF supports the Chromium sandbox on each platform as described below.

# Usage

Sandbox capabilities are OS-specific and different requirements apply to each platform.

## Windows

To enable the sandbox on Windows the following requirements must be met:

1. Use the same executable for the browser process and all sub-processes. The `CefSettings.browser_subprocess_path` setting cannot be used in combination with the sandbox.
2. Link the executable with the `cef_sandbox` static library.
3. Call the `cef_sandbox_info_create()` function from within the executable (not from a separate DLL) and pass the resulting pointer into both the` CefExecutProcess()` and `CefInitialize()` functions via the `windows_sandbox_info` parameter.

**New requirements starting with M138:**

Starting with M138 the `cef_sandbox` static library can only be linked with applications built as part of the CEF/Chromium build. This is due to [unavoidable](https://github.com/chromiumembedded/cef/issues/3824) dependencies on Chromium's bundled Clang/LLVM/libc++ toolchain. CEF provides pre-built bootstrap executables that can be used "out of the box" with minimal client application changes. Usage is as follows:

1. Build your client application as a DLL (`client.dll`) instead of an executable. For windows applications (`/SUBSYSTEM:WINDOWS`) implement/export the `RunWinMain` function and run with `bootstrap.exe`. For console applications (`/SUBSYSTEM:CONSOLE`) implement/export the `RunConsoleMain` function and run with `bootstrapc.exe`. Function signatures are provided in [cef_sandbox_win.h](https://github.com/chromiumembedded/cef/blob/9a115ea48f315887c531fa2988a20ba733977412/include/cef_sandbox_win.h#L98).
2. Run your client application using the provided bootstrap executable (`bootstrap[c].exe --module=client`) or rename `bootstrap[c].exe` to `client.exe` to have it load `client.dll` from the same directory by default. The bootstrap executable will load your client DLL and call the exported function, passing in a `void* sandbox_info` parameter in addition to the normal [wWinMain](https://learn.microsoft.com/en-us/windows/win32/learnwin32/winmain--the-application-entry-point) or [main](https://learn.microsoft.com/en-us/cpp/cpp/main-function-command-line-args?view=msvc-170#the-main-function-signature) parameters. The `sandbox_info` parameter is then forwarded by the client DLL to `CefExecuteProcess` and `CefInitialize` in the usual manner.
3. Optionally customize the bootstrap executable for your client application. For example, resources such as icons, file information and error strings can be modified using [Visual Studio](https://learn.microsoft.com/en-us/cpp/windows/working-with-resource-files?view=msvc-170) or [Resource Hacker](https://www.angusj.com/resourcehacker/) tools ([example](https://github.com/chromiumembedded/cef/issues/3824#issuecomment-2895663576)). Alternately, you can build and modify the bootstrap executables from source code as part of a CEF/Chromium build (see the "Building bootstrap" section below).
4. Be sure to [code sign](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool) all binaries after modification and before distribution to users ([example](https://github.com/chromiumembedded/cef/issues/3824#issuecomment-2892139995)).

If you don't wish to use the provided bootstrap executables you have a few other options:

1. Build your client application as part of the CEF/Chromium build using Chromium's bundled Clang/LLVM/libc++ toolchain. You will need to integrate your custom build into CEF's top-level [BUILD.gn file](https://github.com/chromiumembedded/cef/blob/master/BUILD.gn). See for example the "cefsimple targets" section in that file for how the cefsimple application is built.
2. Build your client application as an executable with the sandbox disabled. This requires no code changes to existing applications beyond passing nullptr as the `windows_sandbox_info` parameter to `CefExecuteProcess` and `CefInitialize`.

See [cef_sandbox_win.h](https://github.com/chromiumembedded/cef/blob/master/include/cef_sandbox_win.h) for additional details. See [cefsimple_win.cc](https://github.com/chromiumembedded/cef/blob/master/tests/cefsimple/cefsimple_win.cc) for an example implementation.

## macOS

To enable the sandbox on macOS the following requirements must be met:

1. Distribute the `libcef_sandbox.dylib` shared library and load dynamically (M138 and newer) or link the helper process executable with the `cef_sandbox` static library (prior to M138).
2. Call the `cef_sandbox_initialize()` function at the beginning of the helper executable `main()` function and before loading the CEF framework library.

See [cef_sandbox_mac.h](https://github.com/chromiumembedded/cef/blob/master/include/cef_sandbox_mac.h) and [cef_library_loader.h](https://github.com/chromiumembedded/cef/blob/master/include/wrapper/cef_library_loader.h) for additional details. See [cefsimple_mac.mm](https://github.com/chromiumembedded/cef/blob/master/tests/cefsimple/cefsimple_mac.mm) and [process_helper_mac.cc](https://github.com/chromiumembedded/cef/blob/master/tests/cefsimple/process_helper_mac.cc) for an example implementation.

The [V2 sandbox](https://chromium.googlesource.com/chromium/src/+/master/sandbox/mac/seatbelt_sandbox_design.md) was enabled starting with 3538 branch. See [this announcement](https://groups.google.com/d/msg/cef-announce/Fith0A3kWtw/6ds_mJVMCQAJ) for a description of the related changes.

## Linux

Different approaches are used based on system capabilities. See [this document](https://chromium.googlesource.com/chromium/src/+/refs/tags/57.0.2956.2/docs/linux_sandboxing.md) for details.

# Building cef_sandbox (Windows/macOS)

**Sandbox requirements have changed starting with M138. See the above Usage section for details.**

In M138 and newer `cef_sandbox` is no longer distributed as a static library. On Windows it has been removed in favor of bootstrap executables. On macOS it has been replaced with `libcef_sandbox.dylib` which is included in the framework bundle `Libraries` directory and loaded via `dlopen()` at runtime. These new binaries use the same output directory as the standard CEF/Chromium build and there is no longer a need to build `cef_sandbox` independently.

**Deprecated build instructions for older versions:**

Prior to M138 linking the `cef_sandbox` static library is required for using the sandbox on Windows and macOS. This library must be built with special configuration settings in order to link successfully with third-party applications. For example, Chromium's custom bundled libc++ version must be disabled to avoid linker conflicts when using the default platform runtime library on Windows.

The `cef_sandbox` library build is usually performed by the `automate_git.py` tool as part of the official binary distribution build (see the [Automated Build Setup](automated_build_setup.md) page). The special sandbox-specific configuration comes from the `GetConfigArgsSandbox` function in the [gn_args.py](https://github.com/chromiumembedded/cef/blob/master/tools/gn_args.py) tool and the sandbox build is performed by `automate_git.py` in a separate `out/[Config]_sandbox` output directory. A final post-processing action by the [make_distrib.py](https://github.com/chromiumembedded/cef/blob/master/tools/make_distrib.py) tool combines multiple lib files into a single `cef_sandbox.lib` (Windows) or `cef_sandbox.a` (macOS) file which is included with the binary distribution.

The `cef_sandbox` library can also be built locally for development and testing purposes as described below. This example creates a build that is compatible with the official ARM64 macOS distribution available for download from [Spotify](https://cef-builds.spotifycdn.com/index.html). To perform this build you must first set up a local Chromium/CEF checkout as described on the [Master Build Quick Start](master_build_quick_start.md) page and check out the correct branch/version by passing the appropriate `--branch=XXXX` or `--checkout=HHHHHHHH` command-line flags to `automate_git.py`. You can then build the `cef_sandbox` library as follows:

```sh
# Discover what `args.gn` contents are required:
cd /path/to/chromium/src/cef
export GN_DEFINES=is_official_build=true
export GN_OUT_CONFIGS=Debug_GN_arm64_sandbox
python3 tools/gn_args.py

# Create the `chromium/src/out/Debug_GN_arm64_sandbox/args.gn` file with the specified contents.

# Build the cef_sandbox target in the new output directory:
cd /path/to/chromium/src
gn gen out/Debug_GN_arm64_sandbox
ninja -C out/Debug_GN_arm64_sandbox cef_sandbox

# Create a binary distribution for the cef_sandbox build:
cd /path/to/chromium/src/cef/tools
./make_distrib.sh --allow-partial --sandbox --ninja-build --arm64-build --no-archive --no-symbols --no-docs
```

The resulting `chromium/src/cef/binary_distrib/cef_binary_*_macosarm64_sandbox/Debug/cef_sandbox.a` file can then be tested in combination with an existing official binary distribution at the same version.

If a different platform or configuration is desired you can find the appropriate `GN_DEFINES` on the [Automated Build Setup](automated_build_setup.md) page.

# Building bootstrap (Windows)

This section describes how to build the bootstrap executables (`bootstrap.exe` and `bootstrapc.exe`) used with M138 and newer. See the Usage section above for background and client integration instructions.

The bootstrap executable build is usually performed by the `automate_git.py` tool as part of the official binary distribution build (see the [Automated Build Setup](automated_build_setup.md) page). It uses the same output directory as the standard CEF/Chromium build.

Code for the bootstrap executables lives in the [cef\libcef_dll\bootstrap](https://github.com/chromiumembedded/cef/tree/master/libcef_dll/bootstrap) directory and can be built locally for development and testing purposes as described below. For example, you might change the default application [resources](https://github.com/chromiumembedded/cef/tree/master/libcef_dll/bootstrap/win) such as icons, file information or error strings. These executables depend only on the `cef_sandbox` and Chromium `//base` targets and run prior to CEF/Chromium initialization. They must therefore comply with the following restrictions:

1. No access to CEF or Chromium APIs that require or trigger global initialization.
2. No access to WinAPI (or similar) functions when executing as a sandboxed sub-process.
3. No new (non-delay-loaded) DLL dependencies that may interfere with sandbox capabilities ([details](https://github.com/chromiumembedded/cef/blob/7581264dbb9a2a38bd7d6635d011050dc695b88f/cmake/cef_variables.cmake.in#L456)).

The following example creates a build that is compatible with the official 64-bit Windows distribution available for download from [Spotify](https://cef-builds.spotifycdn.com/index.html). To perform this build you must first set up a local Chromium/CEF checkout as described on the [Master Build Quick Start](master_build_quick_start.md) page and check out the correct branch/version by passing the appropriate `--branch=XXXX` or `--checkout=HHHHHHHH` command-line flags to `automate_git.py`. You can then build the bootstrap binaries as follows:

```sh
# Discover what `args.gn` contents are required:
cd c:\path\to\chromium\src\cef
set GN_DEFINES=is_official_build=true
set GN_OUT_CONFIGS=Debug_GN_x64
python3 tools\gn_args.py

# Create or update the `chromium\src\out\Debug_GN_x64\args.gn` file with the specified contents.

# Build the bootstrap targets in the output directory:
cd c:\path\to\chromium\src
set DEPOT_TOOLS_WIN_TOOLCHAIN=0
gn gen out\Debug_GN_x64
ninja -C out\Debug_GN_x64 bootstrap bootstrapc

# Create a binary distribution for the bootstrap build:
cd c:\path\to\chromium\src\cef\tools
make_distrib.bat --allow-partial --sandbox --ninja-build --x64-build --no-archive --no-symbols --no-docs
```

The resulting `chromium\src\cef\binary_distrib\cef_binary_*_windows64_sandbox\Debug\bootstrap.exe` (or `bootstrapc.exe`) file can then be tested in combination with an existing official binary distribution at the same version.

If a different platform or configuration is desired you can find the appropriate `GN_DEFINES` on the [Automated Build Setup](automated_build_setup.md) page.

If you don't wish to invalidate an existing CEF/Chromium build you can optionally create the bootstrap build using a custom output directory (e.g. replace `out\Debug_GN_x64` with `out\Debug_GN_x64_bootstrap`). However, you will then need to manually copy the bootstrap executables as the last step instead of running `make_distrib.bat`.