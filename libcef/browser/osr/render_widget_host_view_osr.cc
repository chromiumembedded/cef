// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/osr/osr_util.h"
#include "libcef/browser/osr/synthetic_gesture_target_osr.h"
#include "libcef/browser/osr/touch_selection_controller_client_osr.h"
#include "libcef/browser/osr/video_consumer_osr.h"
#include "libcef/browser/thread_util.h"

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/common/switches.h"
#include "content/browser/bad_message.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/cursor_manager.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/input/motion_event_web.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/common/content_switches_internal.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_factory.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/velocity_tracker/motion_event.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace {

// The maximum number of damage rects to cache for outstanding frame requests
// (for OnAcceleratedPaint).
const size_t kMaxDamageRects = 10;

const float kDefaultScaleFactor = 1.0;

display::ScreenInfo ScreenInfoFrom(const CefScreenInfo& src) {
  display::ScreenInfo screenInfo;
  screenInfo.device_scale_factor = src.device_scale_factor;
  screenInfo.depth = src.depth;
  screenInfo.depth_per_component = src.depth_per_component;
  screenInfo.is_monochrome = src.is_monochrome ? true : false;
  screenInfo.rect =
      gfx::Rect(src.rect.x, src.rect.y, src.rect.width, src.rect.height);
  screenInfo.available_rect =
      gfx::Rect(src.available_rect.x, src.available_rect.y,
                src.available_rect.width, src.available_rect.height);

  return screenInfo;
}

class CefDelegatedFrameHostClient : public content::DelegatedFrameHostClient {
 public:
  explicit CefDelegatedFrameHostClient(CefRenderWidgetHostViewOSR* view)
      : view_(view) {}

  CefDelegatedFrameHostClient(const CefDelegatedFrameHostClient&) = delete;
  CefDelegatedFrameHostClient& operator=(const CefDelegatedFrameHostClient&) =
      delete;

  ui::Layer* DelegatedFrameHostGetLayer() const override {
    return view_->GetRootLayer();
  }

  bool DelegatedFrameHostIsVisible() const override {
    // Called indirectly from DelegatedFrameHost::WasShown.
    return view_->IsShowing();
  }

  SkColor DelegatedFrameHostGetGutterColor() const override {
    // When making an element on the page fullscreen the element's background
    // may not match the page's, so use black as the gutter color to avoid
    // flashes of brighter colors during the transition.
    if (view_->render_widget_host()->delegate() &&
        view_->render_widget_host()->delegate()->IsFullscreen()) {
      return SK_ColorBLACK;
    }
    return *view_->GetBackgroundColor();
  }

  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override {
    view_->render_widget_host()->DidProcessFrame(frame_token, activation_time);
  }

  float GetDeviceScaleFactor() const override {
    return view_->GetDeviceScaleFactor();
  }

  std::vector<viz::SurfaceId> CollectSurfaceIdsForEviction() override {
    return view_->render_widget_host()->CollectSurfaceIdsForEviction();
  }

  void InvalidateLocalSurfaceIdOnEviction() override {
    view_->InvalidateLocalSurfaceId();
  }

  bool ShouldShowStaleContentOnEviction() override { return false; }

 private:
  CefRenderWidgetHostViewOSR* const view_;
};

ui::GestureProvider::Config CreateGestureProviderConfig() {
  ui::GestureProvider::Config config = ui::GetGestureProviderConfig(
      ui::GestureProviderConfigType::CURRENT_PLATFORM);
  return config;
}

ui::LatencyInfo CreateLatencyInfo(const blink::WebInputEvent& event) {
  ui::LatencyInfo latency_info;
  // The latency number should only be added if the timestamp is valid.
  base::TimeTicks time = event.TimeStamp();
  if (!time.is_null()) {
    latency_info.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, time);
  }
  return latency_info;
}

gfx::Rect GetViewBounds(AlloyBrowserHostImpl* browser) {
  if (!browser) {
    return gfx::Rect();
  }

  CefRect rc;
  CefRefPtr<CefRenderHandler> handler =
      browser->GetClient()->GetRenderHandler();
  CHECK(handler);

  handler->GetViewRect(browser, rc);
  CHECK_GT(rc.width, 0);
  CHECK_GT(rc.height, 0);

  return gfx::Rect(rc.x, rc.y, rc.width, rc.height);
}

ui::ImeTextSpan::UnderlineStyle GetImeUnderlineStyle(
    cef_composition_underline_style_t style) {
  switch (style) {
    case CEF_CUS_SOLID:
      return ui::ImeTextSpan::UnderlineStyle::kSolid;
    case CEF_CUS_DOT:
      return ui::ImeTextSpan::UnderlineStyle::kDot;
    case CEF_CUS_DASH:
      return ui::ImeTextSpan::UnderlineStyle::kDash;
    case CEF_CUS_NONE:
      return ui::ImeTextSpan::UnderlineStyle::kNone;
  }

  DCHECK(false);
  return ui::ImeTextSpan::UnderlineStyle::kSolid;
}

}  // namespace

// Logic copied from RenderWidgetHostViewAura::CreateSelectionController.
void CefRenderWidgetHostViewOSR::CreateSelectionController() {
  ui::TouchSelectionController::Config tsc_config;
  tsc_config.max_tap_duration = base::Milliseconds(
      ui::GestureConfiguration::GetInstance()->long_press_time_in_ms());
  tsc_config.tap_slop = ui::GestureConfiguration::GetInstance()
                            ->max_touch_move_in_pixels_for_click();
  tsc_config.enable_longpress_drag_selection = false;
  selection_controller_ = std::make_unique<ui::TouchSelectionController>(
      selection_controller_client_.get(), tsc_config);
}

CefRenderWidgetHostViewOSR::CefRenderWidgetHostViewOSR(
    SkColor background_color,
    bool use_shared_texture,
    bool use_external_begin_frame,
    content::RenderWidgetHost* widget,
    CefRenderWidgetHostViewOSR* parent_host_view)
    : content::RenderWidgetHostViewBase(widget),
      background_color_(background_color),
      render_widget_host_(content::RenderWidgetHostImpl::From(widget)),
      has_parent_(parent_host_view != nullptr),
      parent_host_view_(parent_host_view),
      pinch_zoom_enabled_(content::IsPinchToZoomEnabled()),
      mouse_wheel_phase_handler_(this),
      gesture_provider_(CreateGestureProviderConfig(), this),
      weak_ptr_factory_(this) {
  DCHECK(render_widget_host_);
  DCHECK(!render_widget_host_->GetView());

  if (parent_host_view_) {
    browser_impl_ = parent_host_view_->browser_impl();
    DCHECK(browser_impl_);
  } else if (content::RenderViewHost::From(render_widget_host_)) {
    // AlloyBrowserHostImpl might not be created at this time for popups.
    browser_impl_ = AlloyBrowserHostImpl::GetBrowserForHost(
        content::RenderViewHost::From(render_widget_host_));
  }

  delegated_frame_host_client_ =
      std::make_unique<CefDelegatedFrameHostClient>(this);

  // Matching the attributes from BrowserCompositorMac.
  delegated_frame_host_ = std::make_unique<content::DelegatedFrameHost>(
      AllocateFrameSinkId(), delegated_frame_host_client_.get(),
      false /* should_register_frame_sink_id */);

  root_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);

  bool opaque = SkColorGetA(background_color_) == SK_AlphaOPAQUE;
  GetRootLayer()->SetFillsBoundsOpaquely(opaque);
  GetRootLayer()->SetColor(background_color_);

  external_begin_frame_enabled_ = use_external_begin_frame;

  auto context_factory = content::GetContextFactory();

  // Matching the attributes from RecyclableCompositorMac.
  compositor_ = std::make_unique<ui::Compositor>(
      context_factory->AllocateFrameSinkId(), context_factory,
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      false /* enable_pixel_canvas */, use_external_begin_frame);
  compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);

  compositor_->SetDelegate(this);
  compositor_->SetRootLayer(root_layer_.get());
  compositor_->AddChildFrameSink(GetFrameSinkId());

  content::RenderWidgetHostImpl* render_widget_host_impl =
      content::RenderWidgetHostImpl::From(render_widget_host_);
  if (render_widget_host_impl) {
    render_widget_host_impl->SetCompositorForFlingScheduler(compositor_.get());
  }

  cursor_manager_ = std::make_unique<content::CursorManager>(this);

  // This may result in a call to GetFrameSinkId().
  render_widget_host_->SetView(this);

  if (GetTextInputManager()) {
    GetTextInputManager()->AddObserver(this);
  }

  if (browser_impl_ && !parent_host_view_) {
    // For child/popup views this will be called from the associated InitAs*()
    // method.
    SetRootLayerSize(false /* force */);
    if (!render_widget_host_->is_hidden()) {
      Show();
    }
  }

  selection_controller_client_ =
      std::make_unique<CefTouchSelectionControllerClientOSR>(this);
  CreateSelectionController();
}

