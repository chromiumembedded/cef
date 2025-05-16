// Copyright 2025 The Chromium Embedded Framework Authors. Portions Copyright
// 2018 the Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include <dlfcn.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include <stdio.h>

#include <memory>
#include <sstream>

#include "include/cef_sandbox_mac.h"

namespace {

// Relative path to the library from the Helper executable.
constexpr char kLibraryPath[] =
    "../../../Chromium Embedded Framework.framework/"
    "Libraries/libcef_sandbox.dylib";

std::string GetLibraryPath() {
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

  // Append the relative path to the library.
  std::stringstream ss;
  ss << parent_dir << "/" << kLibraryPath;
  return ss.str();
}

void* LoadLibrary(const char* path) {
  void* handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL | RTLD_FIRST);
  if (!handle) {
    fprintf(stderr, "dlopen %s: %s\n", path, dlerror());
    return nullptr;
  }
  return handle;
}

void UnloadLibrary(void* handle) {
  // Returns 0 on success.
  if (dlclose(handle)) {
    fprintf(stderr, "dlclose: %s\n", dlerror());
  }
}

void* GetLibraryPtr(void* handle, const char* name) {
  void* ptr = dlsym(handle, name);
  if (!ptr) {
    fprintf(stderr, "dlsym: %s\n", dlerror());
  }
  return ptr;
}

void* SandboxInitialize(void* handle, int argc, char** argv) {
  if (auto* ptr = (decltype(&cef_sandbox_initialize))GetLibraryPtr(
          handle, "cef_sandbox_initialize")) {
    return ptr(argc, argv);
  }
  return nullptr;
}

void SandboxDestroy(void* handle, void* sandbox_context) {
  if (auto* ptr = (decltype(&cef_sandbox_destroy))GetLibraryPtr(
          handle, "cef_sandbox_destroy")) {
    ptr(sandbox_context);
  }
}

}  // namespace

CefScopedSandboxContext::CefScopedSandboxContext() = default;

CefScopedSandboxContext::~CefScopedSandboxContext() {
  if (library_handle_) {
    if (sandbox_context_) {
      SandboxDestroy(library_handle_, sandbox_context_);
    }
    UnloadLibrary(library_handle_);
  }
}

bool CefScopedSandboxContext::Initialize(int argc, char** argv) {
  if (!library_handle_) {
    const std::string& library_path = GetLibraryPath();
    if (library_path.empty()) {
      fprintf(stderr, "App does not have the expected bundle structure.\n");
      return false;
    }
    library_handle_ = LoadLibrary(library_path.c_str());
  }
  if (!library_handle_ || sandbox_context_) {
    return false;
  }
  sandbox_context_ = SandboxInitialize(library_handle_, argc, argv);
  return !!sandbox_context_;
}
