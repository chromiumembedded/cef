// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>

#include "include/base/cef_logging.h"
#include "include/wrapper/cef_certificate_util_win.h"
#include "include/wrapper/cef_library_loader.h"
#include "include/wrapper/cef_util_win.h"

namespace {

bool IsEqualIgnoringCase(const std::wstring& str1, const std::wstring& str2) {
  return std::equal(
      str1.begin(), str1.end(), str2.begin(), str2.end(),
      [](wchar_t a, wchar_t b) { return std::tolower(a) == std::tolower(b); });
}

HMODULE Load(const std::wstring& dll_path,
             const char* thumbprint,
             bool allow_unsigned,
             bool is_subprocess) {
  if (!is_subprocess) {
    // Load the client DLL as untrusted (e.g. without executing DllMain or
    // loading additional modules) so that we can first check requirements.
    // LoadLibrary's "default search order" is tricky and we don't want to guess
    // about what DLL it will load. DONT_RESOLVE_DLL_REFERENCES is the only
    // option that doesn't execute DllMain while still allowing us retrieve the
    // path using GetModuleFileName. No execution of the DLL should be attempted
    // while loaded in this mode.
    if (HMODULE hModule = ::LoadLibraryEx(
            dll_path.data(), nullptr,
            DONT_RESOLVE_DLL_REFERENCES | LOAD_WITH_ALTERED_SEARCH_PATH)) {
      const auto& module_path = cef_util::GetModulePath(hModule);
      if (!IsEqualIgnoringCase(module_path, dll_path)) {
        LOG(FATAL) << "Found libcef.dll at unexpected path";
      }

      // Generate a FATAL error and crash if validation fails.
      cef_certificate_util::ValidateCodeSigningAssert(dll_path, thumbprint,
                                                      allow_unsigned);

      FreeLibrary(hModule);
    } else {
      LOG(FATAL) << "Failed to load libcef.dll";
    }
  }

  // Load normally.
  if (HMODULE hModule = ::LoadLibraryEx(dll_path.data(), nullptr,
                                        LOAD_WITH_ALTERED_SEARCH_PATH)) {
    return hModule;
  }

  LOG(FATAL) << "Failed to load libcef.dll";
  return nullptr;
}

}  // namespace

CefScopedLibraryLoader::CefScopedLibraryLoader() = default;

bool CefScopedLibraryLoader::LoadInMainAssert(const wchar_t* dll_path,
                                              const char* thumbprint,
                                              bool allow_unsigned) {
  CHECK(!handle_);
  handle_ = Load(dll_path, thumbprint, allow_unsigned, false);
  return handle_ != nullptr;
}

bool CefScopedLibraryLoader::LoadInSubProcessAssert() {
  CHECK(!handle_);

  constexpr wchar_t kProcessTypeW[] = L"type";
  const auto& command_line =
      cef_util::ParseCommandLineArgs(::GetCommandLineW());
  if (command_line.size() <= 1) {
    return false;
  }
  if (cef_util::GetCommandLineValue(command_line, kProcessTypeW).empty()) {
    return false;
  }

  const auto& dll_path =
      cef_util::GetCommandLineValue(command_line, switches::kLibcefPathW);
  if (dll_path.empty()) {
    // Default is libcef.dll in the same directory the executable and loaded by
    // the delayload helper.
    return true;
  }

  handle_ = Load(dll_path, nullptr, true, true);
  return handle_ != nullptr;
}

CefScopedLibraryLoader::~CefScopedLibraryLoader() {
  if (handle_) {
    ::FreeLibrary(handle_);
  }
}
