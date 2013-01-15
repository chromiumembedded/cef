// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_DELEGATE_H_
#pragma once

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_http_handler_delegate.h"

namespace content {
class RenderViewHost;
}

class CefDevToolsBindingHandler
    : public content::DevToolsHttpHandler::DevToolsAgentHostBinding {
 public:
  CefDevToolsBindingHandler();

  // DevToolsAgentHostBinding overrides.
  virtual std::string GetIdentifier(
      content::DevToolsAgentHost* agent_host) OVERRIDE;
  virtual content::DevToolsAgentHost* ForIdentifier(
      const std::string& identifier) OVERRIDE;

  std::string GetIdentifier(content::RenderViewHost* rvh);

 private:
  void GarbageCollect();

  std::string random_seed_;

  // Map of identifier to DevToolsAgentHost. Keeps the DevToolsAgentHost objects
  // alive until the associated RVH is disconnected.
  typedef std::map<std::string, scoped_refptr<content::DevToolsAgentHost> >
      AgentsMap;
  AgentsMap agents_map_;

  DISALLOW_COPY_AND_ASSIGN(CefDevToolsBindingHandler);
};

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
  scoped_ptr<CefDevToolsBindingHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(CefDevToolsDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DELEGATE_H_
