// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2016 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include <windows.h>

#include "libcef/common/crash_reporting_win.h"

#include "base/debug/crash_logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "components/crash/core/common/crash_keys.h"

namespace crash_reporting_win {

namespace {

// exported in crash_reporter_client.cc:
//    size_t __declspec(dllexport) __cdecl GetCrashKeyCountImpl.
typedef size_t(__cdecl* GetCrashKeyCount)();

// exported in crash_reporter_client.cc:
//    bool __declspec(dllexport) __cdecl GetCrashKeyImpl.
typedef bool(__cdecl* GetCrashKey)(size_t, const char**, size_t*);

size_t GetCrashKeyCountTrampoline() {
  static GetCrashKeyCount get_crash_key_count = []() {
    HMODULE elf_module = GetModuleHandle(chrome::kChromeElfDllName);
    return reinterpret_cast<GetCrashKeyCount>(
        elf_module ? GetProcAddress(elf_module, "GetCrashKeyCountImpl")
                   : nullptr);
  }();
  if (get_crash_key_count) {
    return (get_crash_key_count)();
  }
  return 0;
}

bool GetCrashKeyTrampoline(size_t index,
                           const char** key_name,
                           size_t* max_length) {
  static GetCrashKey get_crash_key = []() {
    HMODULE elf_module = GetModuleHandle(chrome::kChromeElfDllName);
    return reinterpret_cast<GetCrashKey>(
        elf_module ? GetProcAddress(elf_module, "GetCrashKeyImpl")
                   : nullptr);
  }();
  if (get_crash_key) {
    return (get_crash_key)(index, key_name, max_length);
  }
  return false;
}


// From chrome/common/child_process_logging_win.cc:

// exported in breakpad_win.cc/crashpad_win.cc:
//    void __declspec(dllexport) __cdecl SetCrashKeyValueImpl.
typedef void(__cdecl* SetCrashKeyValue)(const wchar_t*, const wchar_t*);

// exported in breakpad_win.cc/crashpad_win.cc:
//    void __declspec(dllexport) __cdecl ClearCrashKeyValueImpl.
typedef void(__cdecl* ClearCrashKeyValue)(const wchar_t*);

void SetCrashKeyValueTrampoline(const base::StringPiece& key,
                                const base::StringPiece& value) {
  static SetCrashKeyValue set_crash_key = []() {
    HMODULE elf_module = GetModuleHandle(chrome::kChromeElfDllName);
    return reinterpret_cast<SetCrashKeyValue>(
        elf_module ? GetProcAddress(elf_module, "SetCrashKeyValueImpl")
                   : nullptr);
  }();
  if (set_crash_key) {
    (set_crash_key)(base::UTF8ToWide(key).data(),
                    base::UTF8ToWide(value).data());
  }
}

void ClearCrashKeyValueTrampoline(const base::StringPiece& key) {
  static ClearCrashKeyValue clear_crash_key = []() {
    HMODULE elf_module = GetModuleHandle(chrome::kChromeElfDllName);
    return reinterpret_cast<ClearCrashKeyValue>(
        elf_module ? GetProcAddress(elf_module, "ClearCrashKeyValueImpl")
                   : nullptr);
  }();
  if (clear_crash_key)
    (clear_crash_key)(base::UTF8ToWide(key).data());
}

}  // namespace

bool InitializeCrashReportingForModule() {
  base::debug::SetCrashKeyReportingFunctions(&SetCrashKeyValueTrampoline,
                                             &ClearCrashKeyValueTrampoline);

  std::vector<base::debug::CrashKey> keys;

  size_t key_ct = GetCrashKeyCountTrampoline();
  if (key_ct > 0U) {
    keys.reserve(key_ct);

    const char* key_name;
    size_t max_length;

    for (size_t i = 0; i < key_ct; ++i) {
      if (GetCrashKeyTrampoline(i, &key_name, &max_length))
        keys.push_back({key_name, max_length});
    }
  }

  if (!keys.empty()) {
    base::debug::InitCrashKeys(&keys[0], keys.size(),
                               crash_keys::kChunkMaxLength);
    return true;
  }

  return false;
}

}  // namespace crash_reporting_win
