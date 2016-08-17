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
#include "libcef/browser/osr/text_input_client_osr_mac.h"

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/view_messages.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/events/latency_info.h"

namespace {

CefTextInputClientOSRMac* GetInputClientFromContext(
    const NSTextInputContext* context) {
  if (!context)
    return NULL;
  return reinterpret_cast<CefTextInputClientOSRMac*>([context client]);
}

}  // namespace


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

  void BrowserCompositorMacSendReclaimCompositorResources(
      int output_surface_id,
      bool is_swap_ack,
      const cc::ReturnedResourceArray& resources) override {
    view_->render_widget_host()->Send(new ViewMsg_ReclaimCompositorResources(
        view_->render_widget_host()->GetRoutingID(), output_surface_id,
        is_swap_ack, resources));
  }

  void BrowserCompositorMacOnLostCompositorResources() override {
    view_->render_widget_host()->ScheduleComposite();
  }

  void BrowserCompositorMacUpdateVSyncParameters(
      const base::TimeTicks& timebase,
      const base::TimeDelta& interval) override {
    view_->render_widget_host()->UpdateVSyncParameters(timebase, interval);
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

void CefRenderWidgetHostViewOSR::TextInputStateChanged(
    const content::TextInputState& params) {
  [NSApp updateWindows];
}

void CefRenderWidgetHostViewOSR::ImeCancelComposition() {
  CefTextInputClientOSRMac* client = GetInputClientFromContext(
      text_input_context_osr_mac_);
  if (client)
    [client cancelComposition];
}

void CefRenderWidgetHostViewOSR::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  CefTextInputClientOSRMac* client = GetInputClientFromContext(
      text_input_context_osr_mac_);
  if (!client)
    return;

  composition_range_ = range;
  composition_bounds_ = character_bounds;
}

void CefRenderWidgetHostViewOSR::SelectionChanged(
    const base::string16& text,
    size_t offset,
    const gfx::Range& range) {
  if (range.is_empty() || text.empty()) {
    selected_text_.clear();
  } else {
    size_t pos = range.GetMin() - offset;
    size_t n = range.length();

    DCHECK(pos + n <= text.length()) << "The text can not fully cover range.";
    if (pos >= text.length()) {
      DCHECK(false) << "The text can not cover range.";
      return;
    }
    selected_text_ = base::UTF16ToUTF8(text.substr(pos, n));
  }

  RenderWidgetHostViewBase::SelectionChanged(text, offset, range);
}

void CefRenderWidgetHostViewOSR::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  if (params.anchor_rect == params.focus_rect)
    caret_rect_ = params.anchor_rect;
  first_selection_rect_ = params.anchor_rect;
}

CefTextInputContext CefRenderWidgetHostViewOSR::GetNSTextInputContext() {
  if (!text_input_context_osr_mac_) {
    CefTextInputClientOSRMac* text_input_client_osr_mac =
        [[CefTextInputClientOSRMac alloc] initWithRenderWidgetHostViewOSR:
            this];

    text_input_context_osr_mac_ = [[NSTextInputContext alloc] initWithClient:
        text_input_client_osr_mac];
  }

  return text_input_context_osr_mac_;
}

void CefRenderWidgetHostViewOSR::HandleKeyEventBeforeTextInputClient(
    CefEventHandle keyEvent) {
  CefTextInputClientOSRMac* client = GetInputClientFromContext(
      text_input_context_osr_mac_);
  if (client)
    [client HandleKeyEventBeforeTextInputClient: keyEvent];
}

void CefRenderWidgetHostViewOSR::HandleKeyEventAfterTextInputClient(
    CefEventHandle keyEvent) {
  CefTextInputClientOSRMac* client = GetInputClientFromContext(
      text_input_context_osr_mac_);
  if (client)
    [client HandleKeyEventAfterTextInputClient: keyEvent];
}

