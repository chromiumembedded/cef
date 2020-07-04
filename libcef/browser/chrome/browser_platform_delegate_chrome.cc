// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome/browser_platform_delegate_chrome.h"

#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

CefBrowserPlatformDelegateChrome::CefBrowserPlatformDelegateChrome(
    SkColor background_color)
    : background_color_(background_color) {}

content::WebContents* CefBrowserPlatformDelegateChrome::CreateWebContents(
    CefBrowserHostImpl::CreateParams& create_params,
    bool& own_web_contents) {
  // Get or create the request context and profile.
  CefRefPtr<CefRequestContextImpl> request_context_impl =
      CefRequestContextImpl::GetOrCreateForRequestContext(
          create_params.request_context);
  CHECK(request_context_impl);
  auto cef_browser_context = request_context_impl->GetBrowserContext();
  CHECK(cef_browser_context);
  auto profile = cef_browser_context->AsProfile();

  if (!create_params.request_context) {
    // Using the global request context.
    create_params.request_context = request_context_impl.get();
  }

  // Create a Browser.
  Browser::CreateParams params =
      Browser::CreateParams(profile, /*user_gesture=*/false);
  chrome_browser_ = new Browser(params);

  chrome::AddTabAt(chrome_browser_, create_params.url, /*idx=*/-1,
                   /*foreground=*/true);

  auto web_contents =
      chrome_browser_->tab_strip_model()->GetActiveWebContents();
  CHECK(web_contents);

  own_web_contents = false;
  return web_contents;
}

void CefBrowserPlatformDelegateChrome::WebContentsDestroyed(
    content::WebContents* web_contents) {
  CefBrowserPlatformDelegate::WebContentsDestroyed(web_contents);

  // TODO(chrome-runtime): Find a better way to be notified of Browser
  // destruction.
  browser_->WindowDestroyed();
}

void CefBrowserPlatformDelegateChrome::BrowserDestroyed(
    CefBrowserHostImpl* browser) {
  CefBrowserPlatformDelegate::BrowserDestroyed(browser);

  // Release the reference added in CreateHostWindow.
  browser->Release();
}

bool CefBrowserPlatformDelegateChrome::CreateHostWindow() {
  // Keep a reference to the CEF browser.
  browser_->AddRef();

  chrome_browser_->window()->Show();
  return true;
}

void CefBrowserPlatformDelegateChrome::CloseHostWindow() {}

CefWindowHandle CefBrowserPlatformDelegateChrome::GetHostWindowHandle() const {
  return kNullWindowHandle;
}

SkColor CefBrowserPlatformDelegateChrome::GetBackgroundColor() const {
  return background_color_;
}

void CefBrowserPlatformDelegateChrome::WasResized() {}

void CefBrowserPlatformDelegateChrome::SendKeyEvent(const CefKeyEvent& event) {}

void CefBrowserPlatformDelegateChrome::SendMouseClickEvent(
    const CefMouseEvent& event,
    CefBrowserHost::MouseButtonType type,
    bool mouseUp,
    int clickCount) {}

void CefBrowserPlatformDelegateChrome::SendMouseMoveEvent(
    const CefMouseEvent& event,
    bool mouseLeave) {}

void CefBrowserPlatformDelegateChrome::SendMouseWheelEvent(
    const CefMouseEvent& event,
    int deltaX,
    int deltaY) {}

void CefBrowserPlatformDelegateChrome::SendTouchEvent(
    const CefTouchEvent& event) {}

void CefBrowserPlatformDelegateChrome::SendFocusEvent(bool setFocus) {}

gfx::Point CefBrowserPlatformDelegateChrome::GetScreenPoint(
    const gfx::Point& view) const {
  return view;
}

void CefBrowserPlatformDelegateChrome::ViewText(const std::string& text) {}

bool CefBrowserPlatformDelegateChrome::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return false;
}

CefEventHandle CefBrowserPlatformDelegateChrome::GetEventHandle(
    const content::NativeWebKeyboardEvent& event) const {
  return kNullEventHandle;
}

std::unique_ptr<CefMenuRunner>
CefBrowserPlatformDelegateChrome::CreateMenuRunner() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool CefBrowserPlatformDelegateChrome::IsWindowless() const {
  return false;
}

bool CefBrowserPlatformDelegateChrome::IsViewsHosted() const {
  return false;
}
