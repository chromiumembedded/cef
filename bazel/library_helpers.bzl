# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

load("//bazel/win:variables.bzl",
     WIN_COMMON_COPTS="COMMON_COPTS",
     WIN_COMMON_COPTS_RELEASE="COMMON_COPTS_RELEASE",
     WIN_COMMON_COPTS_DEBUG="COMMON_COPTS_DEBUG",
     WIN_COMMON_DEFINES="COMMON_DEFINES",
     WIN_COMMON_DEFINES_RELEASE="COMMON_DEFINES_RELEASE",
     WIN_COMMON_DEFINES_DEBUG="COMMON_DEFINES_DEBUG")
load("//bazel/linux:variables.bzl",
     LINUX_COMMON_COPTS="COMMON_COPTS",
     LINUX_COMMON_COPTS_RELEASE="COMMON_COPTS_RELEASE",
     LINUX_COMMON_COPTS_DEBUG="COMMON_COPTS_DEBUG",
     LINUX_COMMON_DEFINES="COMMON_DEFINES",
     LINUX_COMMON_DEFINES_RELEASE="COMMON_DEFINES_RELEASE",
     LINUX_COMMON_DEFINES_DEBUG="COMMON_DEFINES_DEBUG")
load("//bazel/mac:variables.bzl",
     MAC_COMMON_COPTS="COMMON_COPTS",
     MAC_COMMON_COPTS_RELEASE="COMMON_COPTS_RELEASE",
     MAC_COMMON_COPTS_DEBUG="COMMON_COPTS_DEBUG")
load("@rules_cc//cc:defs.bzl", "cc_library", "objc_library")

def declare_cc_library(copts=[], local_defines=[], **kwargs):
    """
    cc_library wrapper that applies common copts and local_defines.
    """
    # NOTE: objc_library does not support local_defines on MacOS, so on
    # that platform we put the defines in copts instead.
    cc_library(
        copts = select({
            "@platforms//os:windows": WIN_COMMON_COPTS,
            "@platforms//os:linux": LINUX_COMMON_COPTS,
            "@platforms//os:macos": MAC_COMMON_COPTS,
            "//conditions:default": None,
        }) + select({
            "@cef//:windows_opt": WIN_COMMON_COPTS_RELEASE,
            "@cef//:windows_dbg": WIN_COMMON_COPTS_DEBUG,
            "@cef//:windows_fastbuild": WIN_COMMON_COPTS_RELEASE,
            "@cef//:linux_opt": LINUX_COMMON_COPTS_RELEASE,
            "@cef//:linux_dbg": LINUX_COMMON_COPTS_DEBUG,
            "@cef//:linux_fastbuild": LINUX_COMMON_COPTS_RELEASE,
            "@cef//:macos_opt": MAC_COMMON_COPTS_RELEASE,
            "@cef//:macos_dbg": MAC_COMMON_COPTS_DEBUG,
            "@cef//:macos_fastbuild": MAC_COMMON_COPTS_RELEASE,
            "//conditions:default": None,
        }) + copts,
        local_defines = select({
            "@platforms//os:windows": WIN_COMMON_DEFINES,
            "@platforms//os:linux": LINUX_COMMON_DEFINES,
            "//conditions:default": None,
        }) + select({
            "@cef//:windows_opt": WIN_COMMON_DEFINES_RELEASE,
            "@cef//:windows_dbg": WIN_COMMON_DEFINES_DEBUG,
            "@cef//:windows_fastbuild": WIN_COMMON_DEFINES_RELEASE,
            "@cef//:linux_opt": LINUX_COMMON_DEFINES_RELEASE,
            "@cef//:linux_dbg": LINUX_COMMON_DEFINES_DEBUG,
            "@cef//:linux_fastbuild": LINUX_COMMON_DEFINES_RELEASE,
            "//conditions:default": None,
        }) + local_defines,
        **kwargs
    )

def declare_objc_library(copts=[], **kwargs):
    """
    objc_library wrapper that applies common copts.
    """
    # NOTE: objc_library does not support local_defines on MacOS, so on
    # that platform we put the defines in copts instead.
    objc_library(
        copts = select({
            "@platforms//os:windows": WIN_COMMON_COPTS,
            "@platforms//os:linux": LINUX_COMMON_COPTS,
            "@platforms//os:macos": MAC_COMMON_COPTS,
            "//conditions:default": None,
        }) + select({
            "@cef//:windows_opt": WIN_COMMON_COPTS_RELEASE,
            "@cef//:windows_dbg": WIN_COMMON_COPTS_DEBUG,
            "@cef//:windows_fastbuild": WIN_COMMON_COPTS_RELEASE,
            "@cef//:linux_opt": LINUX_COMMON_COPTS_RELEASE,
            "@cef//:linux_dbg": LINUX_COMMON_COPTS_DEBUG,
            "@cef//:linux_fastbuild": LINUX_COMMON_COPTS_RELEASE,
            "@cef//:macos_opt": MAC_COMMON_COPTS_RELEASE,
            "@cef//:macos_dbg": MAC_COMMON_COPTS_DEBUG,
            "@cef//:macos_fastbuild": MAC_COMMON_COPTS_RELEASE,
            "//conditions:default": None,
        }) + copts,
        **kwargs
    )
