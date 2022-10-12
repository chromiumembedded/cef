// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PRINTING_PRINT_UTIL_H_
#define CEF_LIBCEF_BROWSER_PRINTING_PRINT_UTIL_H_
#pragma once

#include "include/cef_browser.h"

namespace content {
class WebContents;
}

namespace print_util {

// Called from CefBrowserHostBase::Print.
void Print(content::WebContents* web_contents, bool print_preview_disabled);

// Called from CefBrowserHostBase::PrintToPDF.
void PrintToPDF(content::WebContents* web_contents,
                const CefString& path,
                const CefPdfPrintSettings& settings,
                CefRefPtr<CefPdfPrintCallback> callback);

}  // namespace print_util

#endif  // CEF_LIBCEF_BROWSER_PRINTING_PRINT_UTIL_H_
