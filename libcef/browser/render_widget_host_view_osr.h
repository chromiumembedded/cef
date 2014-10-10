// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_RENDER_WIDGET_HOST_VIEW_OSR_H_
#define CEF_LIBCEF_BROWSER_RENDER_WIDGET_HOST_VIEW_OSR_H_
#pragma once

#include <vector>

#include "include/cef_base.h"
#include "include/cef_browser.h"

#include "base/memory/weak_ptr.h"
#include "content/browser/compositor/delegated_frame_host.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "ui/compositor/compositor.h"

#if defined(OS_MACOSX)
#include "content/browser/compositor/browser_compositor_view_mac.h"
#endif

#if defined(OS_WIN)
#include "ui/gfx/win/window_impl.h"
#endif

namespace content {
class RenderWidgetHost;
class RenderWidgetHostImpl;
class BackingStore;
}

class CefBrowserHostImpl;
class CefWebContentsViewOSR;

#if defined(OS_MACOSX)
#ifdef __OBJC__
@class CALayer;
@class NSWindow;
#else
class CALayer;
class NSWindow;
#endif
#endif

#if defined(USE_X11)
class CefWindowX11;
#endif

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
//    "The lifetime of the RenderWidgetHostView is tied to the render process.
//     If the render process dies, the RenderWidgetHostView goes away and all
//     references to it must become NULL."
//
// RenderWidgetHostView class hierarchy described in render_widget_host_view.h.
///////////////////////////////////////////////////////////////////////////////

class CefRenderWidgetHostViewOSR
    : public content::RenderWidgetHostViewBase,
#if defined(OS_MACOSX)
      public content::BrowserCompositorViewMacClient,
#endif
      public content::DelegatedFrameHostClient {
 public:
  explicit CefRenderWidgetHostViewOSR(content::RenderWidgetHost* widget);
  virtual ~CefRenderWidgetHostViewOSR();

  // RenderWidgetHostView implementation.
  virtual void InitAsChild(gfx::NativeView parent_view) OVERRIDE;
  virtual content::RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::Vector2dF GetLastScrollOffset() const OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;
  virtual ui::TextInputClient* GetTextInputClient() OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual bool HasFocus() const OVERRIDE;
  virtual bool IsSurfaceAvailableForCopy() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
  virtual void SetBackgroundOpaque(bool opaque) OVERRIDE;
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;

#if defined(OS_MACOSX)
  virtual void SetActive(bool active) OVERRIDE;
  virtual void SetTakesFocusOnlyOnMouseDown(bool flag) OVERRIDE;
  virtual void SetWindowVisibility(bool visible) OVERRIDE;
  virtual void WindowFrameChanged() OVERRIDE;
  virtual void ShowDefinitionForSelection() OVERRIDE;
  virtual bool SupportsSpeech() const OVERRIDE;
  virtual void SpeakSelection() OVERRIDE;
  virtual bool IsSpeaking() const OVERRIDE;
  virtual void StopSpeaking() OVERRIDE;
#endif  // defined(OS_MACOSX)

  // RenderWidgetHostViewBase implementation.
  virtual void OnSwapCompositorFrame(
      uint32 output_surface_id,
      scoped_ptr<cc::CompositorFrame> frame) OVERRIDE;
  virtual void InitAsPopup(content::RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE;
  virtual void InitAsFullscreen(
      content::RenderWidgetHostView* reference_host_view) OVERRIDE;
  virtual void WasShown() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void MovePluginWindows(
      const std::vector<content::WebPluginGeometry>& moves) OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual void UpdateCursor(const content::WebCursor& cursor) OVERRIDE;
  virtual void SetIsLoading(bool is_loading) OVERRIDE;
  virtual void TextInputStateChanged(
      const ViewHostMsg_TextInputState_Params& params) OVERRIDE;
  virtual void ImeCancelComposition() OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status,
                                 int error_code) OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void SetTooltipText(const base::string16& tooltip_text) OVERRIDE;
  virtual void SelectionChanged(const base::string16& text,
                                size_t offset,
                                const gfx::Range& range) OVERRIDE;
  virtual gfx::Size GetRequestedRendererSize() const OVERRIDE;
  virtual gfx::Size GetPhysicalBackingSize() const OVERRIDE;
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) OVERRIDE;
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      const SkColorType color_type) OVERRIDE;
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual bool CanCopyToVideoFrame() const OVERRIDE;
  virtual bool CanSubscribeFrame() const OVERRIDE;
  virtual void BeginFrameSubscription(
      scoped_ptr<content::RenderWidgetHostViewFrameSubscriber> subscriber)
      OVERRIDE;
  virtual void EndFrameSubscription() OVERRIDE;
  virtual void AcceleratedSurfaceInitialized(int host_id,
                                             int route_id) OVERRIDE;
  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params_in_pixel,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params_in_pixel,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfaceSuspend() OVERRIDE;
  virtual void AcceleratedSurfaceRelease() OVERRIDE;
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) OVERRIDE;
  virtual void GetScreenInfo(blink::WebScreenInfo* results) OVERRIDE;
  virtual gfx::Rect GetBoundsInRootWindow() OVERRIDE;
  virtual gfx::GLSurfaceHandle GetCompositingSurface() OVERRIDE;
  virtual content::BrowserAccessibilityManager*
      CreateBrowserAccessibilityManager(
          content::BrowserAccessibilityDelegate* delegate) OVERRIDE;

