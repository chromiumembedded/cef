// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "print_settings.h"
#include "units.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"

namespace printing {

// Global SequenceNumber used for generating unique cookie values.
static base::AtomicSequenceNumber cookie_seq(base::LINKER_INITIALIZED);

PrintSettings::PrintSettings()
    : min_shrink(1.25),
      max_shrink(2.0),
      desired_dpi(72),
      dpi_(0),
      landscape_(false) {
}

void PrintSettings::Clear() {
  ranges.clear();
  min_shrink = 1.25;
  max_shrink = 2.;
  desired_dpi = 72;
  printer_name_.clear();
  device_name_.clear();
  page_setup_pixels_.Clear();
  dpi_ = 0;
  landscape_ = false;
}

#ifdef WIN32
void PrintSettings::Init(HDC hdc,
                         const DEVMODE& dev_mode,
                         const PageRanges& new_ranges,
                         const std::wstring& new_device_name) {
  DCHECK(hdc);
  printer_name_ = dev_mode.dmDeviceName;
  device_name_ = new_device_name;
  ranges = new_ranges;
  landscape_ = dev_mode.dmOrientation == DMORIENT_LANDSCAPE;

  dpi_ = GetDeviceCaps(hdc, LOGPIXELSX);
  // No printer device is known to advertise different dpi in X and Y axis; even
  // the fax device using the 200x100 dpi setting. It's ought to break so many
  // applications that it's not even needed to care about. WebKit doesn't
  // support different dpi settings in X and Y axis.
  DCHECK_EQ(dpi_, GetDeviceCaps(hdc, LOGPIXELSY));

  DCHECK_EQ(GetDeviceCaps(hdc, SCALINGFACTORX), 0);
  DCHECK_EQ(GetDeviceCaps(hdc, SCALINGFACTORY), 0);

  // Initialize page_setup_pixels_.
  gfx::Size physical_size_pixels(GetDeviceCaps(hdc, PHYSICALWIDTH),
                                 GetDeviceCaps(hdc, PHYSICALHEIGHT));
  gfx::Rect printable_area_pixels(GetDeviceCaps(hdc, PHYSICALOFFSETX),
                                  GetDeviceCaps(hdc, PHYSICALOFFSETY),
                                  GetDeviceCaps(hdc, HORZRES),
                                  GetDeviceCaps(hdc, VERTRES));

  SetPrinterPrintableArea(physical_size_pixels, printable_area_pixels);
}
#endif

void PrintSettings::SetPrinterPrintableArea(
    gfx::Size const& physical_size_pixels,
    gfx::Rect const& printable_area_pixels) {

  // Start by setting the user configuration
  // Hard-code text_height = 0.5cm = ~1/5 of inch
  page_setup_pixels_.Init(physical_size_pixels,
                          printable_area_pixels,
                          ConvertUnit(500, kHundrethsMMPerInch, dpi_));

  // Now apply user configured settings.
  PageMargins margins;
  margins.header = 500;
  margins.footer = 500;
  margins.left = 500;
  margins.top = 500;
  margins.right = 500;
  margins.bottom = 500;
  page_setup_pixels_.SetRequestedMargins(margins);
}

void PrintSettings::RenderParams(PrintParams* params) const {
  DCHECK(params);
  params->printable_size.SetSize(page_setup_pixels_.content_area().width(),
                                 page_setup_pixels_.content_area().height());
  params->dpi = dpi_;
  // Currently hardcoded at 1.25. See PrintSettings' constructor.
  params->min_shrink = min_shrink;
  // Currently hardcoded at 2.0. See PrintSettings' constructor.
  params->max_shrink = max_shrink;
  // Currently hardcoded at 72dpi. See PrintSettings' constructor.
  params->desired_dpi = desired_dpi;
  // Always use an invalid cookie.
  params->document_cookie = 0;
}

bool PrintSettings::Equals(const PrintSettings& rhs) const {
  // Do not test the display device name (printer_name_) for equality since it
  // may sometimes be chopped off at 30 chars. As long as device_name is the
  // same, that's fine.
  return ranges == rhs.ranges &&
      min_shrink == rhs.min_shrink &&
      max_shrink == rhs.max_shrink &&
      desired_dpi == rhs.desired_dpi &&
      device_name_ == rhs.device_name_ &&
      page_setup_pixels_.Equals(rhs.page_setup_pixels_) &&
      dpi_ == rhs.dpi_ &&
      landscape_ == rhs.landscape_;
}

int PrintSettings::NewCookie() {
  // A cookie of 0 is used to mark a document as unassigned, count from 1.
  return cookie_seq.GetNext() + 1;
}

}  // namespace printing

