// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include <stdint.h>

#include <utility>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/osr/osr_util.h"
#include "libcef/browser/osr/software_output_device_osr.h"
#include "libcef/browser/osr/synthetic_gesture_target_osr.h"
#include "libcef/browser/thread_util.h"

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/gl_helper.h"
#include "components/viz/common/switches.h"
#include "content/browser/bad_message.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/renderer_host/cursor_manager.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/input/motion_event_web.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/common/content_switches_internal.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_factory.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_frame.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace {

// The maximum number of damage rects to cache for outstanding frame requests
// (for OnAcceleratedPaint).
const size_t kMaxDamageRects = 10;

const float kDefaultScaleFactor = 1.0;

// The maximum number of retry counts if frame capture fails.
const int kFrameRetryLimit = 2;

static content::ScreenInfo ScreenInfoFrom(const CefScreenInfo& src) {
  content::ScreenInfo screenInfo;
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

class CefCompositorFrameSinkClient
    : public viz::mojom::CompositorFrameSinkClient {
 public:
  CefCompositorFrameSinkClient(viz::mojom::CompositorFrameSinkClient* forward,
                               CefRenderWidgetHostViewOSR* rwhv)
      : forward_(forward), render_widget_host_view_(rwhv) {}

  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override {
    forward_->DidReceiveCompositorFrameAck(resources);
  }

  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const base::flat_map<uint32_t, gfx::PresentationFeedback>&
                        feedbacks) override {
    if (render_widget_host_view_) {
      render_widget_host_view_->OnPresentCompositorFrame();
    }
    forward_->OnBeginFrame(args, feedbacks);
  }

  void OnBeginFramePausedChanged(bool paused) override {
    forward_->OnBeginFramePausedChanged(paused);
  }

  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override {
    forward_->ReclaimResources(resources);
  }

 private:
  viz::mojom::CompositorFrameSinkClient* const forward_;
  CefRenderWidgetHostViewOSR* const render_widget_host_view_;
};

#if !defined(OS_MACOSX)

class CefDelegatedFrameHostClient : public content::DelegatedFrameHostClient {
 public:
  explicit CefDelegatedFrameHostClient(CefRenderWidgetHostViewOSR* view)
      : view_(view) {}

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
        view_->render_widget_host()->delegate()->IsFullscreenForCurrentTab()) {
      return SK_ColorBLACK;
    }
    return *view_->GetBackgroundColor();
  }

  void OnBeginFrame(base::TimeTicks frame_time) override {
    // TODO(cef): Maybe we can use this method in combination with
    // OnSetNeedsBeginFrames() instead of using CefBeginFrameTimer.
    // See https://codereview.chromium.org/1841083007.
  }

  void OnFrameTokenChanged(uint32_t frame_token) override {
    view_->render_widget_host()->DidProcessFrame(frame_token);
  }

  float GetDeviceScaleFactor() const override {
    return view_->GetDeviceScaleFactor();
  }

  std::vector<viz::SurfaceId> CollectSurfaceIdsForEviction() override {
    return view_->render_widget_host()->CollectSurfaceIdsForEviction();
  }

  void InvalidateLocalSurfaceIdOnEviction() override {}

  bool ShouldShowStaleContentOnEviction() override { return false; }

 private:
  CefRenderWidgetHostViewOSR* const view_;

  DISALLOW_COPY_AND_ASSIGN(CefDelegatedFrameHostClient);
};

#endif  // !defined(OS_MACOSX)

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
        ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, time, 1);
  }
  return latency_info;
}

}  // namespace

// Used for managing copy requests when GPU compositing is enabled. Based on
// RendererOverridesHandler::InnerSwapCompositorFrame and
// DelegatedFrameHost::CopyFromCompositingSurface.
class CefCopyFrameGenerator {
 public:
  CefCopyFrameGenerator(int frame_rate_threshold_us,
                        CefRenderWidgetHostViewOSR* view)
      : view_(view),
        frame_retry_count_(0),
        next_frame_time_(base::TimeTicks::Now()),
        frame_duration_(
            base::TimeDelta::FromMicroseconds(frame_rate_threshold_us)),
        weak_ptr_factory_(this) {}

  void GenerateCopyFrame(const gfx::Rect& damage_rect) {
    if (!view_->render_widget_host())
      return;
    // The below code is similar in functionality to
    // DelegatedFrameHost::CopyFromCompositingSurface but we reuse the same
    // SkBitmap in the GPU codepath and avoid scaling where possible.
    // Let the compositor copy into a new SkBitmap
    std::unique_ptr<viz::CopyOutputRequest> request =
        std::make_unique<viz::CopyOutputRequest>(
            viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
            base::Bind(
                &CefCopyFrameGenerator::CopyFromCompositingSurfaceHasResult,
                weak_ptr_factory_.GetWeakPtr(), damage_rect));

    request->set_area(gfx::Rect(view_->GetCompositorViewportPixelSize()));
    view_->GetRootLayer()->RequestCopyOfOutput(std::move(request));
  }

  void set_frame_rate_threshold_us(int frame_rate_threshold_us) {
    frame_duration_ =
        base::TimeDelta::FromMicroseconds(frame_rate_threshold_us);
  }

 private:
  void CopyFromCompositingSurfaceHasResult(
      const gfx::Rect& damage_rect,
      std::unique_ptr<viz::CopyOutputResult> result) {
    if (result->IsEmpty() || result->size().IsEmpty() ||
        !view_->render_widget_host()) {
      OnCopyFrameCaptureFailure(damage_rect);
      return;
    }

    std::unique_ptr<SkBitmap> source =
        std::make_unique<SkBitmap>(result->AsSkBitmap());
    DCHECK(source);

    if (source) {
      std::shared_ptr<SkBitmap> bitmap(std::move(source));

      base::TimeTicks now = base::TimeTicks::Now();
      base::TimeDelta next_frame_in = next_frame_time_ - now;
      if (next_frame_in > frame_duration_ / 4) {
        next_frame_time_ += frame_duration_;
        base::PostDelayedTaskWithTraits(
            FROM_HERE, {content::BrowserThread::UI},
            base::Bind(&CefCopyFrameGenerator::OnCopyFrameCaptureSuccess,
                       weak_ptr_factory_.GetWeakPtr(), damage_rect, bitmap),
            next_frame_in);
      } else {
        next_frame_time_ = now + frame_duration_;
        OnCopyFrameCaptureSuccess(damage_rect, bitmap);
      }

      // Reset the frame retry count on successful frame generation.
      frame_retry_count_ = 0;
    } else {
      OnCopyFrameCaptureFailure(damage_rect);
    }
  }

  void OnCopyFrameCaptureFailure(const gfx::Rect& damage_rect) {
    const bool force_frame = (++frame_retry_count_ <= kFrameRetryLimit);
    if (force_frame) {
      // Retry with the same |damage_rect|.
      CEF_POST_TASK(CEF_UIT,
                    base::Bind(&CefCopyFrameGenerator::GenerateCopyFrame,
                               weak_ptr_factory_.GetWeakPtr(), damage_rect));
    }
  }

  void OnCopyFrameCaptureSuccess(const gfx::Rect& damage_rect,
                                 std::shared_ptr<SkBitmap> bitmap) {
    view_->OnPaint(damage_rect, bitmap->width(), bitmap->height(),
                   bitmap->getPixels());
  }

  CefRenderWidgetHostViewOSR* const view_;
  int frame_retry_count_;
  base::TimeTicks next_frame_time_;
  base::TimeDelta frame_duration_;

  base::WeakPtrFactory<CefCopyFrameGenerator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefCopyFrameGenerator);
};