CefRenderWidgetHostViewOSR::~CefRenderWidgetHostViewOSR() {
  ReleaseCompositor();
  root_layer_.reset(nullptr);

  DCHECK(!parent_host_view_);
  DCHECK(!popup_host_view_);
  DCHECK(!child_host_view_);
  DCHECK(guest_host_views_.empty());

  if (text_input_manager_) {
    text_input_manager_->RemoveObserver(this);
  }
}

void CefRenderWidgetHostViewOSR::ReleaseCompositor() {
  if (!compositor_) {
    return;  // already released
  }

  // Marking the DelegatedFrameHost as removed from the window hierarchy is
  // necessary to remove all connections to its old ui::Compositor.
  if (is_showing_) {
    delegated_frame_host_->WasHidden(
        content::DelegatedFrameHost::HiddenCause::kOther);
  }
  delegated_frame_host_->DetachFromCompositor();

  delegated_frame_host_.reset(nullptr);
  compositor_.reset(nullptr);
}

// Called for full-screen widgets.
void CefRenderWidgetHostViewOSR::InitAsChild(gfx::NativeView parent_view) {
  DCHECK(parent_host_view_);
  DCHECK(browser_impl_);

  if (parent_host_view_->child_host_view_) {
    // Cancel the previous popup widget.
    parent_host_view_->child_host_view_->CancelWidget();
  }

  parent_host_view_->set_child_host_view(this);

  // The parent view should not render while the full-screen view exists.
  parent_host_view_->Hide();

  SetRootLayerSize(false /* force */);
  Show();
}

void CefRenderWidgetHostViewOSR::SetSize(const gfx::Size& size) {}

void CefRenderWidgetHostViewOSR::SetBounds(const gfx::Rect& rect) {}

gfx::NativeView CefRenderWidgetHostViewOSR::GetNativeView() {
  return gfx::NativeView();
}

gfx::NativeViewAccessible
CefRenderWidgetHostViewOSR::GetNativeViewAccessible() {
  return gfx::NativeViewAccessible();
}

void CefRenderWidgetHostViewOSR::Focus() {}

bool CefRenderWidgetHostViewOSR::HasFocus() {
  return false;
}

bool CefRenderWidgetHostViewOSR::IsSurfaceAvailableForCopy() {
  return delegated_frame_host_
             ? delegated_frame_host_->CanCopyFromCompositingSurface()
             : false;
}

void CefRenderWidgetHostViewOSR::ShowWithVisibility(
    content::PageVisibilityState) {
  if (is_showing_) {
    return;
  }

  if (!content::GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled() &&
      !browser_impl_ &&
      (!parent_host_view_ || !parent_host_view_->browser_impl_)) {
    return;
  }

  is_showing_ = true;

  // If the viz::LocalSurfaceId is invalid, we may have been evicted,
  // and no other visual properties have since been changed. Allocate a new id
  // and start synchronizing.
  if (!GetLocalSurfaceId().is_valid()) {
    AllocateLocalSurfaceId();
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                GetLocalSurfaceId());
  }

  if (render_widget_host_) {
    render_widget_host_->WasShown(
        /*record_tab_switch_time_request=*/{});

    // Call OnRenderFrameMetadataChangedAfterActivation for every frame.
    auto provider = content::RenderWidgetHostImpl::From(render_widget_host_)
                        ->render_frame_metadata_provider();
    provider->AddObserver(this);
    provider->ReportAllFrameSubmissionsForTesting(true);
  }

  if (delegated_frame_host_) {
    delegated_frame_host_->AttachToCompositor(compositor_.get());
    delegated_frame_host_->WasShown(GetLocalSurfaceId(), GetViewBounds().size(),
                                    /*record_tab_switch_time_request=*/{});
  }

  if (!content::GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled()) {
    // Start generating frames when we're visible and at the correct size.
    if (!video_consumer_) {
      video_consumer_ = std::make_unique<CefVideoConsumerOSR>(this);
      UpdateFrameRate();
    } else {
      video_consumer_->SetActive(true);
    }
  }
}

void CefRenderWidgetHostViewOSR::Hide() {
  if (!is_showing_) {
    return;
  }

  is_showing_ = false;

  if (browser_impl_) {
    browser_impl_->CancelContextMenu();
  }

  if (selection_controller_client_) {
    selection_controller_client_->CloseQuickMenuAndHideHandles();
  }

  if (video_consumer_) {
    video_consumer_->SetActive(false);
  }

  if (render_widget_host_) {
    render_widget_host_->WasHidden();

    auto provider = content::RenderWidgetHostImpl::From(render_widget_host_)
                        ->render_frame_metadata_provider();
    provider->RemoveObserver(this);
  }

  if (delegated_frame_host_) {
    delegated_frame_host_->WasHidden(
        content::DelegatedFrameHost::HiddenCause::kOther);
    delegated_frame_host_->DetachFromCompositor();
  }
}

bool CefRenderWidgetHostViewOSR::IsShowing() {
  return is_showing_;
}

void CefRenderWidgetHostViewOSR::EnsureSurfaceSynchronizedForWebTest() {
  ++latest_capture_sequence_number_;
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseInfiniteDeadline(),
                              std::nullopt);
}

content::TouchSelectionControllerClientManager*
CefRenderWidgetHostViewOSR::GetTouchSelectionControllerClientManager() {
  return selection_controller_client_.get();
}

gfx::Rect CefRenderWidgetHostViewOSR::GetViewBounds() {
  if (IsPopupWidget()) {
    return popup_position_;
  }

  return current_view_bounds_;
}

void CefRenderWidgetHostViewOSR::SetBackgroundColor(SkColor color) {
  // The renderer will feed its color back to us with the first CompositorFrame.
  // We short-cut here to show a sensible color before that happens.
  UpdateBackgroundColorFromRenderer(color);

  DCHECK(SkColorGetA(color) == SK_AlphaOPAQUE ||
         SkColorGetA(color) == SK_AlphaTRANSPARENT);
  content::RenderWidgetHostViewBase::SetBackgroundColor(color);
}

std::optional<SkColor> CefRenderWidgetHostViewOSR::GetBackgroundColor() {
  return background_color_;
}

void CefRenderWidgetHostViewOSR::UpdateBackgroundColor() {}

std::optional<content::DisplayFeature>
CefRenderWidgetHostViewOSR::GetDisplayFeature() {
  return std::nullopt;
}

void CefRenderWidgetHostViewOSR::SetDisplayFeatureForTesting(
    const content::DisplayFeature* display_feature) {
  DCHECK(false);
}

blink::mojom::PointerLockResult CefRenderWidgetHostViewOSR::LockPointer(
    bool request_unadjusted_movement) {
  return blink::mojom::PointerLockResult::kPermissionDenied;
}

