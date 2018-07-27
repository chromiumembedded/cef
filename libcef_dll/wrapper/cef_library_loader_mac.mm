// Copyright (c) 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/wrapper/cef_library_loader.h"

#include <libgen.h>
#include <mach-o/dyld.h>
#include <stdio.h>

#include <memory>
#include <sstream>

namespace {

const char kFrameworkPath[] =
    "Chromium Embedded Framework.framework/Chromium Embedded Framework";
const char kPathFromHelperExe[] = "../../..";
const char kPathFromMainExe[] = "../Frameworks";

std::string GetFrameworkPath(bool helper) {
  uint32_t exec_path_size = 0;
  int rv = _NSGetExecutablePath(NULL, &exec_path_size);
  if (rv != -1) {
    return std::string();
  }

  std::unique_ptr<char[]> exec_path(new char[exec_path_size]);
  rv = _NSGetExecutablePath(exec_path.get(), &exec_path_size);
  if (rv != 0) {
    return std::string();
  }

  // Get the directory path of the executable.
  const char* parent_dir = dirname(exec_path.get());
  if (!parent_dir) {
    return std::string();
  }

  // Append the relative path to the framework.
  std::stringstream ss;
  ss << parent_dir << "/" << (helper ? kPathFromHelperExe : kPathFromMainExe)
     << "/" << kFrameworkPath;
  return ss.str();
}

}  // namespace

CefScopedLibraryLoader::CefScopedLibraryLoader() : loaded_(false) {}

bool CefScopedLibraryLoader::Load(bool helper) {
  if (loaded_) {
    return false;
  }

  const std::string& framework_path = GetFrameworkPath(helper);
  if (framework_path.empty()) {
    fprintf(stderr, "App does not have the expected bundle structure.\n");
    return false;
  }

  // Load the CEF framework library.
  if (!cef_load_library(framework_path.c_str())) {
    fprintf(stderr, "Failed to load the CEF framework.\n");
    return false;
  }

  loaded_ = true;
  return true;
}

CefScopedLibraryLoader::~CefScopedLibraryLoader() {
  if (loaded_) {
    // Unload the CEF framework library.
    cef_unload_library();
  }
}
