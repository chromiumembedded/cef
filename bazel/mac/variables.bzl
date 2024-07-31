# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

MACOS_DEPLOYMENT_TARGET="10.15"
MACOS_BUNDLE_ID_BASE="org.cef"
CEF_FRAMEWORK_NAME="Chromium Embedded Framework"

#
# Common 'linkopts' for cc_binary targets.
#

# Standard link frameworks.
STANDARD_FRAMEWORKS = [
    "AppKit",
]

COMMON_LINKOPTS_DEBUG = [
]

COMMON_LINKOPTS_RELEASE = [
]

COMMON_LINKOPTS = [
    "-framework {}".format(lib) for lib in STANDARD_FRAMEWORKS
]  + select({
    "@cef//:macos_dbg": COMMON_LINKOPTS_DEBUG,
    "//conditions:default": COMMON_LINKOPTS_RELEASE,
})

#
# Common 'copts' for cc_libary and cc_binary targets.
#

COMMON_COPTS = [
    "-Wno-undefined-var-template",
    "-Wno-missing-field-initializers",
    "-Wno-deprecated-copy",
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