blink::mojom::PointerLockResult CefRenderWidgetHostViewOSR::ChangePointerLock(
    bool request_unadjusted_movement) {
  return blink::mojom::PointerLockResult::kPermissionDenied;
}

void CefRenderWidgetHostViewOSR::UnlockPointer() {}

void CefRenderWidgetHostViewOSR::TakeFallbackContentFrom(
    content::RenderWidgetHostView* view) {
  DCHECK(!static_cast<RenderWidgetHostViewBase*>(view)
              ->IsRenderWidgetHostViewChildFrame());
  CefRenderWidgetHostViewOSR* view_cef =
      static_cast<CefRenderWidgetHostViewOSR*>(view);
  SetBackgroundColor(view_cef->background_color_);
  if (delegated_frame_host_ && view_cef->delegated_frame_host_) {
    delegated_frame_host_->TakeFallbackContentFrom(
        view_cef->delegated_frame_host_.get());
  }
}

void CefRenderWidgetHostViewOSR::OnPresentCompositorFrame() {}

void CefRenderWidgetHostViewOSR::OnDidUpdateVisualPropertiesComplete(
    const cc::RenderFrameMetadata& metadata) {
  if (host()->is_hidden()) {
    // When an embedded child responds, we want to accept its changes to the
    // viz::LocalSurfaceId. However we do not want to embed surfaces while
    // hidden. Nor do we want to embed invalid ids when we are evicted. Becoming
    // visible will generate a new id, if necessary, and begin embedding.
    UpdateLocalSurfaceIdFromEmbeddedClient(metadata.local_surface_id);
  } else {
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                metadata.local_surface_id);
  }
}

void CefRenderWidgetHostViewOSR::AllocateLocalSurfaceId() {
  if (!parent_local_surface_id_allocator_) {
    parent_local_surface_id_allocator_ =
        std::make_unique<viz::ParentLocalSurfaceIdAllocator>();
  }
  parent_local_surface_id_allocator_->GenerateId();
}

const viz::LocalSurfaceId&
CefRenderWidgetHostViewOSR::GetCurrentLocalSurfaceId() const {
  return parent_local_surface_id_allocator_->GetCurrentLocalSurfaceId();
}

void CefRenderWidgetHostViewOSR::UpdateLocalSurfaceIdFromEmbeddedClient(
    const std::optional<viz::LocalSurfaceId>&
        embedded_client_local_surface_id) {
  if (embedded_client_local_surface_id) {
    parent_local_surface_id_allocator_->UpdateFromChild(
        *embedded_client_local_surface_id);
  } else {
    AllocateLocalSurfaceId();
  }
}

const viz::LocalSurfaceId&
CefRenderWidgetHostViewOSR::GetOrCreateLocalSurfaceId() {
  if (!parent_local_surface_id_allocator_) {
    AllocateLocalSurfaceId();
  }
  return GetCurrentLocalSurfaceId();
}

void CefRenderWidgetHostViewOSR::InvalidateLocalSurfaceId() {
  if (!parent_local_surface_id_allocator_) {
    return;
  }
  parent_local_surface_id_allocator_->Invalidate();
}

void CefRenderWidgetHostViewOSR::AddDamageRect(uint32_t sequence,
                                               const gfx::Rect& rect) {
  // Associate the given damage rect with the presentation token.
  // For OnAcceleratedPaint we'll lookup the corresponding damage area based on
  // the frame token which is passed back to OnPresentCompositorFrame.
  base::AutoLock lock_scope(damage_rect_lock_);

  // We assume our presentation_token is a counter. Since we're using an ordered
  // map we can enforce a max size and remove oldest from the front. Worst case,
  // if a damage rect isn't associated, we can simply pass the entire view size.
  while (damage_rects_.size() >= kMaxDamageRects) {
    damage_rects_.erase(damage_rects_.begin());
  }
  damage_rects_[sequence] = rect;
}

void CefRenderWidgetHostViewOSR::InvalidateLocalSurfaceIdAndAllocationGroup() {
  InvalidateLocalSurfaceId();
}

void CefRenderWidgetHostViewOSR::ClearFallbackSurfaceForCommitPending() {
  if (delegated_frame_host_) {
    delegated_frame_host_->ClearFallbackSurfaceForCommitPending();
  }
  InvalidateLocalSurfaceId();
}

void CefRenderWidgetHostViewOSR::ResetFallbackToFirstNavigationSurface() {
  if (delegated_frame_host_) {
    delegated_frame_host_->ResetFallbackToFirstNavigationSurface();
  }
}

void CefRenderWidgetHostViewOSR::InitAsPopup(
    content::RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds,
    const gfx::Rect& anchor_rect) {
  DCHECK_EQ(parent_host_view_, parent_host_view);
  DCHECK(browser_impl_);

  if (parent_host_view_->popup_host_view_) {
    // Cancel the previous popup widget.
    parent_host_view_->popup_host_view_->CancelWidget();
  }

  parent_host_view_->set_popup_host_view(this);

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);

  handler->OnPopupShow(browser_impl_.get(), true);

  CefRect view_rect;
  handler->GetViewRect(browser_impl_.get(), view_rect);
  gfx::Rect client_pos(bounds.x() - view_rect.x, bounds.y() - view_rect.y,
                       bounds.width(), bounds.height());

  popup_position_ = client_pos;

  CefRect widget_pos(client_pos.x(), client_pos.y(), client_pos.width(),
                     client_pos.height());

  if (handler.get()) {
    handler->OnPopupSize(browser_impl_.get(), widget_pos);
  }

  // The size doesn't change for popups so we need to force the
  // initialization.
  SetRootLayerSize(true /* force */);
  Show();
}

void CefRenderWidgetHostViewOSR::UpdateCursor(const ui::Cursor& cursor) {}

content::CursorManager* CefRenderWidgetHostViewOSR::GetCursorManager() {
  return cursor_manager_.get();
}

void CefRenderWidgetHostViewOSR::SetIsLoading(bool is_loading) {
  if (!is_loading) {
    return;
  }
  // Make sure gesture detection is fresh.
  gesture_provider_.ResetDetection();
  forward_touch_to_popup_ = false;
}

void CefRenderWidgetHostViewOSR::RenderProcessGone() {
  Destroy();
}

void CefRenderWidgetHostViewOSR::Destroy() {
  if (!is_destroyed_) {
    is_destroyed_ = true;

    if (has_parent_) {
      CancelWidget();
    } else {
      if (popup_host_view_) {
        popup_host_view_->CancelWidget();
      }
      if (child_host_view_) {
        child_host_view_->CancelWidget();
      }
      if (!guest_host_views_.empty()) {
        // Guest RWHVs will be destroyed when the associated RWHVGuest is
        // destroyed. This parent RWHV may be destroyed first, so disassociate
        // the guest RWHVs here without destroying them.
        for (auto guest_host_view : guest_host_views_) {
          guest_host_view->parent_host_view_ = nullptr;
        }
        guest_host_views_.clear();
      }
      Hide();
    }
  }

  delete this;
}

void CefRenderWidgetHostViewOSR::UpdateTooltipUnderCursor(
    const std::u16string& tooltip_text) {
  if (!browser_impl_.get()) {
    return;
  }

  CefString tooltip(tooltip_text);
  CefRefPtr<CefDisplayHandler> handler =
      browser_impl_->GetClient()->GetDisplayHandler();
  if (handler.get()) {
    handler->OnTooltip(browser_impl_.get(), tooltip);
  }
}

gfx::Size CefRenderWidgetHostViewOSR::GetCompositorViewportPixelSize() {
  return gfx::ScaleToCeiledSize(GetRequestedRendererSize(),
                                GetDeviceScaleFactor());
}

uint32_t CefRenderWidgetHostViewOSR::GetCaptureSequenceNumber() const {
  return latest_capture_sequence_number_;
}

