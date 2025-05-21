// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/bootstrap_util_win.h"

#include "base/check_op.h"
#include "base/command_line.h"

namespace bootstrap_util {

namespace {

// Changes to these values require rebuilding libcef.dll.
constexpr wchar_t kWindowsSelfName[] = TEXT("bootstrap");
constexpr wchar_t kConsoleSelfName[] = TEXT("bootstrapc");

// Returns the file name only without extension, if any.
inline std::wstring NamePart(const base::FilePath& path) {
  return path.BaseName().RemoveExtension().value();
}

}  // namespace

bool IsDefaultExeName(const std::wstring& name) {
  return base::FilePath::CompareEqualIgnoreCase(name, kWindowsSelfName) ||
         base::FilePath::CompareEqualIgnoreCase(name, kConsoleSelfName);
}

std::wstring GetModuleValue(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kModule)) {
    const auto& value = command_line.GetSwitchValuePath(switches::kModule);
    if (!value.empty()) {
      return NamePart(value);
    }
  }

  return std::wstring();
}

base::FilePath GetExePath() {
  HMODULE hModule = ::GetModuleHandle(nullptr);
  CHECK(hModule);
  return GetModulePath(hModule);
}

base::FilePath GetModulePath(HMODULE module) {
  wchar_t buffer[MAX_PATH];
  DWORD length = ::GetModuleFileName(module, buffer, MAX_PATH);
  CHECK_NE(length, 0U);
  CHECK_LT(length, static_cast<DWORD>(MAX_PATH));

  return base::FilePath(buffer);
}

std::wstring GetValidatedModuleValue(const base::CommandLine& command_line,
                                     const base::FilePath& exe_path) {
  // Allow module value configuration if the bootstrap executable has the
  // default name.
  const auto& value = GetModuleValue(command_line);
  if (!value.empty() && IsDefaultExeName(NamePart(exe_path))) {
    return value;
  }
  return std::wstring();
}

std::wstring GetDefaultModuleValue(const base::FilePath& exe_path) {
  return NamePart(exe_path);
}

bool IsModulePathAllowed(const base::FilePath& module_path,
                         const base::FilePath& exe_path) {
  // Allow any module path if the bootstrap executable has the default name.
  if (IsDefaultExeName(NamePart(exe_path))) {
    return true;
  }

  // Module must be at the same path as the executable.
  return module_path.DirName() == exe_path.DirName();
}

}  // namespace bootstrap_util
