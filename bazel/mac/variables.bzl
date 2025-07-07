# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

MACOS_DEPLOYMENT_TARGET="12.0"
MACOS_BUNDLE_ID_BASE="org.cef"
CEF_FRAMEWORK_NAME="Chromium Embedded Framework"

#
# Common 'linkopts' for macos_application targets.
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
# Common 'copts' for cc_libary, objc_library and macos_application targets.
# We include defines in 'copts' because objc_library does not support
# 'local_defines'. See https://github.com/bazelbuild/bazel/issues/17482.
#

COMMON_COPTS = [
    "-Wno-undefined-var-template",
    "-Wno-missing-field-initializers",
    "-Wno-deprecated-copy",

    # Used by apps to test if the sandbox is enabled
    "-DCEF_USE_SANDBOX",
]

COMMON_COPTS_DEBUG = [
]

COMMON_COPTS_RELEASE = [
    # Not a debug build
    "-DNDEBUG",
]
