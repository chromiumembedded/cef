// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef _CEF_BROWSER_SETTINGS_H
#define _CEF_BROWSER_SETTINGS_H

#include "include/internal/cef_types_wrappers.h"
struct WebPreferences;

void BrowserToWebSettings(const CefBrowserSettings& cef, WebPreferences& web);

#endif // _CEF_BROWSER_SETTINGS_H
