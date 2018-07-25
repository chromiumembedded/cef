// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include <algorithm>
#include <limits>
#include <utility>

#import <Cocoa/Cocoa.h>

#include "libcef/browser/browser_host_impl.h"

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/view_messages.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/display/screen.h"

class MacHelper : public content::BrowserCompositorMacClient,
                  public ui::AcceleratedWidgetMacNSView {
 public:
  explicit MacHelper(CefRenderWidgetHostViewOSR* view) : view_(view) {}
  virtual ~MacHelper() {}

  // BrowserCompositorMacClient methods:

  SkColor BrowserCompositorMacGetGutterColor() const override {
    // When making an element on the page fullscreen the element's background
    // may not match the page's, so use black as the gutter color to avoid
    // flashes of brighter colors during the transition.
    if (view_->render_widget_host()->delegate() &&
        view_->render_widget_host()->delegate()->IsFullscreenForCurrentTab()) {
      return SK_ColorBLACK;
    }
    return *view_->GetBackgroundColor();
  }

  void BrowserCompositorMacOnBeginFrame(base::TimeTicks frame_time) override {}

  void OnFrameTokenChanged(uint32_t frame_token) override {
    view_->render_widget_host()->DidProcessFrame(frame_token);
  }

  void DidReceiveFirstFrameAfterNavigation() override {
    view_->render_widget_host()->DidReceiveFirstFrameAfterNavigation();
  }

  void DestroyCompositorForShutdown() override {}

  bool SynchronizeVisualProperties(
      const base::Optional<viz::LocalSurfaceId>&
          child_allocated_local_surface_id) override {
    auto* browser_compositor = view_->browser_compositor();
    if (child_allocated_local_surface_id) {
      browser_compositor->UpdateRendererLocalSurfaceIdFromChild(
          *child_allocated_local_surface_id);
    } else {
      browser_compositor->AllocateNewRendererLocalSurfaceId();
    }

    if (auto* host = browser_compositor->GetDelegatedFrameHost()) {
      host->EmbedSurface(browser_compositor->GetRendererLocalSurfaceId(),
                         browser_compositor->GetRendererSize(),
                         cc::DeadlinePolicy::UseDefaultDeadline());
    }

    return view_->render_widget_host()->SynchronizeVisualProperties();
  }

  // AcceleratedWidgetMacNSView methods:

  void AcceleratedWidgetCALayerParamsUpdated() override {}

 private:
  // Guaranteed to outlive this object.
  CefRenderWidgetHostViewOSR* view_;

  DISALLOW_COPY_AND_ASSIGN(MacHelper);
};

void CefRenderWidgetHostViewOSR::SetActive(bool active) {}

void CefRenderWidgetHostViewOSR::ShowDefinitionForSelection() {}

void CefRenderWidgetHostViewOSR::SpeakSelection() {}

bool CefRenderWidgetHostViewOSR::ShouldContinueToPauseForFrame() {
  return browser_compositor_->ShouldContinueToPauseForFrame();
}

viz::ScopedSurfaceIdAllocator
CefRenderWidgetHostViewOSR::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  base::OnceCallback<void()> allocation_task = base::BindOnce(
      base::IgnoreResult(
          &CefRenderWidgetHostViewOSR::OnDidUpdateVisualPropertiesComplete),
      weak_ptr_factory_.GetWeakPtr(), metadata);
  return browser_compositor_->GetScopedRendererSurfaceIdAllocator(
      std::move(allocation_task));
}

display::Display CefRenderWidgetHostViewOSR::GetDisplay() {
  content::ScreenInfo screen_info;
  GetScreenInfo(&screen_info);

  // Start with a reasonable display representation.
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(nullptr);

  // Populate attributes based on |screen_info|.
  display.set_bounds(screen_info.rect);
  display.set_work_area(screen_info.available_rect);
  display.set_device_scale_factor(screen_info.device_scale_factor);
  display.set_color_space(screen_info.color_space);
  display.set_color_depth(screen_info.depth);
  display.set_depth_per_component(screen_info.depth_per_component);
  display.set_is_monochrome(screen_info.is_monochrome);
  display.SetRotationAsDegree(screen_info.orientation_angle);

  return display;
}

void CefRenderWidgetHostViewOSR::OnDidUpdateVisualPropertiesComplete(
    const cc::RenderFrameMetadata& metadata) {
  DCHECK_EQ(current_device_scale_factor_, metadata.device_scale_factor);
  browser_compositor_->SynchronizeVisualProperties(
      metadata.device_scale_factor, metadata.viewport_size_in_pixels,
      metadata.local_surface_id.value_or(viz::LocalSurfaceId()));
}

const viz::LocalSurfaceId& CefRenderWidgetHostViewOSR::GetLocalSurfaceId()
    const {
  return browser_compositor_->GetRendererLocalSurfaceId();
}

ui::Compositor* CefRenderWidgetHostViewOSR::GetCompositor() const {
  return browser_compositor_->GetCompositor();
}

ui::Layer* CefRenderWidgetHostViewOSR::GetRootLayer() const {
  return browser_compositor_->GetRootLayer();
}

content::DelegatedFrameHost* CefRenderWidgetHostViewOSR::GetDelegatedFrameHost()
    const {
  return browser_compositor_->GetDelegatedFrameHost();
}

bool CefRenderWidgetHostViewOSR::UpdateNSViewAndDisplay() {
  return browser_compositor_->UpdateNSViewAndDisplay(
      GetRootLayer()->bounds().size(), GetDisplay());
}

void CefRenderWidgetHostViewOSR::PlatformCreateCompositorWidget(
    bool is_guest_view_hack) {
  // Create a borderless non-visible 1x1 window.
  window_ = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1, 1)
                                        styleMask:NSBorderlessWindowMask
                                          backing:NSBackingStoreBuffered
                                            defer:NO];

  // Create a CALayer which is used by BrowserCompositorViewMac for rendering.
  background_layer_ = [[[CALayer alloc] init] retain];
  [background_layer_ setBackgroundColor:CGColorGetConstantColor(kCGColorClear)];
  NSView* content_view = [window_ contentView];
  [content_view setLayer:background_layer_];
  [content_view setWantsLayer:YES];

  mac_helper_ = new MacHelper(this);
  browser_compositor_.reset(new content::BrowserCompositorMac(
      mac_helper_, mac_helper_, render_widget_host_->is_hidden(), true,
      GetDisplay(), AllocateFrameSinkId(is_guest_view_hack)));
}

void CefRenderWidgetHostViewOSR::PlatformDestroyCompositorWidget() {
  DCHECK(window_);

  [window_ close];
  window_ = nil;
  [background_layer_ release];
  background_layer_ = nil;

  browser_compositor_.reset();

  delete mac_helper_;
  mac_helper_ = nullptr;
}
