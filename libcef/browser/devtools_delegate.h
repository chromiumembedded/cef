// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_DELEGATE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/devtools_http_handler_delegate.h"

namespace net {
class URLRequestContextGetter;
}

namespace content {
class DevToolsHttpHandler;
}

class CefDevToolsDelegate : public content::DevToolsHttpHandlerDelegate {
 public:
  CefDevToolsDelegate(int port, net::URLRequestContextGetter* context_getter);
  virtual ~CefDevToolsDelegate();

  // Stops http server.
  void Stop();

  // DevToolsHttpProtocolHandler::Delegate overrides.
  virtual std::string GetDiscoveryPageHTML() OVERRIDE;
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual bool BundlesFrontendResources() OVERRIDE;
  virtual std::string GetFrontendResourcesBaseURL() OVERRIDE;

 private:
  net::URLRequestContextGetter* context_getter_;
  content::DevToolsHttpHandler* devtools_http_handler_;

  DISALLOW_COPY_AND_ASSIGN(CefDevToolsDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DELEGATE_H_
