// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2011 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/common/util_mac.h"

#include "libcef/common/cef_switches.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/path_service.h"

namespace util_mac {

namespace {

// Returns the path to the top-level app bundle that contains the main process
// executable.
base::FilePath GetMainBundlePath() {
  return base::mac::GetAppBundlePath(GetMainProcessPath());
}

// Returns the path to the Frameworks directory inside the top-level app bundle.
base::FilePath GetFrameworksPath() {
  base::FilePath bundle_path = GetMainBundlePath();
  if (bundle_path.empty())
    return base::FilePath();

  return bundle_path.Append(FILE_PATH_LITERAL("Contents"))
                    .Append(FILE_PATH_LITERAL("Frameworks"));
}

}  // namespace

bool GetLocalLibraryDirectory(base::FilePath* result) {
  return base::mac::GetLocalDirectory(NSLibraryDirectory, result);
}

base::FilePath GetFrameworkDirectory() {
  base::FilePath frameworks_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kFrameworkDirPath);
  if (!frameworks_path.empty())
    return frameworks_path;

  frameworks_path = GetFrameworksPath();
  if (frameworks_path.empty())
    return base::FilePath();

  return frameworks_path.Append(
      FILE_PATH_LITERAL("Chromium Embedded Framework.framework"));
}

base::FilePath GetFrameworkResourcesDirectory() {
  base::FilePath frameworks_path = GetFrameworkDirectory();
  if (frameworks_path.empty())
    return base::FilePath();

  return frameworks_path.Append(FILE_PATH_LITERAL("Resources"));
}

base::FilePath GetMainProcessPath() {
  base::FilePath path;
  PathService::Get(base::FILE_EXE, &path);
  DCHECK(!path.empty());
  return path;
}

base::FilePath GetMainResourcesDirectory() {
  base::FilePath bundle_path = GetMainBundlePath();
  if (bundle_path.empty())
    return base::FilePath();

  return bundle_path.Append(FILE_PATH_LITERAL("Contents"))
                    .Append(FILE_PATH_LITERAL("Resources"));
}

base::FilePath GetChildProcessPath() {
  base::FilePath frameworks_path = GetFrameworksPath();
  if (frameworks_path.empty())
    return base::FilePath();

  std::string exe_name = GetMainProcessPath().BaseName().value();
  return frameworks_path.Append(FILE_PATH_LITERAL(exe_name + " Helper.app"))
                        .Append(FILE_PATH_LITERAL("Contents"))
                        .Append(FILE_PATH_LITERAL("MacOS"))
                        .Append(FILE_PATH_LITERAL(exe_name + " Helper"));
}

}  // namespace util_mac
