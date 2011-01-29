// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef_context.h"
#include "browser_impl.h"
#include "browser_settings.h"
#include "browser_webview_mac.h"

#import <Cocoa/Cocoa.h>

#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webpreferences.h"

using WebKit::WebRect;
using WebKit::WebSize;

CefWindowHandle CefBrowserImpl::GetWindowHandle()
{
  AutoLock lock_scope(this);
  return window_info_.m_View;
}

gfx::NativeWindow CefBrowserImpl::UIT_GetMainWndHandle() const {
  REQUIRE_UIT();
  return (NSWindow*)window_info_.m_View;
}

void CefBrowserImpl::UIT_CreateBrowser(const CefString& url)
{
  REQUIRE_UIT();
  Lock();

  // Add a reference that will be released in UIT_DestroyBrowser().
  AddRef();

  // Add the new browser to the list maintained by the context
  _Context->AddBrowser(this);

  if (!settings_.developer_tools_disabled)
    dev_tools_agent_.reset(new BrowserDevToolsAgent());

  NSView* parentView = (NSView*)window_info_.m_ParentView;
  gfx::Rect contentRect(window_info_.m_x, window_info_.m_y,
                        window_info_.m_nWidth, window_info_.m_nHeight);

  WebPreferences prefs;
  BrowserToWebSettings(settings_, prefs);

  // Create the webview host object
  webviewhost_.reset(
      WebViewHost::Create(parentView, contentRect, delegate_.get(),
                          dev_tools_agent_.get(), prefs));
  delegate_->RegisterDragDrop();

  if (!settings_.developer_tools_disabled)
    dev_tools_agent_->SetWebView(webviewhost_->webview());

  BrowserWebView* browserView = (BrowserWebView*)webviewhost_->view_handle();
  browserView.browser = this;
  window_info_.m_View = (void*)browserView;

  Unlock();

  if(handler_.get()) {
    // Notify the handler that we're done creating the new window
    handler_->HandleAfterCreated(this);
  }

  if(url.size() > 0) {
    CefRefPtr<CefFrame> frame = GetMainFrame();
    frame->AddRef();
    UIT_LoadURL(frame, url);
  }
}

void CefBrowserImpl::UIT_SetFocus(WebWidgetHost* host, bool enable)
{
  REQUIRE_UIT();
  if (!host)
    return;
  
  NSView* view = host->view_handle();
  if (!view)
    return;

  if (enable)
    [[view window] makeFirstResponder:view];
}

WebKit::WebWidget* CefBrowserImpl::UIT_CreatePopupWidget()
{
  REQUIRE_UIT();
  
  DCHECK(!popuphost_);
  popuphost_ = WebWidgetHost::Create(NULL, popup_delegate_.get());
  
  // TODO(port): Show window.

  return popuphost_->webwidget();
}

void CefBrowserImpl::UIT_ClosePopupWidget()
{
  REQUIRE_UIT();
  
  // TODO(port): Close window.
  popuphost_ = NULL;
}

bool CefBrowserImpl::UIT_ViewDocumentString(WebKit::WebFrame *frame)
{
  REQUIRE_UIT();
  
  // TODO(port): Add implementation.
  NOTIMPLEMENTED();
  return false;
}

void CefBrowserImpl::UIT_PrintPage(int page_number, int total_pages,
                                   const gfx::Size& canvas_size,
                                   WebKit::WebFrame* frame) {
  REQUIRE_UIT();

  // TODO(port): Add implementation.
  NOTIMPLEMENTED();
}

void CefBrowserImpl::UIT_PrintPages(WebKit::WebFrame* frame) {
  REQUIRE_UIT();

  // TODO(port): Add implementation.
  NOTIMPLEMENTED();
}

int CefBrowserImpl::UIT_GetPagesCount(WebKit::WebFrame* frame)
{
	REQUIRE_UIT();
  
  // TODO(port): Add implementation.
  NOTIMPLEMENTED();
  return 0;
}

void CefBrowserImpl::UIT_CloseDevTools()
{
  REQUIRE_UIT();
  
  if(!dev_tools_agent_.get())
    return;

  // TODO(port): Add implementation.
  NOTIMPLEMENTED();
}