#if defined(TOOLKIT_VIEWS) || defined(USE_AURA)
  virtual void ShowDisambiguationPopup(const gfx::Rect& rect_pixels,
                                       const SkBitmap& zoomed_bitmap) OVERRIDE;
#endif

#if defined(OS_MACOSX)
  virtual bool PostProcessEventForPluginIme(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
#endif

#if defined(OS_MACOSX) || defined(USE_AURA)
  virtual void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) OVERRIDE;
#endif

#if defined(OS_WIN)
  virtual void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent) OVERRIDE;
  virtual gfx::NativeViewId GetParentForWindowlessPlugin() const OVERRIDE;
#endif

#if defined(OS_MACOSX)
  // BrowserCompositorViewMacClient implementation.
  virtual bool BrowserCompositorViewShouldAckImmediately() const OVERRIDE;
  virtual void BrowserCompositorViewFrameSwapped(
      const std::vector<ui::LatencyInfo>& latency_info) OVERRIDE;
  virtual NSView* BrowserCompositorSuperview() OVERRIDE;
  virtual ui::Layer* BrowserCompositorRootLayer() OVERRIDE;
#endif  // defined(OS_MACOSX)

  // DelegatedFrameHostClient implementation.
  virtual ui::Compositor* GetCompositor() const OVERRIDE;
  virtual ui::Layer* GetLayer() OVERRIDE;
  virtual content::RenderWidgetHostImpl* GetHost() OVERRIDE;
  virtual bool IsVisible() OVERRIDE;
  virtual scoped_ptr<content::ResizeLock> CreateResizeLock(
      bool defer_compositor_lock) OVERRIDE;
  virtual gfx::Size DesiredFrameSize() OVERRIDE;
  virtual float CurrentDeviceScaleFactor() OVERRIDE;
  virtual gfx::Size ConvertViewSizeToPixel(const gfx::Size& size) OVERRIDE;
  virtual content::DelegatedFrameHost* GetDelegatedFrameHost() const OVERRIDE;

  bool InstallTransparency();

  void WasResized();
  void OnScreenInfoChanged();
  void Invalidate(CefBrowserHost::PaintElementType type);
  void SendKeyEvent(const content::NativeWebKeyboardEvent& event);
  void SendMouseEvent(const blink::WebMouseEvent& event);
  void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  void SendFocusEvent(bool focus);

  void HoldResize();
  void ReleaseResize();

  bool IsPopupWidget() const {
    return popup_type_ != blink::WebPopupTypeNone;
  }

#if defined(OS_MACOSX)
  NSTextInputContext* GetNSTextInputContext();
  void HandleKeyEventBeforeTextInputClient(CefEventHandle keyEvent);
  void HandleKeyEventAfterTextInputClient(CefEventHandle keyEvent);

  bool GetCachedFirstRectForCharacterRange(gfx::Range range, gfx::Rect* rect,
                                           gfx::Range* actual_range) const;