// Used to control the VSync rate in subprocesses when BeginFrame scheduling is
// enabled.
class CefBeginFrameTimer : public viz::DelayBasedTimeSourceClient {
 public:
  CefBeginFrameTimer(int frame_rate_threshold_us, const base::Closure& callback)
      : callback_(callback) {
    time_source_.reset(new viz::DelayBasedTimeSource(
        base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::UI})
            .get()));
    time_source_->SetTimebaseAndInterval(
        base::TimeTicks(),
        base::TimeDelta::FromMicroseconds(frame_rate_threshold_us));
    time_source_->SetClient(this);
  }

  void SetActive(bool active) { time_source_->SetActive(active); }

  bool IsActive() const { return time_source_->Active(); }

  void SetFrameRateThresholdUs(int frame_rate_threshold_us) {
    time_source_->SetTimebaseAndInterval(
        base::TimeTicks::Now(),
        base::TimeDelta::FromMicroseconds(frame_rate_threshold_us));
  }

 private:
  // cc::TimerSourceClient implementation.
  void OnTimerTick() override { callback_.Run(); }

  const base::Closure callback_;
  std::unique_ptr<viz::DelayBasedTimeSource> time_source_;

  DISALLOW_COPY_AND_ASSIGN(CefBeginFrameTimer);
};

CefRenderWidgetHostViewOSR::CefRenderWidgetHostViewOSR(
    SkColor background_color,
    bool use_shared_texture,
    bool use_external_begin_frame,
    content::RenderWidgetHost* widget,
    CefRenderWidgetHostViewOSR* parent_host_view,
    bool is_guest_view_hack)
    : content::RenderWidgetHostViewBase(widget),
      background_color_(background_color),
      frame_rate_threshold_us_(0),
#if !defined(OS_MACOSX)
      compositor_widget_(gfx::kNullAcceleratedWidget),
#endif
      software_output_device_(NULL),
      hold_resize_(false),
      pending_resize_(false),
      pending_resize_force_(false),
      render_widget_host_(content::RenderWidgetHostImpl::From(widget)),
      has_parent_(parent_host_view != NULL),
      parent_host_view_(parent_host_view),
      popup_host_view_(NULL),
      child_host_view_(NULL),
      is_showing_(!render_widget_host_->is_hidden()),
      is_destroyed_(false),
      pinch_zoom_enabled_(content::IsPinchToZoomEnabled()),
      is_scroll_offset_changed_pending_(false),
      mouse_wheel_phase_handler_(this),
      gesture_provider_(CreateGestureProviderConfig(), this),
      forward_touch_to_popup_(false),
      weak_ptr_factory_(this) {
  DCHECK(render_widget_host_);
  DCHECK(!render_widget_host_->GetView());

  current_device_scale_factor_ = kDefaultScaleFactor;

  if (parent_host_view_) {
    browser_impl_ = parent_host_view_->browser_impl();
    DCHECK(browser_impl_);
  } else if (content::RenderViewHost::From(render_widget_host_)) {
    // CefBrowserHostImpl might not be created at this time for popups.
    browser_impl_ = CefBrowserHostImpl::GetBrowserForHost(
        content::RenderViewHost::From(render_widget_host_));
  }

#if !defined(OS_MACOSX)
  local_surface_id_allocator_.GenerateId();
  local_surface_id_allocation_ =
      local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation();
  delegated_frame_host_client_.reset(new CefDelegatedFrameHostClient(this));

  // Matching the attributes from BrowserCompositorMac.
  delegated_frame_host_ = std::make_unique<content::DelegatedFrameHost>(
      AllocateFrameSinkId(is_guest_view_hack),
      delegated_frame_host_client_.get(),
      false /* should_register_frame_sink_id */);

  root_layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
#endif

  PlatformCreateCompositorWidget(is_guest_view_hack);

  bool opaque = SkColorGetA(background_color_) == SK_AlphaOPAQUE;
  GetRootLayer()->SetFillsBoundsOpaquely(opaque);
  GetRootLayer()->SetColor(background_color_);

  external_begin_frame_enabled_ = use_external_begin_frame;

#if !defined(OS_MACOSX)
  // On macOS the ui::Compositor is created/owned by the platform view.
  content::ImageTransportFactory* factory =
      content::ImageTransportFactory::GetInstance();
  ui::ContextFactoryPrivate* context_factory_private =
      factory->GetContextFactoryPrivate();
  // Matching the attributes from RecyclableCompositorMac.
  compositor_.reset(new ui::Compositor(
      context_factory_private->AllocateFrameSinkId(),
      content::GetContextFactory(), context_factory_private,
      base::ThreadTaskRunnerHandle::Get(), false /* enable_pixel_canvas */,
      use_external_begin_frame ? this : nullptr, use_external_begin_frame));
  compositor_->SetAcceleratedWidget(compositor_widget_);

  // Tell the compositor to use shared textures if the client can handle
  // OnAcceleratedPaint.
  compositor_->EnableSharedTexture(use_shared_texture);

  compositor_->SetDelegate(this);
  compositor_->SetRootLayer(root_layer_.get());
  compositor_->AddChildFrameSink(GetFrameSinkId());
#endif

  if (browser_impl_.get())
    ResizeRootLayer(false);

  cursor_manager_.reset(new content::CursorManager(this));

  // Do this last because it may result in a call to SetNeedsBeginFrames.
  render_widget_host_->SetView(this);

  if (GetTextInputManager())
    GetTextInputManager()->AddObserver(this);

  if (render_widget_host_->delegate() &&
      render_widget_host_->delegate()->GetInputEventRouter()) {
    render_widget_host_->delegate()->GetInputEventRouter()->AddFrameSinkIdOwner(
        GetFrameSinkId(), this);
  }
}

CefRenderWidgetHostViewOSR::~CefRenderWidgetHostViewOSR() {
#if defined(OS_MACOSX)
  if (is_showing_)
    browser_compositor_->SetRenderWidgetHostIsHidden(true);
#else
  // Marking the DelegatedFrameHost as removed from the window hierarchy is
  // necessary to remove all connections to its old ui::Compositor.
  if (is_showing_)
    delegated_frame_host_->WasHidden();
  delegated_frame_host_->DetachFromCompositor();
#endif

  PlatformDestroyCompositorWidget();

  if (copy_frame_generator_.get())
    copy_frame_generator_.reset(NULL);

#if !defined(OS_MACOSX)
  delegated_frame_host_.reset(NULL);
  compositor_.reset(NULL);
  root_layer_.reset(NULL);
#endif

  DCHECK(parent_host_view_ == NULL);
  DCHECK(popup_host_view_ == NULL);
  DCHECK(child_host_view_ == NULL);
  DCHECK(guest_host_views_.empty());

  if (text_input_manager_)
    text_input_manager_->RemoveObserver(this);
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

  ResizeRootLayer(false);
  Show();
}

void CefRenderWidgetHostViewOSR::SetSize(const gfx::Size& size) {}

void CefRenderWidgetHostViewOSR::SetBounds(const gfx::Rect& rect) {}

gfx::NativeView CefRenderWidgetHostViewOSR::GetNativeView() const {
  return gfx::NativeView();
}

gfx::NativeViewAccessible
CefRenderWidgetHostViewOSR::GetNativeViewAccessible() {
  return gfx::NativeViewAccessible();
}

void CefRenderWidgetHostViewOSR::Focus() {}

bool CefRenderWidgetHostViewOSR::HasFocus() const {
  return false;
}

bool CefRenderWidgetHostViewOSR::IsSurfaceAvailableForCopy() const {
  return GetDelegatedFrameHost()->CanCopyFromCompositingSurface();
}

