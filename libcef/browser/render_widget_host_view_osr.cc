// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/render_widget_host_view_osr.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/callback_helpers.h"
#include "cc/output/copy_output_request.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/compositor/resize_lock.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/public/browser/context_factory.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

const float kDefaultScaleFactor = 1.0;

// The rate at which new calls to OnPaint will be generated.
const int kDefaultFrameRate = 30;
const int kMaximumFrameRate = 60;

// The maximum number of retry counts if frame capture fails.
const int kFrameRetryLimit = 2;

// When accelerated compositing is enabled and a widget resize is pending,
// we delay further resizes of the UI. The following constant is the maximum
// length of time that we should delay further UI resizes while waiting for a
// resized frame from a renderer.
const int kResizeLockTimeoutMs = 67;

static blink::WebScreenInfo webScreenInfoFrom(const CefScreenInfo& src) {
  blink::WebScreenInfo webScreenInfo;
  webScreenInfo.deviceScaleFactor = src.device_scale_factor;
  webScreenInfo.depth = src.depth;
  webScreenInfo.depthPerComponent = src.depth_per_component;
  webScreenInfo.isMonochrome = src.is_monochrome ? true : false;
  webScreenInfo.rect = blink::WebRect(src.rect.x, src.rect.y,
                                      src.rect.width, src.rect.height);
  webScreenInfo.availableRect = blink::WebRect(src.available_rect.x,
                                               src.available_rect.y,
                                               src.available_rect.width,
                                               src.available_rect.height);

  return webScreenInfo;
}

// Root layer passed to the ui::Compositor.
class CefRootLayer : public ui::Layer, public ui::LayerDelegate {
 public:
  CefRootLayer()
      : Layer(ui::LAYER_TEXTURED) {
    set_delegate(this);
  }

  virtual ~CefRootLayer() { }

  // Overridden from LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
  }

  virtual void OnDelegatedFrameDamage(
      const gfx::Rect& damage_rect_in_dip) OVERRIDE {
  }

  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {
  }

  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Closure();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CefRootLayer);
};

// Used to prevent further resizes while a resize is pending.
class CefResizeLock : public content::ResizeLock {
 public:
  CefResizeLock(CefRenderWidgetHostViewOSR* host,
                const gfx::Size new_size,
                bool defer_compositor_lock,
                double timeout)
      : ResizeLock(new_size, defer_compositor_lock),
        host_(host),
        cancelled_(false),
        weak_ptr_factory_(this) {
    DCHECK(host_);
    host_->HoldResize();

    CEF_POST_DELAYED_TASK(
        CEF_UIT,
        base::Bind(&CefResizeLock::CancelLock,
                   weak_ptr_factory_.GetWeakPtr()),
        timeout);
  }

  virtual ~CefResizeLock() {
    CancelLock();
  }

  virtual bool GrabDeferredLock() OVERRIDE {
    return ResizeLock::GrabDeferredLock();
  }

  virtual void UnlockCompositor() OVERRIDE {
    ResizeLock::UnlockCompositor();
    compositor_lock_ = NULL;
  }

 protected:
  virtual void LockCompositor() OVERRIDE {
    ResizeLock::LockCompositor();
    compositor_lock_ = host_->compositor()->GetCompositorLock();
  }

  void CancelLock() {
    if (cancelled_)
      return;
    cancelled_ = true;
    UnlockCompositor();
    host_->ReleaseResize();
  }

 private:
  CefRenderWidgetHostViewOSR* host_;
  scoped_refptr<ui::CompositorLock> compositor_lock_;
  bool cancelled_;
  base::WeakPtrFactory<CefResizeLock> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefResizeLock);
};

}  // namespace

CefRenderWidgetHostViewOSR::CefRenderWidgetHostViewOSR(
    content::RenderWidgetHost* widget)
    : delegated_frame_host_(new content::DelegatedFrameHost(this)),
      compositor_widget_(gfx::kNullAcceleratedWidget),
      frame_rate_threshold_ms_(1000 / kDefaultFrameRate),
      frame_pending_(false),
      frame_in_progress_(false),
      frame_retry_count_(0),
      hold_resize_(false),
      pending_resize_(false),
      render_widget_host_(content::RenderWidgetHostImpl::From(widget)),
      parent_host_view_(NULL),
      popup_host_view_(NULL),
      is_showing_(true),
      is_destroyed_(false),
      is_scroll_offset_changed_pending_(false),
