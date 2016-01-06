// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/browser_platform_delegate_osr.h"

#include <utility>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/osr/render_widget_host_view_osr.h"
#include "libcef/browser/osr/web_contents_view_osr.h"
#include "libcef/common/drag_data_impl.h"

#include "content/public/browser/render_view_host.h"

CefBrowserPlatformDelegateOsr::CefBrowserPlatformDelegateOsr(
    scoped_ptr<CefBrowserPlatformDelegateNative> native_delegate)
    : native_delegate_(std::move(native_delegate)),
      view_osr_(nullptr) {
  native_delegate_->set_windowless_handler(this);
}

void CefBrowserPlatformDelegateOsr::CreateViewForWebContents(
    content::WebContentsView** view,
    content::RenderViewHostDelegateView** delegate_view) {
  DCHECK(!view_osr_);

  // Use the OSR view instead of the default platform view.
  view_osr_ = new CefWebContentsViewOSR(
      !!native_delegate_->window_info().transparent_painting_enabled);
  *view = view_osr_;
  *delegate_view = view_osr_;
}

void CefBrowserPlatformDelegateOsr::WebContentsCreated(
    content::WebContents* web_contents) {
  DCHECK(view_osr_);
  DCHECK(!view_osr_->web_contents());

  // Associate the WebContents with the OSR view.
  view_osr_->set_web_contents(web_contents);
}

void CefBrowserPlatformDelegateOsr::BrowserCreated(
    CefBrowserHostImpl* browser) {
  CefBrowserPlatformDelegate::BrowserCreated(browser);

  if (browser->IsPopup()) {
    // Associate the RenderWidget host view with the browser now because the
    // browser wasn't known at the time that the host view was created.
    content::RenderViewHost* host =
        browser->web_contents()->GetRenderViewHost();
    DCHECK(host);
    CefRenderWidgetHostViewOSR* view =
        static_cast<CefRenderWidgetHostViewOSR*>(host->GetWidget()->GetView());
    // |view| will be null if the popup is a DevTools window.
    if (view)
      view->set_browser_impl(browser);
  }
}

void CefBrowserPlatformDelegateOsr::BrowserDestroyed(
    CefBrowserHostImpl* browser) {
  CefBrowserPlatformDelegate::BrowserDestroyed(browser);

  view_osr_ = nullptr;
}

void CefBrowserPlatformDelegateOsr::WasResized() {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->WasResized();
}

void CefBrowserPlatformDelegateOsr::SendKeyEvent(
    const content::NativeWebKeyboardEvent& event) {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->SendKeyEvent(event);
}

void CefBrowserPlatformDelegateOsr::SendMouseEvent(
    const blink::WebMouseEvent& event) {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->SendMouseEvent(event);
}

void CefBrowserPlatformDelegateOsr::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->SendMouseWheelEvent(event);
}

void CefBrowserPlatformDelegateOsr::SendFocusEvent(bool setFocus) {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->SendFocusEvent(setFocus);
}

gfx::Point CefBrowserPlatformDelegateOsr::GetScreenPoint(
    const gfx::Point& view) const {
  CefRefPtr<CefRenderHandler> handler = browser_->client()->GetRenderHandler();
  if (handler.get()) {
    int screenX = 0, screenY = 0;
    if (handler->GetScreenPoint(browser_, view.x(), view.y(),
                                screenX, screenY)) {
      return gfx::Point(screenX, screenY);
    }
  }
  return view;
}

void CefBrowserPlatformDelegateOsr::ViewText(const std::string& text) {
  native_delegate_->ViewText(text);
}

void CefBrowserPlatformDelegateOsr::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  native_delegate_->HandleKeyboardEvent(event);
}

void CefBrowserPlatformDelegateOsr::HandleExternalProtocol(const GURL& url) {
  native_delegate_->HandleExternalProtocol(url);
}

void CefBrowserPlatformDelegateOsr::TranslateKeyEvent(
    content::NativeWebKeyboardEvent& result,
    const CefKeyEvent& key_event) const {
  native_delegate_->TranslateKeyEvent(result, key_event);
}

void CefBrowserPlatformDelegateOsr::TranslateClickEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    CefBrowserHost::MouseButtonType type,
    bool mouseUp, int clickCount) const {
  native_delegate_->TranslateClickEvent(result, mouse_event, type, mouseUp,
                                        clickCount);
}

void CefBrowserPlatformDelegateOsr::TranslateMoveEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    bool mouseLeave) const {
  native_delegate_->TranslateMoveEvent(result, mouse_event, mouseLeave);
}

void CefBrowserPlatformDelegateOsr::TranslateWheelEvent(
    blink::WebMouseWheelEvent& result,
    const CefMouseEvent& mouse_event,
    int deltaX, int deltaY) const {
  native_delegate_->TranslateWheelEvent(result, mouse_event, deltaX, deltaY);
}

CefEventHandle CefBrowserPlatformDelegateOsr::GetEventHandle(
    const content::NativeWebKeyboardEvent& event) const {
  return native_delegate_->GetEventHandle(event);
}

scoped_ptr<CefFileDialogRunner>
    CefBrowserPlatformDelegateOsr::CreateFileDialogRunner() {
  return native_delegate_->CreateFileDialogRunner();
}

