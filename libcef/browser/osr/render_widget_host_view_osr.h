// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_RENDER_WIDGET_HOST_VIEW_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_RENDER_WIDGET_HOST_VIEW_OSR_H_
#pragma once

#include <map>
#include <set>
#include <vector>

#include "include/cef_base.h"
#include "include/cef_browser.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/osr/motion_event_osr.h"

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/browser/renderer_host/input/mouse_wheel_phase_handler.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/public/common/widget_type.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/external_begin_frame_client.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gesture_detection/motion_event_generic.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_LINUX)
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_MACOSX)
#include "content/browser/renderer_host/browser_compositor_view_mac.h"
#endif

#if defined(OS_WIN)
#include "ui/gfx/win/window_impl.h"
#endif

#if defined(USE_AURA)
#include "third_party/blink/public/platform/web_cursor_info.h"
#include "ui/base/cursor/cursor.h"
#endif

namespace content {
class DelegatedFrameHost;
class DelegatedFrameHostClient;
class RenderWidgetHost;
class RenderWidgetHostImpl;
class RenderWidgetHostViewGuest;
class BackingStore;
class CursorManager;
}  // namespace content

class CefBeginFrameTimer;
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

#if defined(OS_MACOSX)
class MacHelper;
#endif

class CefRenderWidgetHostViewOSR : public content::RenderWidgetHostViewBase,
                                   public ui::ExternalBeginFrameClient,
                                   public ui::CompositorDelegate,
                                   public content::TextInputManager::Observer,
                                   public ui::GestureProviderClient {
 public:
  CefRenderWidgetHostViewOSR(SkColor background_color,
                             bool use_shared_texture,
                             bool use_external_begin_frame,
                             content::RenderWidgetHost* widget,
                             CefRenderWidgetHostViewOSR* parent_host_view,
                             bool is_guest_view_hack);
  ~CefRenderWidgetHostViewOSR() override;

  // RenderWidgetHostView implementation.
  void InitAsChild(gfx::NativeView parent_view) override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void Focus() override;
  bool HasFocus() const override;
  uint32_t GetCaptureSequenceNumber() const override;
  bool IsSurfaceAvailableForCopy() const override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  void EnsureSurfaceSynchronizedForWebTest() override;
  gfx::Rect GetViewBounds() const override;
  void SetBackgroundColor(SkColor color) override;
  base::Optional<SkColor> GetBackgroundColor() const override;
  void UpdateBackgroundColor() override;
  bool LockMouse() override;
  void UnlockMouse() override;
  void TakeFallbackContentFrom(content::RenderWidgetHostView* view) override;

#if defined(OS_MACOSX)
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override;
  void SpeakSelection() override;
#endif  // defined(OS_MACOSX)

  // RenderWidgetHostViewBase implementation.
  void DidCreateNewRendererCompositorFrameSink(
      viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink)
      override;
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      base::Optional<viz::HitTestRegionList> hit_test_region_list) override;
  void ClearCompositorFrame() override;
  void ResetFallbackToFirstNavigationSurface() override;
  void InitAsPopup(content::RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& pos) override;
  void InitAsFullscreen(
      content::RenderWidgetHostView* reference_host_view) override;
  void InitAsGuest(content::RenderWidgetHostView* parent_host_view,
                   content::RenderWidgetHostViewGuest* guest_view) override;
  void UpdateCursor(const content::WebCursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void RenderProcessGone(base::TerminationStatus status,
                         int error_code) override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  content::CursorManager* GetCursorManager() override;
  gfx::Size GetCompositorViewportPixelSize() const override;
  void CopyFromSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  void GetScreenInfo(content::ScreenInfo* results) const override;
  void TransformPointToRootSurface(gfx::PointF* point) override;
  gfx::Rect GetBoundsInRootWindow() override;
  viz::ScopedSurfaceIdAllocator DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;
  viz::SurfaceId GetCurrentSurfaceId() const override;
  content::BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      content::BrowserAccessibilityDelegate* delegate,
      bool for_root_frame) override;
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override;
  std::unique_ptr<content::SyntheticGestureTarget>
  CreateSyntheticGestureTarget() override;
  void SetNeedsBeginFrames(bool enabled) override;
  void SetWantsAnimateOnlyBeginFrames() override;
  bool TransformPointToLocalCoordSpaceLegacy(
      const gfx::PointF& point,
      const viz::SurfaceId& original_surface,
      gfx::PointF* transformed_point) override;
  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      RenderWidgetHostViewBase* target_view,
      gfx::PointF* transformed_point) override;
  void DidNavigate() override;
  void SelectionChanged(const base::string16& text,
                        size_t offset,
                        const gfx::Range& range) override;
  const viz::LocalSurfaceIdAllocation& GetLocalSurfaceIdAllocation()
      const override;
  const viz::FrameSinkId& GetFrameSinkId() const override;
  viz::FrameSinkId GetRootFrameSinkId() override;

  // ui::ExternalBeginFrameClient implementation:
  void OnDisplayDidFinishFrame(const viz::BeginFrameAck& ack) override;
  void OnNeedsExternalBeginFrames(bool needs_begin_frames) override;

  // ui::CompositorDelegate implementation.
  std::unique_ptr<viz::SoftwareOutputDevice> CreateSoftwareOutputDevice(
      ui::Compositor* compositor) override;

  // TextInputManager::Observer implementation.
  void OnUpdateTextInputStateCalled(
      content::TextInputManager* text_input_manager,
      RenderWidgetHostViewBase* updated_view,
      bool did_update_state) override;

  // ui::GestureProviderClient implementation.
  void ProcessAckedTouchEvent(const content::TouchEventWithLatencyInfo& touch,
                              content::InputEventAckState ack_result) override;
  void OnGestureEvent(const ui::GestureEventData& gesture) override;

  bool InstallTransparency();

  void SynchronizeVisualProperties(bool force = false);
  void OnScreenInfoChanged();
  void Invalidate(CefBrowserHost::PaintElementType type);
  void SendExternalBeginFrame();
  void SendKeyEvent(const content::NativeWebKeyboardEvent& event);
  void SendMouseEvent(const blink::WebMouseEvent& event);
  void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  void SendTouchEvent(const CefTouchEvent& event);
  bool ShouldRouteEvents() const;
  void SendFocusEvent(bool focus);
  void UpdateFrameRate();

  void HoldResize();
  void ReleaseResize();

  void OnPaint(const gfx::Rect& damage_rect,
               int bitmap_width,
               int bitmap_height,
               void* bitmap_pixels);

  bool IsPopupWidget() const {
    return widget_type_ == content::WidgetType::kPopup;
  }

  void ImeSetComposition(const CefString& text,
                         const std::vector<CefCompositionUnderline>& underlines,
                         const CefRange& replacement_range,
                         const CefRange& selection_range);
  void ImeCommitText(const CefString& text,
                     const CefRange& replacement_range,
                     int relative_cursor_pos);
  void ImeFinishComposingText(bool keep_selection);
  void ImeCancelComposition() override;

  CefRefPtr<CefBrowserHostImpl> browser_impl() const { return browser_impl_; }
  void set_browser_impl(CefRefPtr<CefBrowserHostImpl> browser) {
    browser_impl_ = browser;
  }

  void set_popup_host_view(CefRenderWidgetHostViewOSR* popup_view) {
    if (popup_view != popup_host_view_)
      forward_touch_to_popup_ = false;
    popup_host_view_ = popup_view;
  }
  void set_child_host_view(CefRenderWidgetHostViewOSR* popup_view) {
    child_host_view_ = popup_view;
  }

  ui::Compositor* GetCompositor() const;
  content::RenderWidgetHostImpl* render_widget_host() const {
    return render_widget_host_;
  }
  ui::Layer* GetRootLayer() const;

