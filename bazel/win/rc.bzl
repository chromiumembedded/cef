# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

load("//bazel/win:cc_env.bzl", "collect_compilation_env", "use_cpp_toolchain")

def _compile_rc_impl(ctx):
    rc_file = ctx.file.rc_file
    output = ctx.outputs.out

    inputs = [rc_file] + ctx.files.srcs
    includes = ["/i{}/{}".format(ctx.label.package, i) for i in ctx.attr.includes]

    # Grab all include paths/files required for the run
    for dep in ctx.attr.deps:
      comp_ctx = dep[CcInfo].compilation_context

      includes += ["/i{}".format(i) for i in comp_ctx.quote_includes.to_list()]
      includes += ["/i{}".format(i) for i in comp_ctx.system_includes.to_list()]
      inputs += comp_ctx.headers.to_list()

    ctx.actions.run(
        executable = ctx.executable._tool,
        inputs = inputs,
        outputs = [output],
        env = collect_compilation_env(ctx),
        arguments = includes + ["/fo", output.path, rc_file.path],
        mnemonic = "CompileRC"
    )

    return DefaultInfo(files = depset([output]))

compile_rc = rule(
    implementation = _compile_rc_impl,
    attrs = {
        "rc_file": attr.label(allow_single_file = [".rc"]),
        "srcs": attr.label_list(allow_files = True),
        "deps": attr.label_list(providers = [CcInfo]),
        "includes": attr.string_list(),
        "out": attr.output(),
        "_cc_toolchain": attr.label(default = Label("@bazel_tools//tools/cpp:current_cc_toolchain")),
        "_tool": attr.label(
          default = "@winsdk//:rc_pybin",
          executable = True,
          cfg = "exec"
        )
    },
    fragments = ["cpp"],
    toolchains = use_cpp_toolchain(),
)
