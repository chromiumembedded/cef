// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef_context.h"
#include "browser_impl.h"
#include "browser_settings.h"

#include <gtk/gtk.h>

#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webpreferences.h"

using WebKit::WebRect;
using WebKit::WebSize;

void CefBrowserImpl::ParentWindowWillClose()
{
  // TODO(port): Implement this method if necessary.
}

CefWindowHandle CefBrowserImpl::GetWindowHandle()
{
  AutoLock lock_scope(this);
  return window_info_.m_Widget;
}

bool CefBrowserImpl::IsWindowRenderingDisabled()
{
  // TODO(port): Add support for off-screen rendering.
  return false;
}

gfx::NativeWindow CefBrowserImpl::UIT_GetMainWndHandle() {
  REQUIRE_UIT();
  GtkWidget* toplevel = gtk_widget_get_ancestor(window_info_.m_Widget,
      GTK_TYPE_WINDOW);
  return GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
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

  WebPreferences prefs;
  BrowserToWebSettings(settings_, prefs);

  // Create the webview host object
  webviewhost_.reset(
      WebViewHost::Create(window_info_.m_ParentWidget, gfx::Rect(),
                          delegate_.get(), NULL, dev_tools_agent_.get(),
                          prefs));

  if (!settings_.developer_tools_disabled)
    dev_tools_agent_->SetWebView(webviewhost_->webview());

  window_info_.m_Widget = webviewhost_->view_handle();

  Unlock();

  if(handler_.get()) {
    // Notify the handler that we're done creating the new window
    handler_->HandleAfterCreated(this);
  }

  if(url.size() > 0)
    UIT_LoadURL(GetMainFrame(), url);
}

void CefBrowserImpl::UIT_SetFocus(WebWidgetHost* host, bool enable)
{
  REQUIRE_UIT();
  if (!host)
    return;

  if(enable)
    gtk_widget_grab_focus(host->view_handle());
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

// static
void CefBrowserImpl::UIT_CloseView(gfx::NativeView view)
{
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableFunction(
      &gtk_widget_destroy, GTK_WIDGET(view)));
}