void CefRenderWidgetHostViewOSR::CopyFromSurface(
    const gfx::Rect& src_rect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  if (delegated_frame_host_) {
    delegated_frame_host_->CopyFromCompositingSurface(src_rect, output_size,
                                                      std::move(callback));
  }
}

display::ScreenInfos CefRenderWidgetHostViewOSR::GetNewScreenInfosForUpdate() {
  display::ScreenInfo display_screen_info;

  if (browser_impl_) {
    CefScreenInfo screen_info(kDefaultScaleFactor, 0, 0, false, CefRect(),
                              CefRect());

    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    if (!handler->GetScreenInfo(browser_impl_.get(), screen_info) ||
        screen_info.rect.width == 0 || screen_info.rect.height == 0 ||
        screen_info.available_rect.width == 0 ||
        screen_info.available_rect.height == 0) {
      // If a screen rectangle was not provided, try using the view rectangle
      // instead. Otherwise, popup views may be drawn incorrectly, or not at
      // all.
      CefRect screenRect;
      handler->GetViewRect(browser_impl_.get(), screenRect);
      CHECK_GT(screenRect.width, 0);
      CHECK_GT(screenRect.height, 0);

      if (screen_info.rect.width == 0 || screen_info.rect.height == 0) {
        screen_info.rect = screenRect;
      }

      if (screen_info.available_rect.width == 0 ||
          screen_info.available_rect.height == 0) {
        screen_info.available_rect = screenRect;
      }
    }

    display_screen_info = ScreenInfoFrom(screen_info);
  }

  return display::ScreenInfos(display_screen_info);
}

void CefRenderWidgetHostViewOSR::TransformPointToRootSurface(
    gfx::PointF* point) {}

gfx::Rect CefRenderWidgetHostViewOSR::GetBoundsInRootWindow() {
  if (!browser_impl_.get()) {
    return gfx::Rect();
  }

  CefRect rc;
  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->client()->GetRenderHandler();
  CHECK(handler);
  if (handler->GetRootScreenRect(browser_impl_.get(), rc)) {
    return gfx::Rect(rc.x, rc.y, rc.width, rc.height);
  }
  return GetViewBounds();
}

#if !BUILDFLAG(IS_MAC)
viz::ScopedSurfaceIdAllocator
CefRenderWidgetHostViewOSR::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  base::OnceCallback<void()> allocation_task = base::BindOnce(
      &CefRenderWidgetHostViewOSR::OnDidUpdateVisualPropertiesComplete,
      weak_ptr_factory_.GetWeakPtr(), metadata);
  return viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}
#endif

viz::SurfaceId CefRenderWidgetHostViewOSR::GetCurrentSurfaceId() const {
  return delegated_frame_host_ ? delegated_frame_host_->GetCurrentSurfaceId()
                               : viz::SurfaceId();
}

void CefRenderWidgetHostViewOSR::ImeSetComposition(
    const CefString& text,
    const std::vector<CefCompositionUnderline>& underlines,
    const CefRange& replacement_range,
    const CefRange& selection_range) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::ImeSetComposition");
  if (!render_widget_host_) {
    return;
  }

  std::vector<ui::ImeTextSpan> web_underlines;
  web_underlines.reserve(underlines.size());
  for (const CefCompositionUnderline& line : underlines) {
    web_underlines.emplace_back(
        ui::ImeTextSpan::Type::kComposition, line.range.from, line.range.to,
        line.thick ? ui::ImeTextSpan::Thickness::kThick
                   : ui::ImeTextSpan::Thickness::kThin,
        GetImeUnderlineStyle(line.style), line.background_color, line.color,
        std::vector<std::string>());
  }
  gfx::Range range(replacement_range.from, replacement_range.to);

  // Start Monitoring for composition updates before we set.
  RequestImeCompositionUpdate(true);

  render_widget_host_->ImeSetComposition(
      text, web_underlines, range, selection_range.from, selection_range.to);
}

void CefRenderWidgetHostViewOSR::ImeCommitText(
    const CefString& text,
    const CefRange& replacement_range,
    int relative_cursor_pos) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::ImeCommitText");
  if (!render_widget_host_) {
    return;
  }

  gfx::Range range(replacement_range.from, replacement_range.to);
  render_widget_host_->ImeCommitText(text, std::vector<ui::ImeTextSpan>(),
                                     range, relative_cursor_pos);

  // Stop Monitoring for composition updates after we are done.
  RequestImeCompositionUpdate(false);
}

void CefRenderWidgetHostViewOSR::ImeFinishComposingText(bool keep_selection) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::ImeFinishComposingText");
  if (!render_widget_host_) {
    return;
  }

  render_widget_host_->ImeFinishComposingText(keep_selection);

  // Stop Monitoring for composition updates after we are done.
  RequestImeCompositionUpdate(false);
}

void CefRenderWidgetHostViewOSR::ImeCancelComposition() {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::ImeCancelComposition");
  if (!render_widget_host_) {
    return;
  }

  render_widget_host_->ImeCancelComposition();

  // Stop Monitoring for composition updates after we are done.
  RequestImeCompositionUpdate(false);
}

void CefRenderWidgetHostViewOSR::SelectionChanged(const std::u16string& text,
                                                  size_t offset,
                                                  const gfx::Range& range) {
  RenderWidgetHostViewBase::SelectionChanged(text, offset, range);

  if (!browser_impl_.get()) {
    return;
  }

  CefString selected_text;
  if (!range.is_empty() && !text.empty()) {
    size_t pos = range.GetMin() - offset;
    size_t n = range.length();
    if (pos + n <= text.length()) {
      selected_text = text.substr(pos, n);
    }
  }

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);

  CefRange cef_range(range.start(), range.end());
  handler->OnTextSelectionChanged(browser_impl_.get(), selected_text,
                                  cef_range);
}

const viz::LocalSurfaceId& CefRenderWidgetHostViewOSR::GetLocalSurfaceId()
    const {
  return const_cast<CefRenderWidgetHostViewOSR*>(this)
      ->GetOrCreateLocalSurfaceId();
}

void CefRenderWidgetHostViewOSR::UpdateFrameSinkIdRegistration() {
  RenderWidgetHostViewBase::UpdateFrameSinkIdRegistration();
  if (delegated_frame_host_) {
    delegated_frame_host_->SetIsFrameSinkIdOwner(is_frame_sink_id_owner());
  }
}

const viz::FrameSinkId& CefRenderWidgetHostViewOSR::GetFrameSinkId() const {
  return delegated_frame_host_
             ? delegated_frame_host_->frame_sink_id()
             : viz::FrameSinkIdAllocator::InvalidFrameSinkId();
}

viz::FrameSinkId CefRenderWidgetHostViewOSR::GetRootFrameSinkId() {
  return compositor_ ? compositor_->frame_sink_id() : viz::FrameSinkId();
}

void CefRenderWidgetHostViewOSR::NotifyHostAndDelegateOnWasShown(
    blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request) {
  // We don't call RenderWidgetHostViewBase::OnShowWithPageVisibility, so this
  // method should not be called.
  DCHECK(false);
}

void CefRenderWidgetHostViewOSR::
    RequestSuccessfulPresentationTimeFromHostOrDelegate(
        blink::mojom::RecordContentToVisibleTimeRequestPtr
            visible_time_request) {
  // We don't call RenderWidgetHostViewBase::OnShowWithPageVisibility, so this
  // method should not be called.
  DCHECK(false);
}

void CefRenderWidgetHostViewOSR::
    CancelSuccessfulPresentationTimeRequestForHostAndDelegate() {
  // We don't call RenderWidgetHostViewBase::OnShowWithPageVisibility, so this
  // method should not be called.
  DCHECK(false);
}

std::unique_ptr<content::SyntheticGestureTarget>
CefRenderWidgetHostViewOSR::CreateSyntheticGestureTarget() {
  return std::make_unique<CefSyntheticGestureTargetOSR>(host());
}

