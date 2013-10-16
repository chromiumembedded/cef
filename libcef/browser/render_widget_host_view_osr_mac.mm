// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/render_widget_host_view_osr.h"
#include "libcef/browser/text_input_client_osr_mac.h"

static CefTextInputClientOSRMac* GetInputClientFromContext(
    const NSTextInputContext* context) {
  if (!context)
    return NULL;
  return reinterpret_cast<CefTextInputClientOSRMac*>([context client]);
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

void CefRenderWidgetHostViewOSR::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  CefTextInputClientOSRMac* client = GetInputClientFromContext(
      text_input_context_osr_mac_);
  if (!client)
    return;

  client->markedRange_ = range.ToNSRange();
  client->composition_range_ = range;
  client->composition_bounds_ = character_bounds;
}

void CefRenderWidgetHostViewOSR::ImeCancelComposition() {
  CefTextInputClientOSRMac* client = GetInputClientFromContext(
      text_input_context_osr_mac_);
  if (client)
    [client cancelComposition];
}

void CefRenderWidgetHostViewOSR::TextInputTypeChanged(
    ui::TextInputType type,
    ui::TextInputMode mode,
    bool can_compose_inline) {
  [NSApp updateWindows];
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
  int min_y_offset = kint32max;
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

gfx::Rect CefRenderWidgetHostViewOSR::GetFirstRectForCompositionRange(
    const gfx::Range& range, gfx::Range* actual_range) const {
  CefTextInputClientOSRMac* client = GetInputClientFromContext(
     text_input_context_osr_mac_);

  DCHECK(client);
  DCHECK(actual_range);
  DCHECK(!client->composition_bounds_.empty());
  DCHECK_LE(range.start(), client->composition_bounds_.size());
  DCHECK_LE(range.end(), client->composition_bounds_.size());

  if (range.is_empty()) {
    *actual_range = range;
    if (range.start() == client->composition_bounds_.size()) {
      return gfx::Rect(client->composition_bounds_[range.start() - 1].right(),
                       client->composition_bounds_[range.start() - 1].y(),
                       0,
                       client->composition_bounds_[range.start() - 1].height());
    } else {
      return gfx::Rect(client->composition_bounds_[range.start()].x(),
                       client->composition_bounds_[range.start()].y(),
                       0,
                       client->composition_bounds_[range.start()].height());
    }
  }

  size_t end_idx;
  if (!GetLineBreakIndex(client->composition_bounds_,
      range, &end_idx)) {
    end_idx = range.end();
  }
  *actual_range = gfx::Range(range.start(), end_idx);
  gfx::Rect rect = client->composition_bounds_[range.start()];
  for (size_t i = range.start() + 1; i < end_idx; ++i) {
    rect.Union(client->composition_bounds_[i]);
  }
  return rect;
}

gfx::Range CefRenderWidgetHostViewOSR::ConvertCharacterRangeToCompositionRange(
    const gfx::Range& request_range) const {
  CefTextInputClientOSRMac* client = GetInputClientFromContext(
     text_input_context_osr_mac_);
  DCHECK(client);

  if (client->composition_range_.is_empty())
    return gfx::Range::InvalidRange();

  if (request_range.is_reversed())
    return gfx::Range::InvalidRange();

  if (request_range.start() < client->composition_range_.start()
      || request_range.start() > client->composition_range_.end()
      || request_range.end() > client->composition_range_.end())
    return gfx::Range::InvalidRange();

  return gfx::Range(request_range.start() - client->composition_range_.start(),
                    request_range.end() - client->composition_range_.start());
}

bool CefRenderWidgetHostViewOSR::GetCachedFirstRectForCharacterRange(
    gfx::Range range, gfx::Rect* rect, gfx::Range* actual_range) const {
  DCHECK(rect);
  CefTextInputClientOSRMac* client = GetInputClientFromContext(
     text_input_context_osr_mac_);

  // If requested range is same as caret location, we can just return it.
  if (selection_range_.is_empty() && gfx::Range(range) == selection_range_) {
    if (actual_range)
      *actual_range = range;
    *rect = client->caret_rect_;
    return true;
  }

  const gfx::Range request_range_in_composition =
      ConvertCharacterRangeToCompositionRange(gfx::Range(range));
  if (request_range_in_composition == gfx::Range::InvalidRange())
    return false;

  // If firstRectForCharacterRange in WebFrame is failed in renderer,
  // ImeCompositionRangeChanged will be sent with empty vector.
  if (client->composition_bounds_.empty())
    return false;

  DCHECK_EQ(client->composition_bounds_.size(),
      client->composition_range_.length());

  gfx::Range ui_actual_range;
  *rect = GetFirstRectForCompositionRange(request_range_in_composition,
                                          &ui_actual_range);
  if (actual_range) {
    *actual_range = gfx::Range(
        client->composition_range_.start() + ui_actual_range.start(),
        client->composition_range_.start() + ui_actual_range.end()).ToNSRange();
  }
  return true;
}
