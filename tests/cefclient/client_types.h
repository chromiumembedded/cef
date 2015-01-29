// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_CLIENT_TYPES_H_
#define CEF_TESTS_CEFCLIENT_CLIENT_TYPES_H_

#include "include/cef_base.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>

// The Linux client uses GTK instead of the underlying platform type (X11).
#define ClientWindowHandle GtkWidget*
#else
#define ClientWindowHandle CefWindowHandle
#endif

#endif  // CEF_TESTS_CEFCLIENT_CLIENT_TYPES_H_

