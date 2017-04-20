// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/browser_view_impl.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_util.h"
#include "libcef/browser/context.h"
#include "libcef/browser/views/window_impl.h"
#include "libcef/browser/thread_util.h"

#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/content_accelerators/accelerator_util.h"

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

void CefBrowserViewImpl::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  if (!root_view())
    return;

  views::FocusManager* focus_manager = root_view()->GetFocusManager();
  if (!focus_manager)
    return;

  if (HandleAccelerator(event, focus_manager))
    return;

  // Give the CefWindowDelegate a chance to handle the event.
  CefRefPtr<CefWindow> window =
      view_util::GetWindowFor(root_view()->GetWidget());
  CefWindowImpl* window_impl = static_cast<CefWindowImpl*>(window.get());
  if (window_impl) {
    CefKeyEvent cef_event;
    if (browser_util::GetCefKeyEvent(event, cef_event) &&
        window_impl->OnKeyEvent(cef_event)) {
      return;
    }
  }

  // Proceed with default native handling.
  unhandled_keyboard_event_handler_.HandleKeyboardEvent(event, focus_manager);
}

CefRefPtr<CefBrowser> CefBrowserViewImpl::GetBrowser() {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  return browser_;
}

void CefBrowserViewImpl::SetPreferAccelerators(bool prefer_accelerators) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (root_view())
    root_view()->set_allow_accelerators(prefer_accelerators);
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
  SetBackgroundColor(
      CefContext::Get()->GetBackgroundColor(&settings, STATE_DISABLED));
}

CefBrowserViewView* CefBrowserViewImpl::CreateRootView() {
  return new CefBrowserViewView(delegate(), this);
}

void CefBrowserViewImpl::InitializeRootView() {
  static_cast<CefBrowserViewView*>(root_view())->Initialize();
}

bool CefBrowserViewImpl::HandleAccelerator(
    const content::NativeWebKeyboardEvent& event,
    views::FocusManager* focus_manager) {
  // Previous calls to TranslateMessage can generate Char events as well as
  // RawKeyDown events, even if the latter triggered an accelerator.  In these
  // cases, we discard the Char events.
  if (event.GetType() == blink::WebInputEvent::kChar &&
      ignore_next_char_event_) {
    ignore_next_char_event_ = false;
    return true;
  }

  // It's necessary to reset this flag, because a RawKeyDown event may not
  // always generate a Char event.
  ignore_next_char_event_ = false;

  if (event.GetType() == blink::WebInputEvent::kRawKeyDown) {
    ui::Accelerator accelerator =
        ui::GetAcceleratorFromNativeWebKeyboardEvent(event);

    // This is tricky: we want to set ignore_next_char_event_ if
    // ProcessAccelerator returns true. But ProcessAccelerator might delete
    // |this| if the accelerator is a "close tab" one. So we speculatively
    // set the flag and fix it if no event was handled.
    ignore_next_char_event_ = true;

    if (focus_manager->ProcessAccelerator(accelerator))
      return true;

    // ProcessAccelerator didn't handle the accelerator, so we know both
    // that |this| is still valid, and that we didn't want to set the flag.
    ignore_next_char_event_ = false;
  }

  return false;
}
