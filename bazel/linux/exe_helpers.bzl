# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

load("//bazel:copy_filegroups.bzl", "copy_filegroups")
load("//bazel/linux:fix_rpath.bzl", "fix_rpath")
load("//bazel/linux:variables.bzl",
     "COMMON_LINKOPTS",
     "COMMON_COPTS", "COMMON_COPTS_RELEASE", "COMMON_COPTS_DEBUG",
     "COMMON_DEFINES", "COMMON_DEFINES_RELEASE", "COMMON_DEFINES_DEBUG")
load("@rules_cc//cc:defs.bzl", "cc_binary")

def declare_exe(name, srcs=[], deps=[], linkopts=[], copts=[], local_defines=[], data=[], **kwargs):
    # Copy SOs and resources into the current project.
    copy_target = "{}_sos_and_resources".format(name)
    copy_filegroups(
        name = copy_target,
        filegroups = [
            "@cef//:sos",
            "@cef//:resources",
        ],
        remove_prefixes = [
            "Debug",
            "Release",
            "Resources",
        ],
    )

    # Executable target.
    binary_target = "{}_incorrect_rpath".format(name)
    cc_binary(
        name = binary_target,
        srcs = srcs,
        deps = [
            "@cef//:cef_wrapper",
            "@cef//:cef",
            "@cef//:cef_sandbox",
        ] + deps,
        linkopts = COMMON_LINKOPTS + linkopts,
        copts = COMMON_COPTS + select({
            "@cef//:linux_dbg": COMMON_COPTS_DEBUG,
            "//conditions:default": COMMON_COPTS_RELEASE,
        }) + copts,
        local_defines = COMMON_DEFINES + select({
            "@cef//:linux_dbg": COMMON_DEFINES_DEBUG,
            "//conditions:default": COMMON_DEFINES_RELEASE,
        }) + local_defines,
        data = [
            ":{}".format(copy_target),
        ] + data,
        target_compatible_with = ["@platforms//os:linux"],
        **kwargs
    )

    # Set rpath to $ORIGIN so that libraries can be loaded from next to the
    # executable.
    fix_rpath(
        name = "{}_fixed_rpath".format(name),
        src = ":{}".format(binary_target),
        out = name,
        target_compatible_with = ["@platforms//os:linux"],
    )