#endif  // defined(OS_MACOSX)

  CefRefPtr<CefBrowserHostImpl> browser_impl() const { return browser_impl_; }
  void set_browser_impl(CefRefPtr<CefBrowserHostImpl> browser) {
    browser_impl_ = browser;
  }

  void set_popup_host_view(CefRenderWidgetHostViewOSR* popup_view) {
    popup_host_view_ = popup_view;
  }

  ui::Compositor* compositor() const { return compositor_.get(); }
  content::RenderWidgetHostImpl* render_widget_host() const
      { return render_widget_host_; }

 private:
  void SetFrameRate();
  void ResizeRootLayer();

  // Implementation based on RendererOverridesHandler::InnerSwapCompositorFrame
  // and DelegatedFrameHost::CopyFromCompositingSurface.
  void GenerateFrame(bool force_frame);
  void InternalGenerateFrame();
  void CopyFromCompositingSurfaceHasResult(
      scoped_ptr<cc::CopyOutputResult> result);
  void PrepareTextureCopyOutputResult(
      scoped_ptr<cc::CopyOutputResult> result);
  static void CopyFromCompositingSurfaceFinishedProxy(
      base::WeakPtr<CefRenderWidgetHostViewOSR> view,
      scoped_ptr<cc::SingleReleaseCallback> release_callback,
      scoped_ptr<SkBitmap> bitmap,
      scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock,
      bool result);
  void CopyFromCompositingSurfaceFinished(
      scoped_ptr<SkBitmap> bitmap,
      scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock,
      bool result);
  void PrepareBitmapCopyOutputResult(
      scoped_ptr<cc::CopyOutputResult> result);
  void OnFrameCaptureFailure();
  void OnFrameCaptureSuccess(
      const SkBitmap& bitmap,
      scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock);
  void OnFrameCaptureCompletion(bool force_frame);

  void CancelPopupWidget();

  void OnScrollOffsetChanged();

#if defined(OS_MACOSX)
  // Returns composition character boundary rectangle. The |range| is
  // composition based range. Also stores |actual_range| which is corresponding
  // to actually used range for returned rectangle.
  gfx::Rect GetFirstRectForCompositionRange(const gfx::Range& range,
                                            gfx::Range* actual_range) const;

  // Converts from given whole character range to composition oriented range. If
  // the conversion failed, return gfx::Range::InvalidRange.
  gfx::Range ConvertCharacterRangeToCompositionRange(
      const gfx::Range& request_range) const;

  // Returns true if there is line break in |range| and stores line breaking
  // point to |line_breaking_point|. The |line_break_point| is valid only if
  // this function returns true.
  static bool GetLineBreakIndex(const std::vector<gfx::Rect>& bounds,
                                const gfx::Range& range,
                                size_t* line_break_point);

  void DestroyNSTextInputOSR();
#endif  // defined(OS_MACOSX)

  void PlatformCreateCompositorWidget();
  void PlatformDestroyCompositorWidget();

  scoped_ptr<content::DelegatedFrameHost> delegated_frame_host_;
  scoped_ptr<ui::Compositor> compositor_;
  gfx::AcceleratedWidget compositor_widget_;
  scoped_ptr<ui::Layer> root_layer_;

#if defined(OS_WIN)
  scoped_ptr<gfx::WindowImpl> window_;
#elif defined(OS_MACOSX)
  NSWindow* window_;
  CALayer* background_layer_;
  scoped_ptr<content::BrowserCompositorViewMac> compositor_view_;
#elif defined(USE_X11)
  CefWindowX11* window_;
#endif

  int frame_rate_threshold_ms_;
  base::TimeTicks frame_start_time_;
  bool frame_pending_;
  bool frame_in_progress_;
  int frame_retry_count_;
  scoped_ptr<SkBitmap> bitmap_;

  bool hold_resize_;
  bool pending_resize_;

  // The associated Model.  While |this| is being Destroyed,
  // |render_widget_host_| is NULL and the message loop is run one last time
  // Message handlers must check for a NULL |render_widget_host_|.
  content::RenderWidgetHostImpl* render_widget_host_;
  CefRenderWidgetHostViewOSR* parent_host_view_;
  CefRenderWidgetHostViewOSR* popup_host_view_;

  CefRefPtr<CefBrowserHostImpl> browser_impl_;

  bool is_showing_;
  bool is_destroyed_;
  gfx::Rect popup_position_;

  // The last scroll offset of the view.
  gfx::Vector2dF last_scroll_offset_;
  bool is_scroll_offset_changed_pending_;

#if defined(OS_MACOSX)
  NSTextInputContext* text_input_context_osr_mac_;
#endif

  base::WeakPtrFactory<CefRenderWidgetHostViewOSR> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefRenderWidgetHostViewOSR);
};

#endif  // CEF_LIBCEF_BROWSER_RENDER_WIDGET_HOST_VIEW_OSR_H_

