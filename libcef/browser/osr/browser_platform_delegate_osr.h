// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_H_

#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/native/browser_platform_delegate_native.h"

class CefRenderWidgetHostViewOSR;
class CefWebContentsViewOSR;

// Base implementation of windowless browser functionality.
class CefBrowserPlatformDelegateOsr :
    public CefBrowserPlatformDelegate,
    public CefBrowserPlatformDelegateNative::WindowlessHandler {
 public:
  // CefBrowserPlatformDelegate methods:
  void CreateViewForWebContents(
      content::WebContentsView** view,
      content::RenderViewHostDelegateView** delegate_view) override;
  void WebContentsCreated(content::WebContents* web_contents) override;
  void BrowserCreated(CefBrowserHostImpl* browser) override;
  void BrowserDestroyed(CefBrowserHostImpl* browser) override;
  void WasResized() override;
  void SendKeyEvent(const content::NativeWebKeyboardEvent& event) override;
  void SendMouseEvent(const blink::WebMouseEvent& event) override;
  void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event) override;
  void SendFocusEvent(bool setFocus) override;
  gfx::Point GetScreenPoint(const gfx::Point& view) const override;
  void ViewText(const std::string& text) override;
  void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  void HandleExternalProtocol(const GURL& url) override;
  void TranslateKeyEvent(content::NativeWebKeyboardEvent& result,
                         const CefKeyEvent& key_event) const override;
  void TranslateClickEvent(blink::WebMouseEvent& result,
                           const CefMouseEvent& mouse_event,
                           CefBrowserHost::MouseButtonType type,
                           bool mouseUp, int clickCount) const override;
  void TranslateMoveEvent(blink::WebMouseEvent& result,
                          const CefMouseEvent& mouse_event,
                          bool mouseLeave) const override;
  void TranslateWheelEvent(blink::WebMouseWheelEvent& result,
                           const CefMouseEvent& mouse_event,
                           int deltaX, int deltaY) const override;
  CefEventHandle GetEventHandle(
      const content::NativeWebKeyboardEvent& event) const override;
  scoped_ptr<CefFileDialogRunner> CreateFileDialogRunner() override;
  scoped_ptr<CefJavaScriptDialogRunner> CreateJavaScriptDialogRunner() override;
  scoped_ptr<CefMenuRunner> CreateMenuRunner() override;
  bool IsWindowless() const override;
  void WasHidden(bool hidden) override;
  void NotifyScreenInfoChanged() override;
  void Invalidate(cef_paint_element_type_t type) override;
  void SetWindowlessFrameRate(int frame_rate) override;
  void DragTargetDragEnter(CefRefPtr<CefDragData> drag_data,
                           const CefMouseEvent& event,
                           cef_drag_operations_mask_t allowed_ops) override;
  void DragTargetDragOver(const CefMouseEvent& event,
                          cef_drag_operations_mask_t allowed_ops) override;
  void DragTargetDragLeave() override;
  void DragTargetDrop(const CefMouseEvent& event) override;
  void DragSourceEndedAt(int x, int y,
                         cef_drag_operations_mask_t op) override;
  void DragSourceSystemDragEnded() override;

  // CefBrowserPlatformDelegateNative::WindowlessHandler methods:
  CefWindowHandle GetParentWindowHandle() const override;
  gfx::Point GetParentScreenPoint(const gfx::Point& view) const override;

 protected:
  // Platform-specific behaviors will be delegated to |native_delegate|.
  explicit CefBrowserPlatformDelegateOsr(
      scoped_ptr<CefBrowserPlatformDelegateNative> native_delegate);

  // Returns the primary OSR host view for the underlying browser. If a
  // full-screen host view currently exists then it will be returned. Otherwise,
  // the main host view will be returned.
  CefRenderWidgetHostViewOSR* GetOSRHostView() const;

  scoped_ptr<CefBrowserPlatformDelegateNative> native_delegate_;
  CefWebContentsViewOSR* view_osr_;  // Not owned by this class.
};

#endif  // CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_H_
