// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/browser_platform_delegate_osr.h"

#include <utility>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/image_impl.h"
#include "libcef/browser/osr/osr_accessibility_util.h"
#include "libcef/browser/osr/render_widget_host_view_osr.h"
#include "libcef/browser/osr/web_contents_view_osr.h"
#include "libcef/common/drag_data_impl.h"

#include "base/message_loop/message_loop_current.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_view_host.h"
#include "ui/events/base_event_utils.h"

CefBrowserPlatformDelegateOsr::CefBrowserPlatformDelegateOsr(
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate)
    : native_delegate_(std::move(native_delegate)), view_osr_(nullptr) {
  native_delegate_->set_windowless_handler(this);
}

void CefBrowserPlatformDelegateOsr::CreateViewForWebContents(
    content::WebContentsView** view,
    content::RenderViewHostDelegateView** delegate_view) {
  DCHECK(!view_osr_);

  // Use the OSR view instead of the default platform view.
  view_osr_ = new CefWebContentsViewOSR(
      GetBackgroundColor(), CanUseSharedTexture(), CanUseExternalBeginFrame());
  *view = view_osr_;
  *delegate_view = view_osr_;
}

void CefBrowserPlatformDelegateOsr::WebContentsCreated(
    content::WebContents* web_contents) {
  DCHECK(view_osr_);
  DCHECK(!view_osr_->web_contents());

  // Associate the WebContents with the OSR view.
  view_osr_->WebContentsCreated(web_contents);
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

bool CefBrowserPlatformDelegateOsr::CanUseSharedTexture() const {
  return native_delegate_->CanUseSharedTexture();
}

bool CefBrowserPlatformDelegateOsr::CanUseExternalBeginFrame() const {
  return native_delegate_->CanUseExternalBeginFrame();
}

SkColor CefBrowserPlatformDelegateOsr::GetBackgroundColor() const {
  return native_delegate_->GetBackgroundColor();
}

void CefBrowserPlatformDelegateOsr::SynchronizeVisualProperties() {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->SynchronizeVisualProperties();
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

void CefBrowserPlatformDelegateOsr::SendTouchEvent(const CefTouchEvent& event) {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->SendTouchEvent(event);
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
    if (handler->GetScreenPoint(browser_, view.x(), view.y(), screenX,
                                screenY)) {
      return gfx::Point(screenX, screenY);
    }
  }
  return view;
}

void CefBrowserPlatformDelegateOsr::ViewText(const std::string& text) {
  native_delegate_->ViewText(text);
}

bool CefBrowserPlatformDelegateOsr::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return native_delegate_->HandleKeyboardEvent(event);
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
    bool mouseUp,
    int clickCount) const {
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
    int deltaX,
    int deltaY) const {
  native_delegate_->TranslateWheelEvent(result, mouse_event, deltaX, deltaY);
}

CefEventHandle CefBrowserPlatformDelegateOsr::GetEventHandle(
    const content::NativeWebKeyboardEvent& event) const {
  return native_delegate_->GetEventHandle(event);
}

std::unique_ptr<CefFileDialogRunner>
CefBrowserPlatformDelegateOsr::CreateFileDialogRunner() {
  return native_delegate_->CreateFileDialogRunner();
}

std::unique_ptr<CefJavaScriptDialogRunner>
CefBrowserPlatformDelegateOsr::CreateJavaScriptDialogRunner() {
  return native_delegate_->CreateJavaScriptDialogRunner();
}

std::unique_ptr<CefMenuRunner>
CefBrowserPlatformDelegateOsr::CreateMenuRunner() {
  return native_delegate_->CreateMenuRunner();
}

bool CefBrowserPlatformDelegateOsr::IsWindowless() const {
  return true;
}

bool CefBrowserPlatformDelegateOsr::IsViewsHosted() const {
  return false;
}

