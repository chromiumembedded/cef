// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_PRINTING_PRINT_SETTINGS_H_
#define CEF_LIBCEF_PRINTING_PRINT_SETTINGS_H_
#pragma once

#include "include/internal/cef_types.h"

#include "printing/page_range.h"
#include "printing/page_setup.h"
#include "ui/gfx/rect.h"

typedef struct HDC__* HDC;
typedef struct _devicemodeW DEVMODE;

namespace printing {

// Parameters for a render request.
struct PrintParams {
  // In pixels according to dpi_x and dpi_y.
  gfx::Size printable_size;

  // Specifies dots per inch.
  double dpi;

  // Minimum shrink factor. See PrintSettings::min_shrink for more information.
  double min_shrink;

  // Maximum shrink factor. See PrintSettings::max_shrink for more information.
  double max_shrink;

  // Desired apparent dpi on paper.
  int desired_dpi;

  // Cookie for the document to ensure correctness.
  int document_cookie;

  // Indicates if the user only wants to print the current selection.
  bool selection_only;

  // Indicates if the user wants to print to file.
  bool to_file;

  // Warning: do not compare document_cookie.
  bool Equals(const PrintParams& rhs) const {
    return printable_size == rhs.printable_size &&
           dpi == rhs.dpi &&
           min_shrink == rhs.min_shrink &&
           max_shrink == rhs.max_shrink &&
           desired_dpi == rhs.desired_dpi &&
           selection_only == rhs.selection_only &&
           to_file == rhs.to_file;
  }
};

// Page measurements information.
class PageMeasurements {
 public:
  PageMeasurements();
  void Clear();

  // Equality operator.
  bool Equals(const PageMeasurements& rhs) const;

  enum cef_paper_type_t page_type;
  // Page length and width represented in inches.
  // These should be filled in if page_type is PT_CUSTOM.
  double page_length;
  double page_width;
};

// OS-independent print settings.
class PrintSettings {
 public:
  PrintSettings();

  // Reinitialize the settings to the default values.
  void Clear();

#ifdef WIN32
  // Reads the settings from the selected device context. Calculates derived
  // values like printable_area_.
  void Init(HDC hdc,
            const DEVMODE& dev_mode,
            const PageRanges& new_ranges,
            const CefString& new_device_name,
            bool selection_only,
            bool to_file);
#endif

  // Set printer printable area in pixels.
  void SetPrinterPrintableArea(gfx::Size const& physical_size_pixels,
                               gfx::Rect const& printable_area_pixels);

  // Initializes the print parameters that needs to be sent to the renderer
  // process.
  void RenderParams(PrintParams* params) const;

  // Equality operator.
  // NOTE: printer_name is NOT tested for equality since it doesn't affect the
  // output.
  bool Equals(const PrintSettings& rhs) const;

  const CefString& printer_name() const { return printer_name_; }
  void set_device_name(const CefString& device_name) {
    device_name_ = device_name;
  }
  const CefString& device_name() const { return device_name_; }
  int dpi() const { return dpi_; }
  const PageSetup& page_setup_pixels() const { return page_setup_pixels_; }

  void UpdatePrintOptions(cef_print_options_t& print_options);
  void UpdateFromPrintOptions(const cef_print_options_t& print_options);

  // Multi-page printing. Each PageRange describes a from-to page combination.
  // This permits printing selected pages only.
  PageRanges ranges;

  // By imaging to a width a little wider than the available pixels, thin pages
  // will be scaled down a little, matching the way they print in IE and Camino.
  // This lets them use fewer sheets than they would otherwise, which is
  // presumably why other browsers do this. Wide pages will be scaled down more
  // than this.
  double min_shrink;

  // This number determines how small we are willing to reduce the page content
  // in order to accommodate the widest line. If the page would have to be
  // reduced smaller to make the widest line fit, we just clip instead (this
  // behavior matches MacIE and Mozilla, at least)
  double max_shrink;

  // Desired visible dots per inch rendering for output. Printing should be
  // scaled to ScreenDpi/dpix*desired_dpi.
  int desired_dpi;

  // Indicates if the user only wants to print the current selection.
  bool selection_only;

  // Indicates if the user wants to print to file.
  bool to_file;

  // Cookie generator. It is used to initialize PrintedDocument with its
  // associated PrintSettings, to be sure that each generated PrintedPage is
  // correctly associated with its corresponding PrintedDocument.
  static int NewCookie();

  // Requested Page Margins in pixels based on desired_dpi.
  // These are in terms of desired_dpi since printer dpi may vary.
  PageMargins requested_margins;

  // Is the orientation landscape or portrait.
  bool landscape;

  // Page Measurements.
  PageMeasurements page_measurements;

 private:
  void ResetRequestedPageMargins();
  //////////////////////////////////////////////////////////////////////////////
  // Settings that can't be changed without side-effects.

  // Printer name as shown to the user.
  CefString printer_name_;

  // Printer device name as opened by the OS.
  CefString device_name_;

  // Page setup in pixel units, dpi adjusted.
  PageSetup page_setup_pixels_;

  // Printer's device effective dots per inch in both axis.
  int dpi_;
};

}  // namespace printing

#endif  // CEF_LIBCEF_PRINTING_PRINT_SETTINGS_H_

