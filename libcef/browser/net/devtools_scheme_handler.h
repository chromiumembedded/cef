// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_DEVTOOLS_SCHEME_HANDLER_H_
#define CEF_LIBCEF_BROWSER_NET_DEVTOOLS_SCHEME_HANDLER_H_
#pragma once

class CefURLRequestManager;

namespace scheme {

extern const char kChromeDevToolsHost[];

// Register the chrome-devtools scheme handler.
void RegisterChromeDevToolsHandler(CefURLRequestManager* request_manager);

}  // namespace scheme

#endif  // CEF_LIBCEF_BROWSER_NET_DEVTOOLS_SCHEME_HANDLER_H_
