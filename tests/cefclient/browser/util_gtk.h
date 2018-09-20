// Copyright (c) 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_UTIL_GTK_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_UTIL_GTK_H_
#pragma once

#include "include/base/cef_macros.h"
#include "include/base/cef_platform_thread.h"

namespace client {

// Scoped helper that manages the global GDK lock by calling gdk_threads_enter()
// and gdk_threads_leave(). The lock is not reentrant so this helper implements
// additional checking to avoid deadlocks.
//
// When using GTK in multi-threaded mode you must do the following:
// 1. Call gdk_threads_init() before making any other GTK/GDK/GLib calls.
// 2. Acquire the global lock before making any GTK/GDK calls, and release the
//    lock afterwards. This should only be done with callbacks that do not
//    originate from GTK signals (because those callbacks already hold the
//    lock).
//
// See https://www.geany.org/manual/gtk/gtk-faq/x482.html for more information.
class ScopedGdkThreadsEnter {
 public:
  ScopedGdkThreadsEnter();
  ~ScopedGdkThreadsEnter();

 private:
  bool take_lock_;

  static base::PlatformThreadId locked_thread_;

  DISALLOW_COPY_AND_ASSIGN(ScopedGdkThreadsEnter);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_UTIL_GTK_H_