bool CefRenderWidgetHostViewOSR::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewBase* target_view,
    gfx::PointF* transformed_point) {
  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  return target_view->TransformPointToLocalCoordSpace(
      point, GetCurrentSurfaceId(), transformed_point);
}

void CefRenderWidgetHostViewOSR::DidNavigate() {
  if (!IsShowing()) {
    // Navigating while hidden should not allocate a new LocalSurfaceID. Once
    // sizes are ready, or we begin to Show, we can then allocate the new
    // LocalSurfaceId.
    InvalidateLocalSurfaceId();
  } else {
    if (is_first_navigation_) {
      // The first navigation does not need a new LocalSurfaceID. The renderer
      // can use the ID that was already provided.
      SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                                  GetLocalSurfaceId());
    } else {
      SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                                  std::nullopt);
    }
  }
  if (delegated_frame_host_) {
    delegated_frame_host_->DidNavigate();
  }
  is_first_navigation_ = false;
}

void CefRenderWidgetHostViewOSR::OnFrameComplete(
    const viz::BeginFrameAck& ack) {
  DCHECK(begin_frame_pending_);
  DCHECK_EQ(begin_frame_source_.source_id(), ack.frame_id.source_id);
  DCHECK_EQ(begin_frame_number_, ack.frame_id.sequence_number);
  begin_frame_pending_ = false;
}