#if defined(OS_MACOSX)
      text_input_context_osr_mac_(NULL),
#endif
      weak_ptr_factory_(this) {
  DCHECK(render_widget_host_);
  render_widget_host_->SetView(this);

  // CefBrowserHostImpl might not be created at this time for popups.
  if (render_widget_host_->IsRenderView()) {
    browser_impl_ = CefBrowserHostImpl::GetBrowserForHost(
        content::RenderViewHost::From(render_widget_host_));
  }

  root_layer_.reset(new CefRootLayer);

  PlatformCreateCompositorWidget();
#if !defined(OS_MACOSX)
  // On OS X the ui::Compositor is created/owned by the platform view.
  compositor_.reset(
      new ui::Compositor(compositor_widget_,
                         content::GetContextFactory(),
                         base::MessageLoopProxy::current()));
#endif
  compositor_->SetRootLayer(root_layer_.get());

  if (browser_impl_.get()) {
    SetFrameRate();
    ResizeRootLayer();
    compositor_->SetScaleAndSize(CurrentDeviceScaleFactor(),
                                 root_layer_->bounds().size());
  }
}

CefRenderWidgetHostViewOSR::~CefRenderWidgetHostViewOSR() {
  // Marking the DelegatedFrameHost as removed from the window hierarchy is
  // necessary to remove all connections to its old ui::Compositor.
  if (is_showing_)
    delegated_frame_host_->WasHidden();
  delegated_frame_host_->RemovingFromWindow();

  PlatformDestroyCompositorWidget();

  delegated_frame_host_.reset(NULL);
  compositor_.reset(NULL);
  root_layer_.reset(NULL);
}

void CefRenderWidgetHostViewOSR::InitAsChild(gfx::NativeView parent_view) {
}

content::RenderWidgetHost*
    CefRenderWidgetHostViewOSR::GetRenderWidgetHost() const {
  return render_widget_host_;
}

void CefRenderWidgetHostViewOSR::SetSize(const gfx::Size& size) {
}

void CefRenderWidgetHostViewOSR::SetBounds(const gfx::Rect& rect) {
}

gfx::Vector2dF CefRenderWidgetHostViewOSR::GetLastScrollOffset() const {
  return last_scroll_offset_;
}

gfx::NativeView CefRenderWidgetHostViewOSR::GetNativeView() const {
  return gfx::NativeView();
}

gfx::NativeViewId CefRenderWidgetHostViewOSR::GetNativeViewId() const {
  return gfx::NativeViewId();
}

gfx::NativeViewAccessible
    CefRenderWidgetHostViewOSR::GetNativeViewAccessible() {
  return gfx::NativeViewAccessible();
}

ui::TextInputClient* CefRenderWidgetHostViewOSR::GetTextInputClient() {
  return NULL;
}

void CefRenderWidgetHostViewOSR::Focus() {
}

bool CefRenderWidgetHostViewOSR::HasFocus() const {
  return false;
}

bool CefRenderWidgetHostViewOSR::IsSurfaceAvailableForCopy() const {
  return delegated_frame_host_->CanCopyToBitmap();
}

void CefRenderWidgetHostViewOSR::Show() {
  WasShown();
}

void CefRenderWidgetHostViewOSR::Hide() {
  WasHidden();
}

bool CefRenderWidgetHostViewOSR::IsShowing() {
  return is_showing_;
}

gfx::Rect CefRenderWidgetHostViewOSR::GetViewBounds() const {
  if (IsPopupWidget())
    return popup_position_;

  if (!browser_impl_.get())
    return gfx::Rect();

  CefRect rc;
  browser_impl_->GetClient()->GetRenderHandler()->GetViewRect(
      browser_impl_.get(), rc);
  return gfx::Rect(rc.x, rc.y, rc.width, rc.height);
}

void CefRenderWidgetHostViewOSR::SetBackgroundOpaque(bool opaque) {
  content::RenderWidgetHostViewBase::SetBackgroundOpaque(opaque);
  if (render_widget_host_)
    render_widget_host_->SetBackgroundOpaque(opaque);
}

bool CefRenderWidgetHostViewOSR::LockMouse() {
  return false;
}

void CefRenderWidgetHostViewOSR::UnlockMouse() {
}

