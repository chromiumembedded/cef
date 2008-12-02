// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "page_setup.h"

#include "base/logging.h"

namespace printing {

PageMargins::PageMargins()
    : header(0),
      footer(0),
      left(0),
      right(0),
      top(0),
      bottom(0) {
}

void PageMargins::Clear() {
  header = 0;
  footer = 0;
  left = 0;
  right = 0;
  top = 0;
  bottom = 0;
}

bool PageMargins::Equals(const PageMargins& rhs) const {
  return header == rhs.header &&
      footer == rhs.footer &&
      left == rhs.left &&
      top == rhs.top &&
      right == rhs.right &&
      bottom == rhs.bottom;
}

PageSetup::PageSetup() : text_height_(0) {
}

void PageSetup::Clear() {
  physical_size_.SetSize(0, 0);
  printable_area_.SetRect(0, 0, 0, 0);
  overlay_area_.SetRect(0, 0, 0, 0);
  content_area_.SetRect(0, 0, 0, 0);
  effective_margins_.Clear();
  text_height_ = 0;
}

bool PageSetup::Equals(const PageSetup& rhs) const {
  return physical_size_ == rhs.physical_size_ &&
      printable_area_ == rhs.printable_area_ &&
      overlay_area_ == rhs.overlay_area_ &&
      content_area_ == rhs.content_area_ &&
      effective_margins_.Equals(rhs.effective_margins_) &&
      requested_margins_.Equals(rhs.requested_margins_) &&
      text_height_ == rhs.text_height_;
}

void PageSetup::Init(const gfx::Size& physical_size,
                     const gfx::Rect& printable_area,
                     int text_height) {
  DCHECK_LE(printable_area.right(), physical_size.width());
  // I've seen this assert triggers on Canon GP160PF PCL 5e and HP LaserJet 5.
  // Since we don't know the dpi here, just disable the check.
  // DCHECK_LE(printable_area.bottom(), physical_size.height());
  DCHECK_GE(printable_area.x(), 0);
  DCHECK_GE(printable_area.y(), 0);
  DCHECK_GE(text_height, 0);
  physical_size_ = physical_size;
  printable_area_ = printable_area;
  text_height_ = text_height;

  // Calculate the effective margins. The tricky part.
  effective_margins_.header = std::max(requested_margins_.header,
                                       printable_area_.y());
  effective_margins_.footer = std::max(requested_margins_.footer,
                                       physical_size.height() -
                                           printable_area_.bottom());
  effective_margins_.left = std::max(requested_margins_.left,
                                     printable_area_.x());
  effective_margins_.top = std::max(std::max(requested_margins_.top,
                                             printable_area_.y()),
                                    effective_margins_.header + text_height);
  effective_margins_.right = std::max(requested_margins_.right,
                                      physical_size.width() -
                                          printable_area_.right());
  effective_margins_.bottom = std::max(std::max(requested_margins_.bottom,
                                                physical_size.height() -
                                                    printable_area_.bottom()),
                                       effective_margins_.footer + text_height);

  // Calculate the overlay area. If the margins are excessive, the overlay_area
  // size will be (0, 0).
  overlay_area_.set_x(effective_margins_.left);
  overlay_area_.set_y(effective_margins_.header);
  overlay_area_.set_width(std::max(0,
                                   physical_size.width() -
                                       effective_margins_.right -
                                       overlay_area_.x()));
  overlay_area_.set_height(std::max(0,
                                    physical_size.height() -
                                        effective_margins_.footer -
                                        overlay_area_.y()));

  // Calculate the content area. If the margins are excessive, the content_area
  // size will be (0, 0).
  content_area_.set_x(effective_margins_.left);
  content_area_.set_y(effective_margins_.top);
  content_area_.set_width(std::max(0,
                                   physical_size.width() -
                                       effective_margins_.right -
                                       content_area_.x()));
  content_area_.set_height(std::max(0,
                                    physical_size.height() -
                                        effective_margins_.bottom -
                                        content_area_.y()));
}

void PageSetup::SetRequestedMargins(const PageMargins& requested_margins) {
  requested_margins_ = requested_margins;
  if (physical_size_.width() && physical_size_.height())
    Init(physical_size_, printable_area_, text_height_);
}

}  // namespace printing

