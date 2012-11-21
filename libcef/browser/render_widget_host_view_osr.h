// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_RENDER_WIDGET_HOST_VIEW_OSR_H_
#define CEF_LIBCEF_BROWSER_RENDER_WIDGET_HOST_VIEW_OSR_H_
#pragma once

#include <vector>

#include "include/cef_base.h"

#include "content/browser/renderer_host/render_widget_host_view_base.h"

namespace content {
  class RenderWidgetHost;
  class BackingStore;
}

class CefBrowserHostImpl;
class CefWebContentsViewOSR;

///////////////////////////////////////////////////////////////////////////////
// CefRenderWidgetHostViewOSR
//
//  An object representing the "View" of a rendered web page. This object is
//  responsible for sending paint events to the the CefRenderHandler
//  when window rendering is disabled. It is the implementation of the
//  RenderWidgetHostView that the cross-platform RenderWidgetHost object uses
//  to display the data.
//
//  Comment excerpted from render_widget_host.h:
//
//    "The lifetime of the RenderWidgetHostHWND is tied to the render process.
//     If the render process dies, the RenderWidgetHostHWND goes away and all
//     references to it must become NULL."
//
// RenderWidgetHostView class hierarchy described in render_widget_host_view.h.
///////////////////////////////////////////////////////////////////////////////

class CefRenderWidgetHostViewOSR : public content::RenderWidgetHostViewBase {
 public:
  // RenderWidgetHostView methods.
  virtual void InitAsChild(gfx::NativeView parent_view) OVERRIDE;
  virtual content::RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;
  virtual bool HasFocus() const OVERRIDE;
  virtual bool IsSurfaceAvailableForCopy() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;

  // RenderWidgetHostViewPort methods.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
      const gfx::Rect& pos) OVERRIDE;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) OVERRIDE;
  virtual void WasShown() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void MovePluginWindows(
      const gfx::Vector2d& scroll_offset,
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual void UpdateCursor(const WebCursor& cursor) OVERRIDE;
  virtual void SetIsLoading(bool is_loading) OVERRIDE;
  virtual void TextInputStateChanged(
      const ViewHostMsg_TextInputState_Params& params) OVERRIDE;
  virtual void ImeCancelComposition() OVERRIDE;
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect,
      const gfx::Vector2d& scroll_delta,
      const std::vector<gfx::Rect>& copy_rects) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status,
      int error_code) OVERRIDE;
#if defined(OS_WIN) && !defined(USE_AURA)
  virtual void WillWmDestroy() OVERRIDE;
#endif
  virtual gfx::Rect GetBoundsInRootWindow() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void SetTooltipText(const string16& tooltip_text) OVERRIDE;
  virtual content::BackingStore*
      AllocBackingStore(const gfx::Size& size) OVERRIDE;
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool)>& callback,
      skia::PlatformBitmap* output) OVERRIDE;
  virtual void OnAcceleratedCompositingStateChange() OVERRIDE;
  virtual void SetHasHorizontalScrollbar(
      bool has_horizontal_scrollbar) OVERRIDE;
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) OVERRIDE;
  virtual gfx::GLSurfaceHandle GetCompositingSurface() OVERRIDE;
  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfaceSuspend() OVERRIDE;
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) OVERRIDE;
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;

#if defined(OS_WIN) && !defined(USE_AURA)
  virtual void SetClickthroughRegion(SkRegion* region) OVERRIDE;
#endif

  // CefRenderWidgetHostViewOSR methods
  void Invalidate(const gfx::Rect& rect);
  void Paint(const std::vector<gfx::Rect>& copy_rects);

  CefRefPtr<CefBrowserHostImpl> get_browser_impl() const;
  void set_browser_impl(CefRefPtr<CefBrowserHostImpl> browser);

 private:
  friend class CefWebContentsViewOSR;

  explicit CefRenderWidgetHostViewOSR(content::RenderWidgetHost* widget);
  virtual ~CefRenderWidgetHostViewOSR();

  // The associated Model.  While |this| is being Destroyed,
  // |render_widget_host_| is NULL and the Windows message loop is run one last
  // time. Message handlers must check for a NULL |render_widget_host_|.
  content::RenderWidgetHostImpl* render_widget_host_;

  CefRefPtr<CefBrowserHostImpl> browser_impl_;

  bool about_to_validate_and_paint_;
  std::vector<gfx::Rect> pending_update_rects_;

  DISALLOW_COPY_AND_ASSIGN(CefRenderWidgetHostViewOSR);
};

#endif  // CEF_LIBCEF_BROWSER_RENDER_WIDGET_HOST_VIEW_OSR_H_
