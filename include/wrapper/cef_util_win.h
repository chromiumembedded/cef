// Copyright (c) 2025 Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// ---------------------------------------------------------------------------
//
// The contents of this file are only available to applications that link
// against the libcef_dll_wrapper target.
//

#ifndef CEF_INCLUDE_WRAPPER_CEF_UTIL_WIN_H_
#define CEF_INCLUDE_WRAPPER_CEF_UTIL_WIN_H_
#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace cef_util {

// Returns the fully qualified file path for the executable module.
std::wstring GetExePath();

// Returns the fully qualified file path for |module|.
std::wstring GetModulePath(HMODULE module);

// Returns the value of GetLastError() as a string.
std::wstring GetLastErrorAsString();

// Parse command line arguments for |hInstance|.
std::vector<std::wstring> ParseCommandLineArgs(const wchar_t* str);

// Returns the value for |name| in |command_line|, if any.
std::wstring GetCommandLineValue(const std::vector<std::wstring>& command_line,
                                 const std::wstring& name);

}  // namespace cef_util

#endif  // CEF_INCLUDE_WRAPPER_CEF_UTIL_WIN_H_
