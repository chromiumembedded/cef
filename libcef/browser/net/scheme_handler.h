// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SCHEME_HANDLER_H_
#define CEF_LIBCEF_BROWSER_NET_SCHEME_HANDLER_H_
#pragma once

#include "include/cef_frame.h"

#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace net {
class HostResolver;
class URLRequestJobFactoryImpl;
}

class CefURLRequestManager;

namespace scheme {

// Install the internal scheme handlers provided by Chromium that cannot be
// overridden.
void InstallInternalProtectedHandlers(
    net::URLRequestJobFactoryImpl* job_factory,
    CefURLRequestManager* request_manager,
    content::ProtocolHandlerMap* protocol_handlers,
    net::HostResolver* host_resolver);

// Register the internal scheme handlers that can be overridden.
void RegisterInternalHandlers(CefURLRequestManager* request_manager);

// Used to fire any asynchronous content updates.
void DidFinishLoad(CefRefPtr<CefFrame> frame, const GURL& validated_url);

}  // namespace scheme

#endif  // CEF_LIBCEF_BROWSER_NET_SCHEME_HANDLER_H_
