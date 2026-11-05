// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_TOUCHPAD_WIN_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_TOUCHPAD_WIN_H_
#pragma once

#include <windows.h>

#include <memory>
#include <set>

#include "tests/shared/browser/osr_touchpad.h"

namespace client {

// Receives Precision Touchpad HID input in the foreground OSR window. Raw
// Input registration is per process/device class, so only the focused view
// registers, and an old view must not unregister a newer view's registration.
class OsrTouchpadWin {
 public:
  OsrTouchpadWin(HWND window, OsrTouchpadCallback callback);
  ~OsrTouchpadWin();

  OsrTouchpadWin(const OsrTouchpadWin&) = delete;
  OsrTouchpadWin& operator=(const OsrTouchpadWin&) = delete;

  bool SetFocused(bool focused);
  bool available() const;
  void OnInput(HRAWINPUT input);
  void OnDeviceRemoved(HANDLE device);

 private:
  struct Device;
  const HWND window_;
  const OsrTouchpadCallback callback_;
  std::unique_ptr<Device> device_;
  std::set<HANDLE> rejected_devices_;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_TOUCHPAD_WIN_H_
