# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

def _copy_filegroups_impl(ctx):
    inputs = ctx.files.filegroups
    remove_prefixes = ctx.attr.remove_prefixes
    add_prefix = ctx.attr.add_prefix

    outputs = []
    for f in inputs:
        relative_path = f.path
        if relative_path.startswith("external/"):
          # Remove the "external/<repo>" component, if any.
          relative_path = "/".join(relative_path.split("/")[2:])

        for prefix in remove_prefixes:
            # Add trailing forward slash if necessary.
            if prefix[-1] != "/":
                prefix += "/"
            if len(prefix) > 0 and relative_path.startswith(prefix):
                relative_path = relative_path[len(prefix):]
                break

        if len(add_prefix) > 0:
            # Add trailing forward slash if necessary.
            if add_prefix[-1] != "/":
                add_prefix += "/"
            relative_path = add_prefix + relative_path

        out = ctx.actions.declare_file(relative_path)
        outputs.append(out)

        if relative_path.find("/") > 0:
            command="mkdir -p $(dirname {}) && cp {} {}".format(out.path, f.path, out.path)
        else:
            command="cp {} {}".format(f.path, out.path)

        ctx.actions.run_shell(
            outputs=[out],
            inputs=depset([f]),
            command=command
        )

    # Small sanity check
    if len(inputs) != len(outputs):
        fail("Output count should be 1-to-1 with input count.")

    return DefaultInfo(
        files=depset(outputs),
        runfiles=ctx.runfiles(files=outputs)
    )

# Allows the file contents of |filegroups| to be copied next to a cc_binary
# target via the |data| attribute.
# Implementation based on https://stackoverflow.com/a/57983629
copy_filegroups = rule(
    implementation=_copy_filegroups_impl,
    attrs={
        "filegroups": attr.label_list(),
        "remove_prefixes": attr.string_list(default = []),
        "add_prefix": attr.string(default = ""),
    },
)

