# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

#
# Distribution DLLs.
#

DLLS = [
    "chrome_elf.dll",
    "d3dcompiler_47.dll",
    "libcef.dll",
    "libEGL.dll",
    "libGLESv2.dll",
    "vk_swiftshader.dll",
    "vulkan-1.dll",
]

DLLS_X64 = [
    "dxil.dll",
    "dxcompiler.dll",
]

#
# Common 'linkopts' for cc_binary targets.
#

# Windows delayload DLLs.
# Delayload most libraries as the DLLs are simply not required at startup (or
# at all, depending on the process type). Some dlls open handles when they are
# loaded, and we may not want them to be loaded in renderers or other sandboxed
# processes. Conversely, some DLLs must be loaded before sandbox lockdown. In
# unsandboxed processes they will load when first needed. The linker will
# automatically ignore anything which is not linked to the binary at all (it is
# harmless to have an unmatched /delayload). Lists should be kept in sync with
# targets from Chromium's //build/config/win/BUILD.gn file.
DELAYLOAD_DLLS = [
    # Required to support CefScopedLibraryLoader.
    "libcef.dll",

    # "delayloads" target.
    "api-ms-win-core-winrt-error-l1-1-0.dll",
    "api-ms-win-core-winrt-l1-1-0.dll",
    "api-ms-win-core-winrt-string-l1-1-0.dll",
    "advapi32.dll",
    "comctl32.dll",
    "comdlg32.dll",
    "credui.dll",
    "cryptui.dll",
    "d3d11.dll",
    "d3d9.dll",
    "dwmapi.dll",
    "dxgi.dll",
    "dxva2.dll",
    "esent.dll",
    "gdi32.dll",
    "hid.dll",
    "imagehlp.dll",
    "imm32.dll",
    "msi.dll",
    "netapi32.dll",
    "ncrypt.dll",
    "ole32.dll",
    "oleacc.dll",
    "propsys.dll",
    "psapi.dll",
    "rpcrt4.dll",
    "rstrtmgr.dll",
    "setupapi.dll",
    "shell32.dll",
    "shlwapi.dll",
    "uiautomationcore.dll",
    "urlmon.dll",
    "user32.dll",
    "usp10.dll",
    "uxtheme.dll",
    "wer.dll",
    "wevtapi.dll",
    "wininet.dll",
    "winusb.dll",
    "wsock32.dll",
    "wtsapi32.dll",

    # "delayloads_not_for_child_dll" target.
    "crypt32.dll",
    "dbghelp.dll",
    "dhcpcsvc.dll",
    "dwrite.dll",
    "iphlpapi.dll",
    "oleaut32.dll",
    "secur32.dll",
    "userenv.dll",
    "winhttp.dll",
    "winmm.dll",
    "winspool.drv",
    "wintrust.dll",
    "ws2_32.dll",
]

# Standard link libraries.
STANDARD_LIBS = [
    "comctl32.lib",
    "crypt32.lib",
    "delayimp.lib",
    "gdi32.lib",
    "rpcrt4.lib",
    "shlwapi.lib",
    "user32.lib",
    "wintrust.lib",
    "ws2_32.lib",
]

COMMON_LINKOPTS_DEBUG = [
]

COMMON_LINKOPTS_RELEASE = [
]

COMMON_LINKOPTS = [
    # No default manifest (see compile_rc target).
    "/MANIFEST:NO",
    # Allow 32-bit processes to access 3GB of RAM.
    "/LARGEADDRESSAWARE",
    # Generate Debug information.
    # TODO: Remove after fixing opt builds to work without it.
    "/DEBUG",
] + [
    "/DELAYLOAD:{}".format(dll) for dll in DELAYLOAD_DLLS
] + [
    "/DEFAULTLIB:{}".format(lib) for lib in STANDARD_LIBS
] + select({
    # Set the initial stack size to 0.5MiB, instead of the 1.5MiB minimum
    # needed by CEF's main thread. This saves significant memory on threads
    # (like those in the Windows thread pool, and others) whose stack size we
    # can only control through this setting. The main thread (in 32-bit builds
    # only) uses fibers to switch to a 4MiB stack at runtime via
    # CefRunWinMainWithPreferredStackSize().
    "@cef//:windows_32": ["/STACK:0x80000"],
    # Increase the initial stack size to 8MiB from the default 1MiB.
    "//conditions:default": ["/STACK:0x800000"],
}) + select({
    "@cef//:windows_dbg": COMMON_LINKOPTS_DEBUG,
    "//conditions:default": COMMON_LINKOPTS_RELEASE,
})

#
# Common 'copts' for cc_libary and cc_binary targets.
#

COMMON_COPTS = [
]

COMMON_COPTS_DEBUG = [
]

COMMON_COPTS_RELEASE = [
]

#
# Common 'defines' for cc_libary targets.
#

COMMON_DEFINES = [
    # Windows platform
    "WIN32",
    "_WIN32",
    "_WINDOWS",
    # Unicode build           
    "UNICODE",
    "_UNICODE",
    # Targeting Windows 10. We can't say `=_WIN32_WINNT_WIN10` here because
    # some files do `#if WINVER < 0x0600` without including windows.h before,
    # and then _WIN32_WINNT_WIN10 isn't yet known to be 0x0A00.
    "WINVER=0x0A00",
    "_WIN32_WINNT=0x0A00",
    "NTDDI_VERSION=NTDDI_WIN10_FE",
    # Use the standard's templated min/max
    "NOMINMAX",
    # Exclude less common API declarations
    "WIN32_LEAN_AND_MEAN",
    # Disable exceptions
    "_HAS_EXCEPTIONS=0",
]

COMMON_DEFINES_DEBUG = [
]

COMMON_DEFINES_RELEASE = [
    # Not a debug build
    "NDEBUG",
    "_NDEBUG",
]
