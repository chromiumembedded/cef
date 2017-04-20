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
  if (!IsViewsHosted() && render_view_host->GetWidget()->GetView())
    render_view_host->GetWidget()->GetView()->SetHasExternalParent(true);
}

void CefBrowserPlatformDelegate::BrowserCreated(CefBrowserHostImpl* browser) {
  DCHECK(!browser_);
  browser_ = browser;
}

void CefBrowserPlatformDelegate::NotifyBrowserCreated() {
}

void CefBrowserPlatformDelegate::NotifyBrowserDestroyed() {
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

CefRefPtr<CefBrowserView> CefBrowserPlatformDelegate::GetBrowserView() const {
  NOTREACHED();
  return nullptr;
}
#endif  // defined(USE_AURA)

void CefBrowserPlatformDelegate::PopupWebContentsCreated(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* new_web_contents,
    CefBrowserPlatformDelegate* new_platform_delegate,
    bool is_devtools) {
}

void CefBrowserPlatformDelegate::PopupBrowserCreated(
    CefBrowserHostImpl* new_browser,
    bool is_devtools) {
}

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

std::unique_ptr<CefFileDialogRunner>
    CefBrowserPlatformDelegate::CreateFileDialogRunner() {
  return nullptr;
}

std::unique_ptr<CefJavaScriptDialogRunner>
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

void CefBrowserPlatformDelegate::ImeSetComposition(const CefString& text,
    const std::vector<CefCompositionUnderline>& underlines,
    const CefRange& replacement_range,
    const CefRange& selection_range) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::ImeCommitText(
    const CefString& text,
    const CefRange& replacement_range,
    int relative_cursor_pos) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::ImeFinishComposingText(bool keep_selection) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::ImeCancelComposition() {
  NOTREACHED();
}

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

void CefBrowserPlatformDelegate::StartDragging(
    const content::DropData& drop_data,
    blink::WebDragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const content::DragEventSourceInfo& event_info,
    content::RenderWidgetHostImpl* source_rwh) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::UpdateDragCursor(
    blink::WebDragOperation operation) {
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
    webkit_modifiers |= blink::WebInputEvent::kShiftKey;
  if (cef_modifiers & EVENTFLAG_CONTROL_DOWN)
    webkit_modifiers |= blink::WebInputEvent::kControlKey;
  if (cef_modifiers & EVENTFLAG_ALT_DOWN)
    webkit_modifiers |= blink::WebInputEvent::kAltKey;
  if (cef_modifiers & EVENTFLAG_COMMAND_DOWN)
    webkit_modifiers |= blink::WebInputEvent::kMetaKey;
  if (cef_modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
    webkit_modifiers |= blink::WebInputEvent::kLeftButtonDown;
  if (cef_modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
    webkit_modifiers |= blink::WebInputEvent::kMiddleButtonDown;
  if (cef_modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
    webkit_modifiers |= blink::WebInputEvent::kRightButtonDown;
  if (cef_modifiers & EVENTFLAG_CAPS_LOCK_ON)
    webkit_modifiers |= blink::WebInputEvent::kCapsLockOn;
  if (cef_modifiers & EVENTFLAG_NUM_LOCK_ON)
    webkit_modifiers |= blink::WebInputEvent::kNumLockOn;
  if (cef_modifiers & EVENTFLAG_IS_LEFT)
    webkit_modifiers |= blink::WebInputEvent::kIsLeft;
  if (cef_modifiers & EVENTFLAG_IS_RIGHT)
    webkit_modifiers |= blink::WebInputEvent::kIsRight;
  if (cef_modifiers & EVENTFLAG_IS_KEY_PAD)
    webkit_modifiers |= blink::WebInputEvent::kIsKeyPad;
  return webkit_modifiers;
}
