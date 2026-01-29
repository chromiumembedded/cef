This page describes how to update CEF to use the newest Chromium revision.

**Contents**

- [Overview](#overview)
- [Recommended workflow](#recommended-workflow)
  - [1\. Setup a local developer checkout](#1-setup-a-local-developer-checkout)
  - [2\. Check for changes in Chromium's build requirements](#2-check-for-changes-in-chromiums-build-requirements)
  - [3\. Start the update](#3-start-the-update)
    - [A\. Identify the target Chromium version](#a-identify-the-target-chromium-version)
    - [B\. Run CEF's `patch_updater.py` script](#b-run-cefs-patch_updaterpy-script)
    - [C\. Identify potentially relevant Chromium changes](#c-identify-potentially-relevant-chromium-changes)
    - [D\. Identify problematic patterns in the Chromium source code](#d-identify-problematic-patterns-in-the-chromium-source-code)
  - [4\. Build CEF and fix whatever else is broken](#4-build-cef-and-fix-whatever-else-is-broken)
  - [5\. Run CEF tests](#5-run-cef-tests)
  - [6\. Update the compatible Chromium version](#6-update-the-compatible-chromium-version)
  - [7\. Create and submit a PR](#7-create-and-submit-a-pr)
  - [8\. Test your changes on other supported platforms](#8-test-your-changes-on-other-supported-platforms)
- [Resolving automate_git.py update failures](#resolving-automate_gitpy-update-failures)
- [Resolving Chromium breakage](#resolving-chromium-breakage)

---

# Overview

The Chromium developers work very hard to introduce new features and capabilities as quickly as possible. Consequently, projects like CEF that depend on Chromium must also be updated regularly. The update process can be complicated and must be done carefully to avoid introducing new bugs and breakage. Below are the steps to follow when updating CEF to work with a new Chromium revision.

# Recommended workflow

## 1\. Setup a local developer checkout

Setup a local developer checkout of the CEF/Chromium master branch as described on the [Master Build Quick Start](master_build_quick_start.md) page. The standard `automate-git.py` parameters discussed on that page are represented by the `[...]` placeholder in the below examples.

## 2\. Check for changes in Chromium's build requirements

Chromium's build requirements change over time. You may need to update your build environment before proceeding.

* **Windows**: Visual Studio and Windows SDK requirements are listed [here](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/windows_build_instructions.md#Setting-up-Windows) and [here](https://source.chromium.org/chromium/chromium/src/+/main:build/vs_toolchain.py;l=20;bpv=0). Visual Studio bootstrap installers for specific versions can be found [here](https://learn.microsoft.com/en-us/visualstudio/releases/2022/release-history#fixed-version-bootstrappers).
* **MacOS**: SDK and Xcode version requirements are listed [here](https://source.chromium.org/chromium/chromium/src/+/main:build/config/mac/mac_sdk.gni;l=43?q=mac_sdk_official_version&ss=chromium) and [here](https://source.chromium.org/chromium/chromium/src/+/main:build/mac_toolchain.py;l=44?q=MAC_BINARIES_LABEL&ss=chromium%2Fchromium%2Fsrc). SDK versions can also be mapped to Xcode versions using [this table](https://en.wikipedia.org/wiki/Xcode#Xcode_11.x_-_13.x_(since_SwiftUI_framework)).
* **Linux**: Version and distro requirements are listed [here](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md#System-requirements). Most dependency version issues can be avoided by building with a [sysroot image](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/sysroot.md) by setting `GN_DEFINES=use_sysroot=true`.

If the build requirements have changed the [MasterBuildQuickStart page](branches_and_building.md#development) will also need to be updated.

## 3\. Start the update

### A\. Identify the target Chromium version

The Chromium [release cycle](https://chromium.googlesource.com/chromium/src/+/master/docs/process/release_cycle.md) utilizes milestone versions (M98, M99, etc.) and [release channels](https://www.chromium.org/getting-involved/dev-channel/) (canary, beta, stable). New milestone versions branch approximately every 4 weeks and begin life in the canary channel. Over the next 4 weeks they progress through the beta and stable channels to reach Chrome's ~3 billion users globally.

The CEF project has a `master` branch for ongoing development and creates a [release branch](branches_and_building.md#release-branches) that tracks each milestone version. Automated builds are provided for [currently supported](branches_and_building.md#current-release-branches-supported) versions.

* **CEF master updates** target new milestone versions as they are announced to the chromium-dev mailing list. For example, [this email](https://groups.google.com/a/chromium.org/g/chromium-dev/c/uuVm_MltNY4/m/6GO-cpseAQAJ) was sent for the M100 branch. These versions should have 0 as the last component (e.g. "100.0.4896.0"). Updates are performed manually and usually take around 1 week to complete. The "Branch" date for upcoming milestones can be found on the [Chromium schedule](https://chromiumdash.appspot.com/schedule).
* **CEF beta and stable channel updates** use the version information published [here](https://chromiumdash.appspot.com/releases?platform=Windows). These versions should not have 0 as the last component (e.g. "99.0.4844.45"). Updates are performed automatically in most cases and a PR will be submitted to the public repository if the build succeeds.

To manually specify the Chromium version and update the checkout of master:

```sh
python automate-git.py [...] --no-build --fast-update --log-chromium-changes --chromium-checkout=refs/tags/100.0.4896.0
```

To manually specify the Chromium version and update the checkout of an existing release branch:

```sh
python automate-git.py [...] --no-build --fast-update --force-cef-update --branch=4844 --chromium-checkout=refs/tags/99.0.4844.45
```

### B\. Run CEF's `patch_updater.py` script

This will update the Chromium patch files in the `patch/patches` directory. If patch files cannot be updated automatically the `automate-git.py` script will fail.

Claude Code can assist with updating patches. See [tools/claude/README.md](https://github.com/chromiumembedded/cef/blob/master/tools/claude/).

**If one or more patch files have unresolved conflicts you will get a failure like the following:**

```
--------------------------------------------------------------------------------
!!!! FAILED PATCHES, fix manually and run with --resave
content_2015:
  Hunk #4 FAILED at 4220.
  1 out of 5 hunks FAILED -- saving rejects to file content/browser/frame_host/render_frame_host_impl.cc.rej
storage_partition_1973:
  Hunk #1 FAILED at 167.
  1 out of 1 hunk FAILED -- saving rejects to file content/browser/shared_worker/shared_worker_service_impl.cc.rej
--------------------------------------------------------------------------------
```

Use a text editor to manually fix the specified files, and then re-run the `patch_updater.py` script:

```sh
$ cd /path/to/chromium_git/chromium/src/cef/tools
$ python patch_updater.py
```

**If a patched file is missing (moved or deleted) you will get a failure like the following:**

```cpp
--> Reading patch file /path/to/chromium_git/chromium/src/cef/patch/patches/content_mojo_3123.patch
--> Skipping non-existing file /path/to/chromium_git/chromium/src/content/public/browser/document_service_base.h
--> Applying patch to /path/to/chromium_git/chromium/src
(Stripping trailing CRs from patch; use --binary to disable.)
can't find file to patch at input line 5
...
Skip this patch? [y] 
Skipping patch.
2 out of 2 hunks ignored
--> Saving changes to /path/to/chromium_git/chromium/src/cef/patch/patches/content_mojo_3123.patch
Traceback (most recent call last):
  File "/path/to/chromium_git/chromium/src/cef/tools/patch_updater.py", line 294, in <module>
    raise Exception('Failed to add paths: %s' % result['err'])
Exception: Failed to add paths: fatal: pathspec '/path/to/chromium_git/chromium/src/content/public/browser/document_service_base.h' did not match any files

```

You can use this Git command to discover what happened to the missing Chromium file:

```sh
$ cd /path/to/chromium_git/chromium/src
$ git log --full-history -1 -- content/public/browser/document_service_base.h
```

Once you know the offending Git commit hash you can use the `git show <hash>` command or load `https://crrev.com/<hash>` in a web browser to see the contents of the change. Edit the patch file manually in a text editor to fix the paths and then re-run the `patch_updater.py` script as shown above.

**To create a new patch file use this command:**

```sh
$ cd /path/to/chromium_git/chromium/src
$ git diff --no-prefix --relative [path1] [path2] > [name].patch
```

Copy the resulting `[name].patch` file to the `src/cef/patch/patches` directory and add an appropriate entry to the `src/cef/patch/patch.cfg` file.

**To add additional files to an existing patch file use this command:**

```sh
$ cd /path/to/chromium_git/chromium/src/cef/tools
$ python patch_updater.py --resave --patch [name] --add [path1] --add [path2]
```

All paths are relative to the `src` directory by default.

**After all patch files have been fixed you can re-run `automate-git.py` with the additional `--resave` command-line flag to resave the patch files and continue the update process:**

```sh
python automate-git.py [...] --resave
```

### C\. Identify potentially relevant Chromium changes

This step creates a `chromium_update_changes.diff` file in your download directory that will act as your guide when updating the CEF source code. CEF began life as a customized version of content\_shell and there's still a one-to-one relationship between many of the files. The list of relevant paths is taken from CEF's [CHROMIUM_UPDATE.txt](https://github.com/chromiumembedded/cef/blob/master/CHROMIUM_UPDATE.txt) file.

### D\. Identify problematic patterns in the Chromium source code

This step looks for patterns in Chromium src files that may cause issues for CEF. If these patterns are found then the `automate-git.py` script will create a `chromium_update_patterns.txt` file in your download directory and fail with output like the following:

```
Evaluating pattern: static_cast<StoragePartitionImpl\*>(
ERROR Matches found. See chromium_update_changes.diff for output.
```

In that case the contents of `chromium_update_patterns.txt` might look like this:

```text
!!!! WARNING: FOUND PATTERN: static_cast<StoragePartitionImpl\*>(
New instances in non-test files should be converted to call StoragePartition methods.
See storage_partition_1973.patch.

content/browser/loader/navigation_url_loader_impl.cc:1295:  auto* partition = static_cast<StoragePartitionImpl*>(storage_partition);
content/browser/renderer_interface_binders.cc:189:        static_cast<StoragePartitionImpl*>(host->GetStoragePartition())
```

Use the process described in part B to fix these failures.

## 4\. Build CEF and fix whatever else is broken

The diff file created in step C above will assist you in this process. Refer to the [Master Build Quick Start](master_build_quick_start.md) page for build instructions. If Chromium changes are required refer to the "Resolving Chromium breakage" section below.

Claude Code can assist with fixing compile/build errors. See [tools/claude/README.md](https://github.com/chromiumembedded/cef/blob/master/tools/claude/).

## 5\. Run CEF tests

After updating Chromium you should run some tests to verify that everything is working as expected. The tests should eventually be run on all supported platforms.

A\. The `cefclient` sample application supports various runtime modes that should be tested for startup/shutdown crashes or strange runtime behaviors (on MacOS use `open cefclient.app --args <args>`):

```sh
# Chrome bootstrap:
$ cefclient

# Chrome bootstrap + Alloy style:
$ cefclient --use-alloy-style
$ cefclient --use-alloy-style --use-native
$ cefclient --off-screen-rendering-enabled

# Multi-threaded message loop (Windows/Linux only):
$ cefclient --multi-threaded-message-loop
$ cefclient --use-alloy-style --multi-threaded-message-loop

# Chrome style + native parent (Windows/Linux only):
$ cefclient --use-native
```

B\. Run the various manual tests available via the Tests menu in `cefclient` to verify that everything still works. If any particular subsystem had significant changes (like printing, for example) then make sure to give that subsystem additional testing.

C\. Unit tests included in the `ceftests` application are expected to pass on all development platforms.

To run unit tests:
```sh
# Test Chrome style (default)
$ ceftests --use-views

# Test Alloy style
$ ceftests --use-views --use-alloy-style
```

On macOS use `./ceftests.app/Contents/MacOS/ceftests` as the test executable.

Some tests require synthetic events (mouse/key input, focus/activation, fullscreen, etc), device access, or timing considerations that are known to be flaky on headless/CI systems. To disable these tests during automated test runs add the `--gtest_filter=-[test:...]` command-line argument shown below.

On Windows:

```
:: Disabled tests:
:: - AudioOutputTest.*: Flaky
:: - DisplayTest.AutoResize: Flaky
:: - DisplayTest.Title: Flaky
:: - DraggableRegionsTest.*: Flaky
:: - NavigationTest.History: Flaky
:: - NavigationTest.Load*CtrlLeftClick*: Flaky
:: - NavigationTest.Load*MiddleClick*: Flaky
:: - PdfViewerTest.*: Flaky
:: - ScopedTempDir.TempDir: Flaky
:: - ViewsButtonTest.*Click*: Flaky
:: - ViewsButtonTest.MenuButtonCustomPopup*: Flaky
:: - ViewsTextfieldTest.TextfieldKeyEvent: Flaky
:: - ViewsWindowTest.WindowCreateWithOrigin: Flaky
ceftests --use-views [--use-alloy-style] ^
  --gtest_filter=-AudioOutputTest.*:DisplayTest.AutoResize:DisplayTest.Title:DraggableRegionsTest.*:NavigationTest.History:NavigationTest.Load*CtrlLeftClick*:NavigationTest.Load*MiddleClick*:PdfViewerTest.*:ScopedTempDir.TempDir:ViewsButtonTest.*Click*:ViewsButtonTest.MenuButtonCustomPopup*:ViewsTextfieldTest.TextfieldKeyEvent:ViewsWindowTest.WindowCreateWithOrigin
```

On macOS:

```sh
# Disabled tests:
# - DisplayTest.AutoResize: Flaky
# - ViewsButtonTest.MenuButtonCustomPopupActivate: Flaky
# - ViewsWindowTest.Window*Fullscreen*: Fails/crashes on headless agents
./ceftests.app/Contents/MacOS/ceftests --use-views [--use-alloy-style] \
  --gtest_filter=-DisplayTest.AutoResize:ViewsButtonTest.MenuButtonCustomPopupActivate:ViewsWindowTest.Window\*Fullscreen\*
```

On Linux:

```sh
# Headless agents often run using `xvfb-run`.
xvfb-run -a ceftests --use-views [--use-alloy-style] \

# Docker containers may not support the Chromium sandbox.
  --no-sandbox \

# Docker containers often default to 64MB for /dev/shm, which is far too small for Chromium.
# Run docker with `--shm-size=2g` or add `--disable-dev-shm-usage` to increase available space.
  --disable-dev-shm-usage \

# Disabled tests:
# - OSRTest.*: Flaky
# - ViewsWindowTest.WindowMaximize*: Flaky
# - ViewsWindowTest.WindowMinimize*: Flaky
  --gtest_filter=-OSRTest.*:ViewsWindowTest.WindowMaximize*:ViewsWindowTest.WindowMinimize*
```

If automated test runs are slow (tests timing out) you can additionally add `--test-timeout-multiplier=3` for e.g. 3x longer test timeouts.

## 6\. Update the compatible Chromium version

Update the compatible Chromium version listed in CEF's [CHROMIUM_BUILD_COMPATIBILITY.txt](https://github.com/chromiumembedded/cef/blob/master/CHROMIUM_BUILD_COMPATIBILITY.txt) file.

## 7\. Create and submit a PR

Create a commit with your changes and submit a PR as described on the [Contributing With Git](contributing_with_git.md) page. See the [CEF commit history](https://github.com/chromiumembedded/cef/commits/master) for examples of the expected commit message format.

## 8\. Test your changes on other supported platforms

You can update, build and run tests automatically on those platforms with the following command:

```sh
python automate-git.py [...] --run-tests --build-failure-limit=1000 --fast-update --force-cef-update
```

If running in headless mode on Linux install the `xvfb` package and add the `--test-prefix=xvfb-run --test-args="--no-sandbox"` command-line flags.

Access to automated builders as described on the [Automated Build Setup](automated_build_setup.md) page is recommended for this step.

# Resolving automate_git.py update failures

The automate_git.py script can sometimes fail to update an existing Chromium checkout due to errors while running Chromium tooling (git, gclient, etc). When that occurs you can perform the update steps manually.

The following example assumes the usual [file structure](master_build_quick_start.md#file-structure) with `path/to/chromium_git` as your existing automate_git.py download directory.

```sh
# Check out the required Chromium version.
$ cd path/to/chromium_git/chromium/src
$ git checkout refs/tags/100.0.4896.0

# Copy the `cef` directory to `chromium/src/cef` if it doesn't already exist.
$ cd path/to/chromium_git
$ cp -R cef chromium/src/

# Windows only. Apply the patch for `runhooks`.
$ cd path/to/chromium_git/chromium/src/cef/tools
$ python patch_updater.py --reapply --patch runhooks

# Windows only. Don't expect VS to be installed via depot_tools.
$ set DEPOT_TOOLS_WIN_TOOLCHAIN=0

# Update Chromium third_party dependencies and execute runhooks.
$ cd path/to/chromium_git/chromium/src
$ gclient sync --with_branch_heads
```

You can then run `cef_create_projects.[bat|sh]` and `ninja` to build CEF in the usual way.

# Resolving Chromium breakage

In most cases (say, 90% of the time) any code breakage will be due to naming changes, minor code reorganization and/or project name/location changes. The remaining 10% can require pretty significant changes to CEF, usually due to the ongoing refactoring in Chromium code. If you identify a change to Chromium that has broken a required feature for CEF, and you can't work around the breakage by making reasonable changes to CEF, then you should work with the Chromium team to resolve the problem.

1\. Identify the specific Chromium revision that broke the feature and make sure you understand why the change was made.

2\. Post a message to the chromium-dev mailing list explaining why the change broke CEF and either seeking additional information or suggesting a fix that works for both CEF and Chromium.

3\. After feedback from the Chromium developers create an issue in the [Chromium issue tracker](https://crbug.com) and a [code review](http://www.chromium.org/developers/contributing-code) with the fix and publish it with the appropriate (responsible) developer(s) as reviewers.

4\. Follow through with the Chromium developer(s) to get the code review committed.

The CEF build currently contains a patch capability that should be used only as a last resort or as a stop-gap measure if you expect the code review to take a while. The best course of action is always to get your Chromium changes accepted into the Chromium trunk if possible.