// Copyright 2013 the Chromium Embedded Framework Authors. Portions Copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_DELEGATE_H_
#pragma once

#include <stdint.h>

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "components/devtools_http_handler/devtools_http_handler.h"
#include "components/devtools_http_handler/devtools_http_handler_delegate.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {
class RenderViewHost;
}

class CefDevToolsDelegate :
    public devtools_http_handler::DevToolsHttpHandlerDelegate {
 public:
  explicit CefDevToolsDelegate(uint16_t port);
  ~CefDevToolsDelegate() override;

  // Stops http server.
  void Stop();

  // DevToolsHttpHandlerDelegate overrides.
  std::string GetDiscoveryPageHTML() override;
  std::string GetFrontendResource(const std::string& path) override;
  std::string GetPageThumbnailData(const GURL& url) override;
  content::DevToolsExternalAgentProxyDelegate*
      HandleWebSocketConnection(const std::string& path) override;

  // Returns the chrome-devtools URL.
  std::string GetChromeDevToolsURL();

 private:
  scoped_ptr<devtools_http_handler::DevToolsHttpHandler> devtools_http_handler_;

  DISALLOW_COPY_AND_ASSIGN(CefDevToolsDelegate);
};

class CefDevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  CefDevToolsManagerDelegate();
  ~CefDevToolsManagerDelegate() override;

  // DevToolsManagerDelegate implementation.
  void Inspect(content::BrowserContext* browser_context,
               content::DevToolsAgentHost* agent_host) override {}
  void DevToolsAgentStateChanged(content::DevToolsAgentHost* agent_host,
                                 bool attached) override {}
  base::DictionaryValue* HandleCommand(
      content::DevToolsAgentHost* agent_host,
      base::DictionaryValue* command) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CefDevToolsManagerDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DELEGATE_H_
