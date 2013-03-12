// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SCHEME_REGISTRATION_H_
#define CEF_LIBCEF_BROWSER_SCHEME_REGISTRATION_H_
#pragma once

#include <string>
#include <vector>

#include "include/cef_frame.h"

#include "content/public/browser/content_browser_client.h"
#include "googleurl/src/gurl.h"

namespace net {
class URLRequestJobFactoryImpl;
}

namespace scheme {

// Add internal standard schemes.
void AddInternalStandardSchemes(std::vector<std::string>* standard_schemes);

// Returns true if the specified |scheme| is handled internally and should not
// be explicitly registered or unregistered with the URLRequestJobFactory. A
// registered handler for one of these schemes (like "chrome") may still be
// triggered via chaining from an existing ProtocolHandler. |scheme| should
// always be a lower-case string.
bool IsInternalProtectedScheme(const std::string& scheme);

// Install the internal scheme handlers provided by Chromium that cannot be
// overridden.
void InstallInternalProtectedHandlers(
    net::URLRequestJobFactoryImpl* job_factory,
    content::ProtocolHandlerMap* protocol_handlers);

// Register the internal scheme handlers that can be overridden.
void RegisterInternalHandlers();

// Used to fire any asynchronous content updates.
void DidFinishLoad(CefRefPtr<CefFrame> frame, const GURL& validated_url);

}  // namespace scheme

#endif  // CEF_LIBCEF_BROWSER_SCHEME_REGISTRATION_H_
