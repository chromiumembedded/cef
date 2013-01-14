// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_CHROME_BINDINGS_H_
#define CEF_LIBCEF_RENDERER_CHROME_BINDINGS_H_
#pragma once

#include "include/cef_v8.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/frame_impl.h"

namespace scheme {

extern const char kChromeScheme[];
extern const char kChromeProcessMessage[];

void OnContextCreated(CefRefPtr<CefBrowserImpl> browser,
                      CefRefPtr<CefFrameImpl> frame,
                      CefRefPtr<CefV8Context> context);

}  // namespace scheme

#endif  // CEF_LIBCEF_RENDERER_CHROME_BINDINGS_H_
