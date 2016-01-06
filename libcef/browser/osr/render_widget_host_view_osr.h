// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_RENDER_WIDGET_HOST_VIEW_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_RENDER_WIDGET_HOST_VIEW_OSR_H_
#pragma once

#include <set>
#include <vector>

#include "include/cef_base.h"
#include "include/cef_browser.h"

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/browser/compositor/delegated_frame_host.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "ui/compositor/compositor.h"

#if defined(OS_LINUX)
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_MACOSX)
#include "content/browser/compositor/browser_compositor_view_mac.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#endif

#if defined(OS_WIN)
#include "ui/gfx/win/window_impl.h"
#endif

namespace content {
class RenderWidgetHost;
class RenderWidgetHostImpl;
class BackingStore;
}

class CefBeginFrameTimer;
class CefBrowserHostImpl;
class CefCopyFrameGenerator;
class CefSoftwareOutputDeviceOSR;
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
      public ui::AcceleratedWidgetMacNSView,
#endif
      public ui::CompositorDelegate,
      public content::DelegatedFrameHostClient {
 public:
  CefRenderWidgetHostViewOSR(const bool transparent,
                             content::RenderWidgetHost* widget,
                             CefRenderWidgetHostViewOSR* parent_host_view);
  ~CefRenderWidgetHostViewOSR() override;

  // RenderWidgetHostView implementation.
  void InitAsChild(gfx::NativeView parent_view) override;
  content::RenderWidgetHost* GetRenderWidgetHost() const override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  gfx::Vector2dF GetLastScrollOffset() const override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewId GetNativeViewId() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  ui::TextInputClient* GetTextInputClient() override;
  void Focus() override;
  bool HasFocus() const override;
  bool IsSurfaceAvailableForCopy() const override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  gfx::Rect GetViewBounds() const override;
  void SetBackgroundColor(SkColor color) override;
  bool LockMouse() override;
  void UnlockMouse() override;

#if defined(OS_MACOSX)
  void SetActive(bool active) override;
  void SetWindowVisibility(bool visible) override;
  void WindowFrameChanged() override;
  void ShowDefinitionForSelection() override;
  bool SupportsSpeech() const override;
  void SpeakSelection() override;
  bool IsSpeaking() const override;
  void StopSpeaking() override;
#endif  // defined(OS_MACOSX)

  // RenderWidgetHostViewBase implementation.
  void OnSwapCompositorFrame(
      uint32_t output_surface_id,
      scoped_ptr<cc::CompositorFrame> frame) override;
  void ClearCompositorFrame() override;
  void InitAsPopup(content::RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& pos) override;
  void InitAsFullscreen(
      content::RenderWidgetHostView* reference_host_view) override;
  void MovePluginWindows(
      const std::vector<content::WebPluginGeometry>& moves) override;
  void UpdateCursor(const content::WebCursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void TextInputStateChanged(
      const ViewHostMsg_TextInputState_Params& params) override;
  void ImeCancelComposition() override;
  void RenderProcessGone(base::TerminationStatus status,
                         int error_code) override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;

#if defined(OS_MACOSX)
  void SelectionChanged(const base::string16& text,
                        size_t offset,
                        const gfx::Range& range) override;
#endif

  gfx::Size GetRequestedRendererSize() const override;
  gfx::Size GetPhysicalBackingSize() const override;
  void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) override;
  void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const content::ReadbackRequestCallback& callback,
      const SkColorType color_type) override;
  void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(const gfx::Rect&, bool)>& callback) override;
  bool CanCopyToVideoFrame() const override;
  void BeginFrameSubscription(
      scoped_ptr<content::RenderWidgetHostViewFrameSubscriber> subscriber)
      override;
  void EndFrameSubscription() override;
  bool HasAcceleratedSurface(const gfx::Size& desired_size) override;
  void GetScreenInfo(blink::WebScreenInfo* results) override;
  bool GetScreenColorProfile(std::vector<char>* color_profile) override;
  gfx::Rect GetBoundsInRootWindow() override;
  content::BrowserAccessibilityManager*
      CreateBrowserAccessibilityManager(
          content::BrowserAccessibilityDelegate* delegate) override;
  void LockCompositingSurface() override;
  void UnlockCompositingSurface() override;

