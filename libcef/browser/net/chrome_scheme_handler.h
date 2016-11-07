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

#include "net/url_request/url_request_job_factory.h"
#include "url/gurl.h"

namespace base {
class ListValue;
}

namespace content {
class BrowserURLHandler;
}

class CefURLRequestManager;

namespace scheme {

extern const char kChromeURL[];

// Register the chrome scheme handler.
void RegisterChromeHandler(CefURLRequestManager* request_manager);

// Register the WebUI controller factory.
void RegisterWebUIControllerFactory();

// Register the WebUI handler.
void BrowserURLHandlerCreated(content::BrowserURLHandler* handler);

// Used to fire any asynchronous content updates.
void DidFinishChromeLoad(CefRefPtr<CefFrame> frame,
                         const GURL& validated_url);

// Create a new ProtocolHandler that will filter the URLs passed to the default
// "chrome" protocol handler and forward the rest to CEF's handler.
std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
WrapChromeProtocolHandler(
    CefURLRequestManager* request_manager,
    std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
        chrome_protocol_handler);

}  // namespace scheme

#endif  // CEF_LIBCEF_BROWSER_CHROME_SCHEME_HANDLER_H_
