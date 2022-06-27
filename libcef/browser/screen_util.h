// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SCREEN_UTIL_H_
#define CEF_LIBCEF_BROWSER_SCREEN_UTIL_H_
#pragma once

#include "include/internal/cef_types_wrappers.h"

// Create a new rectangle from the input |rect| rectangle that is fully visible
// on provided |screen_rect| screen. The width and height of the resulting
// rectangle are clamped to the screen width and height respectively if they
// would overflow.
CefRect MakeVisibleOnScreenRect(const CefRect& rect, const CefRect& screen);

#endif  // CEF_LIBCEF_BROWSER_SCREEN_UTIL_H_