void CefRenderWidgetHostViewOSR::OnSwapCompositorFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  TRACE_EVENT0("libcef", "CefRenderWidgetHostViewOSR::OnSwapCompositorFrame");

  if (frame->metadata.root_scroll_offset != last_scroll_offset_) {
    last_scroll_offset_ = frame->metadata.root_scroll_offset;

    if (!is_scroll_offset_changed_pending_) {
      // Send the notification asnychronously.
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefRenderWidgetHostViewOSR::OnScrollOffsetChanged,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

  if (frame->delegated_frame_data) {
    delegated_frame_host_->SwapDelegatedFrame(
        output_surface_id,
        frame->delegated_frame_data.Pass(),
        frame->metadata.device_scale_factor,
        frame->metadata.latency_info);

    GenerateFrame(true);
    return;
  }

  if (frame->software_frame_data) {
    DLOG(ERROR) << "Unable to use software frame in CEF windowless rendering";
    if (render_widget_host_)
      render_widget_host_->GetProcess()->ReceivedBadMessage();
    return;
  }
}

void CefRenderWidgetHostViewOSR::InitAsPopup(
    content::RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  parent_host_view_ = static_cast<CefRenderWidgetHostViewOSR*>(
      parent_host_view);
  browser_impl_ = parent_host_view_->browser_impl();
  if (!browser_impl_.get())
    return;

  if (parent_host_view_->popup_host_view_) {
    // Cancel the previous popup widget.
    parent_host_view_->popup_host_view_->CancelPopupWidget();
  }

  parent_host_view_->set_popup_host_view(this);
  browser_impl_->GetClient()->GetRenderHandler()->OnPopupShow(
      browser_impl_.get(), true);

  popup_position_ = pos;

  const float scale_factor = CurrentDeviceScaleFactor();
  const gfx::Rect scaled_size =
      gfx::ToNearestRect(gfx::ScaleRect(pos, scale_factor));

  CefRect widget_pos(scaled_size.x(), scaled_size.y(),
                     scaled_size.width(), scaled_size.height());
  browser_impl_->GetClient()->GetRenderHandler()->OnPopupSize(
      browser_impl_.get(), widget_pos);

  SetFrameRate();
  ResizeRootLayer();
  compositor_->SetScaleAndSize(scale_factor, root_layer_->bounds().size());

  WasShown();
}

void CefRenderWidgetHostViewOSR::InitAsFullscreen(
    content::RenderWidgetHostView* reference_host_view) {
  NOTREACHED() << "Fullscreen widgets are not supported in OSR";
}

void CefRenderWidgetHostViewOSR::WasShown() {
  if (is_showing_)
    return;

  is_showing_ = true;
  if (render_widget_host_)
    render_widget_host_->WasShown(ui::LatencyInfo());
  delegated_frame_host_->AddedToWindow();
  delegated_frame_host_->WasShown(ui::LatencyInfo());
}

void CefRenderWidgetHostViewOSR::WasHidden() {
  if (!is_showing_)
    return;

  if (browser_impl_.get())
    browser_impl_->CancelContextMenu();

  if (render_widget_host_)
    render_widget_host_->WasHidden();
  delegated_frame_host_->WasHidden();
  delegated_frame_host_->RemovingFromWindow();
  is_showing_ = false;
}

void CefRenderWidgetHostViewOSR::MovePluginWindows(
    const std::vector<content::WebPluginGeometry>& moves) {
}

void CefRenderWidgetHostViewOSR::Blur() {
}

void CefRenderWidgetHostViewOSR::UpdateCursor(
    const content::WebCursor& cursor) {
  TRACE_EVENT0("libcef", "CefRenderWidgetHostViewOSR::UpdateCursor");
  if (!browser_impl_.get())
    return;

#if defined(USE_AURA)
  content::WebCursor web_cursor = cursor;

  ui::PlatformCursor platform_cursor;
  if (web_cursor.IsCustom()) {
    // |web_cursor| owns the resulting |platform_cursor|.
    platform_cursor = web_cursor.GetPlatformCursor();
  } else {
    content::WebCursor::CursorInfo cursor_info;
    cursor.GetCursorInfo(&cursor_info);
    platform_cursor = browser_impl_->GetPlatformCursor(cursor_info.type);
  }

  browser_impl_->GetClient()->GetRenderHandler()->OnCursorChange(
      browser_impl_.get(), platform_cursor);
#elif defined(OS_MACOSX)
  // |web_cursor| owns the resulting |native_cursor|.
  content::WebCursor web_cursor = cursor;
  CefCursorHandle native_cursor = web_cursor.GetNativeCursor();
  browser_impl_->GetClient()->GetRenderHandler()->OnCursorChange(
      browser_impl_.get(), native_cursor);
#else
  // TODO(port): Implement this method to work on other platforms as part of
  // off-screen rendering support.
  NOTREACHED();
#endif
}

void CefRenderWidgetHostViewOSR::SetIsLoading(bool is_loading) {
}

#if !defined(OS_MACOSX)
void CefRenderWidgetHostViewOSR::TextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
}