void CefRenderWidgetHostViewOSR::Show() {
  if (is_showing_)
    return;

  is_showing_ = true;

#if defined(OS_MACOSX)
  browser_compositor_->SetRenderWidgetHostIsHidden(false);
#else
  delegated_frame_host_->AttachToCompositor(compositor_.get());
  delegated_frame_host_->WasShown(
      GetLocalSurfaceIdAllocation().local_surface_id(),
      GetRootLayer()->bounds().size(), false);
#endif

  // Note that |render_widget_host_| will retrieve size parameters from the
  // DelegatedFrameHost, so it must have WasShown called after.
  if (render_widget_host_)
    render_widget_host_->WasShown(false);
}

void CefRenderWidgetHostViewOSR::Hide() {
  if (!is_showing_)
    return;

  if (browser_impl_.get())
    browser_impl_->CancelContextMenu();

  if (render_widget_host_)
    render_widget_host_->WasHidden();

#if defined(OS_MACOSX)
  browser_compositor_->SetRenderWidgetHostIsHidden(true);
#else
  GetDelegatedFrameHost()->WasHidden();
  GetDelegatedFrameHost()->DetachFromCompositor();
#endif

  is_showing_ = false;
}

bool CefRenderWidgetHostViewOSR::IsShowing() {
  return is_showing_;
}

void CefRenderWidgetHostViewOSR::EnsureSurfaceSynchronizedForWebTest() {
  ++latest_capture_sequence_number_;
  SynchronizeVisualProperties();
}

gfx::Rect CefRenderWidgetHostViewOSR::GetViewBounds() const {
  if (IsPopupWidget())
    return popup_position_;

  if (!browser_impl_.get())
    return gfx::Rect();

  CefRect rc;
  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);

  handler->GetViewRect(browser_impl_.get(), rc);
  CHECK_GT(rc.width, 0);
  CHECK_GT(rc.height, 0);

  return gfx::Rect(rc.x, rc.y, rc.width, rc.height);
}

void CefRenderWidgetHostViewOSR::SetBackgroundColor(SkColor color) {
  // The renderer will feed its color back to us with the first CompositorFrame.
  // We short-cut here to show a sensible color before that happens.
  UpdateBackgroundColorFromRenderer(color);

  DCHECK(SkColorGetA(color) == SK_AlphaOPAQUE ||
         SkColorGetA(color) == SK_AlphaTRANSPARENT);
  content::RenderWidgetHostViewBase::SetBackgroundColor(color);
}

base::Optional<SkColor> CefRenderWidgetHostViewOSR::GetBackgroundColor() const {
  return background_color_;
}

void CefRenderWidgetHostViewOSR::UpdateBackgroundColor() {}

bool CefRenderWidgetHostViewOSR::LockMouse() {
  return false;
}

void CefRenderWidgetHostViewOSR::UnlockMouse() {}

void CefRenderWidgetHostViewOSR::TakeFallbackContentFrom(
    content::RenderWidgetHostView* view) {
  DCHECK(!static_cast<RenderWidgetHostViewBase*>(view)
              ->IsRenderWidgetHostViewChildFrame());
  DCHECK(!static_cast<RenderWidgetHostViewBase*>(view)
              ->IsRenderWidgetHostViewGuest());
  CefRenderWidgetHostViewOSR* view_cef =
      static_cast<CefRenderWidgetHostViewOSR*>(view);
  SetBackgroundColor(view_cef->background_color_);
  if (GetDelegatedFrameHost() && view_cef->GetDelegatedFrameHost()) {
    GetDelegatedFrameHost()->TakeFallbackContentFrom(
        view_cef->GetDelegatedFrameHost());
  }
  host()->GetContentRenderingTimeoutFrom(view_cef->host());
}

void CefRenderWidgetHostViewOSR::DidCreateNewRendererCompositorFrameSink(
    viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink) {
  renderer_compositor_frame_sink_.reset(
      new CefCompositorFrameSinkClient(renderer_compositor_frame_sink, this));
  if (GetDelegatedFrameHost()) {
    GetDelegatedFrameHost()->DidCreateNewRendererCompositorFrameSink(
        renderer_compositor_frame_sink_.get());
  }
}

void CefRenderWidgetHostViewOSR::OnPresentCompositorFrame() {
  // Is Chromium rendering to a shared texture?
  void* shared_texture = nullptr;
  ui::Compositor* compositor = GetCompositor();
  if (compositor) {
    shared_texture = compositor->GetSharedTexture();
  }

  if (shared_texture) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->GetClient()->GetRenderHandler();
    CHECK(handler);

    CefRenderHandler::RectList rcList;

    {
      // Find the corresponding damage rect. If there isn't one pass the entire
      // view size for a full redraw.
      base::AutoLock lock_scope(damage_rect_lock_);

      // TODO: in the future we need to correlate the presentation
      // notification with the sequence number from BeginFrame
      gfx::Rect damage;
      auto const i = damage_rects_.begin();
      if (i != damage_rects_.end()) {
        damage = i->second;
        damage_rects_.erase(i);
      } else {
        damage = GetViewBounds();
      }
      rcList.push_back(
          CefRect(damage.x(), damage.y(), damage.width(), damage.height()));
    }

    handler->OnAcceleratedPaint(browser_impl_.get(),
                                IsPopupWidget() ? PET_POPUP : PET_VIEW, rcList,
                                shared_texture);
  }
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

