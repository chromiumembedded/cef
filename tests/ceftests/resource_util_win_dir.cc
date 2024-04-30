// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <windows.h>

#include "include/internal/cef_string_wrappers.h"
#include "tests/shared/browser/resource_util.h"

namespace client {

bool GetResourceDir(std::string& dir) {
  wchar_t buff[MAX_PATH];

  // Retrieve the executable path.
  auto len = GetModuleFileName(nullptr, buff, MAX_PATH);
  if (len == 0) {
    return false;
  }

  buff[len] = 0;

  // Remove the executable name from the path.
  auto* pos = wcsrchr(buff, L'\\');
  if (!pos) {
    return false;
  }

  // Add "ceftests_files" to the path.
  wcscpy(pos + 1, L"ceftests_files");
  dir = CefStringWide(buff).ToString();
  return true;
}

}  // namespace client
