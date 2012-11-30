// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_WEB_CONTENTS_VIEW_OSR_H_
#define CEF_LIBCEF_BROWSER_WEB_CONTENTS_VIEW_OSR_H_

#include <vector>

#include "content/port/browser/render_view_host_delegate_view.h"
#include "content/public/browser/web_contents_view.h"

namespace content {
class WebContents;
class WebContentsViewDelegate;
}

class CefBrowserHostImpl;

// An implementation of WebContentsView for off-screen rendering.
class CefWebContentsViewOSR : public content::WebContentsView,
                              public content::RenderViewHostDelegateView {
 public:
  CefWebContentsViewOSR(content::WebContents* web_contents,
      content::WebContentsViewDelegate* delegate);
  virtual ~CefWebContentsViewOSR();

  // WebContentsView methods.
  virtual void CreateView(const gfx::Size& initial_size) OVERRIDE;
  virtual content::RenderWidgetHostView* CreateViewForWidget(
      content::RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual void SetView(content::RenderWidgetHostView* view) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void GetContainerBounds(gfx::Rect *out) const OVERRIDE;
  virtual void SetPageTitle(const string16& title) OVERRIDE;
  virtual void OnTabCrashed(base::TerminationStatus status,
      int error_code) OVERRIDE;
  virtual void SizeContents(const gfx::Size& size) OVERRIDE;
  virtual void RenderViewCreated(content::RenderViewHost* host) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void SetInitialFocus() OVERRIDE;
  virtual void StoreFocus() OVERRIDE;
  virtual void RestoreFocus() OVERRIDE;
  virtual WebDropData* GetDropData() const OVERRIDE;
  virtual bool IsEventTracking() const OVERRIDE;
  virtual void CloseTabAfterEventTracking() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;

  // RenderViewHostDelegateView methods.
  virtual void StartDragging(const WebDropData& drop_data,
      WebKit::WebDragOperationsMask allowed_ops,
      const gfx::ImageSkia& image,
      const gfx::Vector2d& image_offset,
      const content::DragEventSourceInfo& event_info) OVERRIDE;
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
      int item_height,
      double item_font_size,
      int selected_item,
      const std::vector<WebMenuItem>& items,
      bool right_aligned,
      bool allow_multiple_selection) OVERRIDE;

 private:
  content::WebContents* web_contents_;
  content::RenderWidgetHostView* view_;

  DISALLOW_COPY_AND_ASSIGN(CefWebContentsViewOSR);
};

#endif  // CEF_LIBCEF_BROWSER_WEB_CONTENTS_VIEW_OSR_H_
