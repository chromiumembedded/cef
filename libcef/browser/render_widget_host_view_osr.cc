// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/backing_store_osr.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/render_widget_host_view_osr.h"
#include "libcef/common/content_client.h"

#include "base/message_loop/message_loop.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/WebKit/public/web/WebScreenInfo.h"
#include "webkit/common/cursors/webcursor.h"

namespace {

const float kDefaultScaleFactor = 1.0;

static WebKit::WebScreenInfo webScreenInfoFrom(const CefScreenInfo& src) {
  WebKit::WebScreenInfo webScreenInfo;
  webScreenInfo.deviceScaleFactor = src.device_scale_factor;
  webScreenInfo.depth = src.depth;
  webScreenInfo.depthPerComponent = src.depth_per_component;
  webScreenInfo.isMonochrome = src.is_monochrome;
  webScreenInfo.rect = WebKit::WebRect(src.rect.x, src.rect.y,
                                       src.rect.width, src.rect.height);
  webScreenInfo.availableRect = WebKit::WebRect(src.available_rect.x,
                                                src.available_rect.y,
                                                src.available_rect.width,
                                                src.available_rect.height);

  return webScreenInfo;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// CefRenderWidgetHostViewOSR, public:

CefRenderWidgetHostViewOSR::CefRenderWidgetHostViewOSR(
    content::RenderWidgetHost* widget)
        : weak_factory_(this),
          render_widget_host_(content::RenderWidgetHostImpl::From(widget)),
          parent_host_view_(NULL),
          popup_host_view_(NULL),
          about_to_validate_and_paint_(false)
#if defined(OS_MACOSX)
          , text_input_context_osr_mac_(NULL)
#endif
          {
  DCHECK(render_widget_host_);
  render_widget_host_->SetView(this);

  // CefBrowserHostImpl might not be created at this time for popups.
  if (render_widget_host_->IsRenderView()) {
    browser_impl_ = CefBrowserHostImpl::GetBrowserForHost(
        content::RenderViewHost::From(render_widget_host_));
  }
}

CefRenderWidgetHostViewOSR::~CefRenderWidgetHostViewOSR() {
}


// RenderWidgetHostView implementation.
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

gfx::NativeView CefRenderWidgetHostViewOSR::GetNativeView() const {
  return NULL;
}

gfx::NativeViewId CefRenderWidgetHostViewOSR::GetNativeViewId() const {
  if (!browser_impl_.get())
    return gfx::NativeViewId();
  // This Id is used on Windows as HWND to retrieve monitor info
  // If this Id is not a valid window, the main screen monitor info is used
  return reinterpret_cast<gfx::NativeViewId>(browser_impl_->GetWindowHandle());
}

gfx::NativeViewAccessible
    CefRenderWidgetHostViewOSR::GetNativeViewAccessible() {
  return gfx::NativeViewAccessible();
}

bool CefRenderWidgetHostViewOSR::HasFocus() const {
  return false;
}

bool CefRenderWidgetHostViewOSR::IsSurfaceAvailableForCopy() const {
  return false;
}

void CefRenderWidgetHostViewOSR::Show() {
  WasShown();
}

void CefRenderWidgetHostViewOSR::Hide() {
  WasHidden();
}

bool CefRenderWidgetHostViewOSR::IsShowing() {
  return true;
}

gfx::Rect CefRenderWidgetHostViewOSR::GetViewBounds() const {
  if (IsPopupWidget())
    return popup_position_;

  if (!browser_impl_.get())
    return gfx::Rect();
  CefRect rc;
  browser_impl_->GetClient()->GetRenderHandler()->GetViewRect(
      browser_impl_->GetBrowser(), rc);
  return gfx::Rect(rc.x, rc.y, rc.width, rc.height);
}

// Implementation of RenderWidgetHostViewPort.
void CefRenderWidgetHostViewOSR::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  parent_host_view_ = static_cast<CefRenderWidgetHostViewOSR*>(
      parent_host_view);
  browser_impl_ = parent_host_view_->get_browser_impl();
  if (!browser_impl_.get())
    return;

  parent_host_view_->CancelWidget();

  parent_host_view_->set_popup_host_view(this);
  NotifyShowWidget();

  popup_position_ = pos;
  NotifySizeWidget();
}

void CefRenderWidgetHostViewOSR::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTREACHED() << "Fullscreen widgets are not supported in OSR";
}

