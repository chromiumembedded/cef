// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_WEBKIT_GLUE_H_
#define CEF_LIBCEF_BROWSER_WEBKIT_GLUE_H_
#pragma once

#include <string>

#include "include/internal/cef_types.h"

#include "base/compiler_specific.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebGraphicsContext3D.h"
#include "v8/include/v8.h"

#if defined(OS_WIN)
#include <windows.h>  // NOLINT(build/include_order)
#endif

namespace WebKit {
class WebFrame;
class WebView;
}
namespace webkit {
struct WebPluginInfo;
}

namespace webkit_glue {

#if defined(OS_WIN)
// Capture a bitmap of the web view.
void CaptureWebViewBitmap(HWND mainWnd, WebKit::WebView* webview,
                          HBITMAP& bitmap, SIZE& size);

// Save a bitmap image to file, providing optional alternative data in |lpBits|
BOOL SaveBitmapToFile(HBITMAP hBmp, HDC hDC, LPCTSTR file, LPBYTE lpBits);
#endif

// Text encoding objects must be initialized on the main thread.
void InitializeTextEncoding();

// Retrieve the V8 context associated with the frame.
v8::Handle<v8::Context> GetV8Context(WebKit::WebFrame* frame);

// Clear all cached data.
void ClearCache();

// Returns true if the request represents a download based on
// the supplied Content-Type and Content-Disposition headers.
bool ShouldDownload(const std::string& content_disposition,
                    const std::string& mime_type);

// Checks whether a plugin is enabled either by the user or by policy.
bool IsPluginEnabled(const webkit::WebPluginInfo& plugin);

// Create a new WebGraphicsContext3D object.
WebKit::WebGraphicsContext3D* CreateGraphicsContext3D(
    cef_graphics_implementation_t graphics_implementation,
    const WebKit::WebGraphicsContext3D::Attributes& attributes,
    WebKit::WebView* web_view,
    bool renderDirectlyToWebView);

}  // namespace webkit_glue

#endif  // CEF_LIBCEF_BROWSER_WEBKIT_GLUE_H_