#if defined(OS_MACOSX)
  content::BrowserCompositorMac* browser_compositor() const {
    return browser_compositor_.get();
  }
#endif

  void OnPresentCompositorFrame();

 private:
  content::DelegatedFrameHost* GetDelegatedFrameHost() const;

  void SetFrameRate();
  void SetDeviceScaleFactor();
  void ResizeRootLayer(bool force);

  // Called by CefBeginFrameTimer to send a BeginFrame request.
  void OnBeginFrameTimerTick();
  void SendBeginFrame(base::TimeTicks frame_time, base::TimeDelta vsync_period);

  void CancelWidget();

  void OnScrollOffsetChanged();

  void AddGuestHostView(CefRenderWidgetHostViewOSR* guest_host);
  void RemoveGuestHostView(CefRenderWidgetHostViewOSR* guest_host);

  // Register a callback that will be executed when |guest_host_view| receives
  // OnSwapCompositorFrame. The callback triggers repaint of the embedder view.
  void RegisterGuestViewFrameSwappedCallback(
      content::RenderWidgetHostViewGuest* guest_host_view);

  void OnGuestViewFrameSwapped(
      content::RenderWidgetHostViewGuest* guest_host_view);

  void InvalidateInternal(const gfx::Rect& bounds_in_pixels);

  void RequestImeCompositionUpdate(bool start_monitoring);

  viz::FrameSinkId AllocateFrameSinkId(bool is_guest_view_hack);

  void AddDamageRect(uint32_t sequence, const gfx::Rect& rect);

  // Applies background color without notifying the RenderWidget about
  // opaqueness changes.
  void UpdateBackgroundColorFromRenderer(SkColor color);