bool CefRenderWidgetHostViewOSR::GetCachedFirstRectForCharacterRange(
    gfx::Range range, gfx::Rect* rect, gfx::Range* actual_range) const {
  DCHECK(rect);

  const gfx::Range requested_range(range);
  // If requested range is same as caret location, we can just return it.
  if (selection_range_.is_empty() && requested_range == selection_range_) {
    if (actual_range)
      *actual_range = range;
    *rect = caret_rect_;
    return true;
  }

  if (composition_range_.is_empty()) {
    if (!selection_range_.Contains(requested_range))
      return false;
    if (actual_range)
      *actual_range = selection_range_;
    *rect = first_selection_rect_;
    return true;
  }

  const gfx::Range request_range_in_composition =
      ConvertCharacterRangeToCompositionRange(requested_range);
  if (request_range_in_composition == gfx::Range::InvalidRange())
    return false;

  // If firstRectForCharacterRange in WebFrame is failed in renderer,
  // ImeCompositionRangeChanged will be sent with empty vector.
  if (composition_bounds_.empty())
    return false;
  DCHECK_EQ(composition_bounds_.size(), composition_range_.length());

  gfx::Range ui_actual_range;
  *rect = GetFirstRectForCompositionRange(request_range_in_composition,
                                          &ui_actual_range);
  if (actual_range) {
    *actual_range = gfx::Range(
        composition_range_.start() + ui_actual_range.start(),
        composition_range_.start() + ui_actual_range.end());
  }
  return true;
}

gfx::Rect CefRenderWidgetHostViewOSR::GetFirstRectForCompositionRange(
    const gfx::Range& range, gfx::Range* actual_range) const {
  DCHECK(actual_range);
  DCHECK(!composition_bounds_.empty());
  DCHECK(range.start() <= composition_bounds_.size());
  DCHECK(range.end() <= composition_bounds_.size());

  if (range.is_empty()) {
    *actual_range = range;
    if (range.start() == composition_bounds_.size()) {
      return gfx::Rect(composition_bounds_[range.start() - 1].right(),
                       composition_bounds_[range.start() - 1].y(),
                       0,
                       composition_bounds_[range.start() - 1].height());
    } else {
      return gfx::Rect(composition_bounds_[range.start()].x(),
                       composition_bounds_[range.start()].y(),
                       0,
                       composition_bounds_[range.start()].height());
    }
  }

  size_t end_idx;
  if (!GetLineBreakIndex(composition_bounds_, range, &end_idx)) {
    end_idx = range.end();
  }
  *actual_range = gfx::Range(range.start(), end_idx);
  gfx::Rect rect = composition_bounds_[range.start()];
  for (size_t i = range.start() + 1; i < end_idx; ++i) {
    rect.Union(composition_bounds_[i]);
  }
  return rect;
}

gfx::Range CefRenderWidgetHostViewOSR::ConvertCharacterRangeToCompositionRange(
    const gfx::Range& request_range) const {
  if (composition_range_.is_empty())
    return gfx::Range::InvalidRange();

  if (request_range.is_reversed())
    return gfx::Range::InvalidRange();

  if (request_range.start() < composition_range_.start() ||
      request_range.start() > composition_range_.end() ||
      request_range.end() > composition_range_.end()) {
    return gfx::Range::InvalidRange();
  }

  return gfx::Range(
      request_range.start() - composition_range_.start(),
      request_range.end() - composition_range_.start());
}

bool CefRenderWidgetHostViewOSR::GetLineBreakIndex(
    const std::vector<gfx::Rect>& bounds,
    const gfx::Range& range,
    size_t* line_break_point) {
  DCHECK(line_break_point);
  if (range.start() >= bounds.size() || range.is_reversed() || range.is_empty())
    return false;

  // We can't check line breaking completely from only rectangle array. Thus we
  // assume the line breaking as the next character's y offset is larger than
  // a threshold. Currently the threshold is determined as minimum y offset plus
  // 75% of maximum height.
  const size_t loop_end_idx =
      std::min(bounds.size(), static_cast<size_t>(range.end()));
  int max_height = 0;
  int min_y_offset = std::numeric_limits<int32_t>::max();
  for (size_t idx = range.start(); idx < loop_end_idx; ++idx) {
    max_height = std::max(max_height, bounds[idx].height());
    min_y_offset = std::min(min_y_offset, bounds[idx].y());
  }
  int line_break_threshold = min_y_offset + (max_height * 3 / 4);
  for (size_t idx = range.start(); idx < loop_end_idx; ++idx) {
    if (bounds[idx].y() > line_break_threshold) {
      *line_break_point = idx;
      return true;
    }
  }
  return false;
}

void CefRenderWidgetHostViewOSR::DestroyNSTextInputOSR() {
  CefTextInputClientOSRMac* client = GetInputClientFromContext(
      text_input_context_osr_mac_);
  if (client) {
    [client release];
    client = NULL;
  }

  [text_input_context_osr_mac_ release];
  text_input_context_osr_mac_ = NULL;
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

void CefRenderWidgetHostViewOSR::PlatformCreateCompositorWidget() {
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
      mac_helper_, mac_helper_, render_widget_host_->is_hidden(), true));
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