void CefRenderWidgetHostViewOSR::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    base::Optional<viz::HitTestRegionList> hit_test_region_list) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::OnSwapCompositorFrame");

  // Update the frame rate. At this point we should have a valid connection back
  // to the Synthetic Frame Source, which is important so we can actually modify
  // the frame rate to something other than the default of 60Hz.
  if (sync_frame_rate_) {
    if (frame_rate_threshold_us_ != 0) {
      // TODO(cef): Figure out how to set the VSync interval. See issue #2517.
    }
    sync_frame_rate_ = false;
  }

  if (frame.metadata.root_scroll_offset != last_scroll_offset_) {
    last_scroll_offset_ = frame.metadata.root_scroll_offset;

    if (!is_scroll_offset_changed_pending_) {
      // Send the notification asnychronously.
      CEF_POST_TASK(
          CEF_UIT,
          base::Bind(&CefRenderWidgetHostViewOSR::OnScrollOffsetChanged,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

  if (!frame.render_pass_list.empty()) {
    if (software_output_device_) {
      if (!begin_frame_timer_.get()) {
        // If BeginFrame scheduling is enabled SoftwareOutputDevice activity
        // will be controlled via OnSetNeedsBeginFrames. Otherwise, activate
        // the SoftwareOutputDevice now (when the first frame is generated).
        software_output_device_->SetActive(true);
      }

      // The compositor will draw directly to the SoftwareOutputDevice which
      // then calls OnPaint.
      // We would normally call BrowserCompositorMac::SubmitCompositorFrame on
      // macOS, however it contains compositor resize logic that we don't want.
      // Consequently we instead call the SwapDelegatedFrame method directly.
      GetDelegatedFrameHost()->SubmitCompositorFrame(
          local_surface_id, std::move(frame), std::move(hit_test_region_list));
    } else {
      ui::Compositor* compositor = GetCompositor();
      if (!compositor)
        return;

      // Will be nullptr if we're not using shared textures.
      const void* shared_texture = compositor->GetSharedTexture();

      // Determine the damage rectangle for the current frame. This is the
      // same calculation that SwapDelegatedFrame uses.
      viz::RenderPass* root_pass = frame.render_pass_list.back().get();
      gfx::Size frame_size = root_pass->output_rect.size();
      gfx::Rect damage_rect =
          gfx::ToEnclosingRect(gfx::RectF(root_pass->damage_rect));
      damage_rect.Intersect(gfx::Rect(frame_size));

      if (shared_texture) {
        AddDamageRect(frame.metadata.begin_frame_ack.sequence_number,
                      damage_rect);
      }

      // We would normally call BrowserCompositorMac::SubmitCompositorFrame on
      // macOS, however it contains compositor resize logic that we don't
      // want. Consequently we instead call the SwapDelegatedFrame method
      // directly.
      GetDelegatedFrameHost()->SubmitCompositorFrame(
          local_surface_id, std::move(frame), std::move(hit_test_region_list));

      if (!shared_texture) {
        if (!copy_frame_generator_.get()) {
          copy_frame_generator_.reset(
              new CefCopyFrameGenerator(frame_rate_threshold_us_, this));
        }

        // Request a copy of the last compositor frame which will eventually
        // call OnPaint asynchronously.
        copy_frame_generator_->GenerateCopyFrame(damage_rect);
      }
    }
  }
}

void CefRenderWidgetHostViewOSR::ClearCompositorFrame() {
  // This method is only used for content rendering timeout when surface sync is
  // off.
  NOTREACHED();
}

void CefRenderWidgetHostViewOSR::ResetFallbackToFirstNavigationSurface() {
  GetDelegatedFrameHost()->ResetFallbackToFirstNavigationSurface();
}

void CefRenderWidgetHostViewOSR::InitAsPopup(
    content::RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
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

  popup_position_ = pos;

  CefRect widget_pos(pos.x(), pos.y(), pos.width(), pos.height());
  if (handler.get())
    handler->OnPopupSize(browser_impl_.get(), widget_pos);

  ResizeRootLayer(false);
  Show();
}

void CefRenderWidgetHostViewOSR::InitAsFullscreen(
    content::RenderWidgetHostView* reference_host_view) {
  NOTREACHED() << "Fullscreen widgets are not supported in OSR";
}

// Called for the "platform view" created by WebContentsViewGuest and owned by
// RenderWidgetHostViewGuest.
void CefRenderWidgetHostViewOSR::InitAsGuest(
    content::RenderWidgetHostView* parent_host_view,
    content::RenderWidgetHostViewGuest* guest_view) {
  DCHECK_EQ(parent_host_view_, parent_host_view);
  DCHECK(browser_impl_);

  parent_host_view_->AddGuestHostView(this);
  parent_host_view_->RegisterGuestViewFrameSwappedCallback(guest_view);
}

void CefRenderWidgetHostViewOSR::UpdateCursor(
    const content::WebCursor& cursor) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::UpdateCursor");
  if (!browser_impl_.get())
    return;

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);

  content::CursorInfo cursor_info;
  cursor.GetCursorInfo(&cursor_info);

  const cef_cursor_type_t cursor_type =
      static_cast<cef_cursor_type_t>(cursor_info.type);
  CefCursorInfo custom_cursor_info;
  if (cursor.IsCustom()) {
    custom_cursor_info.hotspot.x = cursor_info.hotspot.x();
    custom_cursor_info.hotspot.y = cursor_info.hotspot.y();
    custom_cursor_info.image_scale_factor = cursor_info.image_scale_factor;
    custom_cursor_info.buffer = cursor_info.custom_image.getPixels();
    custom_cursor_info.size.width = cursor_info.custom_image.width();
    custom_cursor_info.size.height = cursor_info.custom_image.height();
  }

#if defined(USE_AURA)
  content::WebCursor web_cursor = cursor;

  ui::PlatformCursor platform_cursor;
  if (web_cursor.IsCustom()) {
    ui::Cursor ui_cursor(ui::CursorType::kCustom);
    SkBitmap bitmap;
    gfx::Point hotspot;
    float scale_factor;
    web_cursor.CreateScaledBitmapAndHotspotFromCustomData(&bitmap, &hotspot,
                                                          &scale_factor);
    ui_cursor.set_custom_bitmap(bitmap);
    ui_cursor.set_custom_hotspot(hotspot);
    ui_cursor.set_device_scale_factor(scale_factor);

    // |web_cursor| owns the resulting |platform_cursor|.
    platform_cursor = web_cursor.GetPlatformCursor(ui_cursor);
  } else {
    platform_cursor = GetPlatformCursor(cursor_info.type);
  }

  handler->OnCursorChange(browser_impl_.get(), platform_cursor, cursor_type,
                          custom_cursor_info);
#elif defined(OS_MACOSX)
  // |web_cursor| owns the resulting |native_cursor|.
  content::WebCursor web_cursor = cursor;
  CefCursorHandle native_cursor = web_cursor.GetNativeCursor();
  handler->OnCursorChange(browser_impl_.get(), native_cursor, cursor_type,
                          custom_cursor_info);
#else
  // TODO(port): Implement this method to work on other platforms as part of
  // off-screen rendering support.
  NOTREACHED();
#endif
}

content::CursorManager* CefRenderWidgetHostViewOSR::GetCursorManager() {
  return cursor_manager_.get();
}

void CefRenderWidgetHostViewOSR::SetIsLoading(bool is_loading) {
  if (!is_loading)
    return;
  // Make sure gesture detection is fresh.
  gesture_provider_.ResetDetection();
  forward_touch_to_popup_ = false;
}

void CefRenderWidgetHostViewOSR::RenderProcessGone(
    base::TerminationStatus status,
    int error_code) {
  Destroy();
}

void CefRenderWidgetHostViewOSR::Destroy() {
  if (!is_destroyed_) {
    is_destroyed_ = true;

    if (has_parent_) {
      CancelWidget();
    } else {
      if (popup_host_view_)
        popup_host_view_->CancelWidget();
      if (child_host_view_)
        child_host_view_->CancelWidget();
      if (!guest_host_views_.empty()) {
        // Guest RWHVs will be destroyed when the associated RWHVGuest is
        // destroyed. This parent RWHV may be destroyed first, so disassociate
        // the guest RWHVs here without destroying them.
        for (auto guest_host_view : guest_host_views_)
          guest_host_view->parent_host_view_ = nullptr;
        guest_host_views_.clear();
      }
      Hide();
    }
  }

  delete this;
}

void CefRenderWidgetHostViewOSR::SetTooltipText(
    const base::string16& tooltip_text) {
  if (!browser_impl_.get())
    return;

  CefString tooltip(tooltip_text);
  CefRefPtr<CefDisplayHandler> handler =
      browser_impl_->GetClient()->GetDisplayHandler();
  if (handler.get()) {
    handler->OnTooltip(browser_impl_.get(), tooltip);
  }
}

gfx::Size CefRenderWidgetHostViewOSR::GetCompositorViewportPixelSize() const {
  return gfx::ScaleToCeiledSize(GetRequestedRendererSize(),
                                current_device_scale_factor_);
}

uint32_t CefRenderWidgetHostViewOSR::GetCaptureSequenceNumber() const {
  return latest_capture_sequence_number_;
}

void CefRenderWidgetHostViewOSR::CopyFromSurface(
    const gfx::Rect& src_rect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  GetDelegatedFrameHost()->CopyFromCompositingSurface(src_rect, output_size,
                                                      std::move(callback));
}

void CefRenderWidgetHostViewOSR::GetScreenInfo(
    content::ScreenInfo* results) const {
  if (!browser_impl_.get())
    return;

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

  *results = ScreenInfoFrom(screen_info);
}

void CefRenderWidgetHostViewOSR::TransformPointToRootSurface(
    gfx::PointF* point) {}

gfx::Rect CefRenderWidgetHostViewOSR::GetBoundsInRootWindow() {
  if (!browser_impl_.get())
    return gfx::Rect();

  CefRect rc;
  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->client()->GetRenderHandler();
  CHECK(handler);
  if (handler->GetRootScreenRect(browser_impl_.get(), rc))
    return gfx::Rect(rc.x, rc.y, rc.width, rc.height);
  return GetViewBounds();
}

viz::SurfaceId CefRenderWidgetHostViewOSR::GetCurrentSurfaceId() const {
  return GetDelegatedFrameHost()
             ? GetDelegatedFrameHost()->GetCurrentSurfaceId()
             : viz::SurfaceId();
}

content::BrowserAccessibilityManager*
CefRenderWidgetHostViewOSR::CreateBrowserAccessibilityManager(
    content::BrowserAccessibilityDelegate* delegate,
    bool for_root_frame) {
  return NULL;
}

void CefRenderWidgetHostViewOSR::ImeSetComposition(
    const CefString& text,
    const std::vector<CefCompositionUnderline>& underlines,
    const CefRange& replacement_range,
    const CefRange& selection_range) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::ImeSetComposition");
  if (!render_widget_host_)
    return;

  std::vector<ui::ImeTextSpan> web_underlines;
  web_underlines.reserve(underlines.size());
  for (const CefCompositionUnderline& line : underlines) {
    web_underlines.push_back(ui::ImeTextSpan(
        ui::ImeTextSpan::Type::kComposition, line.range.from, line.range.to,
        line.thick ? ui::ImeTextSpan::Thickness::kThick
                   : ui::ImeTextSpan::Thickness::kThin,
        line.background_color, line.color, std::vector<std::string>()));
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
  if (!render_widget_host_)
    return;

  gfx::Range range(replacement_range.from, replacement_range.to);
  render_widget_host_->ImeCommitText(text, std::vector<ui::ImeTextSpan>(),
                                     range, relative_cursor_pos);

  // Stop Monitoring for composition updates after we are done.
  RequestImeCompositionUpdate(false);
}

void CefRenderWidgetHostViewOSR::ImeFinishComposingText(bool keep_selection) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::ImeFinishComposingText");
  if (!render_widget_host_)
    return;

  render_widget_host_->ImeFinishComposingText(keep_selection);

  // Stop Monitoring for composition updates after we are done.
  RequestImeCompositionUpdate(false);
}

