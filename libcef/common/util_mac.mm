// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2011 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/common/util_mac.h"

#include "libcef/common/cef_switches.h"

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"

namespace util_mac {

namespace {

// Returns the path to the Frameworks directory inside the top-level app bundle.
base::FilePath GetFrameworksPath() {
  base::FilePath bundle_path = GetMainBundlePath();
  if (bundle_path.empty()) {
    return base::FilePath();
  }

  return bundle_path.Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("Frameworks"));
}

void OverrideFrameworkBundlePath() {
  base::FilePath framework_path = GetFrameworkDirectory();
  DCHECK(!framework_path.empty());

  base::apple::SetOverrideFrameworkBundlePath(framework_path);
}

void OverrideOuterBundlePath() {
  base::FilePath bundle_path = GetMainBundlePath();
  DCHECK(!bundle_path.empty());

  base::apple::SetOverrideOuterBundlePath(bundle_path);
}

void OverrideBaseBundleID() {
  std::string bundle_id = GetMainBundleID();
  DCHECK(!bundle_id.empty());

  base::apple::SetBaseBundleID(bundle_id.c_str());
}

base::FilePath GetNormalChildProcessPath() {
  base::FilePath frameworks_path = GetFrameworksPath();
  if (frameworks_path.empty()) {
    return base::FilePath();
  }

  std::string exe_name = GetMainProcessPath().BaseName().value();
  return frameworks_path.Append(FILE_PATH_LITERAL(exe_name + " Helper.app"))
      .Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("MacOS"))
      .Append(FILE_PATH_LITERAL(exe_name + " Helper"));
}

void OverrideChildProcessPath() {
  base::FilePath child_process_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kBrowserSubprocessPath);

  if (child_process_path.empty()) {
    child_process_path = GetNormalChildProcessPath();
    CHECK(!child_process_path.empty());
  }

  // Used by ChildProcessHost::GetChildPath and PlatformCrashpadInitialization.
  base::PathService::OverrideAndCreateIfNeeded(
      content::CHILD_PROCESS_EXE, child_process_path, /*is_absolute=*/true,
      /*create=*/false);
}

}  // namespace

bool GetLocalLibraryDirectory(base::FilePath* result) {
  return base::apple::GetLocalDirectory(NSLibraryDirectory, result);
}

base::FilePath::StringType GetFrameworkName() {
  return FILE_PATH_LITERAL("Chromium Embedded Framework");
}

base::FilePath GetFrameworkDirectory() {
  base::FilePath frameworks_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kFrameworkDirPath);
  if (!frameworks_path.empty()) {
    return frameworks_path;
  }

  frameworks_path = GetFrameworksPath();
  if (frameworks_path.empty()) {
    return base::FilePath();
  }

  return frameworks_path.Append(GetFrameworkName())
      .AddExtension(FILE_PATH_LITERAL(".framework"));
}

base::FilePath GetFrameworkResourcesDirectory() {
  base::FilePath frameworks_path = GetFrameworkDirectory();
  if (frameworks_path.empty()) {
    return base::FilePath();
  }

  return frameworks_path.Append(FILE_PATH_LITERAL("Resources"));
}

base::FilePath GetMainProcessPath() {
  base::FilePath path;
  base::PathService::Get(base::FILE_EXE, &path);
  DCHECK(!path.empty());
  return path;
}

base::FilePath GetMainBundlePath() {
  base::FilePath main_bundle_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kMainBundlePath);
  if (!main_bundle_path.empty()) {
    return main_bundle_path;
  }

  return base::apple::GetAppBundlePath(GetMainProcessPath());
}

std::string GetMainBundleID() {
  NSBundle* bundle = base::apple::OuterBundle();
  return base::SysNSStringToUTF8([bundle bundleIdentifier]);
}

base::FilePath GetMainResourcesDirectory() {
  base::FilePath bundle_path = GetMainBundlePath();
  if (bundle_path.empty()) {
    return base::FilePath();
  }

  return bundle_path.Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("Resources"));
}

void PreSandboxStartup() {
  OverrideChildProcessPath();
}

void BasicStartupComplete() {
  OverrideFrameworkBundlePath();
  OverrideOuterBundlePath();
  OverrideBaseBundleID();
}

}  // namespace util_mac
