// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_TYPES_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_TYPES_H_
#pragma once

#include "include/cef_base.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>

// The Linux client uses GTK instead of the underlying platform type (X11).
#define ClientWindowHandle GtkWidget*
#else
#define ClientWindowHandle CefWindowHandle
#endif

#if defined(OS_MAC)
#define ClientNativeMacWindow void*
#ifdef __OBJC__
#define CAST_CLIENT_NATIVE_MAC_WINDOW_TO_NSWINDOW(native) \
  (__bridge NSWindow*)native
#define CAST_NSWINDOW_TO_CLIENT_NATIVE_MAC_WINDOW(window) (__bridge void*)window
#endif  // __OBJC__
#endif  // defined OS_MAC

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_TYPES_H_