void CefRenderWidgetHostViewOSR::WasShown() {
  if (render_widget_host_)
    render_widget_host_->WasShown();
}

void CefRenderWidgetHostViewOSR::WasHidden() {
  if (render_widget_host_)
    render_widget_host_->WasHidden();
}

void CefRenderWidgetHostViewOSR::MovePluginWindows(
    const gfx::Vector2d& scroll_offset,
    const std::vector<content::WebPluginGeometry>& moves) {
}

void CefRenderWidgetHostViewOSR::Focus() {
}

void CefRenderWidgetHostViewOSR::Blur() {
}

void CefRenderWidgetHostViewOSR::UpdateCursor(const WebCursor& cursor) {
  TRACE_EVENT0("libcef", "CefRenderWidgetHostViewOSR::UpdateCursor");
  if (!browser_impl_.get())
    return;
#if defined(OS_WIN)
  HMODULE hModule = ::GetModuleHandle(
      CefContentClient::Get()->browser()->GetResourceDllName());
  if (!hModule)
    hModule = ::GetModuleHandle(NULL);
  WebCursor web_cursor = cursor;
  HCURSOR hCursor = web_cursor.GetCursor((HINSTANCE)hModule);
  browser_impl_->GetClient()->GetRenderHandler()->OnCursorChange(
      browser_impl_->GetBrowser(), hCursor);
#elif defined(OS_MACOSX) || defined(TOOLKIT_GTK)
  // cursor is const, and GetNativeCursor is not
  WebCursor web_cursor = cursor;
  CefCursorHandle native_cursor = web_cursor.GetNativeCursor();
  browser_impl_->GetClient()->GetRenderHandler()->OnCursorChange(
      browser_impl_->GetBrowser(), native_cursor);
#else
  // TODO(port): Implement this method to work on other platforms as part of
  // off-screen rendering support.
  NOTREACHED();
#endif
}

void CefRenderWidgetHostViewOSR::SetIsLoading(bool is_loading) {
}

#if !defined(OS_MACOSX)
void CefRenderWidgetHostViewOSR::TextInputTypeChanged(
    ui::TextInputType type,
    ui::TextInputMode mode,
    bool can_compose_inline) {
}

void CefRenderWidgetHostViewOSR::ImeCancelComposition() {
}

#if defined(OS_WIN) || defined(USE_AURA)
void CefRenderWidgetHostViewOSR::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
}
#endif
#endif  // !defined(OS_MACOSX)

void CefRenderWidgetHostViewOSR::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect,
    const gfx::Vector2d& scroll_delta,
    const std::vector<gfx::Rect>& copy_rects,
    const ui::LatencyInfo& latency_info) {
  if (!scroll_rect.IsEmpty()) {
    std::vector<gfx::Rect> dirty_rects(copy_rects);
    dirty_rects.push_back(scroll_rect);
    Paint(dirty_rects);
  } else {
    Paint(copy_rects);
  }
}

void CefRenderWidgetHostViewOSR::RenderProcessGone(
    base::TerminationStatus status,
    int error_code) {
  render_widget_host_ = NULL;
  parent_host_view_ = NULL;
  popup_host_view_ = NULL;
}

#if defined(OS_WIN) && !defined(USE_AURA)
void CefRenderWidgetHostViewOSR::WillWmDestroy() {
  // Will not be called if GetNativeView returns NULL.
  NOTREACHED();
}
#endif

void CefRenderWidgetHostViewOSR::GetScreenInfo(WebKit::WebScreenInfo* results) {
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
          browser_impl_->GetBrowser(), rc)) {
    return gfx::Rect(rc.x, rc.y, rc.width, rc.height);
  }
  return gfx::Rect();
}

void CefRenderWidgetHostViewOSR::OnAccessibilityEvents(
    const std::vector<AccessibilityHostMsg_EventParams>& params) {
}

void CefRenderWidgetHostViewOSR::Destroy() {
  if (IsPopupWidget()) {
    if (parent_host_view_)
      parent_host_view_->CancelWidget();
  } else {
    CancelWidget();
  }

  delete this;
}

