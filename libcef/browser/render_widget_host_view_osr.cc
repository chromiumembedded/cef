// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/backing_store_osr.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/render_widget_host_view_osr.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_client.h"

#include "webkit/glue/webcursor.h"

///////////////////////////////////////////////////////////////////////////////
// CefRenderWidgetHostViewOSR, public:

CefRenderWidgetHostViewOSR::CefRenderWidgetHostViewOSR(
    content::RenderWidgetHost* widget)
        : render_widget_host_(content::RenderWidgetHostImpl::From(widget)),
          about_to_validate_and_paint_(false) {
  DCHECK(render_widget_host_);
  render_widget_host_->SetView(this);

  // CefBrowserHostImpl might not be created at this time for popups.
  browser_impl_ = CefBrowserHostImpl::GetBrowserForHost(
      content::RenderViewHost::From(render_widget_host_));
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
  return gfx::NativeView();
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
}

void CefRenderWidgetHostViewOSR::InitAsFullscreen(
  RenderWidgetHostView* reference_host_view) {
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
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
}

void CefRenderWidgetHostViewOSR::Focus() {
}

void CefRenderWidgetHostViewOSR::Blur() {
}

void CefRenderWidgetHostViewOSR::UpdateCursor(const WebCursor& cursor) {
  if (!browser_impl_.get())
    return;
#if defined(OS_WIN)
  HMODULE hModule = ::GetModuleHandle(
      content::GetContentClient()->browser()->GetResourceDllName());
  if (!hModule)
    hModule = ::GetModuleHandle(NULL);
  WebCursor web_cursor = cursor;
  HCURSOR hCursor = web_cursor.GetCursor((HINSTANCE)hModule);
  browser_impl_->GetClient()->GetRenderHandler()->OnCursorChange(
      browser_impl_->GetBrowser(), hCursor);
#else
  // TODO(port): Implement this method to work on other platforms as part of
  // off-screen rendering support.
  NOTREACHED();
#endif
}

void CefRenderWidgetHostViewOSR::SetIsLoading(bool is_loading) {
}

void CefRenderWidgetHostViewOSR::TextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
}

void CefRenderWidgetHostViewOSR::ImeCancelComposition() {
}

void CefRenderWidgetHostViewOSR::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect,
    const gfx::Vector2d& scroll_delta,
    const std::vector<gfx::Rect>& copy_rects) {
  if (!scroll_rect.IsEmpty()) {
    std::vector<gfx::Rect> dirty_rects(copy_rects);
    dirty_rects.push_back(scroll_rect);
    Paint(dirty_rects);
  } else {
    Paint(copy_rects);
  }
}

void CefRenderWidgetHostViewOSR::RenderViewGone(
    base::TerminationStatus status,
    int error_code) {
  render_widget_host_ = NULL;
}

#if defined(OS_WIN) && !defined(USE_AURA)
void CefRenderWidgetHostViewOSR::WillWmDestroy() {
  // Will not be called if GetNativeView returns NULL.
  NOTREACHED();
}
#endif

gfx::Rect CefRenderWidgetHostViewOSR::GetBoundsInRootWindow() {
  if (!browser_impl_.get())
    return gfx::Rect();
  CefRect rc;
  // If GetRootScreenRect is not implemented use the view rect as the root
  // screen rect.
  if (browser_impl_->GetClient()->GetRenderHandler()->GetRootScreenRect(
          browser_impl_->GetBrowser(), rc) ||
      browser_impl_->GetClient()->GetRenderHandler()->GetViewRect(
          browser_impl_->GetBrowser(), rc)) {
    return gfx::Rect(rc.x, rc.y, rc.width, rc.height);
  }
  return gfx::Rect();
}

void CefRenderWidgetHostViewOSR::Destroy() {
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

content::BackingStore* CefRenderWidgetHostViewOSR::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStoreOSR(render_widget_host_, size);
}

void CefRenderWidgetHostViewOSR::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool)>& callback,
    skia::PlatformBitmap* output) {
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

void CefRenderWidgetHostViewOSR::Invalidate(const gfx::Rect& rect) {
  std::vector<gfx::Rect> dirtyRects;
  dirtyRects.push_back(rect);
  Paint(dirtyRects);
}

void CefRenderWidgetHostViewOSR::Paint(
    const std::vector<gfx::Rect>& copy_rects) {
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
        browser_impl_->GetBrowser(), PET_VIEW, rcList,
        backing_store->getPixels(),
        client_rect.width(), client_rect.height());
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
