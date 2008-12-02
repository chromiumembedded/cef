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
  page_setup_cmm_.Clear();
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

  int old_dpi = dpi_;
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
  // Hard-code text_height = 0.5cm = ~1/5 of inch
  page_setup_pixels_.Init(physical_size_pixels, printable_area_pixels,
                          ConvertUnit(500, kHundrethsMMPerInch, dpi_));

  // Initialize page_setup_cmm_.
  // In theory, we should be using HORZSIZE and VERTSIZE but their value is
  // so wrong it's useless. So read the values in dpi unit and convert them back
  // in 0.01 mm.
  gfx::Size physical_size_cmm(
      ConvertUnit(physical_size_pixels.width(), dpi_, kHundrethsMMPerInch),
      ConvertUnit(physical_size_pixels.height(), dpi_, kHundrethsMMPerInch));
  gfx::Rect printable_area_cmm(
      ConvertUnit(printable_area_pixels.x(), dpi_, kHundrethsMMPerInch),
      ConvertUnit(printable_area_pixels.y(), dpi_, kHundrethsMMPerInch),
      ConvertUnit(printable_area_pixels.width(), dpi_, kHundrethsMMPerInch),
      ConvertUnit(printable_area_pixels.bottom(), dpi_, kHundrethsMMPerInch));

  static const int kRoundingTolerance = 5;
  // Some printers may advertise a slightly larger printable area than the
  // physical area. This is mostly due to integer calculation and rounding.
  if (physical_size_cmm.height() > printable_area_cmm.bottom() &&
      physical_size_cmm.height() <= (printable_area_cmm.bottom() +
                                     kRoundingTolerance)) {
    physical_size_cmm.set_height(printable_area_cmm.bottom());
  }
  if (physical_size_cmm.width() > printable_area_cmm.right() &&
      physical_size_cmm.width() <= (printable_area_cmm.right() +
                                    kRoundingTolerance)) {
    physical_size_cmm.set_width(printable_area_cmm.right());
  }
  page_setup_cmm_.Init(physical_size_cmm, printable_area_cmm, 500);
}
#endif

void PrintSettings::UpdateMarginsMetric(const PageMargins& new_margins) {
  // Apply the new margins in 0.01 mm unit.
  page_setup_cmm_.SetRequestedMargins(new_margins);

  // Converts the margins in dpi unit and apply those too.
  PageMargins pixels_margins;
  pixels_margins.header = ConvertUnit(new_margins.header, kHundrethsMMPerInch,
                                      dpi_);
  pixels_margins.footer = ConvertUnit(new_margins.footer, kHundrethsMMPerInch,
                                      dpi_);
  pixels_margins.left = ConvertUnit(new_margins.left, kHundrethsMMPerInch,
                                    dpi_);
  pixels_margins.top = ConvertUnit(new_margins.top, kHundrethsMMPerInch, dpi_);
  pixels_margins.right = ConvertUnit(new_margins.right, kHundrethsMMPerInch,
                                     dpi_);
  pixels_margins.bottom = ConvertUnit(new_margins.bottom, kHundrethsMMPerInch,
                                      dpi_);
  page_setup_pixels_.SetRequestedMargins(pixels_margins);
}

void PrintSettings::UpdateMarginsMilliInch(const PageMargins& new_margins) {
  // Convert margins from thousandth inches to cmm (0.01mm).
  PageMargins cmm_margins;
  cmm_margins.header =
      ConvertMilliInchToHundredThousanthMeter(new_margins.header);
  cmm_margins.footer =
      ConvertMilliInchToHundredThousanthMeter(new_margins.footer);
  cmm_margins.left = ConvertMilliInchToHundredThousanthMeter(new_margins.left);
  cmm_margins.top = ConvertMilliInchToHundredThousanthMeter(new_margins.top);
  cmm_margins.right =
      ConvertMilliInchToHundredThousanthMeter(new_margins.right);
  cmm_margins.bottom =
      ConvertMilliInchToHundredThousanthMeter(new_margins.bottom);
  UpdateMarginsMetric(cmm_margins);
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
      page_setup_cmm_.Equals(rhs.page_setup_cmm_) &&
      dpi_ == rhs.dpi_ &&
      landscape_ == rhs.landscape_;
}

int PrintSettings::NewCookie() {
  // A cookie of 0 is used to mark a document as unassigned, count from 1.
  return cookie_seq.GetNext() + 1;
}

}  // namespace printing

