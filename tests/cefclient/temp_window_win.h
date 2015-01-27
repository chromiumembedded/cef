// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_TEMP_WINDOW_WIN_H_
#define CEF_TESTS_CEFCLIENT_TEMP_WINDOW_WIN_H_

#include <windows.h>

#include "cefclient/main_message_loop.h"

namespace client {

// Represents a singleton hidden window that acts at temporary parent for
// popup browsers.
class TempWindowWin {
 public:
  // Returns the singleton window HWND.
  static HWND GetHWND();

 private:
  // A single instance will be created/owned by RootWindowManager.
  friend class RootWindowManager;

  TempWindowWin();
  ~TempWindowWin();

  HWND hwnd_;

  DISALLOW_COPY_AND_ASSIGN(TempWindowWin);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_TEMP_WINDOW_WIN_H_