void CefRenderWidgetHostViewOSR::ImeCancelComposition() {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::ImeCancelComposition");
  if (!render_widget_host_)
    return;

  render_widget_host_->ImeCancelComposition();

  // Stop Monitoring for composition updates after we are done.
  RequestImeCompositionUpdate(false);
}

void CefRenderWidgetHostViewOSR::SelectionChanged(const base::string16& text,
                                                  size_t offset,
                                                  const gfx::Range& range) {
  RenderWidgetHostViewBase::SelectionChanged(text, offset, range);

  if (!browser_impl_.get())
    return;

  CefString selected_text;
  if (!range.is_empty() && !text.empty()) {
    size_t pos = range.GetMin() - offset;
    size_t n = range.length();
    if (pos + n <= text.length())
      selected_text = text.substr(pos, n);
  }

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->GetClient()->GetRenderHandler();
  CHECK(handler);

  CefRange cef_range(range.start(), range.end());
  handler->OnTextSelectionChanged(browser_impl_.get(), selected_text,
                                  cef_range);
}

#if !defined(OS_MACOSX)
const viz::LocalSurfaceIdAllocation&
CefRenderWidgetHostViewOSR::GetLocalSurfaceIdAllocation() const {
  return local_surface_id_allocation_;
}
#endif

const viz::FrameSinkId& CefRenderWidgetHostViewOSR::GetFrameSinkId() const {
  return GetDelegatedFrameHost()->frame_sink_id();
}

viz::FrameSinkId CefRenderWidgetHostViewOSR::GetRootFrameSinkId() {
#if defined(OS_MACOSX)
  return browser_compositor_->GetRootFrameSinkId();
#else
  return compositor_->frame_sink_id();
#endif
}

std::unique_ptr<content::SyntheticGestureTarget>
CefRenderWidgetHostViewOSR::CreateSyntheticGestureTarget() {
  return std::make_unique<CefSyntheticGestureTargetOSR>(host());
}

#if !defined(OS_MACOSX)
viz::ScopedSurfaceIdAllocator
CefRenderWidgetHostViewOSR::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  bool force =
      local_surface_id_allocation_ != metadata.local_surface_id_allocation;
  base::OnceCallback<void()> allocation_task =
      base::BindOnce(&CefRenderWidgetHostViewOSR::SynchronizeVisualProperties,
                     weak_ptr_factory_.GetWeakPtr(), force);
  return viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}
#endif

void CefRenderWidgetHostViewOSR::SetNeedsBeginFrames(bool enabled) {
  SetFrameRate();

  if (!external_begin_frame_enabled_) {
    // Start/stop the timer that sends BeginFrame requests.
    begin_frame_timer_->SetActive(enabled);
  }

  if (software_output_device_) {
    // When the SoftwareOutputDevice is active it will call OnPaint for each
    // frame. If the SoftwareOutputDevice is deactivated while an invalidation
    // region is pending it will call OnPaint immediately.
    software_output_device_->SetActive(enabled);
  }
}

void CefRenderWidgetHostViewOSR::SetWantsAnimateOnlyBeginFrames() {
  if (GetDelegatedFrameHost()) {
    GetDelegatedFrameHost()->SetWantsAnimateOnlyBeginFrames();
  }
}

bool CefRenderWidgetHostViewOSR::TransformPointToLocalCoordSpaceLegacy(
    const gfx::PointF& point,
    const viz::SurfaceId& original_surface,
    gfx::PointF* transformed_point) {
  // Transformations use physical pixels rather than DIP, so conversion
  // is necessary.
  gfx::PointF point_in_pixels =
      gfx::ConvertPointToPixel(current_device_scale_factor_, point);
  if (!GetDelegatedFrameHost()->TransformPointToLocalCoordSpaceLegacy(
          point_in_pixels, original_surface, transformed_point)) {
    return false;
  }

  *transformed_point =
      gfx::ConvertPointToDIP(current_device_scale_factor_, *transformed_point);
  return true;
}

bool CefRenderWidgetHostViewOSR::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewBase* target_view,
    gfx::PointF* transformed_point) {
  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  return false;
}

void CefRenderWidgetHostViewOSR::DidNavigate() {
  // With surface synchronization enabled we need to force synchronization on
  // first navigation.
  ResizeRootLayer(true);
#if defined(OS_MACOSX)
  browser_compositor_->DidNavigate();
#else
  if (delegated_frame_host_)
    delegated_frame_host_->DidNavigate();
#endif
}

void CefRenderWidgetHostViewOSR::OnDisplayDidFinishFrame(
    const viz::BeginFrameAck& /*ack*/) {
  // TODO(cef): is there something we need to track with this notification?
}

void CefRenderWidgetHostViewOSR::OnNeedsExternalBeginFrames(
    bool needs_begin_frames) {
  needs_external_begin_frames_ = needs_begin_frames;
}

std::unique_ptr<viz::SoftwareOutputDevice>
CefRenderWidgetHostViewOSR::CreateSoftwareOutputDevice(
    ui::Compositor* compositor) {
  DCHECK_EQ(GetCompositor(), compositor);
  DCHECK(!copy_frame_generator_);
  DCHECK(!software_output_device_);
  software_output_device_ = new CefSoftwareOutputDeviceOSR(
      compositor, background_color_ == SK_ColorTRANSPARENT,
      base::Bind(&CefRenderWidgetHostViewOSR::OnPaint,
                 weak_ptr_factory_.GetWeakPtr()));
  return base::WrapUnique(software_output_device_);
}

bool CefRenderWidgetHostViewOSR::InstallTransparency() {
  if (background_color_ == SK_ColorTRANSPARENT) {
    SetBackgroundColor(background_color_);
#if defined(OS_MACOSX)
    browser_compositor_->SetBackgroundColor(background_color_);
#else
    compositor_->SetBackgroundColor(background_color_);
#endif
    return true;
  }
  return false;
}

