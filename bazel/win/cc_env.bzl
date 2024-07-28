# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain", _use_cpp_toolchain="use_cpp_toolchain")
load("@rules_cc//cc:action_names.bzl", "CPP_COMPILE_ACTION_NAME")

# Since we need windows.h and other headers, we should ensure we have the same
# development environment as a regular cl.exe call. So use the current toolchain
# to grab environment variables to feed into the actual rc.exe call
# Much of this is taken from:
#   https://github.com/bazelbuild/rules_cc/blob/main/examples/my_c_archive/my_c_archive.bzl
def collect_compilation_env(ctx):
    cc_toolchain = find_cpp_toolchain(ctx)
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = ctx.disabled_features,
    )

    compiler_variables = cc_common.create_compile_variables(
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
    )

    return cc_common.get_environment_variables(
      feature_configuration = feature_configuration,
      action_name = CPP_COMPILE_ACTION_NAME,
      variables = compiler_variables,
    )

use_cpp_toolchain=_use_cpp_toolchain
