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
        deps = deps,
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

def _macos_app_with_cef_framework_impl(ctx):
    """Add versioned CEF framework directory with symlinks in app bundle."""
    base_app = ctx.files.base_app[0]
    final_app = ctx.actions.declare_directory(ctx.attr.name + ".app")

    script = ctx.actions.declare_file("fix_cef_symlinks.sh")
    ctx.actions.write(
        output = script,
        content = """#!/bin/bash
set -euo pipefail

BASE_APP="{base_app}"
FINAL_APP="{final_app}"
CEF_SRC="{cef_src}"
CEF_FRAMEWORK_NAME="{cef_framework_name}"

# Extract the base app bundle (it's a zip file).
TEMP_DIR="$(dirname "$FINAL_APP")/temp_extract"
mkdir -p "$TEMP_DIR"
unzip -q "$BASE_APP" -d "$TEMP_DIR"
# Find the .app directory that was extracted and move its contents.
EXTRACTED_APP=$(find "$TEMP_DIR" -name "*.app" -type d | head -1)
mkdir -p "$FINAL_APP"
cp -R "$EXTRACTED_APP"/* "$FINAL_APP"/
rm -rf "$TEMP_DIR"

# Copy the CEF framework to the versioned directory in the app bundle.
FRAMEWORK_DIR="$FINAL_APP/Contents/Frameworks/$CEF_FRAMEWORK_NAME.framework"
mkdir -p "$FRAMEWORK_DIR/Versions"
cp -R -L "$CEF_SRC" "$FRAMEWORK_DIR/Versions/A"

# Create the necessary symlinks.
cd "$FRAMEWORK_DIR"
ln -sf "Versions/A/Chromium Embedded Framework" "Chromium Embedded Framework"
ln -sf "Versions/A/Libraries" "Libraries"
ln -sf "Versions/A/Resources" "Resources"
cd "Versions"
ln -sf "A" "Current"
        """.format(
            base_app = base_app.path,
            final_app = final_app.path,
            cef_src = ctx.files.cef_framework[0].path,
            cef_framework_name = ctx.attr.cef_framework_name,
        ),
        is_executable = True,
    )

    ctx.actions.run(
        inputs = [base_app] + ctx.files.cef_framework + [script],
        outputs = [final_app],
        executable = script,
        mnemonic = "AddCEFFrameworkWithSymlinks",
        progress_message = "Add CEF framework with symlinks in app bundle",
    )

    # Create a launcher script for use by bazel run.
    launcher = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.write(
        output = launcher,
        content = """#!/bin/bash
# App bundle is in the same directory as this script.
SCRIPT_DIR="$(dirname "$0")"
APP_PATH="$SCRIPT_DIR/{}.app"

if [[ -d "$APP_PATH" ]]; then
    open "$APP_PATH"
else
    echo "Error: App bundle not found at $APP_PATH"
    exit 1
fi
""".format(ctx.label.name),
        is_executable = True,
    )

    return [DefaultInfo(files = depset([final_app]), executable = launcher)]

_macos_app_with_cef_framework = rule(
    implementation = _macos_app_with_cef_framework_impl,
    executable = True,
    attrs = {
        "base_app": attr.label(
            mandatory = True,
            allow_files = True,
            doc = "The base app bundle to post-process",
        ),
        "cef_framework": attr.label(
            mandatory = True,
            allow_files = True,
            doc = "The CEF framework source",
        ),
        "cef_framework_name": attr.string(
            mandatory = True,
            doc = "Name of the CEF framework",
        ),
    },
    doc = "Post-processes app bundle to fix CEF framework symlinks",
)

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

    # Create base app bundle without CEF framework directory.
    macos_application(
        name = name + "_base",
        additional_contents = {
            ":HelperBase": "Frameworks",
            ":HelperAlerts": "Frameworks",
            ":HelperGPU": "Frameworks",
            ":HelperPlugin": "Frameworks",
            ":HelperRenderer": "Frameworks",
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

    # Post-process to add versioned CEF framework directory with symlinks.
    _macos_app_with_cef_framework(
        name = name,
        base_app = ":" + name + "_base",
        cef_framework = "@cef//:cef_framework",
        cef_framework_name = CEF_FRAMEWORK_NAME,
    )
