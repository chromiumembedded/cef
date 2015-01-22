// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/util_win.h"

#include "include/base/cef_logging.h"
#include "include/internal/cef_types.h"

namespace client {

void SetUserDataPtr(HWND hWnd, void* ptr) {
  SetLastError(ERROR_SUCCESS);
  LONG_PTR result = ::SetWindowLongPtr(
      hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ptr));
  CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
}

WNDPROC SetWndProcPtr(HWND hWnd, WNDPROC wndProc) {
  WNDPROC old =
      reinterpret_cast<WNDPROC>(::GetWindowLongPtr(hWnd, GWLP_WNDPROC));
  CHECK(old != NULL);
  LONG_PTR result = ::SetWindowLongPtr(
      hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wndProc));
  CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
  return old;
}

}  // namespace client