void CefRenderWidgetHostViewOSR::SynchronizeVisualProperties(bool force) {
  if (hold_resize_) {
    if (!pending_resize_)
      pending_resize_ = true;
    if (force)
      pending_resize_force_ = true;
    return;
  }

  ResizeRootLayer(force);
}

void CefRenderWidgetHostViewOSR::OnScreenInfoChanged() {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::OnScreenInfoChanged");
  if (!render_widget_host_)
    return;

  SynchronizeVisualProperties();

  if (render_widget_host_->delegate())
    render_widget_host_->delegate()->SendScreenRects();
  else
    render_widget_host_->SendScreenRects();

#if defined(OS_MACOSX)
  // RenderWidgetHostImpl will query BrowserCompositorMac for the dimensions
  // to send to the renderer, so it is required that BrowserCompositorMac be
  // updated first. Only notify RenderWidgetHostImpl of the update if any
  // properties it will query have changed.
  if (UpdateNSViewAndDisplay())
    render_widget_host_->NotifyScreenInfoChanged();
#else
  render_widget_host_->NotifyScreenInfoChanged();
#endif

  // We might want to change the cursor scale factor here as well - see the
  // cache for the current_cursor_, as passed by UpdateCursor from the
  // renderer in the rwhv_aura (current_cursor_.SetScaleFactor)

  // Notify the guest hosts if any.
  for (auto guest_host_view : guest_host_views_)
    guest_host_view->OnScreenInfoChanged();
}

void CefRenderWidgetHostViewOSR::Invalidate(
    CefBrowserHost::PaintElementType type) {
  TRACE_EVENT1("cef", "CefRenderWidgetHostViewOSR::Invalidate", "type", type);
  if (!IsPopupWidget() && type == PET_POPUP) {
    if (popup_host_view_)
      popup_host_view_->Invalidate(type);
    return;
  }

  InvalidateInternal(gfx::Rect(GetCompositorViewportPixelSize()));
}

void CefRenderWidgetHostViewOSR::SendExternalBeginFrame() {
  DCHECK(external_begin_frame_enabled_);

  base::TimeTicks frame_time = base::TimeTicks::Now();
  base::TimeTicks deadline = base::TimeTicks();
  base::TimeDelta interval = viz::BeginFrameArgs::DefaultInterval();

  viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, begin_frame_source_.source_id(),
      begin_frame_number_, frame_time, deadline, interval,
      viz::BeginFrameArgs::NORMAL);

  DCHECK(begin_frame_args.IsValid());
  begin_frame_number_++;

  if (render_widget_host_)
    render_widget_host_->ProgressFlingIfNeeded(frame_time);

  if (renderer_compositor_frame_sink_) {
    GetCompositor()->context_factory_private()->IssueExternalBeginFrame(
        GetCompositor(), begin_frame_args);
    renderer_compositor_frame_sink_->OnBeginFrame(begin_frame_args, {});
  }

  if (!IsPopupWidget() && popup_host_view_) {
    popup_host_view_->SendExternalBeginFrame();
  }
}

void CefRenderWidgetHostViewOSR::SendKeyEvent(
    const content::NativeWebKeyboardEvent& event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendKeyEvent");
  if (render_widget_host_ && render_widget_host_->GetView()) {
    // Direct routing requires that events go directly to the View.
    render_widget_host_->ForwardKeyboardEventWithLatencyInfo(
        event, ui::LatencyInfo(event.GetType() == blink::WebInputEvent::kChar ||
                                       event.GetType() ==
                                           blink::WebInputEvent::kRawKeyDown
                                   ? ui::SourceEventType::KEY_PRESS
                                   : ui::SourceEventType::OTHER));
  }
}

void CefRenderWidgetHostViewOSR::SendMouseEvent(
    const blink::WebMouseEvent& event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendMouseEvent");
  if (!IsPopupWidget()) {
    if (browser_impl_.get() &&
        event.GetType() == blink::WebMouseEvent::kMouseDown) {
      browser_impl_->CancelContextMenu();
    }

    if (popup_host_view_) {
      if (popup_host_view_->popup_position_.Contains(
              event.PositionInWidget().x, event.PositionInWidget().y)) {
        blink::WebMouseEvent popup_event(event);
        popup_event.SetPositionInWidget(
            event.PositionInWidget().x - popup_host_view_->popup_position_.x(),
            event.PositionInWidget().y - popup_host_view_->popup_position_.y());
        popup_event.SetPositionInScreen(popup_event.PositionInWidget().x,
                                        popup_event.PositionInWidget().y);

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
        if (guest_bounds.Contains(event.PositionInWidget().x,
                                  event.PositionInWidget().y)) {
          blink::WebMouseEvent guest_event(event);
          guest_event.SetPositionInWidget(
              event.PositionInWidget().x - guest_bounds.x(),
              event.PositionInWidget().y - guest_bounds.y());
          guest_event.SetPositionInScreen(guest_event.PositionInWidget().x,
                                          guest_event.PositionInWidget().y);

          guest_host_view->SendMouseEvent(guest_event);
          return;
        }
      }
    }
  }

  if (render_widget_host_ && render_widget_host_->GetView()) {
    // Direct routing requires that mouse events go directly to the View.
    render_widget_host_->GetView()->ProcessMouseEvent(
        event, ui::LatencyInfo(ui::SourceEventType::OTHER));
  }
}

