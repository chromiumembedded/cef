// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_FRONTEND_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_FRONTEND_H_

#include <memory>

#include "libcef/browser/browser_host_impl.h"

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace base {
class Value;
}

namespace content {
class RenderViewHost;
class WebContents;
}

class PrefService;

class CefDevToolsFrontend : public content::WebContentsObserver,
                            public content::DevToolsAgentHostClient,
                            public net::URLFetcherDelegate {
 public:
  static CefDevToolsFrontend* Show(
      CefRefPtr<CefBrowserHostImpl> inspected_browser,
      const CefWindowInfo& windowInfo,
      CefRefPtr<CefClient> client,
      const CefBrowserSettings& settings,
      const CefPoint& inspect_element_at);

  void Activate();
  void Focus();
  void InspectElementAt(int x, int y);
  void Close();

  void DisconnectFromTarget();

  void CallClientFunction(const std::string& function_name,
                          const base::Value* arg1,
                          const base::Value* arg2,
                          const base::Value* arg3);

  CefRefPtr<CefBrowserHostImpl> frontend_browser() const {
    return frontend_browser_;
  }

 protected:
  CefDevToolsFrontend(CefRefPtr<CefBrowserHostImpl> frontend_browser,
                      content::WebContents* inspected_contents,
                      const CefPoint& inspect_element_at);
  ~CefDevToolsFrontend() override;

  // content::DevToolsAgentHostClient implementation.
  void AgentHostClosed(content::DevToolsAgentHost* agent_host,
                       bool replaced) override;
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               const std::string& message) override;
  void SetPreferences(const std::string& json);
  virtual void HandleMessageFromDevToolsFrontend(const std::string& message);

 private:
  // WebContentsObserver overrides
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void DocumentAvailableInMainFrame() override;
  void WebContentsDestroyed() override;

  // net::URLFetcherDelegate overrides.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void SendMessageAck(int request_id,
                      const base::Value* arg1);

  PrefService* GetPrefs() const;

  CefRefPtr<CefBrowserHostImpl> frontend_browser_;
  content::WebContents* inspected_contents_;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  CefPoint inspect_element_at_;
  std::unique_ptr<content::DevToolsFrontendHost> frontend_host_;
  using PendingRequestsMap = std::map<const net::URLFetcher*, int>;
  PendingRequestsMap pending_requests_;
  base::WeakPtrFactory<CefDevToolsFrontend> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefDevToolsFrontend);
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_FRONTEND_H_
