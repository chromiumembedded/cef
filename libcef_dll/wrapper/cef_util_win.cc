// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/wrapper/cef_util_win.h"

#include <shellapi.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

#if defined(CEF_BUILD_BOOTSTRAP)
#include "base/check_op.h"
#else
#include "include/base/cef_logging.h"
#endif

namespace cef_util {

namespace {

void TrimWhitespace(std::wstring& str) {
  // Trim leading whitespace
  str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](auto ch) {
              return !std::iswspace(ch);
            }));

  // Trim trailing whitespace
  str.erase(std::find_if(str.rbegin(), str.rend(),
                         [](auto ch) { return !std::iswspace(ch); })
                .base(),
            str.end());
}

}  // namespace

std::wstring GetExePath() {
  HMODULE hModule = ::GetModuleHandle(nullptr);
  CHECK(hModule);
  return GetModulePath(hModule);
}

std::wstring GetModulePath(HMODULE module) {
  wchar_t buffer[MAX_PATH];
  DWORD length = ::GetModuleFileName(module, buffer, MAX_PATH);
  CHECK_NE(length, 0U);
  CHECK_LT(length, static_cast<DWORD>(MAX_PATH));
  return buffer;
}

std::wstring GetLastErrorAsString() {
  std::wstring error_message;

  DWORD error_message_id = ::GetLastError();
  if (error_message_id == 0) {
    return error_message;
  }

  LPWSTR message_buffer = NULL;

  DWORD size = FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, error_message_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPWSTR)&message_buffer, 0, NULL);

  if (message_buffer) {
    error_message = std::wstring(message_buffer, size);
    LocalFree(message_buffer);
  }

  return error_message;
}

// Implementation based on CommandLine::ParseFromString.
std::vector<std::wstring> ParseCommandLineArgs(const wchar_t* str) {
  std::wstring command_line = str;
  TrimWhitespace(command_line);

  int num_args = 0;
  wchar_t** args = NULL;
  // When calling CommandLineToArgvW, use the apiset if available.
  // Doing so will bypass loading shell32.dll on Windows.
  HMODULE downlevel_shell32_dll =
      ::LoadLibraryEx(L"api-ms-win-downlevel-shell32-l1-1-0.dll", nullptr,
                      LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (downlevel_shell32_dll) {
    auto command_line_to_argv_w_proc =
        reinterpret_cast<decltype(::CommandLineToArgvW)*>(
            ::GetProcAddress(downlevel_shell32_dll, "CommandLineToArgvW"));
    if (command_line_to_argv_w_proc) {
      args = command_line_to_argv_w_proc(command_line.data(), &num_args);
    }
  } else {
    // Since the apiset is not available, allow the delayload of shell32.dll
    // to take place.
    args = ::CommandLineToArgvW(command_line.data(), &num_args);
  }

  std::vector<std::wstring> result;
  result.reserve(num_args);

  for (int i = 0; i < num_args; ++i) {
    std::wstring arg = args[i];
    TrimWhitespace(arg);
    if (!arg.empty()) {
      result.push_back(arg);
    }
  }

  LocalFree(args);

  if (downlevel_shell32_dll) {
    ::FreeLibrary(downlevel_shell32_dll);
  }

  return result;
}

std::wstring GetCommandLineValue(const std::vector<std::wstring>& command_line,
                                 const std::wstring& name) {
  constexpr wchar_t kPrefix[] = L"--";
  constexpr wchar_t kSeparator[] = L"=";
  constexpr wchar_t kQuoteChar = L'"';

  const std::wstring& start = kPrefix + name + kSeparator;
  for (const auto& arg : command_line) {
    if (arg.find(start) == 0) {
      auto value = arg.substr(start.length());
      if (value.length() > 2 && value[0] == kQuoteChar &&
          value[arg.length() - 1] == kQuoteChar) {
        value = value.substr(1, value.length() - 2);
      }
      return value;
    }
  }
  return std::wstring();
}

}  // namespace cef_util