void CefRenderWidgetHostViewOSR::SetTooltipText(const string16& tooltip_text) {
  if (!browser_impl_.get())
    return;

  CefString tooltip(tooltip_text);
  CefRefPtr<CefDisplayHandler> handler =
      browser_impl_->GetClient()->GetDisplayHandler();
  if (handler.get()) {
    handler->OnTooltip(browser_impl_->GetBrowser(), tooltip);
  }
}

void CefRenderWidgetHostViewOSR::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
}

void CefRenderWidgetHostViewOSR::ScrollOffsetChanged() {
  if (!browser_impl_.get())
    return;
  browser_impl_->GetClient()->GetRenderHandler()->
      OnScrollOffsetChanged(browser_impl_.get());
}

content::BackingStore* CefRenderWidgetHostViewOSR::AllocBackingStore(
    const gfx::Size& size) {
  return render_widget_host_ ?
      new BackingStoreOSR(render_widget_host_, size, GetDeviceScaleFactor()) :
      NULL;
}

void CefRenderWidgetHostViewOSR::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool, const SkBitmap&)>& callback) {
}

void CefRenderWidgetHostViewOSR::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(bool)>& callback) {
}

bool CefRenderWidgetHostViewOSR::CanCopyToVideoFrame() const {
  return false;
}

void CefRenderWidgetHostViewOSR::OnAcceleratedCompositingStateChange() {
}

void CefRenderWidgetHostViewOSR::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
}

void CefRenderWidgetHostViewOSR::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
}

gfx::GLSurfaceHandle CefRenderWidgetHostViewOSR::GetCompositingSurface() {
  return gfx::GLSurfaceHandle();
}

void CefRenderWidgetHostViewOSR::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
}

void CefRenderWidgetHostViewOSR::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
    int gpu_host_id) {
}

void CefRenderWidgetHostViewOSR::AcceleratedSurfaceSuspend() {
}

void CefRenderWidgetHostViewOSR::AcceleratedSurfaceRelease() {
}

bool CefRenderWidgetHostViewOSR::HasAcceleratedSurface(
    const gfx::Size& desired_size) {
  return false;
}

bool CefRenderWidgetHostViewOSR::LockMouse() {
  return false;
}

void CefRenderWidgetHostViewOSR::UnlockMouse() {
}

#if defined(OS_WIN) && !defined(USE_AURA)
void CefRenderWidgetHostViewOSR::SetClickthroughRegion(SkRegion* region) {
}
#endif

void CefRenderWidgetHostViewOSR::SetBackground(const SkBitmap& background) {
  if (!render_widget_host_)
    return;
  RenderWidgetHostViewBase::SetBackground(background);
  render_widget_host_->SetBackground(background);
}

void CefRenderWidgetHostViewOSR::Invalidate(const gfx::Rect& rect,
    CefBrowserHost::PaintElementType type) {
  TRACE_EVENT1("libcef", "CefRenderWidgetHostViewOSR::Invalidate", "type", type); 
  if (!IsPopupWidget() && type == PET_POPUP) {
    if (popup_host_view_)
      popup_host_view_->Invalidate(rect, type);
    return;
  }
  std::vector<gfx::Rect> dirtyRects;
  dirtyRects.push_back(rect);
  Paint(dirtyRects);
}

void CefRenderWidgetHostViewOSR::Paint(
    const std::vector<gfx::Rect>& copy_rects) {
  TRACE_EVENT1("libcef", "CefRenderWidgetHostViewOSR::Paint", "rects", copy_rects.size()); 
  if (about_to_validate_and_paint_ ||
      !browser_impl_.get() ||
      !render_widget_host_) {
    pending_update_rects_.insert(pending_update_rects_.end(),
        copy_rects.begin(), copy_rects.end());
    return;
  }

  about_to_validate_and_paint_ = true;
  BackingStoreOSR* backing_store =
      BackingStoreOSR::From(render_widget_host_->GetBackingStore(true));
  about_to_validate_and_paint_ = false;

  if (backing_store) {
    const gfx::Rect client_rect(backing_store->size());
    SkRegion damaged_rgn;

    for (size_t i = 0; i < copy_rects.size(); ++i) {
      SkIRect skRect = SkIRect::MakeLTRB(
          copy_rects[i].x(), copy_rects[i].y(),
          copy_rects[i].right(), copy_rects[i].bottom());
      damaged_rgn.op(skRect, SkRegion::kUnion_Op);
    }

    for (size_t i = 0; i < pending_update_rects_.size(); ++i) {
      SkIRect skRect = SkIRect::MakeLTRB(
          pending_update_rects_[i].x(), pending_update_rects_[i].y(),
          pending_update_rects_[i].right(), pending_update_rects_[i].bottom());
      damaged_rgn.op(skRect, SkRegion::kUnion_Op);
    }
    pending_update_rects_.clear();

    CefRenderHandler::RectList rcList;
    SkRegion::Cliperator iterator(damaged_rgn,
        SkIRect::MakeWH(client_rect.width(), client_rect.height()));
    for (; !iterator.done(); iterator.next()) {
      const SkIRect& r = iterator.rect();
      rcList.push_back(
          CefRect(r.left(), r.top(), r.width(), r.height()));
    }

    if (rcList.size() == 0)
      return;

    browser_impl_->GetClient()->GetRenderHandler()->OnPaint(
        browser_impl_->GetBrowser(),
        IsPopupWidget() ? PET_POPUP : PET_VIEW,
        rcList,
        backing_store->getPixels(),
        client_rect.width(),
        client_rect.height());
  }
}

