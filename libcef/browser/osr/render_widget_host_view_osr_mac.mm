// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include <limits>
#include <utility>

#import <Cocoa/Cocoa.h>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/osr/text_input_client_osr_mac.h"

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/view_messages.h"
#include "ui/events/latency_info.h"

namespace {

CefTextInputClientOSRMac* GetInputClientFromContext(
    const NSTextInputContext* context) {
  if (!context)
    return NULL;
  return reinterpret_cast<CefTextInputClientOSRMac*>([context client]);
}

}  // namespace

void CefRenderWidgetHostViewOSR::SetActive(bool active) {
}

void CefRenderWidgetHostViewOSR::SetWindowVisibility(bool visible) {
  if (visible)
    Hide();
  else
    Show();
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

void CefRenderWidgetHostViewOSR::TextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  [NSApp updateWindows];
}

void CefRenderWidgetHostViewOSR::ImeCancelComposition() {
  CefTextInputClientOSRMac* client = GetInputClientFromContext(
      text_input_context_osr_mac_);
  if (client)
    [client cancelComposition];
}

bool CefRenderWidgetHostViewOSR::PostProcessEventForPluginIme(
    const content::NativeWebKeyboardEvent& event) {
  return false;
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

NSView* CefRenderWidgetHostViewOSR::AcceleratedWidgetGetNSView() const {
  return [window_ contentView];
}

void CefRenderWidgetHostViewOSR::AcceleratedWidgetGetVSyncParameters(
    base::TimeTicks* timebase, base::TimeDelta* interval) const {
  *timebase = base::TimeTicks();
  *interval = base::TimeDelta();
}

void CefRenderWidgetHostViewOSR::AcceleratedWidgetSwapCompleted() {
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
  const size_t loop_end_idx = std::min(bounds.size(), range.end());
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

  browser_compositor_ = content::BrowserCompositorMac::Create();

  compositor_.reset(browser_compositor_->compositor());
  compositor_->SetRootLayer(root_layer_.get());
  browser_compositor_->accelerated_widget_mac()->SetNSView(this);
  browser_compositor_->compositor()->SetVisible(true);

  // CEF needs the browser compositor to remain responsive whereas normal
  // rendering on OS X does not. This effectively reverts the changes from
  // https://crbug.com/463988#c6
  compositor_->SetLocksWillTimeOut(true);
  browser_compositor_->Unsuspend();
}

void CefRenderWidgetHostViewOSR::PlatformDestroyCompositorWidget() {
  DCHECK(window_);

  // Compositor is owned by and will be freed by BrowserCompositorMac.
  ui::Compositor* compositor = compositor_.release();
  ALLOW_UNUSED_LOCAL(compositor);

  [window_ close];
  window_ = nil;
  [background_layer_ release];
  background_layer_ = nil;

  browser_compositor_->accelerated_widget_mac()->ResetNSView();
  browser_compositor_->compositor()->SetVisible(false);
  browser_compositor_->compositor()->SetScaleAndSize(1.0, gfx::Size(0, 0));
  browser_compositor_->compositor()->SetRootLayer(NULL);
  content::BrowserCompositorMac::Recycle(std::move(browser_compositor_));
}