void CefRenderWidgetHostViewOSR::OnRenderFrameMetadataChangedAfterActivation(
    base::TimeTicks activation_time) {
  auto metadata =
      host_->render_frame_metadata_provider()->LastRenderFrameMetadata();

  if (video_consumer_) {
    // Need to wait for the first frame of the new size before calling
    // SizeChanged. Otherwise, the video frame will be letterboxed.
    video_consumer_->SizeChanged(metadata.viewport_size_in_pixels);
  }

  gfx::PointF root_scroll_offset;
  if (metadata.root_scroll_offset) {
    root_scroll_offset = *metadata.root_scroll_offset;
  }
  if (root_scroll_offset != last_scroll_offset_) {
    last_scroll_offset_ = root_scroll_offset;

    if (!is_scroll_offset_changed_pending_) {
      is_scroll_offset_changed_pending_ = true;

      // Send the notification asynchronously.
      CEF_POST_TASK(
          CEF_UIT,
          base::BindOnce(&CefRenderWidgetHostViewOSR::OnScrollOffsetChanged,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  if (metadata.selection.start != selection_start_ ||
      metadata.selection.end != selection_end_) {
    selection_start_ = metadata.selection.start;
    selection_end_ = metadata.selection.end;
    selection_controller_client_->UpdateClientSelectionBounds(selection_start_,
                                                              selection_end_);
  }
}

std::unique_ptr<viz::HostDisplayClient>
CefRenderWidgetHostViewOSR::CreateHostDisplayClient() {
  host_display_client_ =
      new CefHostDisplayClientOSR(this, gfx::kNullAcceleratedWidget);
  host_display_client_->SetActive(true);
  return base::WrapUnique(host_display_client_);
}

bool CefRenderWidgetHostViewOSR::InstallTransparency() {
  if (background_color_ == SK_ColorTRANSPARENT) {
    SetBackgroundColor(background_color_);
    if (compositor_) {
      compositor_->SetBackgroundColor(background_color_);
    }
    return true;
  }
  return false;
}

void CefRenderWidgetHostViewOSR::WasResized() {
  // Only one resize will be in-flight at a time.
  if (hold_resize_) {
    if (!pending_resize_) {
      pending_resize_ = true;
    }
    return;
  }

  SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                              std::nullopt);
}

void CefRenderWidgetHostViewOSR::SynchronizeVisualProperties(
    const cc::DeadlinePolicy& deadline_policy,
    const std::optional<viz::LocalSurfaceId>& child_local_surface_id) {
  SetFrameRate();

  const bool resized = ResizeRootLayer();
  bool surface_id_updated = false;

  if (!resized && child_local_surface_id) {
    // Update the current surface ID.
    parent_local_surface_id_allocator_->UpdateFromChild(
        *child_local_surface_id);
    surface_id_updated = true;
  }

  // Allocate a new surface ID if the surface has been resized or if the current
  // ID is invalid (meaning we may have been evicted).
  if (resized || !GetCurrentLocalSurfaceId().is_valid()) {
    AllocateLocalSurfaceId();
    surface_id_updated = true;
  }

  if (surface_id_updated) {
    delegated_frame_host_->EmbedSurface(
        GetCurrentLocalSurfaceId(), GetViewBounds().size(), deadline_policy);

    // |render_widget_host_| will retrieve resize parameters from the
    // DelegatedFrameHost and this view, so SynchronizeVisualProperties must be
    // called last.
    if (render_widget_host_) {
      render_widget_host_->SynchronizeVisualProperties();
    }
  }
}

void CefRenderWidgetHostViewOSR::OnScreenInfoChanged() {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::OnScreenInfoChanged");
  InvalidateLocalSurfaceId();
  if (!render_widget_host_) {
    return;
  }

  SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                              std::nullopt);

  if (render_widget_host_->delegate()) {
    render_widget_host_->delegate()->SendScreenRects();
  } else {
    render_widget_host_->SendScreenRects();
  }

  render_widget_host_->NotifyScreenInfoChanged();

  // We might want to change the cursor scale factor here as well - see the
  // cache for the current_cursor_, as passed by UpdateCursor from the
  // renderer in the rwhv_aura (current_cursor_.SetScaleFactor)

  // Notify the guest hosts if any.
  for (auto guest_host_view : guest_host_views_) {
    guest_host_view->OnScreenInfoChanged();
  }
}

void CefRenderWidgetHostViewOSR::Invalidate(
    CefBrowserHost::PaintElementType type) {
  TRACE_EVENT1("cef", "CefRenderWidgetHostViewOSR::Invalidate", "type", type);
  if (!IsPopupWidget() && type == PET_POPUP) {
    if (popup_host_view_) {
      popup_host_view_->Invalidate(type);
    }
    return;
  }
  InvalidateInternal(gfx::Rect(SizeInPixels()));
}

void CefRenderWidgetHostViewOSR::SendExternalBeginFrame() {
  DCHECK(external_begin_frame_enabled_);

  if (begin_frame_pending_) {
    return;
  }
  begin_frame_pending_ = true;

  base::TimeTicks frame_time = base::TimeTicks::Now();
  base::TimeTicks deadline = base::TimeTicks();
  base::TimeDelta interval = viz::BeginFrameArgs::DefaultInterval();

  viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, begin_frame_source_.source_id(),
      ++begin_frame_number_, frame_time, deadline, interval,
      viz::BeginFrameArgs::NORMAL);

  DCHECK(begin_frame_args.IsValid());

  if (render_widget_host_) {
    render_widget_host_->ProgressFlingIfNeeded(frame_time);
  }

  if (compositor_) {
    compositor_->IssueExternalBeginFrame(
        begin_frame_args, /* force= */ true,
        base::BindOnce(&CefRenderWidgetHostViewOSR::OnFrameComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    begin_frame_pending_ = false;
  }

  if (!IsPopupWidget() && popup_host_view_) {
    popup_host_view_->SendExternalBeginFrame();
  }
}

void CefRenderWidgetHostViewOSR::SendKeyEvent(
    const content::NativeWebKeyboardEvent& event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendKeyEvent");
  content::RenderWidgetHostImpl* target_host = render_widget_host_;

  if (selection_controller_client_) {
    selection_controller_client_->CloseQuickMenuAndHideHandles();
  }

  // If there are multiple widgets on the page (such as when there are
  // out-of-process iframes), pick the one that should process this event.
  if (render_widget_host_ && render_widget_host_->delegate()) {
    target_host = render_widget_host_->delegate()->GetFocusedRenderWidgetHost(
        render_widget_host_);
  }

  if (target_host && target_host->GetView()) {
    // Direct routing requires that events go directly to the View.
    target_host->ForwardKeyboardEventWithLatencyInfo(
        event,
        ui::LatencyInfo(event.GetType() == blink::WebInputEvent::Type::kChar ||
                                event.GetType() ==
                                    blink::WebInputEvent::Type::kRawKeyDown
                            ? ui::SourceEventType::KEY_PRESS
                            : ui::SourceEventType::OTHER));
  }
}

void CefRenderWidgetHostViewOSR::SendMouseEvent(
    const blink::WebMouseEvent& event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendMouseEvent");
  if (!IsPopupWidget()) {
    if (browser_impl_ &&
        event.GetType() == blink::WebMouseEvent::Type::kMouseDown &&
        event.button != blink::WebPointerProperties::Button::kRight) {
      browser_impl_->CancelContextMenu();
    }

    if (selection_controller_client_) {
      selection_controller_client_->CloseQuickMenuAndHideHandles();
    }

    if (popup_host_view_) {
      if (popup_host_view_->popup_position_.Contains(
              event.PositionInWidget().x(), event.PositionInWidget().y())) {
        blink::WebMouseEvent popup_event(event);
        popup_event.SetPositionInWidget(
            event.PositionInWidget().x() -
                popup_host_view_->popup_position_.x(),
            event.PositionInWidget().y() -
                popup_host_view_->popup_position_.y());
        popup_event.SetPositionInScreen(popup_event.PositionInWidget().x(),
                                        popup_event.PositionInWidget().y());

        popup_host_view_->SendMouseEvent(popup_event);
        return;
      }
    } else if (!guest_host_views_.empty()) {
      for (auto guest_host_view : guest_host_views_) {
        if (!guest_host_view->render_widget_host_ ||
            !guest_host_view->render_widget_host_->GetView()) {
          continue;
        }
        const gfx::Rect& guest_bounds =
            guest_host_view->render_widget_host_->GetView()->GetViewBounds();
        if (guest_bounds.Contains(event.PositionInWidget().x(),
                                  event.PositionInWidget().y())) {
          blink::WebMouseEvent guest_event(event);
          guest_event.SetPositionInWidget(
              event.PositionInWidget().x() - guest_bounds.x(),
              event.PositionInWidget().y() - guest_bounds.y());
          guest_event.SetPositionInScreen(guest_event.PositionInWidget().x(),
                                          guest_event.PositionInWidget().y());

          guest_host_view->SendMouseEvent(guest_event);
          return;
        }
      }
    }
  }

  if (render_widget_host_ && render_widget_host_->GetView()) {
    if (ShouldRouteEvents()) {
      // RouteMouseEvent wants non-const pointer to WebMouseEvent, but it only
      // forwards it to RenderWidgetTargeter::FindTargetAndDispatch as a const
      // reference, so const_cast here is safe.
      render_widget_host_->delegate()->GetInputEventRouter()->RouteMouseEvent(
          this, const_cast<blink::WebMouseEvent*>(&event),
          ui::LatencyInfo(ui::SourceEventType::OTHER));
    } else {
      render_widget_host_->GetView()->ProcessMouseEvent(
          event, ui::LatencyInfo(ui::SourceEventType::OTHER));
    }
  }
}

void CefRenderWidgetHostViewOSR::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendMouseWheelEvent");

  if (!IsPopupWidget()) {
    if (browser_impl_) {
      browser_impl_->CancelContextMenu();
    }

    if (selection_controller_client_) {
      selection_controller_client_->CloseQuickMenuAndHideHandles();
    }

    if (popup_host_view_) {
      if (popup_host_view_->popup_position_.Contains(
              event.PositionInWidget().x(), event.PositionInWidget().y())) {
        blink::WebMouseWheelEvent popup_mouse_wheel_event(event);
        popup_mouse_wheel_event.SetPositionInWidget(
            event.PositionInWidget().x() -
                popup_host_view_->popup_position_.x(),
            event.PositionInWidget().y() -
                popup_host_view_->popup_position_.y());
        popup_mouse_wheel_event.SetPositionInScreen(
            popup_mouse_wheel_event.PositionInWidget().x(),
            popup_mouse_wheel_event.PositionInWidget().y());

        popup_host_view_->SendMouseWheelEvent(popup_mouse_wheel_event);
        return;
      } else {
        // Scrolling outside of the popup widget so destroy it.
        // Execute asynchronously to avoid deleting the widget from inside
        // some other callback.
        CEF_POST_TASK(
            CEF_UIT,
            base::BindOnce(&CefRenderWidgetHostViewOSR::CancelWidget,
                           popup_host_view_->weak_ptr_factory_.GetWeakPtr()));
      }
    } else if (!guest_host_views_.empty()) {
      for (auto guest_host_view : guest_host_views_) {
        if (!guest_host_view->render_widget_host_ ||
            !guest_host_view->render_widget_host_->GetView()) {
          continue;
        }
        const gfx::Rect& guest_bounds =
            guest_host_view->render_widget_host_->GetView()->GetViewBounds();
        if (guest_bounds.Contains(event.PositionInWidget().x(),
                                  event.PositionInWidget().y())) {
          blink::WebMouseWheelEvent guest_mouse_wheel_event(event);
          guest_mouse_wheel_event.SetPositionInWidget(
              event.PositionInWidget().x() - guest_bounds.x(),
              event.PositionInWidget().y() - guest_bounds.y());
          guest_mouse_wheel_event.SetPositionInScreen(
              guest_mouse_wheel_event.PositionInWidget().x(),
              guest_mouse_wheel_event.PositionInWidget().y());

          guest_host_view->SendMouseWheelEvent(guest_mouse_wheel_event);
          return;
        }
      }
    }
  }

  if (render_widget_host_ && render_widget_host_->GetView()) {
    blink::WebMouseWheelEvent mouse_wheel_event(event);

    mouse_wheel_phase_handler_.SendWheelEndForTouchpadScrollingIfNeeded(false);
    mouse_wheel_phase_handler_.AddPhaseIfNeededAndScheduleEndEvent(
        mouse_wheel_event, false);

    if (ShouldRouteEvents()) {
      render_widget_host_->delegate()
          ->GetInputEventRouter()
          ->RouteMouseWheelEvent(
              this, const_cast<blink::WebMouseWheelEvent*>(&mouse_wheel_event),
              ui::LatencyInfo(ui::SourceEventType::WHEEL));
    } else {
      render_widget_host_->GetView()->ProcessMouseWheelEvent(
          mouse_wheel_event, ui::LatencyInfo(ui::SourceEventType::WHEEL));
    }
  }
}

void CefRenderWidgetHostViewOSR::SendTouchEvent(const CefTouchEvent& event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendTouchEvent");

  if (!IsPopupWidget() && popup_host_view_) {
    if (!forward_touch_to_popup_ && event.type == CEF_TET_PRESSED &&
        pointer_state_.GetPointerCount() == 0) {
      forward_touch_to_popup_ =
          popup_host_view_->popup_position_.Contains(event.x, event.y);
    }

    if (forward_touch_to_popup_) {
      CefTouchEvent popup_event(event);
      popup_event.x -= popup_host_view_->popup_position_.x();
      popup_event.y -= popup_host_view_->popup_position_.y();
      popup_host_view_->SendTouchEvent(popup_event);
      return;
    }
  }

  // Update the touch event first.
  if (!pointer_state_.OnTouch(event)) {
    return;
  }

  if (selection_controller_->WillHandleTouchEvent(pointer_state_)) {
    pointer_state_.CleanupRemovedTouchPoints(event);
    return;
  }

  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(pointer_state_);

  blink::WebTouchEvent touch_event = ui::CreateWebTouchEventFromMotionEvent(
      pointer_state_, result.moved_beyond_slop_region, false);

  pointer_state_.CleanupRemovedTouchPoints(event);

  // Set unchanged touch point to StateStationary for touchmove and
  // touchcancel to make sure only send one ack per WebTouchEvent.
  if (!result.succeeded) {
    pointer_state_.MarkUnchangedTouchPointsAsStationary(&touch_event, event);
  }

  if (!render_widget_host_) {
    return;
  }

  ui::LatencyInfo latency_info = CreateLatencyInfo(touch_event);
  if (ShouldRouteEvents()) {
    render_widget_host_->delegate()->GetInputEventRouter()->RouteTouchEvent(
        this, &touch_event, latency_info);
  } else {
    render_widget_host_->ForwardTouchEventWithLatencyInfo(touch_event,
                                                          latency_info);
  }

  bool touch_end =
      touch_event.GetType() == blink::WebInputEvent::Type::kTouchEnd ||
      touch_event.GetType() == blink::WebInputEvent::Type::kTouchCancel;

  if (touch_end && IsPopupWidget() && parent_host_view_ &&
      parent_host_view_->popup_host_view_ == this) {
    parent_host_view_->forward_touch_to_popup_ = false;
  }
}

bool CefRenderWidgetHostViewOSR::ShouldRouteEvents() const {
  if (!render_widget_host_->delegate()) {
    return false;
  }

  // Do not route events that are currently targeted to page popups such as
  // <select> element drop-downs, since these cannot contain cross-process
  // frames.
  if (!render_widget_host_->delegate()->IsWidgetForPrimaryMainFrame(
          render_widget_host_)) {
    return false;
  }

  return !!render_widget_host_->delegate()->GetInputEventRouter();
}

void CefRenderWidgetHostViewOSR::SetFocus(bool focus) {
  if (!render_widget_host_) {
    return;
  }

  content::RenderWidgetHostImpl* widget =
      content::RenderWidgetHostImpl::From(render_widget_host_);
  if (focus) {
    widget->GotFocus();
    widget->SetActive(true);
  } else {
    if (browser_impl_) {
      browser_impl_->CancelContextMenu();
    }

    if (selection_controller_client_) {
      selection_controller_client_->CloseQuickMenuAndHideHandles();
    }

    widget->SetActive(false);
    widget->LostFocus();
  }
}

void CefRenderWidgetHostViewOSR::OnUpdateTextInputStateCalled(
    content::TextInputManager* text_input_manager,
    content::RenderWidgetHostViewBase* updated_view,
    bool did_update_state) {
  const auto state = text_input_manager->GetTextInputState();
  if (state && !state->show_ime_if_needed) {
    return;
  }

  CefRenderHandler::TextInputMode mode = CEF_TEXT_INPUT_MODE_NONE;
  if (state && state->type != ui::TEXT_INPUT_TYPE_NONE) {
    static_assert(
        static_cast<int>(CEF_TEXT_INPUT_MODE_MAX) ==
            static_cast<int>(ui::TEXT_INPUT_MODE_MAX),
        "Enum values in cef_text_input_mode_t must match ui::TextInputMode");
    mode = static_cast<CefRenderHandler::TextInputMode>(state->mode);
  }

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);

  handler->OnVirtualKeyboardRequested(browser_impl_->GetBrowser(), mode);
}

void CefRenderWidgetHostViewOSR::ProcessAckedTouchEvent(
    const content::TouchEventWithLatencyInfo& touch,
    blink::mojom::InputEventResultState ack_result) {
  const bool event_consumed =
      ack_result == blink::mojom::InputEventResultState::kConsumed;
  gesture_provider_.OnTouchEventAck(touch.event.unique_touch_event_id,
                                    event_consumed, false);
}

void CefRenderWidgetHostViewOSR::OnGestureEvent(
    const ui::GestureEventData& gesture) {
  if ((gesture.type() == ui::ET_GESTURE_PINCH_BEGIN ||
       gesture.type() == ui::ET_GESTURE_PINCH_UPDATE ||
       gesture.type() == ui::ET_GESTURE_PINCH_END) &&
      !pinch_zoom_enabled_) {
    return;
  }

  blink::WebGestureEvent web_event =
      ui::CreateWebGestureEventFromGestureEventData(gesture);

  // without this check, forwarding gestures does not work!
  if (web_event.GetType() == blink::WebInputEvent::Type::kUndefined) {
    return;
  }

  ui::LatencyInfo latency_info = CreateLatencyInfo(web_event);
  if (ShouldRouteEvents()) {
    render_widget_host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
        this, &web_event, latency_info);
  } else {
    render_widget_host_->ForwardGestureEventWithLatencyInfo(web_event,
                                                            latency_info);
  }
}

