// Copyright 2013 the Chromium Embedded Framework Authors. Portions Copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_FRONTEND_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_FRONTEND_H_

#include "libcef/browser/browser_host_impl.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderViewHost;
class WebContents;
}

class CefDevToolsFrontend : public content::WebContentsObserver,
                            public content::DevToolsFrontendHost::Delegate,
                            public content::DevToolsAgentHostClient {
 public:
  static CefDevToolsFrontend* Show(
      CefRefPtr<CefBrowserHostImpl> inspected_browser,
      const CefWindowInfo& windowInfo,
      CefRefPtr<CefClient> client,
      const CefBrowserSettings& settings,
      const CefPoint& inspect_element_at);

  void Activate();
  void Focus();
  void Close();

  CefRefPtr<CefBrowserHostImpl> frontend_browser() const {
    return frontend_browser_;
  }

 private:
  CefDevToolsFrontend(CefRefPtr<CefBrowserHostImpl> frontend_browser,
                      content::DevToolsAgentHost* agent_host);
  ~CefDevToolsFrontend() override;

  // WebContentsObserver overrides.
  void RenderViewCreated(
      content::RenderViewHost* render_view_host) override;
  void WebContentsDestroyed() override;

  // content::DevToolsFrontendHost::Delegate implementation.
  void HandleMessageFromDevToolsFrontend(
      const std::string& message) override;
  void HandleMessageFromDevToolsFrontendToBackend(
      const std::string& message) override;

  // content::DevToolsAgentHostClient implementation.
  void DispatchProtocolMessage(
      content::DevToolsAgentHost* agent_host,
      const std::string& message) override;
  void AgentHostClosed(
      content::DevToolsAgentHost* agent_host,
      bool replaced) override;

  CefRefPtr<CefBrowserHostImpl> frontend_browser_;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  scoped_ptr<content::DevToolsFrontendHost> frontend_host_;

  DISALLOW_COPY_AND_ASSIGN(CefDevToolsFrontend);
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_FRONTEND_H_