void CefRenderWidgetHostViewOSR::ImeCancelComposition() {
}
#endif  // !defined(OS_MACOSX)

void CefRenderWidgetHostViewOSR::RenderProcessGone(
    base::TerminationStatus status,
    int error_code) {
  // TODO(OSR): Need to also clear WebContentsViewOSR::view_?
  render_widget_host_ = NULL;
  parent_host_view_ = NULL;
  popup_host_view_ = NULL;
}

void CefRenderWidgetHostViewOSR::Destroy() {
  if (!is_destroyed_) {
    is_destroyed_ = true;

    if (IsPopupWidget()) {
      CancelPopupWidget();
    } else {
      if (popup_host_view_)
        popup_host_view_->CancelPopupWidget();
      WasHidden();
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

void CefRenderWidgetHostViewOSR::SelectionChanged(
    const base::string16& text,
    size_t offset,
    const gfx::Range& range) {
}

gfx::Size CefRenderWidgetHostViewOSR::GetRequestedRendererSize() const {
  return delegated_frame_host_->GetRequestedRendererSize();
}

gfx::Size CefRenderWidgetHostViewOSR::GetPhysicalBackingSize() const {
  float scale_factor = const_cast<CefRenderWidgetHostViewOSR*>(this)->
      CurrentDeviceScaleFactor();
  return gfx::ToCeiledSize(gfx::ScaleSize(GetRequestedRendererSize(),
                                          scale_factor));
}

void CefRenderWidgetHostViewOSR::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
}

void CefRenderWidgetHostViewOSR::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    const SkColorType color_type) {
  delegated_frame_host_->CopyFromCompositingSurface(
      src_subrect, dst_size, callback, color_type);
}

void CefRenderWidgetHostViewOSR::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(bool)>& callback) {
  delegated_frame_host_->CopyFromCompositingSurfaceToVideoFrame(
      src_subrect, target, callback);
}

bool CefRenderWidgetHostViewOSR::CanCopyToVideoFrame() const {
  return delegated_frame_host_->CanCopyToVideoFrame();
}

bool CefRenderWidgetHostViewOSR::CanSubscribeFrame() const {
  return delegated_frame_host_->CanSubscribeFrame();
}

void CefRenderWidgetHostViewOSR::BeginFrameSubscription(
    scoped_ptr<content::RenderWidgetHostViewFrameSubscriber> subscriber) {
  delegated_frame_host_->BeginFrameSubscription(subscriber.Pass());
}

void CefRenderWidgetHostViewOSR::EndFrameSubscription() {
  delegated_frame_host_->EndFrameSubscription();
}

void CefRenderWidgetHostViewOSR::AcceleratedSurfaceInitialized(
    int host_id,
    int route_id) {
}

void CefRenderWidgetHostViewOSR::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params_in_pixel,
    int gpu_host_id) {
  // Oldschool composited mode is no longer supported.
}

void CefRenderWidgetHostViewOSR::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params_in_pixel,
    int gpu_host_id) {
  // Oldschool composited mode is no longer supported.
}

void CefRenderWidgetHostViewOSR::AcceleratedSurfaceSuspend() {
}

void CefRenderWidgetHostViewOSR::AcceleratedSurfaceRelease() {
}

bool CefRenderWidgetHostViewOSR::HasAcceleratedSurface(
    const gfx::Size& desired_size) {
  // CEF doesn't use GetBackingStore for accelerated pages, so it doesn't
  // matter what is returned here as GetBackingStore is the only caller of this
  // method.
  NOTREACHED();
  return false;
}

