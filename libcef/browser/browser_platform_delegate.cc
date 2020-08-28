// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_platform_delegate.h"

#include "libcef/browser/browser_host_impl.h"

#include "base/logging.h"

CefBrowserPlatformDelegate::CefBrowserPlatformDelegate() = default;

CefBrowserPlatformDelegate::~CefBrowserPlatformDelegate() {
  DCHECK(!browser_);
}

void CefBrowserPlatformDelegate::CreateViewForWebContents(
    content::WebContentsView** view,
    content::RenderViewHostDelegateView** delegate_view) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::WebContentsCreated(
    content::WebContents* web_contents,
    bool owned) {
  // We should not have a browser at this point.
  DCHECK(!browser_);

  DCHECK(!web_contents_);
  web_contents_ = web_contents;
}

void CefBrowserPlatformDelegate::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::WebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK(web_contents_ && web_contents_ == web_contents);
  web_contents_ = nullptr;
}

bool CefBrowserPlatformDelegate::ShouldTransferNavigation(
    bool is_main_frame_navigation) {
  return true;
}

void CefBrowserPlatformDelegate::RenderViewCreated(
    content::RenderViewHost* render_view_host) {}

void CefBrowserPlatformDelegate::RenderViewReady() {}

void CefBrowserPlatformDelegate::BrowserCreated(CefBrowserHostImpl* browser) {
  // We should have an associated WebContents at this point.
  DCHECK(web_contents_);

  DCHECK(!browser_);
  DCHECK(browser);
  browser_ = browser;
}

void CefBrowserPlatformDelegate::CreateExtensionHost(
    const extensions::Extension* extension,
    const GURL& url,
    extensions::ViewType host_type) {
  NOTREACHED();
}

extensions::ExtensionHost* CefBrowserPlatformDelegate::GetExtensionHost()
    const {
  NOTREACHED();
  return nullptr;
}

void CefBrowserPlatformDelegate::NotifyBrowserCreated() {}

void CefBrowserPlatformDelegate::NotifyBrowserDestroyed() {}

void CefBrowserPlatformDelegate::BrowserDestroyed(CefBrowserHostImpl* browser) {
  // WebContentsDestroyed should already be called.
  DCHECK(!web_contents_);

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
#endif

void CefBrowserPlatformDelegate::PopupWebContentsCreated(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* new_web_contents,
    CefBrowserPlatformDelegate* new_platform_delegate,
    bool is_devtools) {}

void CefBrowserPlatformDelegate::PopupBrowserCreated(
    CefBrowserHostImpl* new_browser,
    bool is_devtools) {}

void CefBrowserPlatformDelegate::SendCaptureLostEvent() {
  NOTIMPLEMENTED();
}

#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MAC))
void CefBrowserPlatformDelegate::NotifyMoveOrResizeStarted() {}

void CefBrowserPlatformDelegate::SizeTo(int width, int height) {}
#endif

bool CefBrowserPlatformDelegate::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  return false;
}

bool CefBrowserPlatformDelegate::IsNeverComposited(
    content::WebContents* web_contents) {
  return false;
}

std::unique_ptr<CefFileDialogRunner>
CefBrowserPlatformDelegate::CreateFileDialogRunner() {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<CefJavaScriptDialogRunner>
CefBrowserPlatformDelegate::CreateJavaScriptDialogRunner() {
  NOTIMPLEMENTED();
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

void CefBrowserPlatformDelegate::SendExternalBeginFrame() {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::SetWindowlessFrameRate(int frame_rate) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::ImeSetComposition(
    const CefString& text,
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
    int x,
    int y,
    cef_drag_operations_mask_t op) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::DragSourceSystemDragEnded() {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& eventData) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::AccessibilityLocationChangesReceived(
    const std::vector<content::AXLocationChangeNotificationDetails>& locData) {
  NOTREACHED();
}

gfx::Point CefBrowserPlatformDelegate::GetDialogPosition(
    const gfx::Size& size) {
  NOTREACHED();
  return gfx::Point();
}

gfx::Size CefBrowserPlatformDelegate::GetMaximumDialogSize() {
  NOTREACHED();
  return gfx::Size();
}

void CefBrowserPlatformDelegate::SetAutoResizeEnabled(bool enabled,
                                                      const CefSize& min_size,
                                                      const CefSize& max_size) {
  NOTIMPLEMENTED();
}

void CefBrowserPlatformDelegate::SetAccessibilityState(
    cef_state_t accessibility_state) {
  NOTIMPLEMENTED();
}

bool CefBrowserPlatformDelegate::IsPrintPreviewSupported() const {
  return true;
}

void CefBrowserPlatformDelegate::Print() {
  NOTIMPLEMENTED();
}

void CefBrowserPlatformDelegate::PrintToPDF(
    const CefString& path,
    const CefPdfPrintSettings& settings,
    CefRefPtr<CefPdfPrintCallback> callback) {
  NOTIMPLEMENTED();
}

void CefBrowserPlatformDelegate::Find(int identifier,
                                      const CefString& searchText,
                                      bool forward,
                                      bool matchCase,
                                      bool findNext) {
  NOTIMPLEMENTED();
}

void CefBrowserPlatformDelegate::StopFinding(bool clearSelection) {
  NOTIMPLEMENTED();
}

// static
int CefBrowserPlatformDelegate::TranslateWebEventModifiers(
    uint32 cef_modifiers) {
  int result = 0;
  // Set modifiers based on key state.
  if (cef_modifiers & EVENTFLAG_SHIFT_DOWN)
    result |= blink::WebInputEvent::kShiftKey;
  if (cef_modifiers & EVENTFLAG_CONTROL_DOWN)
    result |= blink::WebInputEvent::kControlKey;
  if (cef_modifiers & EVENTFLAG_ALT_DOWN)
    result |= blink::WebInputEvent::kAltKey;
  if (cef_modifiers & EVENTFLAG_COMMAND_DOWN)
    result |= blink::WebInputEvent::kMetaKey;
  if (cef_modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
    result |= blink::WebInputEvent::kLeftButtonDown;
  if (cef_modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
    result |= blink::WebInputEvent::kMiddleButtonDown;
  if (cef_modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
    result |= blink::WebInputEvent::kRightButtonDown;
  if (cef_modifiers & EVENTFLAG_CAPS_LOCK_ON)
    result |= blink::WebInputEvent::kCapsLockOn;
  if (cef_modifiers & EVENTFLAG_NUM_LOCK_ON)
    result |= blink::WebInputEvent::kNumLockOn;
  if (cef_modifiers & EVENTFLAG_IS_LEFT)
    result |= blink::WebInputEvent::kIsLeft;
  if (cef_modifiers & EVENTFLAG_IS_RIGHT)
    result |= blink::WebInputEvent::kIsRight;
  if (cef_modifiers & EVENTFLAG_IS_KEY_PAD)
    result |= blink::WebInputEvent::kIsKeyPad;
  return result;
}
