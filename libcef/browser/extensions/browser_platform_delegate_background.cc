// Copyright 2017 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/browser_platform_delegate_background.h"

#include <utility>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/thread_util.h"

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"

CefBrowserPlatformDelegateBackground::CefBrowserPlatformDelegateBackground(
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate)
    : native_delegate_(std::move(native_delegate)) {
  native_delegate_->set_windowless_handler(this);
}

bool CefBrowserPlatformDelegateBackground::CreateHostWindow() {
  // Nothing to do here.
  return true;
}

void CefBrowserPlatformDelegateBackground::CloseHostWindow() {
  // No host window, so continue browser destruction now. Do it asynchronously
  // so the call stack has a chance to unwind.
  CEF_POST_TASK(CEF_UIT,
                base::Bind(&CefBrowserHostImpl::WindowDestroyed, browser_));
}

CefWindowHandle CefBrowserPlatformDelegateBackground::GetHostWindowHandle()
    const {
  return kNullWindowHandle;
}

bool CefBrowserPlatformDelegateBackground::CanUseSharedTexture() const {
  return native_delegate_->CanUseSharedTexture();
}

bool CefBrowserPlatformDelegateBackground::CanUseExternalBeginFrame() const {
  return native_delegate_->CanUseExternalBeginFrame();
}

SkColor CefBrowserPlatformDelegateBackground::GetBackgroundColor() const {
  return native_delegate_->GetBackgroundColor();
}

void CefBrowserPlatformDelegateBackground::SynchronizeVisualProperties() {
  // Nothing to do here.
}

void CefBrowserPlatformDelegateBackground::SendKeyEvent(
    const content::NativeWebKeyboardEvent& event) {
  // Nothing to do here.
}

void CefBrowserPlatformDelegateBackground::SendMouseEvent(
    const blink::WebMouseEvent& event) {
  // Nothing to do here.
}

void CefBrowserPlatformDelegateBackground::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  // Nothing to do here.
}

void CefBrowserPlatformDelegateBackground::SendTouchEvent(
    const CefTouchEvent& event) {
  // Nothing to do here.
}

void CefBrowserPlatformDelegateBackground::SendFocusEvent(bool setFocus) {
  // Nothing to do here.
}

gfx::Point CefBrowserPlatformDelegateBackground::GetScreenPoint(
    const gfx::Point& view_pt) const {
  // Nothing to do here.
  return view_pt;
}

void CefBrowserPlatformDelegateBackground::ViewText(const std::string& text) {
  native_delegate_->ViewText(text);
}

bool CefBrowserPlatformDelegateBackground::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // Nothing to do here.
  return false;
}

void CefBrowserPlatformDelegateBackground::HandleExternalProtocol(
    const GURL& url) {
  native_delegate_->HandleExternalProtocol(url);
}

void CefBrowserPlatformDelegateBackground::TranslateKeyEvent(
    content::NativeWebKeyboardEvent& result,
    const CefKeyEvent& key_event) const {
  native_delegate_->TranslateKeyEvent(result, key_event);
}

void CefBrowserPlatformDelegateBackground::TranslateClickEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    CefBrowserHost::MouseButtonType type,
    bool mouseUp,
    int clickCount) const {
  native_delegate_->TranslateClickEvent(result, mouse_event, type, mouseUp,
                                        clickCount);
}

void CefBrowserPlatformDelegateBackground::TranslateMoveEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    bool mouseLeave) const {
  native_delegate_->TranslateMoveEvent(result, mouse_event, mouseLeave);
}

void CefBrowserPlatformDelegateBackground::TranslateWheelEvent(
    blink::WebMouseWheelEvent& result,
    const CefMouseEvent& mouse_event,
    int deltaX,
    int deltaY) const {
  native_delegate_->TranslateWheelEvent(result, mouse_event, deltaX, deltaY);
}

CefEventHandle CefBrowserPlatformDelegateBackground::GetEventHandle(
    const content::NativeWebKeyboardEvent& event) const {
  return native_delegate_->GetEventHandle(event);
}

std::unique_ptr<CefFileDialogRunner>
CefBrowserPlatformDelegateBackground::CreateFileDialogRunner() {
  return native_delegate_->CreateFileDialogRunner();
}

std::unique_ptr<CefJavaScriptDialogRunner>
CefBrowserPlatformDelegateBackground::CreateJavaScriptDialogRunner() {
  return native_delegate_->CreateJavaScriptDialogRunner();
}

std::unique_ptr<CefMenuRunner>
CefBrowserPlatformDelegateBackground::CreateMenuRunner() {
  // No default menu implementation for background browsers.
  return nullptr;
}

bool CefBrowserPlatformDelegateBackground::IsWindowless() const {
  return false;
}

bool CefBrowserPlatformDelegateBackground::IsViewsHosted() const {
  return false;
}

CefWindowHandle CefBrowserPlatformDelegateBackground::GetParentWindowHandle()
    const {
  return GetHostWindowHandle();
}

gfx::Point CefBrowserPlatformDelegateBackground::GetParentScreenPoint(
    const gfx::Point& view) const {
  return GetScreenPoint(view);
}
