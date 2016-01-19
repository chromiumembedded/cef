// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/browser_view_impl.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"

// static
CefRefPtr<CefBrowserView> CefBrowserView::CreateBrowserView(
    CefRefPtr<CefClient> client,
      const CefString& url,
      const CefBrowserSettings& settings,
      CefRefPtr<CefRequestContext> request_context,
      CefRefPtr<CefBrowserViewDelegate> delegate) {
  return CefBrowserViewImpl::Create(client, url, settings, request_context,
                                    delegate);
}

// static
CefRefPtr<CefBrowserView> CefBrowserView::GetForBrowser(
    CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  CefBrowserHostImpl* browser_impl =
      static_cast<CefBrowserHostImpl*>(browser.get());
  if (browser_impl && browser_impl->IsViewsHosted())
    return browser_impl->GetBrowserView();
  return nullptr;
}

// static
CefRefPtr<CefBrowserViewImpl> CefBrowserViewImpl::Create(
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefRequestContext> request_context,
    CefRefPtr<CefBrowserViewDelegate> delegate) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  CefRefPtr<CefBrowserViewImpl> browser_view = new CefBrowserViewImpl(delegate);
  browser_view->SetPendingBrowserCreateParams(client, url, settings,
                                              request_context);
  browser_view->Initialize();
  browser_view->SetDefaults(settings);
  return browser_view;
}

// static
CefRefPtr<CefBrowserViewImpl> CefBrowserViewImpl::CreateForPopup(
    const CefBrowserSettings& settings,
    CefRefPtr<CefBrowserViewDelegate> delegate) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  CefRefPtr<CefBrowserViewImpl> browser_view = new CefBrowserViewImpl(delegate);
  browser_view->Initialize();
  browser_view->SetDefaults(settings);
  return browser_view;
}

void CefBrowserViewImpl::WebContentsCreated(
    content::WebContents* web_contents) {
  if (root_view())
    root_view()->SetWebContents(web_contents);
}

void CefBrowserViewImpl::BrowserCreated(CefBrowserHostImpl* browser) {
  browser_ = browser;
}

void CefBrowserViewImpl::BrowserDestroyed(CefBrowserHostImpl* browser) {
  DCHECK_EQ(browser, browser_);
  browser_ = nullptr;

  if (root_view())
    root_view()->SetWebContents(nullptr);
}

CefRefPtr<CefBrowser> CefBrowserViewImpl::GetBrowser() {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  return browser_;
}

void CefBrowserViewImpl::SetBackgroundColor(cef_color_t color) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::SetBackgroundColor(color);
  if (root_view())
    root_view()->SetResizeBackgroundColor(color);
}

void CefBrowserViewImpl::Detach() {
  ParentClass::Detach();

  // root_view() will be nullptr now.
  DCHECK(!root_view());

  if (browser_) {
    // |browser_| will disappear when WindowDestroyed() indirectly calls
    // BrowserDestroyed() so keep a reference.
    CefRefPtr<CefBrowserHostImpl> browser = browser_;

    // Force the browser to be destroyed.
    browser->WindowDestroyed();
  }
}

void CefBrowserViewImpl::GetDebugInfo(base::DictionaryValue* info,
                                      bool include_children) {
  ParentClass::GetDebugInfo(info, include_children);
  if (browser_)
    info->SetString("url", browser_->GetMainFrame()->GetURL().ToString());
}

void CefBrowserViewImpl::OnBrowserViewAdded() {
  if (!browser_ && pending_browser_create_params_) {
    // Top-level browsers will be created when this view is added to the views
    // hierarchy.
    pending_browser_create_params_->browser_view = this;

    CefBrowserHostImpl::Create(*pending_browser_create_params_);
    DCHECK(browser_);

    pending_browser_create_params_.reset(nullptr);
  }
}

CefBrowserViewImpl::CefBrowserViewImpl(
    CefRefPtr<CefBrowserViewDelegate> delegate)
    : ParentClass(delegate) {
}

void CefBrowserViewImpl::SetPendingBrowserCreateParams(
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefRequestContext> request_context) {
  DCHECK(!pending_browser_create_params_);
  pending_browser_create_params_.reset(new CefBrowserHostImpl::CreateParams());
  pending_browser_create_params_->client = client;
  pending_browser_create_params_->url = url;
  pending_browser_create_params_->settings = settings;
  pending_browser_create_params_->request_context = request_context;
}

void CefBrowserViewImpl::SetDefaults(const CefBrowserSettings& settings) {
  SkColor background_color = SK_ColorWHITE;
  if (CefColorGetA(settings.background_color) > 0) {
    background_color = SkColorSetRGB(
        CefColorGetR(settings.background_color),
        CefColorGetG(settings.background_color),
        CefColorGetB(settings.background_color));
  } else {
    const CefSettings& global_settings = CefContext::Get()->settings();
    if (CefColorGetA(global_settings.background_color) > 0) {
      background_color = SkColorSetRGB(
          CefColorGetR(global_settings.background_color),
          CefColorGetG(global_settings.background_color),
          CefColorGetB(global_settings.background_color));
    }
  }
  SetBackgroundColor(background_color);
}

CefBrowserViewView* CefBrowserViewImpl::CreateRootView() {
  return new CefBrowserViewView(delegate(), this);
}
