# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

load("@aspect_bazel_lib//lib:copy_file.bzl", "copy_file")
load("//bazel:copy_filegroups.bzl", "copy_filegroups")
load("//bazel/win:mt.bzl", "add_manifest")
load("//bazel/win:rc.bzl", "compile_rc")
load("//bazel/win:variables.bzl",
     "COMMON_LINKOPTS",
     "COMMON_COPTS", "COMMON_COPTS_RELEASE", "COMMON_COPTS_DEBUG",
     "COMMON_DEFINES", "COMMON_DEFINES_RELEASE", "COMMON_DEFINES_DEBUG")
load("@rules_cc//cc:defs.bzl", "cc_binary")

def declare_exe(name, srcs, manifest_srcs, rc_file, resources_srcs, resources_deps=[],
                deps=[], linkopts=[], copts=[], local_defines=[], data=[],
                additional_linker_inputs=[], features=[], **kwargs):
    # Resource file.
    res_target = "{}_res".format(name)
    compile_rc(
        name = res_target,
        rc_file = rc_file,
        srcs = resources_srcs,
        deps = resources_deps,
        out = "{}.res".format(name),
        target_compatible_with = ["@platforms//os:windows"],
    )

    # Copy DLLs and resources into the current project.
    copy_target = "{}_dlls_and_resources".format(name)
    copy_filegroups(
        name = copy_target,
        filegroups = [
            "@cef//:dlls",
            "@cef//:resources",
        ],
        remove_prefixes = [
            "Debug",
            "Release",
            "Resources",
        ],
    )

    # Executable target.
    binary_target = "{}_no_manifest".format(name)
    cc_binary(
        name = binary_target,
        srcs = srcs,
        deps = [
            "@cef//:cef_wrapper",
        ] + deps,
        linkopts = [
            "$(location @cef//:cef_lib)",
            "$(location :{})".format(res_target),
        ] + COMMON_LINKOPTS + linkopts,
        copts = COMMON_COPTS + select({
            "@cef//:windows_dbg": COMMON_COPTS_DEBUG,
            "//conditions:default": COMMON_COPTS_RELEASE,
        }) + copts,
        local_defines = COMMON_DEFINES + select({
            "@cef//:windows_dbg": COMMON_DEFINES_DEBUG,
            "//conditions:default": COMMON_DEFINES_RELEASE,
        }) + local_defines,
        additional_linker_inputs = [
            "@cef//:cef_lib",
            ":{}".format(res_target),
        ] + additional_linker_inputs,
        data = [
            ":{}".format(copy_target),
        ] + data,
        features = ["generate_pdb_file"] + features,
        target_compatible_with = ["@platforms//os:windows"],
        **kwargs
    )

    # Add manifest and rename to final executable.
    add_manifest(
        name = name,
        mt_files = manifest_srcs,
        in_binary = ":{}".format(binary_target),
        out_binary = "{}.exe".format(name),
        target_compatible_with = ["@platforms//os:windows"],
    )

def declare_dll(name, srcs, rc_file, resources_srcs, resources_deps=[],
                deps=[], linkopts=[], copts=[], local_defines=[], data=[],
                additional_linker_inputs=[], features=[], bootstrap_suffix='',
                **kwargs):
    # Resource file.
    res_target = "{}_res".format(name)
    compile_rc(
        name = res_target,
        rc_file = rc_file,
        srcs = resources_srcs,
        deps = resources_deps,
        out = "{}.res".format(name),
        target_compatible_with = ["@platforms//os:windows"],
    )

    # Copy/rename bootstrap[c].exe.
    exe_target = "{}_exe".format(name)
    copy_file(
        name = exe_target,
        src = "@cef//:bootstrap{}.exe".format(bootstrap_suffix),
        out = "{}.exe".format(name),
        is_executable = True,
        allow_symlink = False,
    )

    # Copy DLLs and resources into the current project.
    copy_target = "{}_dlls_and_resources".format(name)
    copy_filegroups(
        name = copy_target,
        filegroups = [
            "@cef//:dlls",
            "@cef//:resources",
        ],
        remove_prefixes = [
            "Debug",
            "Release",
            "Resources",
        ],
    )

    # DLL target.
    cc_binary(
        name = name,
        srcs = srcs,
        deps = [
            "@cef//:cef_wrapper",
        ] + deps,
        linkopts = [
            "$(location @cef//:cef_lib)",
            "$(location :{})".format(res_target),
        ] + COMMON_LINKOPTS + linkopts,
        copts = COMMON_COPTS + select({
            "@cef//:windows_dbg": COMMON_COPTS_DEBUG,
            "//conditions:default": COMMON_COPTS_RELEASE,
        }) + copts,
        local_defines = COMMON_DEFINES + select({
            "@cef//:windows_dbg": COMMON_DEFINES_DEBUG,
            "//conditions:default": COMMON_DEFINES_RELEASE,
        }) + local_defines,
        additional_linker_inputs = [
            "@cef//:cef_lib",
            ":{}".format(res_target),
        ] + additional_linker_inputs,
        data = [
            ":{}".format(exe_target),
            ":{}".format(copy_target),
        ] + data,
        features = ["generate_pdb_file"] + features,
        linkshared = True,  # Create a DLL.
        target_compatible_with = ["@platforms//os:windows"],
        **kwargs
    )