#if defined(TOOLKIT_VIEWS) || defined(USE_AURA)
  void ShowDisambiguationPopup(const gfx::Rect& rect_pixels,
                               const SkBitmap& zoomed_bitmap) override;
#endif

#if defined(OS_MACOSX)
  bool PostProcessEventForPluginIme(
      const content::NativeWebKeyboardEvent& event) override;
#endif

#if defined(OS_MACOSX) || defined(USE_AURA)
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override;
#endif

#if defined(OS_WIN)
  void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent) override;
  gfx::NativeViewId GetParentForWindowlessPlugin() const override;
#endif

#if defined(OS_MACOSX)
  // AcceleratedWidgetMacNSView implementation.
  NSView* AcceleratedWidgetGetNSView() const override;
  void AcceleratedWidgetGetVSyncParameters(
      base::TimeTicks* timebase, base::TimeDelta* interval) const override;
  void AcceleratedWidgetSwapCompleted() override;
#endif  // defined(OS_MACOSX)

  bool OnMessageReceived(const IPC::Message& msg) override;

  // Message handlers.
  void OnSetNeedsBeginFrames(bool enabled);

  // ui::CompositorDelegate implementation.
  scoped_ptr<cc::SoftwareOutputDevice> CreateSoftwareOutputDevice(
      ui::Compositor* compositor) override;

  // DelegatedFrameHostClient implementation.
  ui::Layer* DelegatedFrameHostGetLayer() const override;
  bool DelegatedFrameHostIsVisible() const override;
  gfx::Size DelegatedFrameHostDesiredSizeInDIP() const override;
 bool DelegatedFrameCanCreateResizeLock() const override;
  scoped_ptr<content::ResizeLock> DelegatedFrameHostCreateResizeLock(
      bool defer_compositor_lock) override;
  void DelegatedFrameHostResizeLockWasReleased() override;
  void DelegatedFrameHostSendCompositorSwapAck(
      int output_surface_id,
      const cc::CompositorFrameAck& ack) override;
  void DelegatedFrameHostSendReclaimCompositorResources(
      int output_surface_id,
      const cc::CompositorFrameAck& ack) override;
  void DelegatedFrameHostOnLostCompositorResources() override;
  void DelegatedFrameHostUpdateVSyncParameters(
      const base::TimeTicks& timebase,
      const base::TimeDelta& interval) override;

  bool InstallTransparency();

  void WasResized();
  void OnScreenInfoChanged();
  void Invalidate(CefBrowserHost::PaintElementType type);
  void SendKeyEvent(const content::NativeWebKeyboardEvent& event);
  void SendMouseEvent(const blink::WebMouseEvent& event);
  void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  void SendFocusEvent(bool focus);
  void UpdateFrameRate();

  void HoldResize();
  void ReleaseResize();

  void OnPaint(const gfx::Rect& damage_rect,
               int bitmap_width,
               int bitmap_height,
               void* bitmap_pixels);

  bool IsPopupWidget() const {
    return popup_type_ != blink::WebPopupTypeNone;
  }

#if defined(OS_MACOSX)
  NSTextInputContext* GetNSTextInputContext();
  void HandleKeyEventBeforeTextInputClient(CefEventHandle keyEvent);
  void HandleKeyEventAfterTextInputClient(CefEventHandle keyEvent);

  bool GetCachedFirstRectForCharacterRange(gfx::Range range, gfx::Rect* rect,
                                           gfx::Range* actual_range) const;

  const std::string& selected_text() const { return selected_text_; }
  const gfx::Range& composition_range() const { return composition_range_; }
  const base::string16& selection_text() const { return selection_text_; }
  size_t selection_text_offset() const { return selection_text_offset_; }