void CefRenderWidgetHostViewOSR::GetScreenInfo(blink::WebScreenInfo* results) {
  if (!browser_impl_.get())
    return;

  CefScreenInfo screen_info(
      kDefaultScaleFactor, 0, 0, false, CefRect(), CefRect());

  CefRefPtr<CefRenderHandler> handler =
      browser_impl_->client()->GetRenderHandler();
  if (!handler->GetScreenInfo(browser_impl_.get(), screen_info) ||
      screen_info.rect.width == 0 ||
      screen_info.rect.height == 0 ||
      screen_info.available_rect.width == 0 ||
      screen_info.available_rect.height == 0) {
    // If a screen rectangle was not provided, try using the view rectangle
    // instead. Otherwise, popup views may be drawn incorrectly, or not at all.
    CefRect screenRect;
    if (!handler->GetViewRect(browser_impl_.get(), screenRect)) {
      NOTREACHED();
      screenRect = CefRect();
    }

    if (screen_info.rect.width == 0 && screen_info.rect.height == 0)
      screen_info.rect = screenRect;

    if (screen_info.available_rect.width == 0 &&
        screen_info.available_rect.height == 0)
      screen_info.available_rect = screenRect;
  }

  *results = webScreenInfoFrom(screen_info);
}

gfx::Rect CefRenderWidgetHostViewOSR::GetBoundsInRootWindow() {
  if (!browser_impl_.get())
    return gfx::Rect();

  CefRect rc;
  if (browser_impl_->GetClient()->GetRenderHandler()->GetRootScreenRect(
          browser_impl_.get(), rc)) {
    return gfx::Rect(rc.x, rc.y, rc.width, rc.height);
  }
  return gfx::Rect();
}

gfx::GLSurfaceHandle CefRenderWidgetHostViewOSR::GetCompositingSurface() {
  return content::ImageTransportFactory::GetInstance()->
      GetSharedSurfaceHandle();
}

content::BrowserAccessibilityManager*
    CefRenderWidgetHostViewOSR::CreateBrowserAccessibilityManager(
        content::BrowserAccessibilityDelegate* delegate) {
  return NULL;
}

#if defined(TOOLKIT_VIEWS) || defined(USE_AURA)
void CefRenderWidgetHostViewOSR::ShowDisambiguationPopup(
    const gfx::Rect& rect_pixels,
    const SkBitmap& zoomed_bitmap) {
}
#endif

#if !defined(OS_MACOSX) && defined(USE_AURA)
void CefRenderWidgetHostViewOSR::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
}
#endif

ui::Compositor* CefRenderWidgetHostViewOSR::GetCompositor() const {
  return compositor_.get();
}

ui::Layer* CefRenderWidgetHostViewOSR::GetLayer() {
  return root_layer_.get();
}

content::RenderWidgetHostImpl* CefRenderWidgetHostViewOSR::GetHost() {
  return render_widget_host_;
}

bool CefRenderWidgetHostViewOSR::IsVisible() {
  return IsShowing();
}

scoped_ptr<content::ResizeLock> CefRenderWidgetHostViewOSR::CreateResizeLock(
    bool defer_compositor_lock) {
  const gfx::Size& desired_size = root_layer_->bounds().size();
  return scoped_ptr<content::ResizeLock>(new CefResizeLock(
      this,
      desired_size,
      defer_compositor_lock,
      kResizeLockTimeoutMs));
}

gfx::Size CefRenderWidgetHostViewOSR::DesiredFrameSize() {
  return root_layer_->bounds().size();
}

float CefRenderWidgetHostViewOSR::CurrentDeviceScaleFactor() {
  if (!browser_impl_.get())
    return kDefaultScaleFactor;

  CefScreenInfo screen_info(
      kDefaultScaleFactor, 0, 0, false, CefRect(), CefRect());
  if (!browser_impl_->GetClient()->GetRenderHandler()->GetScreenInfo(
          browser_impl_.get(), screen_info)) {
    // Use the default
    return kDefaultScaleFactor;
  }

  return screen_info.device_scale_factor;
}

gfx::Size CefRenderWidgetHostViewOSR::ConvertViewSizeToPixel(
    const gfx::Size& size) {
  return content::ConvertViewSizeToPixel(this, size);
}

content::DelegatedFrameHost*
    CefRenderWidgetHostViewOSR::GetDelegatedFrameHost() const {
  return delegated_frame_host_.get();
}

bool CefRenderWidgetHostViewOSR::InstallTransparency() {
  if (browser_impl_.get() && browser_impl_->IsTransparent()) {
    SetBackgroundOpaque(false);
    return true;
  }
  return false;
}

void CefRenderWidgetHostViewOSR::WasResized() {
  if (hold_resize_) {
    if (!pending_resize_)
      pending_resize_ = true;
    return;
  }

  ResizeRootLayer();
  if (render_widget_host_)
    render_widget_host_->WasResized();
  delegated_frame_host_->WasResized();
}

