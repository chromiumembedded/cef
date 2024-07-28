# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

load("//bazel/win:cc_env.bzl", "collect_compilation_env", "use_cpp_toolchain")

# Copy exe and pdb file without tracking the destination as an output.
# Based on https://github.com/bazelbuild/bazel-skylib/blob/main/rules/private/copy_file_private.bzl
def _write_copy_cmd(ctx, src, dst):
    # Most Windows binaries built with MSVC use a certain argument quoting
    # scheme. Bazel uses that scheme too to quote arguments. However,
    # cmd.exe uses different semantics, so Bazel's quoting is wrong here.
    # To fix that we write the command to a .bat file so no command line
    # quoting or escaping is required.
    bat = ctx.actions.declare_file(ctx.label.name + "-cmd.bat")
    src_path = src.path.replace("/", "\\")
    dst_path = dst.path.replace("/", "\\")
    ctx.actions.write(
        output = bat,
        # Do not use lib/shell.bzl's shell.quote() method, because that uses
        # Bash quoting syntax, which is different from cmd.exe's syntax.
        content = "@copy /Y \"%s\" \"%s\" >NUL\n@copy /Y \"%s\" \"%s\" >NUL" % (
            src_path,
            dst_path,
            src_path.replace(".exe", ".pdb"),
            dst_path.replace(".exe", ".pdb"),
        ),
        is_executable = True,
    )
    return bat

def _add_mt_impl(ctx):
    mt_files = ctx.files.mt_files
    input = ctx.attr.in_binary[DebugPackageInfo].unstripped_file
    output = ctx.outputs.out_binary
    bat = _write_copy_cmd(ctx, input, output)

    inputs = mt_files + [input, bat]

    # Bring over 'data' dependencies from the input.
    deps_inputs =  ctx.runfiles(files = inputs)
    deps_inputs = deps_inputs.merge_all([ctx.attr.in_binary[DefaultInfo].default_runfiles])

    ctx.actions.run(
        executable = ctx.executable._tool,
        inputs = deps_inputs.files,
        outputs = [output],
        env = collect_compilation_env(ctx),
        # The bat file will be executed before the tool command.
        arguments = [bat.path, "-nologo", "-manifest"] + [f.path for f in mt_files] +
                    ["-outputresource:{}".format(output.path)],
        mnemonic = "AddMT"
    )

    return DefaultInfo(files = depset([output]))

add_manifest = rule(
    implementation = _add_mt_impl,
    attrs = {
        "mt_files": attr.label_list(allow_files = [".manifest"]),
        "in_binary": attr.label(providers = [CcInfo], allow_single_file = True),
        "out_binary": attr.output(),
        "_cc_toolchain": attr.label(default = Label("@bazel_tools//tools/cpp:current_cc_toolchain")),
        "_tool": attr.label(
          default = "@winsdk//:mt_pybin",
          executable = True,
          cfg = "exec"
        )
    },
    fragments = ["cpp"],
    toolchains = use_cpp_toolchain(),
)