#if defined(OS_MACOSX)
  display::Display GetDisplay();
  void OnDidUpdateVisualPropertiesComplete(
      const cc::RenderFrameMetadata& metadata);

  friend class MacHelper;
  bool UpdateNSViewAndDisplay();
#endif  // defined(OS_MACOSX)

  void PlatformCreateCompositorWidget(bool is_guest_view_hack);
#if !defined(OS_MACOSX)
  void PlatformResizeCompositorWidget(const gfx::Size& size);
#endif
  void PlatformDestroyCompositorWidget();

#if defined(USE_AURA)
  ui::PlatformCursor GetPlatformCursor(blink::WebCursorInfo::Type type);
#endif

  // The background color of the web content.
  SkColor background_color_;

  int frame_rate_threshold_us_;

#if !defined(OS_MACOSX)
  std::unique_ptr<ui::Compositor> compositor_;
  gfx::AcceleratedWidget compositor_widget_;
  std::unique_ptr<content::DelegatedFrameHost> delegated_frame_host_;
  std::unique_ptr<content::DelegatedFrameHostClient>
      delegated_frame_host_client_;
  std::unique_ptr<ui::Layer> root_layer_;
  viz::LocalSurfaceIdAllocation local_surface_id_allocation_;
  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;
#endif

#if defined(OS_WIN)
  std::unique_ptr<gfx::WindowImpl> window_;
#elif defined(OS_MACOSX)
  NSWindow* window_;
  CALayer* background_layer_;
  std::unique_ptr<content::BrowserCompositorMac> browser_compositor_;
  MacHelper* mac_helper_;
#elif defined(USE_X11)
  CefWindowX11* window_;
  std::unique_ptr<ui::XScopedCursor> invisible_cursor_;
#endif

  std::unique_ptr<content::CursorManager> cursor_manager_;

  // Used to control the VSync rate in subprocesses when BeginFrame scheduling
  // is enabled.
  std::unique_ptr<CefBeginFrameTimer> begin_frame_timer_;

  // Provides |source_id| for BeginFrameArgs that we create.
  viz::StubBeginFrameSource begin_frame_source_;
  uint64_t begin_frame_number_ = viz::BeginFrameArgs::kStartingFrameNumber;

  bool sync_frame_rate_ = false;
  bool external_begin_frame_enabled_ = false;
  bool needs_external_begin_frames_ = false;

  // Used for direct rendering from the compositor when GPU compositing is
  // disabled. This object is owned by the compositor.
  CefSoftwareOutputDeviceOSR* software_output_device_;

  // Used for managing copy requests when GPU compositing is enabled.
  std::unique_ptr<CefCopyFrameGenerator> copy_frame_generator_;

  bool hold_resize_;
  bool pending_resize_;
  bool pending_resize_force_;

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
  base::Lock damage_rect_lock_;
  std::map<uint32_t, gfx::Rect> damage_rects_;

  // Whether pinch-to-zoom should be enabled and pinch events forwarded to the
  // renderer.
  bool pinch_zoom_enabled_;

  // The last scroll offset of the view.
  gfx::Vector2dF last_scroll_offset_;
  bool is_scroll_offset_changed_pending_;

  content::MouseWheelPhaseHandler mouse_wheel_phase_handler_;

  std::unique_ptr<viz::mojom::CompositorFrameSinkClient>
      renderer_compositor_frame_sink_;

  // Latest capture sequence number which is incremented when the caller
  // requests surfaces be synchronized via
  // EnsureSurfaceSynchronizedForLayoutTest().
  uint32_t latest_capture_sequence_number_ = 0u;

  // ui::GestureProviderClient implementation.
  ui::FilteredGestureProvider gesture_provider_;

  CefMotionEventOSR pointer_state_;
  bool forward_touch_to_popup_;

  base::WeakPtrFactory<CefRenderWidgetHostViewOSR> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefRenderWidgetHostViewOSR);
};

#endif  // CEF_LIBCEF_BROWSER_OSR_RENDER_WIDGET_HOST_VIEW_OSR_H_
