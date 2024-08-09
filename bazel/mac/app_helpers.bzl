# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

load("@bazel_skylib//rules:expand_template.bzl", "expand_template")
load("@build_bazel_rules_apple//apple:macos.bzl", "macos_application")
load("//bazel:variables.bzl", "VERSION_PLIST")
load("//bazel/mac:variables.bzl",
     "MACOS_DEPLOYMENT_TARGET",
     "MACOS_BUNDLE_ID_BASE",
     "CEF_FRAMEWORK_NAME",
     "COMMON_LINKOPTS")

def _declare_helper_app(name, info_plist, deps, helper_base_name, helper_suffix, **kwargs):
    """
    Creates a Helper .app target.
    """
    helper_name = "{} Helper".format(name)
    bundle_id_suffix = ""

    if helper_suffix:
        helper_name += " ({})".format(helper_suffix)
        bundle_id_suffix += ".{}".format(helper_suffix.lower())

    # Helper app bundle Info.plist.
    expand_template(
        name = "{}_InfoPList".format(helper_base_name),
        template = info_plist,
        out = "{}Info.plist".format(helper_base_name),
        substitutions = {
            "${EXECUTABLE_NAME}": helper_name,
            "${PRODUCT_NAME}": name,
            "${BUNDLE_ID_SUFFIX}": bundle_id_suffix,
            "${VERSION_SHORT}": VERSION_PLIST,
            "${VERSION_LONG}": VERSION_PLIST,
        },
    )

    # Helper app bundle.
    macos_application(
        name = helper_base_name,
        bundle_name = helper_name,
        bundle_id = "{}.{}.helper{}".format(MACOS_BUNDLE_ID_BASE, name.lower(), bundle_id_suffix),
        infoplists = [":{}_InfoPList".format(helper_base_name)],
        minimum_os_version = MACOS_DEPLOYMENT_TARGET,
        deps = [
            "@cef//:cef_sandbox",
        ] + deps,
        **kwargs,
    )

HELPERS = {
    "HelperBase": "",
    "HelperAlerts": "Alerts",
    "HelperGPU": "GPU",
    "HelperPlugin": "Plugin",
    "HelperRenderer": "Renderer",
}

def declare_all_helper_apps(name, info_plist, deps, **kwargs):
    """
    Creates all Helper .app targets.
    """
    [_declare_helper_app(
        name = name,
        info_plist = info_plist,
        deps = deps,
        helper_base_name = h,
        helper_suffix = v,
        **kwargs,
    ) for h, v in HELPERS.items()]

def declare_main_app(name, info_plist, deps, resources, linkopts=[], **kwargs):
    """
    Creates the main .app target.
    """

    # Main app bundle Info.plist.
    expand_template(
        name = "InfoPList",
        template = info_plist,
        out = "Info.plist",
        substitutions = {
            "${EXECUTABLE_NAME}": name,
            "${PRODUCT_NAME}": name,
            "${VERSION_SHORT}": VERSION_PLIST,
            "${VERSION_LONG}": VERSION_PLIST,
        },
    )

    # Main app bindle.
    macos_application(
        name = name,
        additional_contents = {
            ":HelperBase": "Frameworks",
            ":HelperAlerts": "Frameworks",
            ":HelperGPU": "Frameworks",
            ":HelperPlugin": "Frameworks",
            ":HelperRenderer": "Frameworks",
            "@cef//:cef_framework": "Frameworks/{}.framework".format(CEF_FRAMEWORK_NAME),
        },
        bundle_name = name,
        bundle_id = "{}.{}".format(MACOS_BUNDLE_ID_BASE, name.lower()),
        infoplists = [":InfoPList"],
        linkopts = COMMON_LINKOPTS + linkopts,
        minimum_os_version = MACOS_DEPLOYMENT_TARGET,
        resources = resources,
        target_compatible_with = [
            "@platforms//os:macos",
        ],
        deps = deps,
        **kwargs,
    )
