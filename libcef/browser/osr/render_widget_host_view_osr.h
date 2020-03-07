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
#include "libcef/browser/osr/host_display_client_osr.h"
#include "libcef/browser/osr/motion_event_osr.h"

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "cc/layers/deadline_policy.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/browser/renderer_host/input/mouse_wheel_phase_handler.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/public/common/widget_type.h"
#include "ui/base/cursor/types/cursor_types.h"
#include "ui/compositor/compositor.h"
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

class CefCopyFrameGenerator;
class CefSoftwareOutputDeviceOSR;
class CefVideoConsumerOSR;
class CefWebContentsViewOSR;

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
                                   public ui::CompositorDelegate,
                                   public content::TextInputManager::Observer,
                                   public ui::GestureProviderClient {
 public:
  CefRenderWidgetHostViewOSR(SkColor background_color,
                             bool use_shared_texture,
                             bool use_external_begin_frame,
                             content::RenderWidgetHost* widget,
                             CefRenderWidgetHostViewOSR* parent_host_view);
  ~CefRenderWidgetHostViewOSR() override;

  // RenderWidgetHostView implementation.
  void InitAsChild(gfx::NativeView parent_view) override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  gfx::NativeView GetNativeView() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void Focus() override;
  bool HasFocus() override;
  uint32_t GetCaptureSequenceNumber() const override;
  bool IsSurfaceAvailableForCopy() override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  void EnsureSurfaceSynchronizedForWebTest() override;
  gfx::Rect GetViewBounds() override;
  void SetBackgroundColor(SkColor color) override;
  base::Optional<SkColor> GetBackgroundColor() override;
  void UpdateBackgroundColor() override;
  bool LockMouse(bool request_unadjusted_movement) override;
  void UnlockMouse() override;
  void TakeFallbackContentFrom(content::RenderWidgetHostView* view) override;

#if defined(OS_MACOSX)
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override;
  void SpeakSelection() override;
#endif  // defined(OS_MACOSX)

  // RenderWidgetHostViewBase implementation.
  void ResetFallbackToFirstNavigationSurface() override;
  void InitAsPopup(content::RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& pos) override;
  void InitAsFullscreen(
      content::RenderWidgetHostView* reference_host_view) override;
  void UpdateCursor(const content::WebCursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void RenderProcessGone() override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  content::CursorManager* GetCursorManager() override;
  gfx::Size GetCompositorViewportPixelSize() override;
  void CopyFromSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  void GetScreenInfo(content::ScreenInfo* results) override;
  void TransformPointToRootSurface(gfx::PointF* point) override;
  gfx::Rect GetBoundsInRootWindow() override;

#if !defined(OS_MACOSX)
  viz::ScopedSurfaceIdAllocator DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;
#endif

  viz::SurfaceId GetCurrentSurfaceId() const override;
  content::BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      content::BrowserAccessibilityDelegate* delegate,
      bool for_root_frame) override;
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override;
  std::unique_ptr<content::SyntheticGestureTarget>
  CreateSyntheticGestureTarget() override;
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
  void OnRenderFrameMetadataChangedAfterActivation() override;

  void OnFrameComplete(const viz::BeginFrameAck& ack);

  // ui::CompositorDelegate implementation.
  std::unique_ptr<viz::HostDisplayClient> CreateHostDisplayClient() override;

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

  void WasResized();
  void SynchronizeVisualProperties(
      const cc::DeadlinePolicy& deadline_policy,
      const base::Optional<viz::LocalSurfaceIdAllocation>&
          child_local_surface_id_allocation);
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

  gfx::Size SizeInPixels();
  void OnPaint(const gfx::Rect& damage_rect,
               const gfx::Size& pixel_size,
               const void* pixels);

  void OnBeginFame(base::TimeTicks frame_time);

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

  content::RenderWidgetHostImpl* render_widget_host() const {
    return render_widget_host_;
  }
  ui::Layer* GetRootLayer() const;

  void OnPresentCompositorFrame();

  void OnDidUpdateVisualPropertiesComplete(
      const cc::RenderFrameMetadata& metadata);

 private:
  void SetFrameRate();
  bool SetDeviceScaleFactor();
  bool SetViewBounds();
  bool SetRootLayerSize(bool force);

  // Manages resizing so that only one resize request is in-flight at a time.
  bool ResizeRootLayer();
  void ReleaseResizeHold();

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

  viz::FrameSinkId AllocateFrameSinkId();

  // Forces the view to allocate a new viz::LocalSurfaceId for the next
  // CompositorFrame submission in anticipation of a synchronization operation
  // that does not involve a resize or a device scale factor change.
  void AllocateLocalSurfaceId();
  const viz::LocalSurfaceIdAllocation& GetCurrentLocalSurfaceIdAllocation()
      const;

  // Sets the current viz::LocalSurfaceId, in cases where the embedded client
  // has allocated one. Also sets child sequence number component of the
  // viz::LocalSurfaceId allocator.
  void UpdateLocalSurfaceIdFromEmbeddedClient(
      const base::Optional<viz::LocalSurfaceIdAllocation>&
          local_surface_id_allocation);

  // Returns the current viz::LocalSurfaceIdAllocation.
  const viz::LocalSurfaceIdAllocation& GetOrCreateLocalSurfaceIdAllocation();

  // Marks the current viz::LocalSurfaceId as invalid. AllocateLocalSurfaceId
  // must be called before submitting new CompositorFrames.
  void InvalidateLocalSurfaceId();

  void AddDamageRect(uint32_t sequence, const gfx::Rect& rect);

  // Applies background color without notifying the RenderWidget about
  // opaqueness changes.
  void UpdateBackgroundColorFromRenderer(SkColor color);

#if defined(USE_AURA)
  ui::PlatformCursor GetPlatformCursor(ui::CursorType type);
#endif

  // The background color of the web content.
  SkColor background_color_;

  int frame_rate_threshold_us_ = 0;

  std::unique_ptr<ui::Compositor> compositor_;
  std::unique_ptr<content::DelegatedFrameHost> delegated_frame_host_;
  std::unique_ptr<content::DelegatedFrameHostClient>
      delegated_frame_host_client_;
  std::unique_ptr<ui::Layer> root_layer_;

  // Used to allocate LocalSurfaceIds when this is embedding external content.
  std::unique_ptr<viz::ParentLocalSurfaceIdAllocator>
      parent_local_surface_id_allocator_;
  viz::ParentLocalSurfaceIdAllocator compositor_local_surface_id_allocator_;

#if defined(USE_X11)
  std::unique_ptr<ui::XScopedCursor> invisible_cursor_;
#endif

  std::unique_ptr<content::CursorManager> cursor_manager_;

  // Provides |source_id| for BeginFrameArgs that we create.
  viz::StubBeginFrameSource begin_frame_source_;
  uint64_t begin_frame_number_ = viz::BeginFrameArgs::kStartingFrameNumber;

  bool sync_frame_rate_ = false;
  bool external_begin_frame_enabled_ = false;
  bool needs_external_begin_frames_ = false;

  CefHostDisplayClientOSR* host_display_client_ = nullptr;
  std::unique_ptr<CefVideoConsumerOSR> video_consumer_;

  bool hold_resize_ = false;
  bool pending_resize_ = false;

  // The associated Model.  While |this| is being Destroyed,
  // |render_widget_host_| is NULL and the message loop is run one last time
  // Message handlers must check for a NULL |render_widget_host_|.
  content::RenderWidgetHostImpl* render_widget_host_;

  bool has_parent_;
  CefRenderWidgetHostViewOSR* parent_host_view_;
  CefRenderWidgetHostViewOSR* popup_host_view_ = nullptr;
  CefRenderWidgetHostViewOSR* child_host_view_ = nullptr;
  std::set<CefRenderWidgetHostViewOSR*> guest_host_views_;

  CefRefPtr<CefBrowserHostImpl> browser_impl_;

  bool is_showing_ = false;
  bool is_destroyed_ = false;
  bool is_first_navigation_ = true;
  gfx::Rect current_view_bounds_;
  gfx::Rect popup_position_;
  base::Lock damage_rect_lock_;
  std::map<uint32_t, gfx::Rect> damage_rects_;

  // Whether pinch-to-zoom should be enabled and pinch events forwarded to the
  // renderer.
  bool pinch_zoom_enabled_;

  // The last scroll offset of the view.
  gfx::Vector2dF last_scroll_offset_;
  bool is_scroll_offset_changed_pending_ = false;

  content::MouseWheelPhaseHandler mouse_wheel_phase_handler_;

  // Latest capture sequence number which is incremented when the caller
  // requests surfaces be synchronized via
  // EnsureSurfaceSynchronizedForLayoutTest().
  uint32_t latest_capture_sequence_number_ = 0u;

  // ui::GestureProviderClient implementation.
  ui::FilteredGestureProvider gesture_provider_;

  CefMotionEventOSR pointer_state_;
  bool forward_touch_to_popup_ = false;

  base::WeakPtrFactory<CefRenderWidgetHostViewOSR> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefRenderWidgetHostViewOSR);
};

#endif  // CEF_LIBCEF_BROWSER_OSR_RENDER_WIDGET_HOST_VIEW_OSR_H_
