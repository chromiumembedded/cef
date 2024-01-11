// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_IMPL_H_
#pragma once

#include "include/cef_client.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/views/browser_view_view.h"
#include "libcef/browser/views/view_impl.h"

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

class CefBrowserHostBase;
class CefWindowImpl;
class ChromeBrowserView;

class CefBrowserViewImpl
    : public CefViewImpl<views::View, CefBrowserView, CefBrowserViewDelegate>,
      public CefBrowserViewView::Delegate {
 public:
  using ParentClass =
      CefViewImpl<views::View, CefBrowserView, CefBrowserViewDelegate>;

  CefBrowserViewImpl(const CefBrowserViewImpl&) = delete;
  CefBrowserViewImpl& operator=(const CefBrowserViewImpl&) = delete;

  // Create a new CefBrowserView instance. |delegate| may be nullptr.
  // |window_info| will only be used when creating a Chrome child window.
  static CefRefPtr<CefBrowserViewImpl> Create(
      const CefWindowInfo& window_info,
      CefRefPtr<CefClient> client,
      const CefString& url,
      const CefBrowserSettings& settings,
      CefRefPtr<CefDictionaryValue> extra_info,
      CefRefPtr<CefRequestContext> request_context,
      CefRefPtr<CefBrowserViewDelegate> delegate);

  // Create a new CefBrowserView instance for a popup. |delegate| may be
  // nullptr.
  static CefRefPtr<CefBrowserViewImpl> CreateForPopup(
      const CefBrowserSettings& settings,
      CefRefPtr<CefBrowserViewDelegate> delegate);

  // Called from CefBrowserPlatformDelegateViews.
  void WebContentsCreated(content::WebContents* web_contents);
  void BrowserCreated(CefBrowserHostBase* browser,
                      base::RepeatingClosure on_bounds_changed);
  void BrowserDestroyed(CefBrowserHostBase* browser);

  // Called to handle accelerators when the event is unhandled by the web
  // content and the browser client.
  bool HandleKeyboardEvent(const content::NativeWebKeyboardEvent& event);

  // CefBrowserView methods:
  CefRefPtr<CefBrowser> GetBrowser() override;
  CefRefPtr<CefView> GetChromeToolbar() override;
  void SetPreferAccelerators(bool prefer_accelerators) override;

  // CefView methods:
  CefRefPtr<CefBrowserView> AsBrowserView() override { return this; }
  void RequestFocus() override;
  void SetBackgroundColor(cef_color_t color) override;

  // CefViewAdapter methods:
  void Detach() override;
  std::string GetDebugType() override { return "BrowserView"; }
  void GetDebugInfo(base::Value::Dict* info, bool include_children) override;

  // CefBrowserViewView::Delegate methods:
  void OnBrowserViewAdded() override;
  void OnBoundsChanged() override;
  bool OnGestureEvent(ui::GestureEvent* event) override;

  // Return the WebView representation of this object.
  views::WebView* web_view() const;

  // Return the CEF specialization of BrowserView.
  ChromeBrowserView* chrome_browser_view() const;

  // Return the CefWindowImpl hosting this object.
  CefWindowImpl* cef_window_impl() const;

 private:
  // Create a new implementation object.
  // Always call Initialize() after creation.
  // |delegate| may be nullptr.
  explicit CefBrowserViewImpl(CefRefPtr<CefBrowserViewDelegate> delegate);

  void SetPendingBrowserCreateParams(
      const CefWindowInfo& window_info,
      CefRefPtr<CefClient> client,
      const CefString& url,
      const CefBrowserSettings& settings,
      CefRefPtr<CefDictionaryValue> extra_info,
      CefRefPtr<CefRequestContext> request_context);

  void SetDefaults(const CefBrowserSettings& settings);

  // CefViewImpl methods:
  views::View* CreateRootView() override;
  void InitializeRootView() override;

  // Logic extracted from UnhandledKeyboardEventHandler::HandleKeyboardEvent for
  // the handling of accelerators. Returns true if the event was handled by the
  // accelerator.
  bool HandleAccelerator(const content::NativeWebKeyboardEvent& event,
                         views::FocusManager* focus_manager);

  void RequestFocusInternal();

  std::unique_ptr<CefBrowserCreateParams> pending_browser_create_params_;

  CefRefPtr<CefBrowserHostBase> browser_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  bool ignore_next_char_event_ = false;

  base::RepeatingClosure on_bounds_changed_;

  base::WeakPtrFactory<CefBrowserViewImpl> weak_ptr_factory_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefBrowserViewImpl);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_IMPL_H_
