// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef_context.h"
#include "browser_impl.h"

#import <Cocoa/Cocoa.h>

#include "base/utf_string_conversions.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"

using WebKit::WebRect;
using WebKit::WebSize;

CefWindowHandle CefBrowserImpl::GetWindowHandle()
{
  Lock();
  CefWindowHandle handle = window_info_.m_Window;
  Unlock();
  return handle;
}

gfx::NativeWindow CefBrowserImpl::GetMainWndHandle() const {
  return (NSWindow*)window_info_.m_Window;
}

void CefBrowserImpl::UIT_CreateBrowser(const std::wstring& url)
{
  REQUIRE_UIT();
  
  // Create the new browser window
  // TODO(port): Add implementation.
  
  // Add a reference that will be released on WM_DESTROY.
  AddRef();

  // Add the new browser to the list maintained by the context
  _Context->AddBrowser(this);

  // Create the webview host object
  webviewhost_.reset(
      WebViewHost::Create([GetMainWndHandle() contentView], delegate_.get(),
      NULL, *_Context->web_preferences()));
  delegate_->RegisterDragDrop();
    
  // Size the web view window to the browser window
  // TODO(port): Add implementation.
  
  if(handler_.get()) {
    // Notify the handler that we're done creating the new window
    handler_->HandleAfterCreated(this);
  }

  if(url.size() > 0) {
    CefRefPtr<CefFrame> frame = GetMainFrame();
    frame->AddRef();
    UIT_LoadURL(frame, url.c_str());
  }
}

void CefBrowserImpl::UIT_SetFocus(WebWidgetHost* host, bool enable)
{
  REQUIRE_UIT();
  if (!host)
    return;

  // TODO(port): Add implementation.
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