bool CefRenderWidgetHostViewOSR::InstallTransparency() {
  if (browser_impl_.get() && browser_impl_->IsTransparent()) {
    SkBitmap bg;
    bg.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);  // 1x1 alpha bitmap.
    bg.allocPixels();
    bg.eraseARGB(0x00, 0x00, 0x00, 0x00);
    SetBackground(bg);
    return true;
  }
  return false;
}

void CefRenderWidgetHostViewOSR::CancelWidget() {
  if (IsPopupWidget()) {
    if (render_widget_host_)
      render_widget_host_->LostCapture();

    if (browser_impl_.get()) {
      NotifyHideWidget();
      browser_impl_ = NULL;
    }

    if (parent_host_view_) {
      parent_host_view_->set_popup_host_view(NULL);
      parent_host_view_ = NULL;
    }

    if (!weak_factory_.HasWeakPtrs()) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(&CefRenderWidgetHostViewOSR::ShutdownHost,
          weak_factory_.GetWeakPtr()));
    }
  } else if (popup_host_view_) {
    popup_host_view_->CancelWidget();
  }
}

void CefRenderWidgetHostViewOSR::NotifyShowWidget() {
  if (browser_impl_.get()) {
    browser_impl_->GetClient()->GetRenderHandler()->OnPopupShow(
        browser_impl_->GetBrowser(), true);
  }
}

void CefRenderWidgetHostViewOSR::NotifyHideWidget() {
  if (browser_impl_.get()) {
    browser_impl_->GetClient()->GetRenderHandler()->OnPopupShow(
        browser_impl_->GetBrowser(), false);
  }
}

void CefRenderWidgetHostViewOSR::NotifySizeWidget() {
  if (browser_impl_.get()) {
    CefRect widget_pos(popup_position_.x(), popup_position_.y(),
                       popup_position_.width(), popup_position_.height());
    browser_impl_->GetClient()->GetRenderHandler()->OnPopupSize(
        browser_impl_->GetBrowser(), widget_pos);
  }
}

CefRefPtr<CefBrowserHostImpl>
    CefRenderWidgetHostViewOSR::get_browser_impl() const {
  return browser_impl_;
}

void CefRenderWidgetHostViewOSR::set_browser_impl(
    CefRefPtr<CefBrowserHostImpl> browser) {
  browser_impl_ = browser;
}

void CefRenderWidgetHostViewOSR::set_popup_host_view(
    CefRenderWidgetHostViewOSR* popup_view) {
  popup_host_view_ = popup_view;
}

void CefRenderWidgetHostViewOSR::ShutdownHost() {
  weak_factory_.InvalidateWeakPtrs();
  if (render_widget_host_)
    render_widget_host_->Shutdown();
  // Do not touch any members at this point, |this| has been deleted.
}

void CefRenderWidgetHostViewOSR::set_parent_host_view(
    CefRenderWidgetHostViewOSR* parent_view) {
  parent_host_view_ = parent_view;
}

void CefRenderWidgetHostViewOSR::SendKeyEvent(
    const content::NativeWebKeyboardEvent& event) {
  TRACE_EVENT0("libcef", "CefRenderWidgetHostViewOSR::SendKeyEvent");
  if (!render_widget_host_)
    return;
  render_widget_host_->ForwardKeyboardEvent(event);
}

