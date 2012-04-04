// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/printing/print_settings.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "printing/units.h"

namespace printing {

// Global SequenceNumber used for generating unique cookie values.
static base::AtomicSequenceNumber cookie_seq;

PageMeasurements::PageMeasurements()
    : page_type(PT_LETTER),
      page_length(0.0f),
      page_width(0.0f) {
}

void PageMeasurements::Clear() {
  page_type = PT_LETTER;
  page_length = 0.0f;
  page_width = 0.0f;
}

bool PageMeasurements::Equals(const PageMeasurements& rhs) const {
  return page_type == rhs.page_type &&
      page_length == rhs.page_length &&
      page_width == rhs.page_width;
}

PrintSettings::PrintSettings()
    : min_shrink(1.25),
      max_shrink(2.0),
      desired_dpi(72),
      selection_only(false),
      to_file(false),
      dpi_(0),
      landscape(false) {
  ResetRequestedPageMargins();
}

void PrintSettings::Clear() {
  ranges.clear();
  min_shrink = 1.25;
  max_shrink = 2.;
  desired_dpi = 72;
  selection_only = false;
  to_file = false;
  printer_name_.clear();
  device_name_.clear();
  page_setup_pixels_.Clear();
  dpi_ = 0;
  landscape = false;
  page_measurements.Clear();
  ResetRequestedPageMargins();
}

void PrintSettings::ResetRequestedPageMargins() {
  // Initial requested margins to = 1.0cm = ~2/5 of inch
  const int margin_printer_units =
      ConvertUnit(1000, kHundrethsMMPerInch, desired_dpi);
  // Initial requested header/footer margins to = 0.5cm = ~1/5 of inch
  const int header_footer_margins =
      ConvertUnit(500, kHundrethsMMPerInch, desired_dpi);
  requested_margins.header = header_footer_margins;
  requested_margins.footer = header_footer_margins;
  requested_margins.left = margin_printer_units;
  requested_margins.top = margin_printer_units;
  requested_margins.right = margin_printer_units;
  requested_margins.bottom = margin_printer_units;
}

#ifdef WIN32
void PrintSettings::Init(HDC hdc,
                         const DEVMODE& dev_mode,
                         const PageRanges& new_ranges,
                         const CefString& new_device_name,
                         bool print_selection_only,
                         bool print_to_file) {
  DCHECK(hdc);
  printer_name_ = std::wstring(dev_mode.dmDeviceName);
  device_name_ = new_device_name;
  ranges = new_ranges;
  landscape = dev_mode.dmOrientation == DMORIENT_LANDSCAPE;
  selection_only = print_selection_only;
  to_file = print_to_file;

  bool is_custom_paper = true;
  if (dev_mode.dmFields & DM_PAPERSIZE) {
    switch (dev_mode.dmPaperSize) {
    case DMPAPER_LETTER:
      page_measurements.page_type = PT_LETTER;
      is_custom_paper = false;
      break;
    case DMPAPER_LEGAL:
      page_measurements.page_type = PT_LEGAL;
      is_custom_paper = false;
      break;
    case DMPAPER_EXECUTIVE:
      page_measurements.page_type = PT_EXECUTIVE;
      is_custom_paper = false;
      break;
    case DMPAPER_A3:
      page_measurements.page_type = PT_A3;
      is_custom_paper = false;
      break;
    case DMPAPER_A4:
      page_measurements.page_type = PT_A4;
      is_custom_paper = false;
      break;
    default:
      // We'll translate it as a custom paper size.
      break;
    }
  }

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

  if (is_custom_paper) {
    page_measurements.page_length = ConvertUnitDouble(
      static_cast<double>(physical_size_pixels.height()),
      static_cast<double>(dpi_),
      static_cast<double>(desired_dpi));
    page_measurements.page_width = ConvertUnitDouble(
      static_cast<double>(physical_size_pixels.width()),
      static_cast<double>(dpi_),
      static_cast<double>(desired_dpi));
    if (landscape) {
      double temp = page_measurements.page_length;
      page_measurements.page_length = page_measurements.page_width;
      page_measurements.page_width = temp;
    }
  }
  SetPrinterPrintableArea(physical_size_pixels, printable_area_pixels);
}
#endif

void PrintSettings::SetPrinterPrintableArea(
    gfx::Size const& physical_size_pixels,
    gfx::Rect const& printable_area_pixels) {

  // Hard-code text_height = 0.5cm = ~1/5 of inch
  const int text_height = ConvertUnit(500, kHundrethsMMPerInch, dpi_);

  // Start by setting the user configuration
  page_setup_pixels_.Init(physical_size_pixels,
                          printable_area_pixels,
                          text_height);

  // Now adjust requested margins to the appropriate dpi.
  PageMargins margins;
  margins.header = ConvertUnit(requested_margins.header, desired_dpi, dpi_);
  margins.footer = ConvertUnit(requested_margins.footer, desired_dpi, dpi_);
  margins.left = ConvertUnit(requested_margins.left, desired_dpi, dpi_);
  margins.top = ConvertUnit(requested_margins.top, desired_dpi, dpi_);
  margins.right = ConvertUnit(requested_margins.right, desired_dpi, dpi_);
  margins.bottom = ConvertUnit(requested_margins.bottom, desired_dpi, dpi_);
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
  params->selection_only = selection_only;
  params->to_file = to_file;
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
      landscape == rhs.landscape &&
      page_measurements.Equals(rhs.page_measurements) &&
      requested_margins.Equals(rhs.requested_margins);
}

int PrintSettings::NewCookie() {
  // A cookie of 0 is used to mark a document as unassigned, count from 1.
  return cookie_seq.GetNext() + 1;
}

void PrintSettings::UpdatePrintOptions(cef_print_options_t& print_options) {
  print_options.page_orientation = (landscape) ? LANDSCAPE : PORTRAIT;
  print_options.paper_metrics.paper_type = page_measurements.page_type;

  if (page_measurements.page_type == PT_CUSTOM) {
    print_options.paper_metrics.length = ConvertUnitDouble(
      page_measurements.page_length, desired_dpi, 1);
    print_options.paper_metrics.width = ConvertUnitDouble(
      page_measurements.page_width, desired_dpi, 1);
  }
  print_options.paper_margins.left = ConvertUnitDouble(
      requested_margins.left, desired_dpi, 1);
  print_options.paper_margins.top = ConvertUnitDouble(
      requested_margins.top, desired_dpi, 1);
  print_options.paper_margins.right = ConvertUnitDouble(
      requested_margins.right, desired_dpi, 1);
  print_options.paper_margins.bottom = ConvertUnitDouble(
      requested_margins.bottom, desired_dpi, 1);
  print_options.paper_margins.header = ConvertUnitDouble(
      requested_margins.header, desired_dpi, 1);
  print_options.paper_margins.footer = ConvertUnitDouble(
      requested_margins.footer, desired_dpi, 1);
}
void PrintSettings::UpdateFromPrintOptions(
    const cef_print_options_t& print_options) {
  landscape = print_options.page_orientation == LANDSCAPE;
  page_measurements.page_type = print_options.paper_metrics.paper_type;
  if (page_measurements.page_type == PT_CUSTOM) {
    page_measurements.page_length = ConvertUnitDouble(
      print_options.paper_metrics.length, 1, desired_dpi);
    page_measurements.page_width = ConvertUnitDouble(
      print_options.paper_metrics.width, 1, desired_dpi);
  }
  requested_margins.left = static_cast<int>(ConvertUnitDouble(
      print_options.paper_margins.left, 1, desired_dpi));
  requested_margins.top = static_cast<int>(ConvertUnitDouble(
      print_options.paper_margins.top, 1, desired_dpi));
  requested_margins.right = static_cast<int>(ConvertUnitDouble(
      print_options.paper_margins.right, 1, desired_dpi));
  requested_margins.bottom = static_cast<int>(ConvertUnitDouble(
      print_options.paper_margins.bottom, 1, desired_dpi));
  requested_margins.header = static_cast<int>(ConvertUnitDouble(
      print_options.paper_margins.header, 1, desired_dpi));
  requested_margins.footer = static_cast<int>(ConvertUnitDouble(
      print_options.paper_margins.footer, 1, desired_dpi));
}

}  // namespace printing

