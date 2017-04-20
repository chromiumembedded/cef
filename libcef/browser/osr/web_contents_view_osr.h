// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_WEB_CONTENTS_VIEW_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_WEB_CONTENTS_VIEW_OSR_H_

#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {
class BrowserPluginGuest;
class WebContents;
class WebContentsViewDelegate;
}

class CefBrowserHostImpl;
class CefRenderWidgetHostViewOSR;

// An implementation of WebContentsView for off-screen rendering.
class CefWebContentsViewOSR : public content::WebContentsView,
                              public content::RenderViewHostDelegateView {
 public:
  explicit CefWebContentsViewOSR(SkColor background_color);
  ~CefWebContentsViewOSR() override;

  void WebContentsCreated(content::WebContents* web_contents);
  content::WebContents* web_contents() const { return web_contents_; }

  // WebContentsView methods.
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetContentNativeView() const override;
  gfx::NativeWindow GetTopLevelNativeWindow() const override;
  void GetScreenInfo(content::ScreenInfo* screen_info) const override;
  void GetContainerBounds(gfx::Rect* out) const override;
  void SizeContents(const gfx::Size& size) override;
  void Focus() override;
  void SetInitialFocus() override;
  void StoreFocus() override;
  void RestoreFocus() override;
  content::DropData* GetDropData() const override;
  gfx::Rect GetViewBounds() const override;
  void CreateView(const gfx::Size& initial_size,
                  gfx::NativeView context) override;
  content::RenderWidgetHostViewBase* CreateViewForWidget(
      content::RenderWidgetHost* render_widget_host,
      content::RenderWidgetHost* embedder_render_widget_host) override;
  content::RenderWidgetHostViewBase* CreateViewForPopupWidget(
      content::RenderWidgetHost* render_widget_host) override;
  void SetPageTitle(const base::string16& title) override;
  void RenderViewCreated(content::RenderViewHost* host) override;
  void RenderViewSwappedIn(content::RenderViewHost* host) override;
  void SetOverscrollControllerEnabled(bool enabled) override;

#if defined(OS_MACOSX)
  void SetAllowOtherViews(bool allow) override;
  bool GetAllowOtherViews() const override;
  bool IsEventTracking() const override;
  void CloseTabAfterEventTracking() override;
#endif

  // RenderViewHostDelegateView methods.
  void StartDragging(
      const content::DropData& drop_data,
      blink::WebDragOperationsMask allowed_ops,
      const gfx::ImageSkia& image,
      const gfx::Vector2d& image_offset,
      const content::DragEventSourceInfo& event_info,
      content::RenderWidgetHostImpl* source_rwh) override;
  void UpdateDragCursor(blink::WebDragOperation operation) override;

 private:
  CefRenderWidgetHostViewOSR* GetView() const;
  CefBrowserHostImpl* GetBrowser() const;

  const SkColor background_color_;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(CefWebContentsViewOSR);
};

#endif  // CEF_LIBCEF_BROWSER_OSR_WEB_CONTENTS_VIEW_OSR_H_
