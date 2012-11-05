// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_DELEGATE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_http_handler_delegate.h"

namespace content {
class RenderViewHost;
}

class CefDevToolsDelegate : public content::DevToolsHttpHandlerDelegate {
 public:
  explicit CefDevToolsDelegate(int port);
  virtual ~CefDevToolsDelegate();

  // Stops http server.
  void Stop();

  // DevToolsHttpProtocolHandler::Delegate overrides.
  virtual std::string GetDiscoveryPageHTML() OVERRIDE;
  virtual bool BundlesFrontendResources() OVERRIDE;
  virtual FilePath GetDebugFrontendDir() OVERRIDE;
  virtual std::string GetPageThumbnailData(const GURL& url) OVERRIDE;
  virtual content::RenderViewHost* CreateNewTarget() OVERRIDE;

  // Returns the DevTools URL for the specified RenderViewHost.
  std::string GetDevToolsURL(content::RenderViewHost* rvh, bool http_scheme);

 private:
  content::DevToolsHttpHandler* devtools_http_handler_;
  scoped_ptr<content::DevToolsHttpHandler::RenderViewHostBinding> binding_;

  DISALLOW_COPY_AND_ASSIGN(CefDevToolsDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DELEGATE_H_