void CefRenderWidgetHostViewOSR::UpdateFrameRate() {
  frame_rate_threshold_us_ = 0;
  SetFrameRate();

  if (video_consumer_) {
    video_consumer_->SetFrameRate(base::Microseconds(frame_rate_threshold_us_));
  }

  // Notify the guest hosts if any.
  for (auto guest_host_view : guest_host_views_) {
    guest_host_view->UpdateFrameRate();
  }
}

gfx::Size CefRenderWidgetHostViewOSR::SizeInPixels() {
  return gfx::ScaleToCeiledSize(GetViewBounds().size(), GetDeviceScaleFactor());
}

#if BUILDFLAG(IS_MAC)
void CefRenderWidgetHostViewOSR::SetActive(bool active) {}

void CefRenderWidgetHostViewOSR::ShowDefinitionForSelection() {}

void CefRenderWidgetHostViewOSR::SpeakSelection() {}

void CefRenderWidgetHostViewOSR::SetWindowFrameInScreen(const gfx::Rect& rect) {
}

void CefRenderWidgetHostViewOSR::ShowSharePicker(
    const std::string& title,
    const std::string& text,
    const std::string& url,
    const std::vector<std::string>& file_paths,
    blink::mojom::ShareService::ShareCallback callback) {
  std::move(callback).Run(blink::mojom::ShareError::INTERNAL_ERROR);
}

uint64_t CefRenderWidgetHostViewOSR::GetNSViewId() const {
  return 0;
}
#endif  // BUILDFLAG(IS_MAC)

void CefRenderWidgetHostViewOSR::OnPaint(const gfx::Rect& damage_rect,
                                         const gfx::Size& pixel_size,
                                         const void* pixels) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::OnPaint");

  // Workaround for https://github.com/chromiumembedded/cef/issues/2817
  if (!is_showing_) {
    return;
  }

  if (!pixels) {
    return;
  }

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->client()->GetRenderHandler();
  CHECK(handler);

  gfx::Rect rect_in_pixels(0, 0, pixel_size.width(), pixel_size.height());
  rect_in_pixels.Intersect(damage_rect);

  CefRenderHandler::RectList rcList;
  rcList.emplace_back(rect_in_pixels.x(), rect_in_pixels.y(),
                      rect_in_pixels.width(), rect_in_pixels.height());

  handler->OnPaint(browser_impl_.get(), IsPopupWidget() ? PET_POPUP : PET_VIEW,
                   rcList, pixels, pixel_size.width(), pixel_size.height());

  // Release the resize hold when we reach the desired size.
  if (hold_resize_) {
    DCHECK_GT(cached_scale_factor_, 0);
    gfx::Size expected_size =
        gfx::ScaleToCeiledSize(GetViewBounds().size(), cached_scale_factor_);
    if (pixel_size == expected_size) {
      ReleaseResizeHold();
    }
  }
}

ui::Layer* CefRenderWidgetHostViewOSR::GetRootLayer() const {
  return root_layer_.get();
}

ui::TextInputType CefRenderWidgetHostViewOSR::GetTextInputType() {
  if (text_input_manager_ && text_input_manager_->GetTextInputState()) {
    return text_input_manager_->GetTextInputState()->type;
  }

  return ui::TEXT_INPUT_TYPE_NONE;
}