void CefBrowserPlatformDelegateOsr::WasHidden(bool hidden) {
  // The WebContentsImpl will notify the OSR view.
  content::WebContentsImpl* web_contents =
      static_cast<content::WebContentsImpl*>(browser_->web_contents());
  if (web_contents) {
    if (hidden)
      web_contents->WasHidden();
    else
      web_contents->WasShown();
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

void CefBrowserPlatformDelegateOsr::SendExternalBeginFrame() {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->SendExternalBeginFrame();
}

void CefBrowserPlatformDelegateOsr::SetWindowlessFrameRate(int frame_rate) {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->UpdateFrameRate();
}

void CefBrowserPlatformDelegateOsr::ImeSetComposition(
    const CefString& text,
    const std::vector<CefCompositionUnderline>& underlines,
    const CefRange& replacement_range,
    const CefRange& selection_range) {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view) {
    view->ImeSetComposition(text, underlines, replacement_range,
                            selection_range);
  }
}

void CefBrowserPlatformDelegateOsr::ImeCommitText(
    const CefString& text,
    const CefRange& replacement_range,
    int relative_cursor_pos) {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->ImeCommitText(text, replacement_range, relative_cursor_pos);
}

void CefBrowserPlatformDelegateOsr::ImeFinishComposingText(
    bool keep_selection) {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->ImeFinishComposingText(keep_selection);
}

void CefBrowserPlatformDelegateOsr::ImeCancelComposition() {
  CefRenderWidgetHostViewOSR* view = GetOSRHostView();
  if (view)
    view->ImeCancelComposition();
}

void CefBrowserPlatformDelegateOsr::DragTargetDragEnter(
    CefRefPtr<CefDragData> drag_data,
    const CefMouseEvent& event,
    cef_drag_operations_mask_t allowed_ops) {
  content::WebContentsImpl* web_contents =
      static_cast<content::WebContentsImpl*>(browser_->web_contents());
  if (!web_contents)
    return;

  if (current_rvh_for_drag_)
    DragTargetDragLeave();

  const gfx::Point client_pt(event.x, event.y);
  gfx::PointF transformed_pt;
  current_rwh_for_drag_ =
      web_contents->GetInputEventRouter()
          ->GetRenderWidgetHostAtPoint(
              web_contents->GetRenderViewHost()->GetWidget()->GetView(),
              gfx::PointF(client_pt), &transformed_pt)
          ->GetWeakPtr();
  current_rvh_for_drag_ = web_contents->GetRenderViewHost();

  drag_data_ = drag_data;
  drag_allowed_ops_ = allowed_ops;

  CefDragDataImpl* data_impl = static_cast<CefDragDataImpl*>(drag_data.get());
  base::AutoLock lock_scope(data_impl->lock());
  content::DropData* drop_data = data_impl->drop_data();
  const gfx::Point& screen_pt = GetScreenPoint(client_pt);
  blink::WebDragOperationsMask ops =
      static_cast<blink::WebDragOperationsMask>(allowed_ops);
  int modifiers = TranslateModifiers(event.modifiers);

  current_rwh_for_drag_->FilterDropData(drop_data);

  // Give the delegate an opportunity to cancel the drag.
  if (web_contents->GetDelegate() && !web_contents->GetDelegate()->CanDragEnter(
                                         web_contents, *drop_data, ops)) {
    drag_data_ = nullptr;
    return;
  }

  current_rwh_for_drag_->DragTargetDragEnter(
      *drop_data, transformed_pt, gfx::PointF(screen_pt), ops, modifiers);
}

void CefBrowserPlatformDelegateOsr::DragTargetDragOver(
    const CefMouseEvent& event,
    cef_drag_operations_mask_t allowed_ops) {
  if (!drag_data_)
    return;

  content::WebContentsImpl* web_contents =
      static_cast<content::WebContentsImpl*>(browser_->web_contents());
  if (!web_contents)
    return;

  const gfx::Point client_pt(event.x, event.y);
  const gfx::Point& screen_pt = GetScreenPoint(client_pt);

  gfx::PointF transformed_pt;
  content::RenderWidgetHostImpl* target_rwh =
      web_contents->GetInputEventRouter()->GetRenderWidgetHostAtPoint(
          web_contents->GetRenderViewHost()->GetWidget()->GetView(),
          gfx::PointF(client_pt), &transformed_pt);

  if (target_rwh != current_rwh_for_drag_.get()) {
    if (current_rwh_for_drag_) {
      gfx::PointF transformed_leave_point(client_pt);
      gfx::PointF transformed_screen_point(screen_pt);
      static_cast<content::RenderWidgetHostViewBase*>(
          web_contents->GetRenderWidgetHostView())
          ->TransformPointToCoordSpaceForView(
              gfx::PointF(client_pt),
              static_cast<content::RenderWidgetHostViewBase*>(
                  current_rwh_for_drag_->GetView()),
              &transformed_leave_point);
      static_cast<content::RenderWidgetHostViewBase*>(
          web_contents->GetRenderWidgetHostView())
          ->TransformPointToCoordSpaceForView(
              gfx::PointF(screen_pt),
              static_cast<content::RenderWidgetHostViewBase*>(
                  current_rwh_for_drag_->GetView()),
              &transformed_screen_point);
      current_rwh_for_drag_->DragTargetDragLeave(transformed_leave_point,
                                                 transformed_screen_point);
    }
    DragTargetDragEnter(drag_data_, event, drag_allowed_ops_);
  }

  if (!drag_data_)
    return;

  blink::WebDragOperationsMask ops =
      static_cast<blink::WebDragOperationsMask>(allowed_ops);
  int modifiers = TranslateModifiers(event.modifiers);

  target_rwh->DragTargetDragOver(transformed_pt, gfx::PointF(screen_pt), ops,
                                 modifiers);
}

void CefBrowserPlatformDelegateOsr::DragTargetDragLeave() {
  if (current_rvh_for_drag_ != browser_->web_contents()->GetRenderViewHost() ||
      !drag_data_) {
    return;
  }

  if (current_rwh_for_drag_) {
    current_rwh_for_drag_->DragTargetDragLeave(gfx::PointF(), gfx::PointF());
    current_rwh_for_drag_.reset();
  }

  drag_data_ = nullptr;
}

void CefBrowserPlatformDelegateOsr::DragTargetDrop(const CefMouseEvent& event) {
  if (!drag_data_)
    return;

  content::WebContentsImpl* web_contents =
      static_cast<content::WebContentsImpl*>(browser_->web_contents());
  if (!web_contents)
    return;

  gfx::Point client_pt(event.x, event.y);
  const gfx::Point& screen_pt = GetScreenPoint(client_pt);

  gfx::PointF transformed_pt;
  content::RenderWidgetHostImpl* target_rwh =
      web_contents->GetInputEventRouter()->GetRenderWidgetHostAtPoint(
          web_contents->GetRenderViewHost()->GetWidget()->GetView(),
          gfx::PointF(client_pt), &transformed_pt);

  if (target_rwh != current_rwh_for_drag_.get()) {
    if (current_rwh_for_drag_) {
      gfx::PointF transformed_leave_point(client_pt);
      gfx::PointF transformed_screen_point(screen_pt);
      static_cast<content::RenderWidgetHostViewBase*>(
          web_contents->GetRenderWidgetHostView())
          ->TransformPointToCoordSpaceForView(
              gfx::PointF(client_pt),
              static_cast<content::RenderWidgetHostViewBase*>(
                  current_rwh_for_drag_->GetView()),
              &transformed_leave_point);
      static_cast<content::RenderWidgetHostViewBase*>(
          web_contents->GetRenderWidgetHostView())
          ->TransformPointToCoordSpaceForView(
              gfx::PointF(screen_pt),
              static_cast<content::RenderWidgetHostViewBase*>(
                  current_rwh_for_drag_->GetView()),
              &transformed_screen_point);
      current_rwh_for_drag_->DragTargetDragLeave(transformed_leave_point,
                                                 transformed_screen_point);
    }
    DragTargetDragEnter(drag_data_, event, drag_allowed_ops_);
  }

  if (!drag_data_)
    return;

  {
    CefDragDataImpl* data_impl =
        static_cast<CefDragDataImpl*>(drag_data_.get());
    base::AutoLock lock_scope(data_impl->lock());
    content::DropData* drop_data = data_impl->drop_data();
    int modifiers = TranslateModifiers(event.modifiers);

    target_rwh->DragTargetDrop(*drop_data, transformed_pt,
                               gfx::PointF(screen_pt), modifiers);
  }

  drag_data_ = nullptr;
}

void CefBrowserPlatformDelegateOsr::StartDragging(
    const content::DropData& drop_data,
    blink::WebDragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const content::DragEventSourceInfo& event_info,
    content::RenderWidgetHostImpl* source_rwh) {
  drag_start_rwh_ = source_rwh->GetWeakPtr();

  bool handled = false;

  CefRefPtr<CefRenderHandler> handler =
      browser_->GetClient()->GetRenderHandler();
  if (handler.get()) {
    CefRefPtr<CefImage> cef_image(new CefImageImpl(image));
    CefPoint cef_image_pos(image_offset.x(), image_offset.y());
    CefRefPtr<CefDragDataImpl> drag_data(
        new CefDragDataImpl(drop_data, cef_image, cef_image_pos));
    drag_data->SetReadOnly(true);
    base::MessageLoopCurrent::ScopedNestableTaskAllower allow;
    handled = handler->StartDragging(
        browser_, drag_data.get(),
        static_cast<CefRenderHandler::DragOperationsMask>(allowed_ops),
        event_info.event_location.x(), event_info.event_location.y());
  }

  if (!handled)
    DragSourceSystemDragEnded();
}

void CefBrowserPlatformDelegateOsr::UpdateDragCursor(
    blink::WebDragOperation operation) {
  CefRefPtr<CefRenderHandler> handler =
      browser_->GetClient()->GetRenderHandler();
  if (handler.get()) {
    handler->UpdateDragCursor(
        browser_, static_cast<CefRenderHandler::DragOperation>(operation));
  }
}

void CefBrowserPlatformDelegateOsr::DragSourceEndedAt(
    int x,
    int y,
    cef_drag_operations_mask_t op) {
  if (!drag_start_rwh_)
    return;

  content::WebContentsImpl* web_contents =
      static_cast<content::WebContentsImpl*>(browser_->web_contents());
  if (!web_contents)
    return;

  content::RenderWidgetHostImpl* source_rwh = drag_start_rwh_.get();
  const gfx::Point client_loc(gfx::Point(x, y));
  const gfx::Point& screen_loc = GetScreenPoint(client_loc);
  blink::WebDragOperation drag_op = static_cast<blink::WebDragOperation>(op);

  // |client_loc| and |screen_loc| are in the root coordinate space, for
  // non-root RenderWidgetHosts they need to be transformed.
  gfx::PointF transformed_point(client_loc);
  gfx::PointF transformed_screen_point(screen_loc);
  if (source_rwh && web_contents->GetRenderWidgetHostView()) {
    static_cast<content::RenderWidgetHostViewBase*>(
        web_contents->GetRenderWidgetHostView())
        ->TransformPointToCoordSpaceForView(
            gfx::PointF(client_loc),
            static_cast<content::RenderWidgetHostViewBase*>(
                source_rwh->GetView()),
            &transformed_point);
    static_cast<content::RenderWidgetHostViewBase*>(
        web_contents->GetRenderWidgetHostView())
        ->TransformPointToCoordSpaceForView(
            gfx::PointF(screen_loc),
            static_cast<content::RenderWidgetHostViewBase*>(
                source_rwh->GetView()),
            &transformed_screen_point);
  }

  web_contents->DragSourceEndedAt(transformed_point.x(), transformed_point.y(),
                                  transformed_screen_point.x(),
                                  transformed_screen_point.y(), drag_op,
                                  source_rwh);
}

void CefBrowserPlatformDelegateOsr::DragSourceSystemDragEnded() {
  if (!drag_start_rwh_)
    return;

  content::WebContents* web_contents = browser_->web_contents();
  if (!web_contents)
    return;

  web_contents->SystemDragEnded(drag_start_rwh_.get());

  drag_start_rwh_ = nullptr;
}

void CefBrowserPlatformDelegateOsr::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& eventData) {
  CefRefPtr<CefRenderHandler> handler = browser_->client()->GetRenderHandler();
  if (handler.get()) {
    CefRefPtr<CefAccessibilityHandler> acchandler =
        handler->GetAccessibilityHandler();

    if (acchandler.get()) {
      acchandler->OnAccessibilityTreeChange(
          osr_accessibility_util::ParseAccessibilityEventData(eventData));
    }
  }
}

void CefBrowserPlatformDelegateOsr::AccessibilityLocationChangesReceived(
    const std::vector<content::AXLocationChangeNotificationDetails>& locData) {
  CefRefPtr<CefRenderHandler> handler = browser_->client()->GetRenderHandler();
  if (handler.get()) {
    CefRefPtr<CefAccessibilityHandler> acchandler =
        handler->GetAccessibilityHandler();

    if (acchandler.get()) {
      acchandler->OnAccessibilityLocationChange(
          osr_accessibility_util::ParseAccessibilityLocationData(locData));
    }
  }
}

CefWindowHandle CefBrowserPlatformDelegateOsr::GetParentWindowHandle() const {
  return GetHostWindowHandle();
}

gfx::Point CefBrowserPlatformDelegateOsr::GetParentScreenPoint(
    const gfx::Point& view) const {
  return GetScreenPoint(view);
}

CefRenderWidgetHostViewOSR* CefBrowserPlatformDelegateOsr::GetOSRHostView()
    const {
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
