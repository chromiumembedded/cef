# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

def _configure_gtk_tests(ctx):
    # Override automatic detection with CEF_TESTS_USE_GTK=ON or OFF.
    mode = ctx.os.environ.get("CEF_TESTS_USE_GTK", "AUTO").upper()
    if mode not in ["AUTO", "ON", "OFF"]:
        fail("CEF_TESTS_USE_GTK must be AUTO, ON, or OFF")
    available = False
    if mode != "OFF":
        pkg_config = ctx.which("pkg-config")
        if pkg_config != None:
            available = ctx.execute([
                pkg_config,
                "--exists",
                "gtk+-3.0",
                "x11",
            ]).return_code == 0
        if mode == "ON" and not available:
            fail("CEF_TESTS_USE_GTK requires the gtk+-3.0 and x11 development packages")
    ctx.file("BUILD.bazel", "exports_files([\"defs.bzl\"])\n")
    ctx.file("defs.bzl", "CEF_TESTS_USE_GTK = {}\n".format(available))

configure_gtk_tests = repository_rule(
    implementation = _configure_gtk_tests,
    local = True,
    configure = True,
    environ = [
        "CEF_TESTS_USE_GTK",
        "PATH",
        "PKG_CONFIG_PATH",
        "PKG_CONFIG_LIBDIR",
        "PKG_CONFIG_SYSROOT_DIR",
    ],
)
