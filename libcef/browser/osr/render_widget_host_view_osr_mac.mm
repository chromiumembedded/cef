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

class MacHelper :
  public content::BrowserCompositorMacClient,
  public ui::AcceleratedWidgetMacNSView {
 public:
  explicit MacHelper(CefRenderWidgetHostViewOSR* view)
    : view_(view) {
  }
  virtual ~MacHelper() {}

  // BrowserCompositorMacClient methods:

  NSView* BrowserCompositorMacGetNSView() const override {
    // Intentionally return nil so that
    // BrowserCompositorMac::DelegatedFrameHostDesiredSizeInDIP uses the layer
    // size instead of the NSView size.
    return nil;
  }

  SkColor BrowserCompositorMacGetGutterColor(SkColor color) const override {
    // When making an element on the page fullscreen the element's background
    // may not match the page's, so use black as the gutter color to avoid
    // flashes of brighter colors during the transition.
    if (view_->render_widget_host()->delegate() &&
        view_->render_widget_host()->delegate()->IsFullscreenForCurrentTab()) {
      return SK_ColorBLACK;
    }
    return color;
  }

  void BrowserCompositorMacSendBeginFrame(
      const cc::BeginFrameArgs& args) override {
    view_->render_widget_host()->Send(
      new ViewMsg_BeginFrame(view_->render_widget_host()->GetRoutingID(),
                             args));
  }

  // AcceleratedWidgetMacNSView methods:

  NSView* AcceleratedWidgetGetNSView() const override {
    return [view_->window_ contentView];
  }

  void AcceleratedWidgetGetVSyncParameters(
      base::TimeTicks* timebase, base::TimeDelta* interval) const override {
    *timebase = base::TimeTicks();
    *interval = base::TimeDelta();
  }

  void AcceleratedWidgetSwapCompleted() override {
  }

 private:
  // Guaranteed to outlive this object.
  CefRenderWidgetHostViewOSR* view_;

  DISALLOW_COPY_AND_ASSIGN(MacHelper);
};


ui::AcceleratedWidgetMac* CefRenderWidgetHostViewOSR::GetAcceleratedWidgetMac()
    const {
  if (browser_compositor_)
    return browser_compositor_->GetAcceleratedWidgetMac();
  return nullptr;
}

void CefRenderWidgetHostViewOSR::SetActive(bool active) {
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
      AllocateFrameSinkId(is_guest_view_hack)));
}

void CefRenderWidgetHostViewOSR::PlatformResizeCompositorWidget(const gfx::Size&) {
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
