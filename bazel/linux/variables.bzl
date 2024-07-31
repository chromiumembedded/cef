# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

#
# Distribution SOs.
#

SOS = [
    "libcef.so",
    "libEGL.so",
    "libGLESv2.so",
    "libvk_swiftshader.so",
    "libvulkan.so.1",
]

#
# Common 'linkopts' for cc_binary targets.
#

# Standard link libraries.
STANDARD_LIBS = [
    "X11",
]

COMMON_LINKOPTS_DEBUG = [
]

COMMON_LINKOPTS_RELEASE = [
]

COMMON_LINKOPTS = [
    "-l{}".format(lib) for lib in STANDARD_LIBS
]  + select({
    "@cef//:linux_dbg": COMMON_LINKOPTS_DEBUG,
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
    # Used by apps to test if the sandbox is enabled
    "CEF_USE_SANDBOX",
]

COMMON_DEFINES_DEBUG = [
]

COMMON_DEFINES_RELEASE = [
    # Not a debug build
    "NDEBUG",
]