void CefRenderWidgetHostViewOSR::OnScreenInfoChanged() {
  TRACE_EVENT0("libcef", "CefRenderWidgetHostViewOSR::OnScreenInfoChanged");
  if (!render_widget_host_)
    return;

  // TODO(OSR): Update the backing store.

  render_widget_host_->NotifyScreenInfoChanged();
  // We might want to change the cursor scale factor here as well - see the
  // cache for the current_cursor_, as passed by UpdateCursor from the renderer
  // in the rwhv_aura (current_cursor_.SetScaleFactor)
}

void CefRenderWidgetHostViewOSR::Invalidate(
    CefBrowserHost::PaintElementType type) {
  TRACE_EVENT1("libcef", "CefRenderWidgetHostViewOSR::Invalidate", "type",
               type); 
  if (!IsPopupWidget() && type == PET_POPUP) {
    if (popup_host_view_)
      popup_host_view_->Invalidate(type);
    return;
  }
  
  GenerateFrame(true);
}

void CefRenderWidgetHostViewOSR::SendKeyEvent(
    const content::NativeWebKeyboardEvent& event) {
  TRACE_EVENT0("libcef", "CefRenderWidgetHostViewOSR::SendKeyEvent");
  if (!render_widget_host_)
    return;
  render_widget_host_->ForwardKeyboardEvent(event);
}

void CefRenderWidgetHostViewOSR::SendMouseEvent(
    const blink::WebMouseEvent& event) {
  TRACE_EVENT0("libcef", "CefRenderWidgetHostViewOSR::SendMouseEvent");
  if (!IsPopupWidget()) {
    if (browser_impl_.get() && event.type == blink::WebMouseEvent::MouseDown)
      browser_impl_->CancelContextMenu();

    if (popup_host_view_ &&
        popup_host_view_->popup_position_.Contains(event.x, event.y)) {
      blink::WebMouseEvent popup_event(event);
      popup_event.x -= popup_host_view_->popup_position_.x();
      popup_event.y -= popup_host_view_->popup_position_.y();
      popup_event.windowX = popup_event.x;
      popup_event.windowY = popup_event.y;

      popup_host_view_->SendMouseEvent(popup_event);
      return;
    }
  }
  if (!render_widget_host_)
    return;
  render_widget_host_->ForwardMouseEvent(event);
}

void CefRenderWidgetHostViewOSR::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  TRACE_EVENT0("libcef", "CefRenderWidgetHostViewOSR::SendMouseWheelEvent");
  if (!IsPopupWidget()) {
    if (browser_impl_.get())
      browser_impl_->CancelContextMenu();

    if (popup_host_view_) {
      if (popup_host_view_->popup_position_.Contains(event.x, event.y)) {
        blink::WebMouseWheelEvent popup_event(event);
        popup_event.x -= popup_host_view_->popup_position_.x();
        popup_event.y -= popup_host_view_->popup_position_.y();
        popup_event.windowX = popup_event.x;
        popup_event.windowY = popup_event.y;
        popup_host_view_->SendMouseWheelEvent(popup_event);
        return;
      } else {
        // Scrolling outside of the popup widget so destroy it.
        // Execute asynchronously to avoid deleting the widget from inside some
        // other callback.
        CEF_POST_TASK(CEF_UIT,
            base::Bind(&CefRenderWidgetHostViewOSR::CancelPopupWidget,
                       popup_host_view_->weak_ptr_factory_.GetWeakPtr()));
      }
    }
  }
  if (!render_widget_host_)
    return;
  render_widget_host_->ForwardWheelEvent(event);
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
    widget->Blur();
  }
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
    pending_resize_ = false;
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefRenderWidgetHostViewOSR::WasResized,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void CefRenderWidgetHostViewOSR::SetFrameRate() {
  if (!browser_impl_.get())
    return;
  int frame_rate = browser_impl_->settings().windowless_frame_rate;
  if (frame_rate < 1)
    frame_rate = kDefaultFrameRate;
  else if (frame_rate > kMaximumFrameRate)
    frame_rate = kMaximumFrameRate;
  frame_rate_threshold_ms_ = 1000 / frame_rate;
}

void CefRenderWidgetHostViewOSR::ResizeRootLayer() {
  gfx::Size size;
  if (!IsPopupWidget())
    size = GetViewBounds().size();
  else
    size = popup_position_.size();
  root_layer_->SetBounds(gfx::Rect(0, 0, size.width(), size.height()));
}

