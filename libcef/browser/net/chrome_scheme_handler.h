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

#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_job_factory.h"
#include "url/gurl.h"

namespace base {
class ListValue;
}

namespace content {
class BrowserContext;
}

class CefURLRequestManager;

namespace scheme {

extern const char kChromeURL[];

// Register the chrome scheme handler.
void RegisterChromeHandler(CefURLRequestManager* request_manager);

// Used to redirect about: URLs to chrome: URLs.
bool WillHandleBrowserAboutURL(GURL* url,
                               content::BrowserContext* browser_context);

// Used to fire any asynchronous content updates.
void DidFinishChromeLoad(CefRefPtr<CefFrame> frame,
                         const GURL& validated_url);

// Create a new ProtocolHandler that will filter the URLs passed to the default
// "chrome" protocol handler and forward the rest to CEF's handler.
scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
WrapChromeProtocolHandler(
    CefURLRequestManager* request_manager,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        chrome_protocol_handler);

}  // namespace scheme

#endif  // CEF_LIBCEF_BROWSER_CHROME_SCHEME_HANDLER_H_
