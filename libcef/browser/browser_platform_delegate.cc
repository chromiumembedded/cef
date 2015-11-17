// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_platform_delegate.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/osr/browser_platform_delegate_osr.h"

#include "base/logging.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"

CefBrowserPlatformDelegate::CefBrowserPlatformDelegate()
    : browser_(nullptr) {
}

CefBrowserPlatformDelegate::~CefBrowserPlatformDelegate() {
  DCHECK(!browser_);
}

void CefBrowserPlatformDelegate::CreateViewForWebContents(
    content::WebContentsView** view,
    content::RenderViewHostDelegateView** delegate_view) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::WebContentsCreated(
    content::WebContents* web_contents) {
}

void CefBrowserPlatformDelegate::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  // Indicate that the view has an external parent (namely us). This changes the
  // default view behavior in some cases (e.g. focus handling on Linux).
  if (render_view_host->GetWidget()->GetView())
    render_view_host->GetWidget()->GetView()->SetHasExternalParent(true);
}

void CefBrowserPlatformDelegate::BrowserCreated(CefBrowserHostImpl* browser) {
  DCHECK(!browser_);
  browser_ = browser;
}

void CefBrowserPlatformDelegate::BrowserDestroyed(CefBrowserHostImpl* browser) {
  DCHECK(browser_ && browser_ == browser);
  browser_ = nullptr;
}

bool CefBrowserPlatformDelegate::CreateHostWindow() {
  NOTREACHED();
  return true;
}

void CefBrowserPlatformDelegate::CloseHostWindow() {
  NOTREACHED();
}

#if defined(USE_AURA)
views::Widget* CefBrowserPlatformDelegate::GetWindowWidget() const {
  NOTREACHED();
  return nullptr;
}
#endif

void CefBrowserPlatformDelegate::SendCaptureLostEvent() {
  content::RenderWidgetHostImpl* widget =
      content::RenderWidgetHostImpl::From(
          browser_->web_contents()->GetRenderViewHost()->GetWidget());
  if (widget)
    widget->LostCapture();
}

#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MACOSX))
void CefBrowserPlatformDelegate::NotifyMoveOrResizeStarted() {
  // Dismiss any existing popups.
  content::RenderViewHost* host = browser_->web_contents()->GetRenderViewHost();
  if (host)
    host->NotifyMoveOrResizeStarted();
}

void CefBrowserPlatformDelegate::SizeTo(int width, int height) {
}
#endif

#if defined(OS_MACOSX)
void CefBrowserPlatformDelegate::SetWindowVisibility(bool visible) {
  content::RenderWidgetHostView* view =
      browser_->web_contents()->GetRenderWidgetHostView();
  if (view)
    view->SetWindowVisibility(visible);
}
#endif

scoped_ptr<CefFileDialogRunner>
    CefBrowserPlatformDelegate::CreateFileDialogRunner() {
  return nullptr;
}

scoped_ptr<CefJavaScriptDialogRunner>
    CefBrowserPlatformDelegate::CreateJavaScriptDialogRunner() {
  return nullptr;
}

void CefBrowserPlatformDelegate::WasHidden(bool hidden) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::NotifyScreenInfoChanged() {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::Invalidate(cef_paint_element_type_t type) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::SetWindowlessFrameRate(int frame_rate) {
  NOTREACHED();
}

#if defined(OS_MACOSX)
CefTextInputContext CefBrowserPlatformDelegate::GetNSTextInputContext() {
  NOTREACHED();
  return nullptr;
}

void CefBrowserPlatformDelegate::HandleKeyEventBeforeTextInputClient(
    CefEventHandle keyEvent) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::HandleKeyEventAfterTextInputClient(
    CefEventHandle keyEvent) {
  NOTREACHED();
}
#endif

void CefBrowserPlatformDelegate::DragTargetDragEnter(
    CefRefPtr<CefDragData> drag_data,
    const CefMouseEvent& event,
    cef_drag_operations_mask_t allowed_ops) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::DragTargetDragOver(
    const CefMouseEvent& event,
    cef_drag_operations_mask_t allowed_ops) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::DragTargetDragLeave() {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::DragTargetDrop(const CefMouseEvent& event) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::DragSourceEndedAt(
    int x, int y,
    cef_drag_operations_mask_t op) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::DragSourceSystemDragEnded() {
  NOTREACHED();
}

// static
int CefBrowserPlatformDelegate::TranslateModifiers(uint32 cef_modifiers) {
  int webkit_modifiers = 0;
  // Set modifiers based on key state.
  if (cef_modifiers & EVENTFLAG_SHIFT_DOWN)
    webkit_modifiers |= blink::WebInputEvent::ShiftKey;
  if (cef_modifiers & EVENTFLAG_CONTROL_DOWN)
    webkit_modifiers |= blink::WebInputEvent::ControlKey;
  if (cef_modifiers & EVENTFLAG_ALT_DOWN)
    webkit_modifiers |= blink::WebInputEvent::AltKey;
  if (cef_modifiers & EVENTFLAG_COMMAND_DOWN)
    webkit_modifiers |= blink::WebInputEvent::MetaKey;
  if (cef_modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
    webkit_modifiers |= blink::WebInputEvent::LeftButtonDown;
  if (cef_modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
    webkit_modifiers |= blink::WebInputEvent::MiddleButtonDown;
  if (cef_modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
    webkit_modifiers |= blink::WebInputEvent::RightButtonDown;
  if (cef_modifiers & EVENTFLAG_CAPS_LOCK_ON)
    webkit_modifiers |= blink::WebInputEvent::CapsLockOn;
  if (cef_modifiers & EVENTFLAG_NUM_LOCK_ON)
    webkit_modifiers |= blink::WebInputEvent::NumLockOn;
  if (cef_modifiers & EVENTFLAG_IS_LEFT)
    webkit_modifiers |= blink::WebInputEvent::IsLeft;
  if (cef_modifiers & EVENTFLAG_IS_RIGHT)
    webkit_modifiers |= blink::WebInputEvent::IsRight;
  if (cef_modifiers & EVENTFLAG_IS_KEY_PAD)
    webkit_modifiers |= blink::WebInputEvent::IsKeyPad;
  return webkit_modifiers;
}
