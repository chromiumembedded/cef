// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_BOOTSTRAP_UTIL_WIN_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_BOOTSTRAP_UTIL_WIN_H_
#pragma once

#include <windows.h>

#include <string>

#include "base/files/file_path.h"

namespace base {
class CommandLine;
}

namespace bootstrap_util {

namespace switches {
// Changes to this value require rebuilding libcef.dll.
inline constexpr char kModule[] = "module";
}  // namespace switches

// Returns true if |name| is one of the default bootstrap executable names.
bool IsDefaultExeName(const std::wstring& name);

// Returns the command-line configured module value without validation.
std::wstring GetModuleValue(const base::CommandLine& command_line);

// The following functions can only be called in unsandboxed processes.

// Returns the fully qualified file path for the executable module.
base::FilePath GetExePath();

// Returns the fully qualified file path for |module|.
base::FilePath GetModulePath(HMODULE module);

// Returns the command-line configured module value if it passes validation.
std::wstring GetValidatedModuleValue(const base::CommandLine& command_line,
                                     const base::FilePath& exe_path);

// Returns the default module name (executable name without extension).
std::wstring GetDefaultModuleValue(const base::FilePath& exe_path);

// Returns true if loading |module_path| is allowed.
bool IsModulePathAllowed(const base::FilePath& module_path,
                         const base::FilePath& exe_path);

}  // namespace bootstrap_util

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_BOOTSTRAP_UTIL_WIN_H_
