// Copyright 2013 the Chromium Embedded Framework Authors. Portions Copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/devtools_frontend.h"

#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/devtools_delegate.h"
#include "libcef/browser/request_context_impl.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/content_client.h"
#include "net/base/net_util.h"

// static
CefDevToolsFrontend* CefDevToolsFrontend::Show(
    CefRefPtr<CefBrowserHostImpl> inspected_browser,
    const CefWindowInfo& windowInfo,
    CefRefPtr<CefClient> client,
    const CefBrowserSettings& settings) {
  CefRefPtr<CefBrowserHostImpl> frontend_browser =
      CefBrowserHostImpl::Create(windowInfo, client, CefString(),
                                 settings,
                                 inspected_browser->GetWindowHandle(), true,
                                 inspected_browser->GetRequestContext());

  // CefDevToolsFrontend will delete itself when the frontend WebContents is
  // destroyed.
  CefDevToolsFrontend* devtools_frontend = new CefDevToolsFrontend(
      static_cast<CefBrowserHostImpl*>(frontend_browser.get()),
      content::DevToolsAgentHost::GetOrCreateFor(
          inspected_browser->GetWebContents()->GetRenderViewHost()).get());

  // Need to load the URL after creating the DevTools objects.
  CefDevToolsDelegate* delegate =
      CefContentBrowserClient::Get()->devtools_delegate();
  frontend_browser->GetMainFrame()->LoadURL(delegate->GetChromeDevToolsURL());

  devtools_frontend->Activate();
  devtools_frontend->Focus();

  return devtools_frontend;
}

void CefDevToolsFrontend::Activate() {
  frontend_browser_->ActivateContents(web_contents());
}

void CefDevToolsFrontend::Focus() {
  web_contents()->GetView()->Focus();
}

void CefDevToolsFrontend::Close() {
  frontend_browser_->CloseBrowser(true);
}

CefDevToolsFrontend::CefDevToolsFrontend(
    CefRefPtr<CefBrowserHostImpl> frontend_browser,
    content::DevToolsAgentHost* agent_host)
    : WebContentsObserver(frontend_browser->GetWebContents()),
      frontend_browser_(frontend_browser),
      agent_host_(agent_host) {
  frontend_host_.reset(
      content::DevToolsClientHost::CreateDevToolsFrontendHost(
          web_contents(), this));
}

CefDevToolsFrontend::~CefDevToolsFrontend() {
}

void CefDevToolsFrontend::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  content::DevToolsClientHost::SetupDevToolsFrontendClient(
      web_contents()->GetRenderViewHost());
  content::DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
      agent_host_.get(), frontend_host_.get());
}

void CefDevToolsFrontend::DocumentOnLoadCompletedInMainFrame(int32 page_id) {
  web_contents()->GetRenderViewHost()->ExecuteJavascriptInWebFrame(
      base::string16(),
      base::ASCIIToUTF16("InspectorFrontendAPI.setUseSoftMenu(true);"));
}

void CefDevToolsFrontend::WebContentsDestroyed(
    content::WebContents* web_contents) {
  content::DevToolsManager::GetInstance()->ClientHostClosing(
      frontend_host_.get());
  delete this;
}

void CefDevToolsFrontend::InspectedContentsClosing() {
  Close();
}
