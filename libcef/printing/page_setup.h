// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _PRINTING_PAGE_SETUP_H
#define _PRINTING_PAGE_SETUP_H

#include "base/gfx/rect.h"

namespace printing {

// Margins for a page setup.
class PageMargins {
 public:
  PageMargins();

  void Clear();

  // Equality operator.
  bool Equals(const PageMargins& rhs) const;

  // Vertical space for the overlay from the top of the sheet.
  int header;
  // Vertical space for the overlay from the bottom of the sheet.
  int footer;
  // Margin on each side of the sheet.
  int left;
  int right;
  int top;
  int bottom;
};

// Settings that define the size and printable areas of a page. Unit is
// unspecified.
class PageSetup {
 public:
  PageSetup();

  void Clear();

  // Equality operator.
  bool Equals(const PageSetup& rhs) const;

  void Init(const gfx::Size& physical_size, const gfx::Rect& printable_area,
            int text_height);

  void SetRequestedMargins(const PageMargins& requested_margins);

  const gfx::Size& physical_size() const { return physical_size_; }
  const gfx::Rect& overlay_area() const { return overlay_area_; }
  const gfx::Rect& content_area() const { return content_area_; }
  const PageMargins& effective_margins() const {
    return effective_margins_;
  }

 private:
  // Physical size of the page, including non-printable margins.
  gfx::Size physical_size_;

  // The printable area as specified by the printer driver. We can't get
  // larger than this.
  gfx::Rect printable_area_;

  // The printable area for headers and footers.
  gfx::Rect overlay_area_;

  // The printable area as selected by the user's margins.
  gfx::Rect content_area_;

  // Effective margins.
  PageMargins effective_margins_;

  // Requested margins.
  PageMargins requested_margins_;

  // Space that must be kept free for the overlays.
  int text_height_;
};

}  // namespace printing

#endif  // _PRINTING_PAGE_SETUP_H

