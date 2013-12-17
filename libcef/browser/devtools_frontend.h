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
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_frontend_host_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderViewHost;
class WebContents;
}

class CefDevToolsFrontend : public content::WebContentsObserver,
                            public content::DevToolsFrontendHostDelegate {
 public:
  static CefDevToolsFrontend* Show(
      CefRefPtr<CefBrowserHostImpl> inspected_browser,
      const CefWindowInfo& windowInfo,
      CefRefPtr<CefClient> client,
      const CefBrowserSettings& settings);

  void Activate();
  void Focus();
  void Close();

  CefRefPtr<CefBrowserHostImpl> frontend_browser() const {
    return frontend_browser_;
  }

 private:
  CefDevToolsFrontend(CefRefPtr<CefBrowserHostImpl> frontend_browser,
                      content::DevToolsAgentHost* agent_host);
  virtual ~CefDevToolsFrontend();

  // WebContentsObserver overrides
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DocumentOnLoadCompletedInMainFrame(int32 page_id) OVERRIDE;
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  // DevToolsFrontendHostDelegate implementation
  virtual void DispatchOnEmbedder(const std::string& message) OVERRIDE {}

  virtual void InspectedContentsClosing() OVERRIDE;

  CefRefPtr<CefBrowserHostImpl> frontend_browser_;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  scoped_ptr<content::DevToolsClientHost> frontend_host_;

  DISALLOW_COPY_AND_ASSIGN(CefDevToolsFrontend);
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_FRONTEND_H_
