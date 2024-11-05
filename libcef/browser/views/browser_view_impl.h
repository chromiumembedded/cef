// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_IMPL_H_
#pragma once

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cef/include/cef_client.h"
#include "cef/include/views/cef_browser_view.h"
#include "cef/include/views/cef_browser_view_delegate.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/views/browser_view_view.h"
#include "cef/libcef/browser/views/view_impl.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

class CefBrowserHostBase;
class CefWidget;
class CefWindowImpl;
class ChromeBrowserView;
class Profile;

class CefBrowserViewImpl
    : public CefViewImpl<views::View, CefBrowserView, CefBrowserViewDelegate>,
      public CefBrowserViewView::Delegate {
 public:
  using ParentClass =
      CefViewImpl<views::View, CefBrowserView, CefBrowserViewDelegate>;

  CefBrowserViewImpl(const CefBrowserViewImpl&) = delete;
  CefBrowserViewImpl& operator=(const CefBrowserViewImpl&) = delete;

  ~CefBrowserViewImpl() override;

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
      CefRefPtr<CefBrowserViewDelegate> delegate,
      bool is_devtools,
      cef_runtime_style_t opener_runtime_style);

  // Called from CefBrowserPlatformDelegate[Chrome]Views.
  void WebContentsCreated(content::WebContents* web_contents);
  void WebContentsDestroyed(content::WebContents* web_contents);
  void BrowserCreated(CefBrowserHostBase* browser,
                      base::RepeatingClosure on_bounds_changed);
  void BrowserDestroyed(CefBrowserHostBase* browser);
  void RequestFocusSync();

  // Called to handle accelerators when the event is unhandled by the web
  // content and the browser client.
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event);

  // CefBrowserView methods:
  CefRefPtr<CefBrowser> GetBrowser() override;
  CefRefPtr<CefView> GetChromeToolbar() override;
  void SetPreferAccelerators(bool prefer_accelerators) override;
  cef_runtime_style_t GetRuntimeStyle() override;

  // CefView methods:
  CefRefPtr<CefBrowserView> AsBrowserView() override { return this; }
  void RequestFocus() override;
  void SetBackgroundColor(cef_color_t color) override;

  // CefViewAdapter methods:
  void Detach() override;
  std::string GetDebugType() override { return "BrowserView"; }
  void GetDebugInfo(base::Value::Dict* info, bool include_children) override;

  // CefBrowserViewView::Delegate methods:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnBoundsChanged() override;
  bool OnGestureEvent(ui::GestureEvent* event) override;

  // Return the WebView representation of this object.
  views::WebView* web_view() const;
  views::View* content_view() const override { return web_view(); }

  // Return the CEF specialization of BrowserView.
  ChromeBrowserView* chrome_browser_view() const;

  // Return the CefWindowImpl hosting this object.
  CefWindowImpl* cef_window_impl() const;

  bool IsAlloyStyle() const { return is_alloy_style_; }
  bool IsChromeStyle() const { return !is_alloy_style_; }

  base::WeakPtr<CefBrowserViewImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Create a new implementation object.
  // Always call Initialize() after creation.
  // |delegate| may be nullptr.
  CefBrowserViewImpl(CefRefPtr<CefBrowserViewDelegate> delegate,
                     bool is_devtools_popup,
                     std::optional<cef_runtime_style_t> opener_runtime_style);

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
  bool HandleAccelerator(const input::NativeWebKeyboardEvent& event,
                         views::FocusManager* focus_manager);

  void DisassociateFromWidget();

  // True if the browser is Alloy style, otherwise Chrome style.
  const bool is_alloy_style_;

  std::unique_ptr<CefBrowserCreateParams> pending_browser_create_params_;

  CefRefPtr<CefBrowserHostBase> browser_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  bool ignore_next_char_event_ = false;

  base::RepeatingClosure on_bounds_changed_;

  raw_ptr<CefWidget> cef_widget_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;

  base::WeakPtrFactory<CefBrowserViewImpl> weak_ptr_factory_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefBrowserViewImpl);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BROWSER_VIEW_IMPL_H_
