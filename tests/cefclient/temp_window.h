// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_TEMP_WINDOW_H_
#define CEF_TESTS_CEFCLIENT_TEMP_WINDOW_H_

#include "cefclient/client_types.h"

#if defined(OS_WIN)
#include "cefclient/temp_window_win.h"
#elif defined(OS_LINUX)
#include "cefclient/temp_window_x11.h"
#elif defined(OS_MACOSX)
#include "cefclient/temp_window_mac.h"
#endif

namespace client {

#if defined(OS_WIN)
typedef TempWindowWin TempWindow;
#elif defined(OS_LINUX)
typedef TempWindowX11 TempWindow;
#elif defined(OS_MACOSX)
typedef TempWindowMac TempWindow;
#endif

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_TEMP_WINDOW_H_
