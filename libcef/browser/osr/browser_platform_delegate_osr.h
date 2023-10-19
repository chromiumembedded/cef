// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_H_

#include "libcef/browser/alloy/browser_platform_delegate_alloy.h"
#include "libcef/browser/native/browser_platform_delegate_native.h"

class CefRenderWidgetHostViewOSR;
class CefWebContentsViewOSR;

namespace content {
class RenderWidgetHostImpl;
}  // namespace content

// Base implementation of windowless browser functionality.
class CefBrowserPlatformDelegateOsr
    : public CefBrowserPlatformDelegateAlloy,
      public CefBrowserPlatformDelegateNative::WindowlessHandler {
 public:
  // CefBrowserPlatformDelegate methods:
  void CreateViewForWebContents(
      content::WebContentsView** view,
      content::RenderViewHostDelegateView** delegate_view) override;
  void WebContentsCreated(content::WebContents* web_contents,
                          bool owned) override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void BrowserCreated(CefBrowserHostBase* browser) override;
  void NotifyBrowserDestroyed() override;
  void BrowserDestroyed(CefBrowserHostBase* browser) override;
  SkColor GetBackgroundColor() const override;
  void WasResized() override;
  void SendKeyEvent(const CefKeyEvent& event) override;
  void SendMouseClickEvent(const CefMouseEvent& event,
                           CefBrowserHost::MouseButtonType type,
                           bool mouseUp,
                           int clickCount) override;
  void SendMouseMoveEvent(const CefMouseEvent& event, bool mouseLeave) override;
  void SendMouseWheelEvent(const CefMouseEvent& event,
                           int deltaX,
                           int deltaY) override;
  void SendTouchEvent(const CefTouchEvent& event) override;
  void SetFocus(bool setFocus) override;
  gfx::Point GetScreenPoint(const gfx::Point& view,
                            bool want_dip_coords) const override;
  void ViewText(const std::string& text) override;
  bool HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  CefEventHandle GetEventHandle(
      const content::NativeWebKeyboardEvent& event) const override;
  std::unique_ptr<CefJavaScriptDialogRunner> CreateJavaScriptDialogRunner()
      override;
  std::unique_ptr<CefMenuRunner> CreateMenuRunner() override;
  bool IsWindowless() const override;
  void WasHidden(bool hidden) override;
  bool IsHidden() const override;
  void NotifyScreenInfoChanged() override;
  void Invalidate(cef_paint_element_type_t type) override;
  void SendExternalBeginFrame() override;
  void SetWindowlessFrameRate(int frame_rate) override;
  void ImeSetComposition(const CefString& text,
                         const std::vector<CefCompositionUnderline>& underlines,
                         const CefRange& replacement_range,
                         const CefRange& selection_range) override;
  void ImeCommitText(const CefString& text,
                     const CefRange& replacement_range,
                     int relative_cursor_pos) override;
  void ImeFinishComposingText(bool keep_selection) override;
  void ImeCancelComposition() override;
  void DragTargetDragEnter(CefRefPtr<CefDragData> drag_data,
                           const CefMouseEvent& event,
                           cef_drag_operations_mask_t allowed_ops) override;
  void DragTargetDragOver(const CefMouseEvent& event,
                          cef_drag_operations_mask_t allowed_ops) override;
  void DragTargetDragLeave() override;
  void DragTargetDrop(const CefMouseEvent& event) override;
  void StartDragging(const content::DropData& drop_data,
                     blink::DragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const blink::mojom::DragEventSourceInfo& event_info,
                     content::RenderWidgetHostImpl* source_rwh) override;
  void UpdateDragOperation(ui::mojom::DragOperation operation,
                           bool document_is_handling_drag) override;
  void DragSourceEndedAt(int x, int y, cef_drag_operations_mask_t op) override;
  void DragSourceSystemDragEnded() override;
  void AccessibilityEventReceived(
      const content::AXEventNotificationDetails& eventData) override;
  void AccessibilityLocationChangesReceived(
      const std::vector<content::AXLocationChangeNotificationDetails>& locData)
      override;

  // CefBrowserPlatformDelegateNative::WindowlessHandler methods:
  CefWindowHandle GetParentWindowHandle() const override;
  gfx::Point GetParentScreenPoint(const gfx::Point& view,
                                  bool want_dip_coords) const override;

 protected:
  // Platform-specific behaviors will be delegated to |native_delegate|.
  CefBrowserPlatformDelegateOsr(
      std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
      bool use_shared_texture,
      bool use_external_begin_frame);

  // Returns the primary OSR host view for the underlying browser. If a
  // full-screen host view currently exists then it will be returned. Otherwise,
  // the main host view will be returned.
  CefRenderWidgetHostViewOSR* GetOSRHostView() const;

  std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate_;
  const bool use_shared_texture_;
  const bool use_external_begin_frame_;

  CefWebContentsViewOSR* view_osr_ = nullptr;  // Not owned by this class.

  // Pending drag/drop data.
  CefRefPtr<CefDragData> drag_data_;
  cef_drag_operations_mask_t drag_allowed_ops_;

  // We keep track of the RenderWidgetHost we're dragging over. If it changes
  // during a drag, we need to re-send the DragEnter message.
  base::WeakPtr<content::RenderWidgetHostImpl> current_rwh_for_drag_;

  // We also keep track of the RenderViewHost we're dragging over to avoid
  // sending the drag exited message after leaving the current
  // view. |current_rvh_for_drag_| should not be dereferenced.
  void* current_rvh_for_drag_ = nullptr;

  // We keep track of the RenderWidgetHost from which the current drag started,
  // in order to properly route the drag end message to it.
  base::WeakPtr<content::RenderWidgetHostImpl> drag_start_rwh_;

  // Set to true when the document is handling the drag. This means that the
  // document has registered an interest in the dropped data and the renderer
  // process should pass the data to the document on drop.
  bool document_is_handling_drag_ = false;
};

#endif  // CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_H_
