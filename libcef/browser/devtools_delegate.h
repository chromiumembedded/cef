// Copyright 2013 the Chromium Embedded Framework Authors. Portions Copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

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
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {
class RenderViewHost;
}

class CefDevToolsDelegate : public content::DevToolsHttpHandlerDelegate {
 public:
  explicit CefDevToolsDelegate(uint16 port);
  ~CefDevToolsDelegate() override;

  // Stops http server.
  void Stop();

  // DevToolsHttpHandlerDelegate overrides.
  std::string GetDiscoveryPageHTML() override;
  bool BundlesFrontendResources() override;
  base::FilePath GetDebugFrontendDir() override;
  scoped_ptr<net::ServerSocket> CreateSocketForTethering(
      std::string* name) override;

  // Returns the chrome-devtools URL.
  std::string GetChromeDevToolsURL();

 private:
  scoped_ptr<content::DevToolsHttpHandler> devtools_http_handler_;

  DISALLOW_COPY_AND_ASSIGN(CefDevToolsDelegate);
};

class CefDevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  explicit CefDevToolsManagerDelegate(
      content::BrowserContext* browser_context);
  ~CefDevToolsManagerDelegate() override;

  // DevToolsManagerDelegate implementation.
  void Inspect(content::BrowserContext* browser_context,
               content::DevToolsAgentHost* agent_host) override {}
  void DevToolsAgentStateChanged(content::DevToolsAgentHost* agent_host,
                                 bool attached) override {}
  base::DictionaryValue* HandleCommand(
      content::DevToolsAgentHost* agent_host,
      base::DictionaryValue* command) override;
  scoped_ptr<content::DevToolsTarget> CreateNewTarget(
      const GURL& url) override;
  void EnumerateTargets(TargetCallback callback) override;
  std::string GetPageThumbnailData(const GURL& url) override;

 private:
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(CefDevToolsManagerDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DELEGATE_H_