scoped_ptr<CefJavaScriptDialogRunner>
    CefBrowserPlatformDelegateOsr::CreateJavaScriptDialogRunner() {
  return native_delegate_->CreateJavaScriptDialogRunner();
}

scoped_ptr<CefMenuRunner> CefBrowserPlatformDelegateOsr::CreateMenuRunner() {
  return native_delegate_->CreateMenuRunner();
}

bool CefBrowserPlatformDelegateOsr::IsWindowless() const {
  return true;
}

void CefBrowserPlatformDelegateOsr::WasHidden(bool hidden) {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view) {
    if (hidden)
      view->Hide();
    else
      view->Show();
  }
}

void CefBrowserPlatformDelegateOsr::NotifyScreenInfoChanged() {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->OnScreenInfoChanged();
}

void CefBrowserPlatformDelegateOsr::Invalidate(cef_paint_element_type_t type) {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->Invalidate(type);
}

void CefBrowserPlatformDelegateOsr::SetWindowlessFrameRate(int frame_rate) {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->UpdateFrameRate();
}

void CefBrowserPlatformDelegateOsr::DragTargetDragEnter(
    CefRefPtr<CefDragData> drag_data,
    const CefMouseEvent& event,
    cef_drag_operations_mask_t allowed_ops) {
  content::RenderViewHost* rvh = browser_->web_contents()->GetRenderViewHost();
  if (!rvh)
    return;

  CefDragDataImpl* data_impl = static_cast<CefDragDataImpl*>(drag_data.get());
  base::AutoLock lock_scope(data_impl->lock());
  const content::DropData& drop_data = data_impl->drop_data();
  const gfx::Point client_pt(event.x, event.y);
  const gfx::Point& screen_pt = GetScreenPoint(client_pt);
  blink::WebDragOperationsMask ops =
      static_cast<blink::WebDragOperationsMask>(allowed_ops);
  int modifiers = TranslateModifiers(event.modifiers);

  rvh->DragTargetDragEnter(drop_data, client_pt, screen_pt, ops, modifiers);
}

void CefBrowserPlatformDelegateOsr::DragTargetDragOver(
    const CefMouseEvent& event,
    cef_drag_operations_mask_t allowed_ops) {
  content::RenderViewHost* rvh = browser_->web_contents()->GetRenderViewHost();
  if (!rvh)
    return;

  const gfx::Point client_pt(event.x, event.y);
  const gfx::Point& screen_pt = GetScreenPoint(client_pt);
  blink::WebDragOperationsMask ops =
      static_cast<blink::WebDragOperationsMask>(allowed_ops);
  int modifiers = TranslateModifiers(event.modifiers);

  rvh->DragTargetDragOver(client_pt, screen_pt, ops, modifiers);
}

void CefBrowserPlatformDelegateOsr::DragTargetDragLeave() {
  content::RenderViewHost* rvh = browser_->web_contents()->GetRenderViewHost();
  if (!rvh)
    return;

  rvh->DragTargetDragLeave();
}

void CefBrowserPlatformDelegateOsr::DragTargetDrop(const CefMouseEvent& event) {
  content::RenderViewHost* rvh = browser_->web_contents()->GetRenderViewHost();
  if (!rvh)
    return;

  const gfx::Point client_pt(event.x, event.y);
  const gfx::Point& screen_pt = GetScreenPoint(client_pt);
  int modifiers = TranslateModifiers(event.modifiers);

  rvh->DragTargetDrop(client_pt, screen_pt, modifiers);
}

void CefBrowserPlatformDelegateOsr::DragSourceEndedAt(
    int x, int y,
    cef_drag_operations_mask_t op) {
  content::RenderViewHost* rvh = browser_->web_contents()->GetRenderViewHost();
  if (!rvh)
    return;

  const gfx::Point& screen_pt = GetScreenPoint(gfx::Point(x, y));
  blink::WebDragOperation drag_op = static_cast<blink::WebDragOperation>(op);

  rvh->DragSourceEndedAt(x, y, screen_pt.x(), screen_pt.y(), drag_op);
}

void CefBrowserPlatformDelegateOsr::DragSourceSystemDragEnded() {
  content::RenderViewHost* rvh = browser_->web_contents()->GetRenderViewHost();
  if (!rvh)
    return;

  rvh->DragSourceSystemDragEnded();
}

CefWindowHandle CefBrowserPlatformDelegateOsr::GetParentWindowHandle() const {
  return GetHostWindowHandle();
}

gfx::Point CefBrowserPlatformDelegateOsr::GetParentScreenPoint(
    const gfx::Point& view) const {
  return GetScreenPoint(view);
}

CefRenderWidgetHostViewOSR*
    CefBrowserPlatformDelegateOsr::GetOSRHostView() const {
  content::WebContents* web_contents = browser_->web_contents();
  CefRenderWidgetHostViewOSR* fs_view =
      static_cast<CefRenderWidgetHostViewOSR*>(
          web_contents->GetFullscreenRenderWidgetHostView());
  if (fs_view)
    return fs_view;

  content::RenderViewHost* host = web_contents->GetRenderViewHost();
  if (host) {
    return static_cast<CefRenderWidgetHostViewOSR*>(
        host->GetWidget()->GetView());
  }

  return nullptr;
}