void CefRenderWidgetHostViewOSR::GenerateFrame(bool force_frame) {
  if (force_frame && !frame_pending_)
    frame_pending_ = true;

  // No frame needs to be generated at this time.
  if (!frame_pending_)
    return;

  // Don't attempt to generate a frame while one is currently in-progress.
  if (frame_in_progress_)
    return;
  frame_in_progress_ = true;

  // Don't exceed the frame rate threshold.
  const int64 frame_rate_delta =
      (base::TimeTicks::Now() - frame_start_time_).InMilliseconds();
  if (frame_rate_delta < frame_rate_threshold_ms_) {
    // Generate the frame after the necessary time has passed.
    CEF_POST_DELAYED_TASK(CEF_UIT,
        base::Bind(&CefRenderWidgetHostViewOSR::InternalGenerateFrame,
                   weak_ptr_factory_.GetWeakPtr()),
        frame_rate_threshold_ms_ - frame_rate_delta);
    return;
  }

  InternalGenerateFrame();
}

void CefRenderWidgetHostViewOSR::InternalGenerateFrame() {
  frame_pending_ = false;
  frame_start_time_ = base::TimeTicks::Now();

  // The below code is similar in functionality to
  // DelegatedFrameHost::CopyFromCompositingSurface but we reuse the same
  // SkBitmap in the GPU codepath and avoid scaling where possible.
  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateRequest(base::Bind(
          &CefRenderWidgetHostViewOSR::CopyFromCompositingSurfaceHasResult,
          weak_ptr_factory_.GetWeakPtr()));

  const gfx::Rect& src_subrect_in_pixel =
      content::ConvertRectToPixel(CurrentDeviceScaleFactor(),
                                  root_layer_->bounds());
  request->set_area(src_subrect_in_pixel);
  RequestCopyOfOutput(request.Pass());
}

void CefRenderWidgetHostViewOSR::CopyFromCompositingSurfaceHasResult(
    scoped_ptr<cc::CopyOutputResult> result) {
  if (result->IsEmpty() || result->size().IsEmpty()) {
    OnFrameCaptureFailure();
    return;
  }

  if (result->HasTexture()) {
    PrepareTextureCopyOutputResult(result.Pass());
    return;
  }

  DCHECK(result->HasBitmap());
  PrepareBitmapCopyOutputResult(result.Pass());
}

void CefRenderWidgetHostViewOSR::PrepareTextureCopyOutputResult(
    scoped_ptr<cc::CopyOutputResult> result) {
  DCHECK(result->HasTexture());
  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(&CefRenderWidgetHostViewOSR::OnFrameCaptureFailure,
                 weak_ptr_factory_.GetWeakPtr()));

  const gfx::Size& result_size = result->size();
  SkIRect bitmap_size;
  if (bitmap_)
    bitmap_->getBounds(&bitmap_size);

  if (!bitmap_ ||
      bitmap_size.width() != result_size.width() ||
      bitmap_size.height() != result_size.height()) {
    // Create a new bitmap if the size has changed.
    bitmap_.reset(new SkBitmap);
    bitmap_->allocN32Pixels(result_size.width(),
                            result_size.height(),
                            true);
    if (bitmap_->drawsNothing())
      return;
  }

  content::ImageTransportFactory* factory =
      content::ImageTransportFactory::GetInstance();
  content::GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;

  scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock(
      new SkAutoLockPixels(*bitmap_));
  uint8* pixels = static_cast<uint8*>(bitmap_->getPixels());

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());
  if (!texture_mailbox.IsTexture())
    return;

  ignore_result(scoped_callback_runner.Release());

  gl_helper->CropScaleReadbackAndCleanMailbox(
      texture_mailbox.mailbox(),
      texture_mailbox.sync_point(),
      result_size,
      gfx::Rect(result_size),
      result_size,
      pixels,
      kN32_SkColorType,
      base::Bind(
          &CefRenderWidgetHostViewOSR::CopyFromCompositingSurfaceFinishedProxy,
          weak_ptr_factory_.GetWeakPtr(),
          base::Passed(&release_callback),
          base::Passed(&bitmap_),
          base::Passed(&bitmap_pixels_lock)),
      content::GLHelper::SCALER_QUALITY_FAST);
}

