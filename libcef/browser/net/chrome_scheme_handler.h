// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_CHROME_SCHEME_HANDLER_H_
#define CEF_LIBCEF_BROWSER_NET_CHROME_SCHEME_HANDLER_H_
#pragma once

#include <string>

#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_process_message.h"

#include "url/gurl.h"

namespace base {
class ListValue;
}

namespace content {
class BrowserURLHandler;
}

namespace url {
class Origin;
}

namespace scheme {

extern const char kChromeURL[];

// Register the WebUI controller factory.
void RegisterWebUIControllerFactory();

// Register the WebUI handler.
void BrowserURLHandlerCreated(content::BrowserURLHandler* handler);

// Returns true if WebUI is allowed to make network requests.
bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin);

}  // namespace scheme

#endif  // CEF_LIBCEF_BROWSER_CHROME_SCHEME_HANDLER_H_
