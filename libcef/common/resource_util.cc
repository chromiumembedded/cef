// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2011 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "cef/libcef/common/resource_util.h"

#if BUILDFLAG(IS_LINUX)
#include <dlfcn.h>
#endif

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"

#if BUILDFLAG(IS_LINUX)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "cef/libcef/common/util_mac.h"
#endif

namespace resource_util {

namespace {

#if BUILDFLAG(IS_LINUX)

// Based on chrome/common/chrome_paths_linux.cc.
// See http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
// for a spec on where config files go. The net result on most systems is that
// we use "~/.config/cef_user_data".
bool GetDefaultUserDataDirectory(base::FilePath* result) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath config_dir(base::nix::GetXDGDirectory(
      env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir));
  *result = config_dir.Append(FILE_PATH_LITERAL("cef_user_data"));
  return true;
}

#elif BUILDFLAG(IS_MAC)

// Based on chrome/common/chrome_paths_mac.mm.
bool GetDefaultUserDataDirectory(base::FilePath* result) {
  if (!base::PathService::Get(base::DIR_APP_DATA, result)) {
    return false;
  }
  *result = result->Append(FILE_PATH_LITERAL("CEF"));
  *result = result->Append(FILE_PATH_LITERAL("User Data"));
  return true;
}

#elif BUILDFLAG(IS_WIN)

// Based on chrome/common/chrome_paths_win.cc.
bool GetDefaultUserDataDirectory(base::FilePath* result) {
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, result)) {
    return false;
  }
  *result = result->Append(FILE_PATH_LITERAL("CEF"));
  *result = result->Append(FILE_PATH_LITERAL("User Data"));
  return true;
}

#endif

base::FilePath GetUserDataPath(CefSettings* settings,
                               const base::CommandLine* command_line) {
  // |settings| will be non-nullptr in the main process only.
  if (settings) {
    CefString root_cache_path;
    if (settings->root_cache_path.length > 0) {
      root_cache_path = CefString(&settings->root_cache_path);
    }
    if (!root_cache_path.empty()) {
      return base::FilePath(root_cache_path);
    } else {
      LOG(WARNING) << "Please customize CefSettings.root_cache_path for your "
                      "application. Use of the default value may lead to "
                      "unintended process singleton behavior.";
    }
  }

  // This may be set for sub-processes.
  base::FilePath result =
      command_line->GetSwitchValuePath(switches::kUserDataDir);
  if (!result.empty()) {
    return result;
  }

  if (GetDefaultUserDataDirectory(&result)) {
    return result;
  }

  if (base::PathService::Get(base::DIR_TEMP, &result)) {
    return result;
  }

  DCHECK(false);
  return result;
}

}  // namespace

#if BUILDFLAG(IS_MAC)

base::FilePath GetResourcesDir() {
  return util_mac::GetFrameworkResourcesDirectory();
}

#else  // !BUILDFLAG(IS_MAC)

base::FilePath GetResourcesDir() {
  base::FilePath pak_dir;
  base::PathService::Get(base::DIR_ASSETS, &pak_dir);
  return pak_dir;
}

#endif  // !BUILDFLAG(IS_MAC)

void OverrideUserDataDir(CefSettings* settings,
                         const base::CommandLine* command_line) {
  const base::FilePath& user_data_path =
      GetUserDataPath(settings, command_line);
  base::PathService::Override(chrome::DIR_USER_DATA, user_data_path);

  // Path used for crash dumps.
  base::PathService::Override(chrome::DIR_CRASH_DUMPS, user_data_path);

  // Path used for spell checking dictionary files.
  base::PathService::OverrideAndCreateIfNeeded(
      chrome::DIR_APP_DICTIONARIES,
      user_data_path.Append(FILE_PATH_LITERAL("Dictionaries")),
      false,  // May not be an absolute path.
      true);  // Create if necessary.
}

#if BUILDFLAG(IS_LINUX)
void OverrideAssetPath() {
  Dl_info dl_info;
  if (dladdr(reinterpret_cast<const void*>(&OverrideAssetPath), &dl_info)) {
    base::FilePath path = base::FilePath(dl_info.dli_fname).DirName();
    base::PathService::Override(base::DIR_ASSETS, path);
  }
}
#endif

}  // namespace resource_util