void CefRenderWidgetHostViewOSR::SetFrameRate() {
  CefRefPtr<AlloyBrowserHostImpl> browser;
  if (parent_host_view_) {
    // Use the same frame rate as the embedding browser.
    browser = parent_host_view_->browser_impl_;
  } else {
    browser = browser_impl_;
  }
  CHECK(browser);

  // Only set the frame rate one time.
  if (frame_rate_threshold_us_ != 0) {
    return;
  }

  int frame_rate =
      osr_util::ClampFrameRate(browser->settings().windowless_frame_rate);

  frame_rate_threshold_us_ = 1000000 / frame_rate;

  if (compositor_) {
    compositor_->SetDisplayVSyncParameters(
        base::TimeTicks::Now(), base::Microseconds(frame_rate_threshold_us_));
  }

  if (video_consumer_) {
    video_consumer_->SetFrameRate(base::Microseconds(frame_rate_threshold_us_));
  }
}

bool CefRenderWidgetHostViewOSR::SetScreenInfo() {
  // This method should not be called while the resize hold is active.
  DCHECK(!hold_resize_);

  display::ScreenInfo current_info = screen_infos_.current();

  // This will result in a call to GetNewScreenInfosForUpdate().
  UpdateScreenInfo();
  if (screen_infos_.current() == current_info) {
    // Nothing changed.
    return false;
  }

  // Notify the guest hosts if any.
  for (auto guest_host_view : guest_host_views_) {
    content::RenderWidgetHostImpl* rwhi = guest_host_view->render_widget_host();
    if (!rwhi) {
      continue;
    }
    auto guest_view_osr =
        static_cast<CefRenderWidgetHostViewOSR*>(rwhi->GetView());
    if (guest_view_osr) {
      guest_view_osr->SetScreenInfo();
    }
  }

  return true;
}

bool CefRenderWidgetHostViewOSR::SetViewBounds() {
  // This method should not be called while the resize hold is active.
  DCHECK(!hold_resize_);

  // Popup bounds are set in InitAsPopup.
  if (IsPopupWidget()) {
    return false;
  }

  const gfx::Rect& new_bounds = ::GetViewBounds(browser_impl_.get());
  if (new_bounds == current_view_bounds_) {
    return false;
  }

  current_view_bounds_ = new_bounds;
  return true;
}

bool CefRenderWidgetHostViewOSR::SetRootLayerSize(bool force) {
  const bool screen_info_changed = SetScreenInfo();
  const bool view_bounds_changed = SetViewBounds();
  if (!force && !screen_info_changed && !view_bounds_changed) {
    return false;
  }

  GetRootLayer()->SetBounds(gfx::Rect(GetViewBounds().size()));

  if (compositor_) {
    compositor_local_surface_id_allocator_.GenerateId();
    compositor_->SetScaleAndSize(
        GetDeviceScaleFactor(), SizeInPixels(),
        compositor_local_surface_id_allocator_.GetCurrentLocalSurfaceId());
  }

  return (screen_info_changed || view_bounds_changed);
}

bool CefRenderWidgetHostViewOSR::ResizeRootLayer() {
  if (!hold_resize_) {
    // The resize hold is not currently active.
    if (SetRootLayerSize(false /* force */)) {
      // The size has changed. Avoid resizing again until ReleaseResizeHold() is
      // called.
      hold_resize_ = true;
      cached_scale_factor_ = GetDeviceScaleFactor();
      return true;
    }
  } else if (!pending_resize_) {
    // The resize hold is currently active. Another resize will be triggered
    // from ReleaseResizeHold().
    pending_resize_ = true;
  }
  return false;
}

void CefRenderWidgetHostViewOSR::ReleaseResizeHold() {
  DCHECK(hold_resize_);
  hold_resize_ = false;
  cached_scale_factor_ = -1;
  if (pending_resize_) {
    pending_resize_ = false;
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefRenderWidgetHostViewOSR::WasResized,
                                 weak_ptr_factory_.GetWeakPtr()));
  }
}

void CefRenderWidgetHostViewOSR::CancelWidget() {
  if (render_widget_host_) {
    render_widget_host_->LostCapture();
  }

  Hide();

  if (IsPopupWidget() && browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    handler->OnPopupShow(browser_impl_.get(), false);
    browser_impl_ = nullptr;
  }

  if (parent_host_view_) {
    if (parent_host_view_->popup_host_view_ == this) {
      parent_host_view_->set_popup_host_view(nullptr);
    } else if (parent_host_view_->child_host_view_ == this) {
      parent_host_view_->set_child_host_view(nullptr);

      // Start rendering the parent view again.
      parent_host_view_->Show();
    } else {
      parent_host_view_->RemoveGuestHostView(this);
    }
    parent_host_view_ = nullptr;
  }

  if (render_widget_host_ && !is_destroyed_) {
    is_destroyed_ = true;

    // Don't delete the RWHI manually while owned by a std::unique_ptr in RVHI.
    // This matches a CHECK() in RenderWidgetHostImpl::Destroy().
    const bool also_delete = !render_widget_host_->owner_delegate();

    // Results in a call to Destroy().
    render_widget_host_->ShutdownAndDestroyWidget(also_delete);
  }
}

void CefRenderWidgetHostViewOSR::OnScrollOffsetChanged() {
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    handler->OnScrollOffsetChanged(browser_impl_.get(), last_scroll_offset_.x(),
                                   last_scroll_offset_.y());
  }
  is_scroll_offset_changed_pending_ = false;
}

void CefRenderWidgetHostViewOSR::AddGuestHostView(
    CefRenderWidgetHostViewOSR* guest_host) {
  guest_host_views_.insert(guest_host);
}

void CefRenderWidgetHostViewOSR::RemoveGuestHostView(
    CefRenderWidgetHostViewOSR* guest_host) {
  guest_host_views_.erase(guest_host);
}

void CefRenderWidgetHostViewOSR::InvalidateInternal(
    const gfx::Rect& bounds_in_pixels) {
  if (video_consumer_) {
    video_consumer_->RequestRefreshFrame(bounds_in_pixels);
  } else if (host_display_client_) {
    OnPaint(bounds_in_pixels, host_display_client_->GetPixelSize(),
            host_display_client_->GetPixelMemory());
  }
}

void CefRenderWidgetHostViewOSR::RequestImeCompositionUpdate(
    bool start_monitoring) {
  if (!render_widget_host_) {
    return;
  }
  render_widget_host_->RequestCompositionUpdates(false, start_monitoring);
}

void CefRenderWidgetHostViewOSR::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::optional<std::vector<gfx::Rect>>& character_bounds,
    const std::optional<std::vector<gfx::Rect>>& line_bounds) {
  if (browser_impl_.get()) {
    CefRange cef_range(range.start(), range.end());
    CefRenderHandler::RectList rcList;

    if (character_bounds.has_value()) {
      for (auto& rect : character_bounds.value()) {
        rcList.emplace_back(rect.x(), rect.y(), rect.width(), rect.height());
      }
    }

    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->GetClient()->GetRenderHandler();
    CHECK(handler);
    handler->OnImeCompositionRangeChanged(browser_impl_->GetBrowser(),
                                          cef_range, rcList);
  }
}

viz::FrameSinkId CefRenderWidgetHostViewOSR::AllocateFrameSinkId() {
  return render_widget_host_->GetFrameSinkId();
}

void CefRenderWidgetHostViewOSR::UpdateBackgroundColorFromRenderer(
    SkColor color) {
  if (color == background_color_) {
    return;
  }
  background_color_ = color;

  bool opaque = SkColorGetA(color) == SK_AlphaOPAQUE;
  GetRootLayer()->SetFillsBoundsOpaquely(opaque);
  GetRootLayer()->SetColor(color);
}