void CefRenderWidgetHostViewOSR::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::SendMouseWheelEvent");

  blink::WebMouseWheelEvent mouse_wheel_event(event);

  mouse_wheel_phase_handler_.SendWheelEndForTouchpadScrollingIfNeeded(false);
  mouse_wheel_phase_handler_.AddPhaseIfNeededAndScheduleEndEvent(
      mouse_wheel_event, false);

  if (!IsPopupWidget()) {
    if (browser_impl_.get())
      browser_impl_->CancelContextMenu();

    if (popup_host_view_) {
      if (popup_host_view_->popup_position_.Contains(
              mouse_wheel_event.PositionInWidget().x,
              mouse_wheel_event.PositionInWidget().y)) {
        blink::WebMouseWheelEvent popup_mouse_wheel_event(mouse_wheel_event);
        popup_mouse_wheel_event.SetPositionInWidget(
            mouse_wheel_event.PositionInWidget().x -
                popup_host_view_->popup_position_.x(),
            mouse_wheel_event.PositionInWidget().y -
                popup_host_view_->popup_position_.y());
        popup_mouse_wheel_event.SetPositionInScreen(
            popup_mouse_wheel_event.PositionInWidget().x,
            popup_mouse_wheel_event.PositionInWidget().y);

        popup_host_view_->SendMouseWheelEvent(popup_mouse_wheel_event);
        return;
      } else {
        // Scrolling outside of the popup widget so destroy it.
        // Execute asynchronously to avoid deleting the widget from inside
        // some other callback.
        CEF_POST_TASK(
            CEF_UIT,
            base::Bind(&CefRenderWidgetHostViewOSR::CancelWidget,
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
        if (guest_bounds.Contains(mouse_wheel_event.PositionInWidget().x,
                                  mouse_wheel_event.PositionInWidget().y)) {
          blink::WebMouseWheelEvent guest_mouse_wheel_event(mouse_wheel_event);
          guest_mouse_wheel_event.SetPositionInWidget(
              mouse_wheel_event.PositionInWidget().x - guest_bounds.x(),
              mouse_wheel_event.PositionInWidget().y - guest_bounds.y());
          guest_mouse_wheel_event.SetPositionInScreen(
              guest_mouse_wheel_event.PositionInWidget().x,
              guest_mouse_wheel_event.PositionInWidget().y);

          guest_host_view->SendMouseWheelEvent(guest_mouse_wheel_event);
          return;
        }
      }
    }
  }

  if (render_widget_host_ && render_widget_host_->GetView()) {
    // Direct routing requires that mouse events go directly to the View.
    render_widget_host_->GetView()->ProcessMouseWheelEvent(
        mouse_wheel_event, ui::LatencyInfo(ui::SourceEventType::WHEEL));
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
  if (!pointer_state_.OnTouch(event))
    return;

  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(pointer_state_);

  blink::WebTouchEvent touch_event = ui::CreateWebTouchEventFromMotionEvent(
      pointer_state_, result.moved_beyond_slop_region, false);

  pointer_state_.CleanupRemovedTouchPoints(event);

  // Set unchanged touch point to StateStationary for touchmove and
  // touchcancel to make sure only send one ack per WebTouchEvent.
  if (!result.succeeded)
    pointer_state_.MarkUnchangedTouchPointsAsStationary(&touch_event, event);

  if (!render_widget_host_)
    return;

  ui::LatencyInfo latency_info = CreateLatencyInfo(touch_event);
  if (ShouldRouteEvents()) {
    render_widget_host_->delegate()->GetInputEventRouter()->RouteTouchEvent(
        this, &touch_event, latency_info);
  } else {
    render_widget_host_->ForwardTouchEventWithLatencyInfo(touch_event,
                                                          latency_info);
  }

  bool touch_end = touch_event.GetType() == blink::WebInputEvent::kTouchEnd ||
                   touch_event.GetType() == blink::WebInputEvent::kTouchCancel;

  if (touch_end && IsPopupWidget() && parent_host_view_ &&
      parent_host_view_->popup_host_view_ == this) {
    parent_host_view_->forward_touch_to_popup_ = false;
  }
}

bool CefRenderWidgetHostViewOSR::ShouldRouteEvents() const {
  if (!render_widget_host_->delegate())
    return false;

  // Do not route events that are currently targeted to page popups such as
  // <select> element drop-downs, since these cannot contain cross-process
  // frames.
  if (!render_widget_host_->delegate()->IsWidgetForMainFrame(
          render_widget_host_)) {
    return false;
  }

  return !!render_widget_host_->delegate()->GetInputEventRouter();
}

void CefRenderWidgetHostViewOSR::SendFocusEvent(bool focus) {
  if (!render_widget_host_)
    return;

  content::RenderWidgetHostImpl* widget =
      content::RenderWidgetHostImpl::From(render_widget_host_);
  if (focus) {
    widget->GotFocus();
    widget->SetActive(true);
  } else {
    if (browser_impl_.get())
      browser_impl_->CancelContextMenu();

    widget->SetActive(false);
    widget->LostFocus();
  }
}

void CefRenderWidgetHostViewOSR::OnUpdateTextInputStateCalled(
    content::TextInputManager* text_input_manager,
    content::RenderWidgetHostViewBase* updated_view,
    bool did_update_state) {
  const content::TextInputState* state =
      text_input_manager->GetTextInputState();
  if (state && !state->show_ime_if_needed)
    return;

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
    content::InputEventAckState ack_result) {
  const bool event_consumed =
      ack_result == content::INPUT_EVENT_ACK_STATE_CONSUMED;
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
  if (web_event.GetType() == blink::WebInputEvent::kUndefined)
    return;

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

  // Notify the guest hosts if any.
  for (auto guest_host_view : guest_host_views_)
    guest_host_view->UpdateFrameRate();
}

void CefRenderWidgetHostViewOSR::HoldResize() {
  if (!hold_resize_)
    hold_resize_ = true;
}

void CefRenderWidgetHostViewOSR::ReleaseResize() {
  if (!hold_resize_)
    return;

  hold_resize_ = false;
  if (pending_resize_) {
    bool force = pending_resize_force_;
    pending_resize_ = false;
    pending_resize_force_ = false;
    CEF_POST_TASK(
        CEF_UIT,
        base::Bind(&CefRenderWidgetHostViewOSR::SynchronizeVisualProperties,
                   weak_ptr_factory_.GetWeakPtr(), force));
  }
}

void CefRenderWidgetHostViewOSR::OnPaint(const gfx::Rect& damage_rect,
                                         int bitmap_width,
                                         int bitmap_height,
                                         void* bitmap_pixels) {
  TRACE_EVENT0("cef", "CefRenderWidgetHostViewOSR::OnPaint");

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->client()->GetRenderHandler();
  CHECK(handler);

  // Don't execute SynchronizeVisualProperties while the OnPaint callback is
  // pending.
  HoldResize();

  gfx::Rect rect_in_bitmap(0, 0, bitmap_width, bitmap_height);
  rect_in_bitmap.Intersect(damage_rect);

  CefRenderHandler::RectList rcList;
  rcList.push_back(CefRect(rect_in_bitmap.x(), rect_in_bitmap.y(),
                           rect_in_bitmap.width(), rect_in_bitmap.height()));

  handler->OnPaint(browser_impl_.get(), IsPopupWidget() ? PET_POPUP : PET_VIEW,
                   rcList, bitmap_pixels, bitmap_width, bitmap_height);

  ReleaseResize();
}

#if !defined(OS_MACOSX)

ui::Compositor* CefRenderWidgetHostViewOSR::GetCompositor() const {
  return compositor_.get();
}

ui::Layer* CefRenderWidgetHostViewOSR::GetRootLayer() const {
  return root_layer_.get();
}

content::DelegatedFrameHost* CefRenderWidgetHostViewOSR::GetDelegatedFrameHost()
    const {
  return delegated_frame_host_.get();
}

#endif  // !defined(OS_MACOSX)

void CefRenderWidgetHostViewOSR::SetFrameRate() {
  CefRefPtr<CefBrowserHostImpl> browser;
  if (parent_host_view_) {
    // Use the same frame rate as the embedding browser.
    browser = parent_host_view_->browser_impl_;
  } else {
    browser = browser_impl_;
  }
  CHECK(browser);

  // Only set the frame rate one time.
  if (frame_rate_threshold_us_ != 0)
    return;

  ui::Compositor* compositor = GetCompositor();

  int frame_rate;
  if (compositor && compositor->shared_texture_enabled()) {
    // No upper-bound when using OnAcceleratedPaint.
    frame_rate = browser->settings().windowless_frame_rate;
    if (frame_rate <= 0) {
      frame_rate = 1;
    }
    sync_frame_rate_ = true;
  } else {
    frame_rate =
        osr_util::ClampFrameRate(browser->settings().windowless_frame_rate);
  }

  frame_rate_threshold_us_ = 1000000 / frame_rate;

  if (compositor) {
    // TODO(cef): Figure out how to set the VSync interval. See issue #2517.
  }

  if (copy_frame_generator_.get()) {
    copy_frame_generator_->set_frame_rate_threshold_us(
        frame_rate_threshold_us_);
  }

  if (!external_begin_frame_enabled_) {
    if (begin_frame_timer_.get()) {
      begin_frame_timer_->SetFrameRateThresholdUs(frame_rate_threshold_us_);
    } else {
      begin_frame_timer_.reset(new CefBeginFrameTimer(
          frame_rate_threshold_us_,
          base::Bind(&CefRenderWidgetHostViewOSR::OnBeginFrameTimerTick,
                     weak_ptr_factory_.GetWeakPtr())));
    }
  }
}

void CefRenderWidgetHostViewOSR::SetDeviceScaleFactor() {
  float new_scale_factor = kDefaultScaleFactor;

  if (browser_impl_.get()) {
    CefScreenInfo screen_info(kDefaultScaleFactor, 0, 0, false, CefRect(),
                              CefRect());
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    if (handler->GetScreenInfo(browser_impl_.get(), screen_info)) {
      new_scale_factor = screen_info.device_scale_factor;
    }
  }

  current_device_scale_factor_ = new_scale_factor;

  // Notify the guest hosts if any.
  for (auto guest_host_view : guest_host_views_) {
    content::RenderWidgetHostImpl* rwhi = guest_host_view->render_widget_host();
    if (!rwhi)
      continue;
    if (rwhi->GetView())
      rwhi->GetView()->set_current_device_scale_factor(new_scale_factor);
  }
}

void CefRenderWidgetHostViewOSR::ResizeRootLayer(bool force) {
  SetFrameRate();

  const float orgScaleFactor = current_device_scale_factor_;
  SetDeviceScaleFactor();
  const bool scaleFactorDidChange =
      (orgScaleFactor != current_device_scale_factor_);

  gfx::Size size;
  if (!IsPopupWidget())
    size = GetViewBounds().size();
  else
    size = popup_position_.size();

  if (!force && !scaleFactorDidChange &&
      size == GetRootLayer()->bounds().size()) {
    return;
  }

  GetRootLayer()->SetBounds(gfx::Rect(size));

#if defined(OS_MACOSX)
  bool resized = UpdateNSViewAndDisplay();
#else
  const gfx::Size& size_in_pixels =
      gfx::ConvertSizeToPixel(current_device_scale_factor_, size);

  local_surface_id_allocator_.GenerateId();
  local_surface_id_allocation_ =
      local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation();

  if (GetCompositor()) {
    GetCompositor()->SetScaleAndSize(current_device_scale_factor_,
                                     size_in_pixels,
                                     local_surface_id_allocation_);
  }
  PlatformResizeCompositorWidget(size_in_pixels);

  bool resized = true;
  GetDelegatedFrameHost()->EmbedSurface(
      local_surface_id_allocation_.local_surface_id(), size,
      cc::DeadlinePolicy::UseDefaultDeadline());
#endif  // !defined(OS_MACOSX)

  // Note that |render_widget_host_| will retrieve resize parameters from the
  // DelegatedFrameHost, so it must have SynchronizeVisualProperties called
  // after.
  if (resized && render_widget_host_)
    render_widget_host_->SynchronizeVisualProperties();
}

void CefRenderWidgetHostViewOSR::OnBeginFrameTimerTick() {
  const base::TimeTicks frame_time = base::TimeTicks::Now();
  const base::TimeDelta vsync_period =
      base::TimeDelta::FromMicroseconds(frame_rate_threshold_us_);
  SendBeginFrame(frame_time, vsync_period);
}

void CefRenderWidgetHostViewOSR::SendBeginFrame(base::TimeTicks frame_time,
                                                base::TimeDelta vsync_period) {
  TRACE_EVENT1("cef", "CefRenderWidgetHostViewOSR::SendBeginFrame",
               "frame_time_us", frame_time.ToInternalValue());

  base::TimeTicks display_time = frame_time + vsync_period;

  // TODO(brianderson): Use adaptive draw-time estimation.
  base::TimeDelta estimated_browser_composite_time =
      base::TimeDelta::FromMicroseconds(
          (1.0f * base::Time::kMicrosecondsPerSecond) / (3.0f * 60));

  base::TimeTicks deadline = display_time - estimated_browser_composite_time;

  const viz::BeginFrameArgs& begin_frame_args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, begin_frame_source_.source_id(),
      begin_frame_number_, frame_time, deadline, vsync_period,
      viz::BeginFrameArgs::NORMAL);
  DCHECK(begin_frame_args.IsValid());
  begin_frame_number_++;

  if (render_widget_host_)
    render_widget_host_->ProgressFlingIfNeeded(frame_time);

  if (renderer_compositor_frame_sink_) {
    renderer_compositor_frame_sink_->OnBeginFrame(begin_frame_args, {});
  }
}

void CefRenderWidgetHostViewOSR::CancelWidget() {
  if (render_widget_host_)
    render_widget_host_->LostCapture();

  Hide();

  if (IsPopupWidget() && browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    CHECK(handler);
    handler->OnPopupShow(browser_impl_.get(), false);
    browser_impl_ = NULL;
  }

  if (parent_host_view_) {
    if (parent_host_view_->popup_host_view_ == this) {
      parent_host_view_->set_popup_host_view(NULL);
    } else if (parent_host_view_->child_host_view_ == this) {
      parent_host_view_->set_child_host_view(NULL);

      // Start rendering the parent view again.
      parent_host_view_->Show();
    } else {
      parent_host_view_->RemoveGuestHostView(this);
    }
    parent_host_view_ = NULL;
  }

  if (render_widget_host_ && !is_destroyed_) {
    is_destroyed_ = true;

    // Don't delete the RWHI manually while owned by a scoped_ptr in RVHI.
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

void CefRenderWidgetHostViewOSR::RegisterGuestViewFrameSwappedCallback(
    content::RenderWidgetHostViewGuest* guest_host_view) {
  guest_host_view->RegisterFrameSwappedCallback(base::BindOnce(
      &CefRenderWidgetHostViewOSR::OnGuestViewFrameSwapped,
      weak_ptr_factory_.GetWeakPtr(), base::Unretained(guest_host_view)));
  guest_host_view->set_current_device_scale_factor(
      current_device_scale_factor_);
}

void CefRenderWidgetHostViewOSR::OnGuestViewFrameSwapped(
    content::RenderWidgetHostViewGuest* guest_host_view) {
  InvalidateInternal(gfx::ConvertRectToPixel(current_device_scale_factor_,
                                             guest_host_view->GetViewBounds()));

  RegisterGuestViewFrameSwappedCallback(guest_host_view);
}

void CefRenderWidgetHostViewOSR::InvalidateInternal(
    const gfx::Rect& bounds_in_pixels) {
  if (software_output_device_) {
    software_output_device_->OnPaint(bounds_in_pixels);
  } else if (copy_frame_generator_.get()) {
    copy_frame_generator_->GenerateCopyFrame(bounds_in_pixels);
  }
}

void CefRenderWidgetHostViewOSR::RequestImeCompositionUpdate(
    bool start_monitoring) {
  if (!render_widget_host_)
    return;
  render_widget_host_->RequestCompositionUpdates(false, start_monitoring);
}

void CefRenderWidgetHostViewOSR::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  if (browser_impl_.get()) {
    CefRange cef_range(range.start(), range.end());
    CefRenderHandler::RectList rcList;

    for (size_t i = 0; i < character_bounds.size(); ++i) {
      rcList.push_back(CefRect(character_bounds[i].x(), character_bounds[i].y(),
                               character_bounds[i].width(),
                               character_bounds[i].height()));
    }

    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->GetClient()->GetRenderHandler();
    CHECK(handler);
    handler->OnImeCompositionRangeChanged(browser_impl_->GetBrowser(),
                                          cef_range, rcList);
  }
}

viz::FrameSinkId CefRenderWidgetHostViewOSR::AllocateFrameSinkId(
    bool is_guest_view_hack) {
  // GuestViews have two RenderWidgetHostViews and so we need to make sure
  // we don't have FrameSinkId collisions.
  // The FrameSinkId generated here must be unique with FrameSinkId allocated
  // in ContextFactoryPrivate.
  content::ImageTransportFactory* factory =
      content::ImageTransportFactory::GetInstance();
  return is_guest_view_hack
             ? factory->GetContextFactoryPrivate()->AllocateFrameSinkId()
             : viz::FrameSinkId(base::checked_cast<uint32_t>(
                                    render_widget_host_->GetProcess()->GetID()),
                                base::checked_cast<uint32_t>(
                                    render_widget_host_->GetRoutingID()));
}

void CefRenderWidgetHostViewOSR::UpdateBackgroundColorFromRenderer(
    SkColor color) {
  if (color == background_color_)
    return;
  background_color_ = color;

  bool opaque = SkColorGetA(color) == SK_AlphaOPAQUE;
  GetRootLayer()->SetFillsBoundsOpaquely(opaque);
  GetRootLayer()->SetColor(color);
}
