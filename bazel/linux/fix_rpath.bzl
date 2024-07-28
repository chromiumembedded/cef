# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

def _fix_rpath_impl(ctx):
    inputs = ctx.runfiles(files = [ctx.file.src])
    # Bring over 'data' dependencies from the input.
    inputs = inputs.merge_all([ctx.attr.src[DefaultInfo].default_runfiles])

    src = ctx.file.src.path
    out = ctx.outputs.out.path

    ctx.actions.run_shell(
        outputs = [ctx.outputs.out],
        inputs = inputs.files,
        arguments = [src, out],
        command = "cp $1 $2 && " +
                  "chmod +w $2 && " +
                  "patchelf --remove-rpath $2 && " +
                  "patchelf --set-rpath '$ORIGIN' $2"
    )

    return [DefaultInfo(files = depset([ctx.outputs.out]))]

# Set rpath to $ORIGIN so that libraries can be loaded from next to the
# executable. The result can be confirmed with:
# $ objdump -x ./bazel-bin/path/to/binary | grep 'R.*PATH'
#
# Alternatively, define a custom CC toolchain that overrides
# 'runtime_library_search_directories'.
#
# This rule requires preinstallation of the patchelf package:
# $ sudo apt install patchelf
fix_rpath = rule(
    implementation = _fix_rpath_impl,
    attrs = {
        "src": attr.label(allow_single_file = True),
        "out": attr.output(mandatory = True),
    },
)

