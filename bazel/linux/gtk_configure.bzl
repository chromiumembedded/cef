# Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

def _configure_gtk_tests(ctx):
    # Keep sample/test X11 support independent of GTK and libcef capabilities.
    x11_mode = ctx.os.environ.get("CEF_TESTS_USE_X11", "AUTO").upper()
    if x11_mode not in ["AUTO", "ON", "OFF"]:
        fail("CEF_TESTS_USE_X11 must be AUTO, ON, or OFF")
    # Resolve from this package to avoid loading the root BUILD, which imports
    # variables.bzl and therefore depends on this repository's generated defs.
    cef_root = ctx.path(Label("//bazel/linux:gtk_configure.bzl")).dirname.dirname.dirname
    use_x11 = "#define CEF_X11 1" in ctx.read(cef_root.get_child("include/cef_config.h"))
    if x11_mode != "AUTO":
        use_x11 = x11_mode == "ON"
    # Override automatic detection with CEF_TESTS_USE_GTK=ON or OFF.
    mode = ctx.os.environ.get("CEF_TESTS_USE_GTK", "AUTO").upper()
    if mode not in ["AUTO", "ON", "OFF"]:
        fail("CEF_TESTS_USE_GTK must be AUTO, ON, or OFF")
    available = False
    if mode != "OFF":
        pkg_config = ctx.which("pkg-config")
        if pkg_config != None:
            packages = ["gtk+-3.0"] + (["x11"] if use_x11 else [])
            available = ctx.execute([
                pkg_config,
                "--exists",
            ] + packages).return_code == 0
        if mode == "ON" and not available:
            fail("CEF_TESTS_USE_GTK requires GTK development packages and, when CEF_TESTS_USE_X11 is enabled, x11")
    ctx.file("BUILD.bazel", "exports_files([\"defs.bzl\"])\n")
    ctx.file("defs.bzl", "CEF_TESTS_USE_GTK = {}\nCEF_TESTS_USE_X11 = {}\n".format(available, use_x11))

configure_gtk_tests = repository_rule(
    implementation = _configure_gtk_tests,
    local = True,
    configure = True,
    environ = [
        "CEF_TESTS_USE_X11",
        "CEF_TESTS_USE_GTK",
        "PATH",
        "PKG_CONFIG_PATH",
        "PKG_CONFIG_LIBDIR",
        "PKG_CONFIG_SYSROOT_DIR",
    ],
)
