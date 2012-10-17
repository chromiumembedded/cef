// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_SCHEME_HANDLER_H_
#define CEF_LIBCEF_BROWSER_CHROME_SCHEME_HANDLER_H_
#pragma once

#include <string>
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_process_message.h"
#include "googleurl/src/gurl.h"

namespace base {
class ListValue;
}

namespace content {
class BrowserContext;
}

namespace scheme {

extern const char kChromeScheme[];
extern const char kChromeURL[];
extern const char kChromeProcessMessage[];

// Register the chrome scheme handler.
void RegisterChromeHandler();

// Used to redirect about: URLs to chrome: URLs.
bool WillHandleBrowserAboutURL(GURL* url,
                               content::BrowserContext* browser_context);

// Used to fire any asynchronous content updates.
void DidFinishChromeLoad(CefRefPtr<CefFrame> frame,
                         const GURL& validated_url);

// Used to execute messages from render process bindings.
void OnChromeProcessMessage(CefRefPtr<CefBrowser> browser,
                            const base::ListValue& arguments);

}  // namespace scheme

#endif  // CEF_LIBCEF_BROWSER_CHROME_SCHEME_HANDLER_H_
