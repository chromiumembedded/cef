// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_IMPL_H_
#pragma once

#include "include/cef_client.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/views/view_impl.h"
#include "libcef/browser/views/browser_view_view.h"

#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

class CefBrowserViewImpl :
    public CefViewImpl<CefBrowserViewView, CefBrowserView,
                       CefBrowserViewDelegate>,
    public CefBrowserViewView::Delegate {
 public:
  typedef CefViewImpl<CefBrowserViewView, CefBrowserView,
                      CefBrowserViewDelegate> ParentClass;

  // Create a new CefBrowserView instance. |delegate| may be nullptr.
  static CefRefPtr<CefBrowserViewImpl> Create(
      CefRefPtr<CefClient> client,
      const CefString& url,
      const CefBrowserSettings& settings,
      CefRefPtr<CefRequestContext> request_context,
      CefRefPtr<CefBrowserViewDelegate> delegate);

  // Create a new CefBrowserView instance for a popup. |delegate| may be
  // nullptr.
  static CefRefPtr<CefBrowserViewImpl> CreateForPopup(
      const CefBrowserSettings& settings,
      CefRefPtr<CefBrowserViewDelegate> delegate);

  // Called from CefBrowserPlatformDelegateViews.
  void WebContentsCreated(content::WebContents* web_contents);
  void BrowserCreated(CefBrowserHostImpl* browser);
  void BrowserDestroyed(CefBrowserHostImpl* browser);

  // Called to handle accelerators when the event is unhandled by the web
  // content and the browser client.
  void HandleKeyboardEvent(const content::NativeWebKeyboardEvent& event);

  // CefBrowserView methods:
  CefRefPtr<CefBrowser> GetBrowser() override;
  void SetPreferAccelerators(bool prefer_accelerators) override;

  // CefView methods:
  CefRefPtr<CefBrowserView> AsBrowserView() override { return this; }
  void SetBackgroundColor(cef_color_t color) override;

  // CefViewAdapter methods:
  void Detach() override;
  std::string GetDebugType() override { return "BrowserView"; }
  void GetDebugInfo(base::DictionaryValue* info,
                    bool include_children) override;

  // CefBrowserViewView::Delegate methods:
  void OnBrowserViewAdded() override;

 private:
  // Create a new implementation object.
  // Always call Initialize() after creation.
  // |delegate| may be nullptr.
  explicit CefBrowserViewImpl(CefRefPtr<CefBrowserViewDelegate> delegate);

  void SetPendingBrowserCreateParams(
      CefRefPtr<CefClient> client,
      const CefString& url,
      const CefBrowserSettings& settings,
      CefRefPtr<CefRequestContext> request_context);

  void SetDefaults(const CefBrowserSettings& settings);

  // CefViewImpl methods:
  CefBrowserViewView* CreateRootView() override;
  void InitializeRootView() override;

  // Logic extracted from UnhandledKeyboardEventHandler::HandleKeyboardEvent for
  // the handling of accelerators. Returns true if the event was handled by the
  // accelerator.
  bool HandleAccelerator(const content::NativeWebKeyboardEvent& event,
                         views::FocusManager* focus_manager);

  std::unique_ptr<CefBrowserHostImpl::CreateParams>
      pending_browser_create_params_;

  CefRefPtr<CefBrowserHostImpl> browser_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  bool ignore_next_char_event_ = false;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefBrowserViewImpl);
  DISALLOW_COPY_AND_ASSIGN(CefBrowserViewImpl);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_IMPL_H_