#endif  // defined(OS_MACOSX)

  void AddGuestHostView(CefRenderWidgetHostViewOSR* guest_host);
  void RemoveGuestHostView(CefRenderWidgetHostViewOSR* guest_host);

  CefRefPtr<CefBrowserHostImpl> browser_impl() const { return browser_impl_; }
  void set_browser_impl(CefRefPtr<CefBrowserHostImpl> browser) {
    browser_impl_ = browser;
  }

  void set_popup_host_view(CefRenderWidgetHostViewOSR* popup_view) {
    popup_host_view_ = popup_view;
  }
  void set_child_host_view(CefRenderWidgetHostViewOSR* popup_view) {
    child_host_view_ = popup_view;
  }

  ui::Compositor* compositor() const { return compositor_.get(); }
  content::RenderWidgetHostImpl* render_widget_host() const
      { return render_widget_host_; }

 private:
  void SetFrameRate();
  void SetDeviceScaleFactor();
  void ResizeRootLayer();

  // Returns a best guess whether a frame is currently pending.
  bool IsFramePending();

  // Called by CefBeginFrameTimer to send a BeginFrame request.
  void OnBeginFrameTimerTick();
  void SendBeginFrame(base::TimeTicks frame_time,
                      base::TimeDelta vsync_period);

  void CancelWidget();

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

#if defined(USE_AURA)
  ui::PlatformCursor GetPlatformCursor(blink::WebCursorInfo::Type type);
#endif

  const bool transparent_;

  float scale_factor_;
  int frame_rate_threshold_ms_;

  scoped_ptr<content::DelegatedFrameHost> delegated_frame_host_;
  scoped_ptr<ui::Compositor> compositor_;
  gfx::AcceleratedWidget compositor_widget_;
  scoped_ptr<ui::Layer> root_layer_;

#if defined(OS_WIN)
  scoped_ptr<gfx::WindowImpl> window_;
#elif defined(OS_MACOSX)
  NSWindow* window_;
  CALayer* background_layer_;
  scoped_ptr<content::BrowserCompositorMac> browser_compositor_;
#elif defined(USE_X11)
  CefWindowX11* window_;
  scoped_ptr<ui::XScopedCursor> invisible_cursor_;
#endif

  // Used to control the VSync rate in subprocesses when BeginFrame scheduling
  // is enabled.
  scoped_ptr<CefBeginFrameTimer> begin_frame_timer_;

  // Used for direct rendering from the compositor when GPU compositing is
  // disabled. This object is owned by the compositor.
  CefSoftwareOutputDeviceOSR* software_output_device_;

  // Used for managing copy requests when GPU compositing is enabled.
  scoped_ptr<CefCopyFrameGenerator> copy_frame_generator_;

  bool hold_resize_;
  bool pending_resize_;

  // The associated Model.  While |this| is being Destroyed,
  // |render_widget_host_| is NULL and the message loop is run one last time
  // Message handlers must check for a NULL |render_widget_host_|.
  content::RenderWidgetHostImpl* render_widget_host_;

  bool has_parent_;
  CefRenderWidgetHostViewOSR* parent_host_view_;
  CefRenderWidgetHostViewOSR* popup_host_view_;
  CefRenderWidgetHostViewOSR* child_host_view_;
  std::set<CefRenderWidgetHostViewOSR*> guest_host_views_;

  CefRefPtr<CefBrowserHostImpl> browser_impl_;

  bool is_showing_;
  bool is_destroyed_;
  gfx::Rect popup_position_;

  // The last scroll offset of the view.
  gfx::Vector2dF last_scroll_offset_;
  bool is_scroll_offset_changed_pending_;

#if defined(OS_MACOSX)
  NSTextInputContext* text_input_context_osr_mac_;

  // Selected text on the renderer.
  std::string selected_text_;

  // The current composition character range and its bounds.
  gfx::Range composition_range_;
  std::vector<gfx::Rect> composition_bounds_;

  // The current caret bounds.
  gfx::Rect caret_rect_;

  // The current first selection bounds.
  gfx::Rect first_selection_rect_;
#endif

  base::WeakPtrFactory<CefRenderWidgetHostViewOSR> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefRenderWidgetHostViewOSR);
};

#endif  // CEF_LIBCEF_BROWSER_OSR_RENDER_WIDGET_HOST_VIEW_OSR_H_