void CefRenderWidgetHostViewOSR::SendMouseEvent(
    const WebKit::WebMouseEvent& event) {
  TRACE_EVENT0("libcef", "CefRenderWidgetHostViewOSR::SendMouseEvent");
  if (!IsPopupWidget() && popup_host_view_) {
    if (popup_host_view_->popup_position_.Contains(event.x, event.y)) {
      WebKit::WebMouseEvent popup_event(event);
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
    const WebKit::WebMouseWheelEvent& event) {
  TRACE_EVENT0("libcef", "CefRenderWidgetHostViewOSR::SendMouseWheelEvent");
  if (!IsPopupWidget() && popup_host_view_) {
    if (popup_host_view_->popup_position_.Contains(event.x, event.y)) {
      WebKit::WebMouseWheelEvent popup_event(event);
      popup_event.x -= popup_host_view_->popup_position_.x();
      popup_event.y -= popup_host_view_->popup_position_.y();
      popup_event.windowX = popup_event.x;
      popup_event.windowY = popup_event.y;
      popup_host_view_->SendMouseWheelEvent(popup_event);
      return;
    } else {
      // scrolling outside the popup widget, will destroy widget
      CancelWidget();
    }
  }
  if (!render_widget_host_)
    return;
  render_widget_host_->ForwardWheelEvent(event);
}

void CefRenderWidgetHostViewOSR::OnScreenInfoChanged() {
  TRACE_EVENT0("libcef", "CefRenderWidgetHostViewOSR::OnScreenInfoChanged");
  if (!render_widget_host_)
    return;

  BackingStoreOSR* backing_store =
      BackingStoreOSR::From(render_widget_host_->GetBackingStore(true));
  if (backing_store)
    backing_store->ScaleFactorChanged(GetDeviceScaleFactor());

  // What could be taken from UpdateScreenInfo(window_) - updates the renderer
  // cached rectangles
  //render_widget_host_->SendScreenRects();

  render_widget_host_->NotifyScreenInfoChanged();
  // We might want to change the cursor scale factor here as well - see the
  // cache for the current_cursor_, as passed by UpdateCursor from the renderer
  // in the rwhv_aura (current_cursor_.SetScaleFactor)
}

float CefRenderWidgetHostViewOSR::GetDeviceScaleFactor() {
  if (!browser_impl_.get())
    return kDefaultScaleFactor;

  CefScreenInfo screen_info(
      kDefaultScaleFactor, 0, 0, false, CefRect(), CefRect());
  if (!browser_impl_->GetClient()->GetRenderHandler()->GetScreenInfo(
          browser_impl_->GetBrowser(), screen_info)) {
    // Use the default
    return kDefaultScaleFactor;
  }

  return screen_info.device_scale_factor;
}

#if defined(OS_MACOSX)
void CefRenderWidgetHostViewOSR::AboutToWaitForBackingStoreMsg() {
}

bool CefRenderWidgetHostViewOSR::PostProcessEventForPluginIme(
    const content::NativeWebKeyboardEvent& event) {
  return false;
}
#endif

#if defined(OS_MACOSX)
void CefRenderWidgetHostViewOSR::SetActive(bool active) {
}

void CefRenderWidgetHostViewOSR::SetTakesFocusOnlyOnMouseDown(bool flag) {
}

void CefRenderWidgetHostViewOSR::SetWindowVisibility(bool visible) {
}

void CefRenderWidgetHostViewOSR::WindowFrameChanged() {
}

void CefRenderWidgetHostViewOSR::ShowDefinitionForSelection() {
}


bool CefRenderWidgetHostViewOSR::SupportsSpeech() const {
  return false;
}

void CefRenderWidgetHostViewOSR::SpeakSelection() {
}

bool CefRenderWidgetHostViewOSR::IsSpeaking() const {
  return false;
}

void CefRenderWidgetHostViewOSR::StopSpeaking() {
}
#endif  // defined(OS_MACOSX)

#if defined(TOOLKIT_GTK)
GdkEventButton* CefRenderWidgetHostViewOSR::GetLastMouseDown() {
  return NULL;
}

gfx::NativeView CefRenderWidgetHostViewOSR::BuildInputMethodsGtkMenu() {
  return NULL;
}
#endif  // defined(TOOLKIT_GTK)

