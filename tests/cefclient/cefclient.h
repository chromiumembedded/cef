// Copyright (c) 2008 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#pragma once

#include "resource.h"
#include "cef.h"

// Return the main browser window instance.
CefRefPtr<CefBrowser> AppGetBrowser();

// Return the main application window handle.
HWND AppGetMainHwnd();