// static
void CefRenderWidgetHostViewOSR::CopyFromCompositingSurfaceFinishedProxy(
    base::WeakPtr<CefRenderWidgetHostViewOSR> view,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    scoped_ptr<SkBitmap> bitmap,
    scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock,
    bool result) {
  // This method may be called after the view has been deleted.
  uint32 sync_point = 0;
  if (result) {
    content::GLHelper* gl_helper =
        content::ImageTransportFactory::GetInstance()->GetGLHelper();
    sync_point = gl_helper->InsertSyncPoint();
  }
  bool lost_resource = sync_point == 0;
  release_callback->Run(sync_point, lost_resource);

  if (view) {
    view->CopyFromCompositingSurfaceFinished(
        bitmap.Pass(), bitmap_pixels_lock.Pass(), result);
  } else {
    bitmap_pixels_lock.reset();
    bitmap.reset();
  }
}

void CefRenderWidgetHostViewOSR::CopyFromCompositingSurfaceFinished(
    scoped_ptr<SkBitmap> bitmap,
    scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock,
    bool result) {
  // Restore ownership of the bitmap to the view.
  DCHECK(!bitmap_);
  bitmap_ = bitmap.Pass();

  if (result) {
    OnFrameCaptureSuccess(*bitmap_, bitmap_pixels_lock.Pass());
  } else {
    bitmap_pixels_lock.reset();
    OnFrameCaptureFailure();
  }
}

void CefRenderWidgetHostViewOSR::PrepareBitmapCopyOutputResult(
    scoped_ptr<cc::CopyOutputResult> result) {
  DCHECK(result->HasBitmap());
  scoped_ptr<SkBitmap> source = result->TakeBitmap();
  DCHECK(source);
  if (source) {
    scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock(
        new SkAutoLockPixels(*source));
    OnFrameCaptureSuccess(*source, bitmap_pixels_lock.Pass());
  } else {
    OnFrameCaptureFailure();
  }
}

void CefRenderWidgetHostViewOSR::OnFrameCaptureFailure() {
  const bool force_frame = (++frame_retry_count_ <= kFrameRetryLimit);
  OnFrameCaptureCompletion(force_frame);
}

void CefRenderWidgetHostViewOSR::OnFrameCaptureSuccess(
    const SkBitmap& bitmap,
    scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock) {
  SkRect bounds;
  bitmap.getBounds(&bounds);

  CefRenderHandler::RectList rcList;
  rcList.push_back(CefRect(0, 0, bounds.width(), bounds.height()));

  browser_impl_->GetClient()->GetRenderHandler()->OnPaint(
        browser_impl_.get(),
        IsPopupWidget() ? PET_POPUP : PET_VIEW,
        rcList,
        bitmap.getPixels(),
        bounds.width(),
        bounds.height());

  bitmap_pixels_lock.reset();

  // Reset the frame retry count on successful frame generation.
  if (frame_retry_count_ > 0)
    frame_retry_count_ = 0;

  OnFrameCaptureCompletion(false);
}

void CefRenderWidgetHostViewOSR::OnFrameCaptureCompletion(bool force_frame) {
  frame_in_progress_ = false;

  if (frame_pending_) {
    // Another frame was requested while the current frame was in-progress.
    // Generate the pending frame now.
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefRenderWidgetHostViewOSR::GenerateFrame,
                   weak_ptr_factory_.GetWeakPtr(), force_frame));
  }
}

void CefRenderWidgetHostViewOSR::CancelPopupWidget() {
  DCHECK(IsPopupWidget());

  if (render_widget_host_)
    render_widget_host_->LostCapture();

  WasHidden();

  if (browser_impl_.get()) {
    browser_impl_->GetClient()->GetRenderHandler()->OnPopupShow(
        browser_impl_.get(), false);
    browser_impl_ = NULL;
  }

  if (parent_host_view_) {
    parent_host_view_->set_popup_host_view(NULL);
    parent_host_view_ = NULL;
  }

  if (render_widget_host_ && !is_destroyed_) {
    is_destroyed_ = true;
    // Results in a call to Destroy().
    render_widget_host_->Shutdown();
  }
}

void CefRenderWidgetHostViewOSR::OnScrollOffsetChanged() {
  if (browser_impl_.get()) {
    CefRefPtr<CefRenderHandler> handler =
        browser_impl_->client()->GetRenderHandler();
    handler->OnScrollOffsetChanged(browser_impl_.get());
  }
  is_scroll_offset_changed_pending_ = false;
}
