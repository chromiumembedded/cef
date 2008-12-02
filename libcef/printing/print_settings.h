// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _PRINTING_PRINT_SETTINGS_H
#define _PRINTING_PRINT_SETTINGS_H

#include "page_range.h"
#include "page_setup.h"

#include "base/gfx/rect.h"

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

  // Warning: do not compare document_cookie.
  bool Equals(const PrintParams& rhs) const {
    return printable_size == rhs.printable_size &&
           dpi == rhs.dpi &&
           min_shrink == rhs.min_shrink &&
           max_shrink == rhs.max_shrink &&
           desired_dpi == rhs.desired_dpi;
  }
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
            const std::wstring& new_device_name);
#endif

  // Sets margins in 0.01 millimeter unit.
  void UpdateMarginsMetric(const PageMargins& new_margins);

  // Sets margins in thousandth of inch.
  void UpdateMarginsMilliInch(const PageMargins& new_margins);

  // Initializes the print parameters that needs to be sent to the renderer
  // process.
  void RenderParams(PrintParams* params) const;

  // Equality operator.
  // NOTE: printer_name is NOT tested for equality since it doesn't affect the
  // output.
  bool Equals(const PrintSettings& rhs) const;

  const std::wstring& printer_name() const { return printer_name_; }
  void set_device_name(const std::wstring& device_name) {
    device_name_ = device_name;
  }
  const std::wstring& device_name() const { return device_name_; }
  int dpi() const { return dpi_; }
  const PageSetup& page_setup_cmm() const { return page_setup_cmm_; }
  const PageSetup& page_setup_pixels() const { return page_setup_pixels_; }

  // Multipage printing. Each PageRange describes a from-to page combinaison.
  // This permits printing some selected pages only.
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

  // Cookie generator. It is used to initialize PrintedDocument with its
  // associated PrintSettings, to be sure that each generated PrintedPage is
  // correctly associated with its corresponding PrintedDocument.
  static int NewCookie();

 private:
  //////////////////////////////////////////////////////////////////////////////
  // Settings that can't be changed without side-effects.

  // Printer name as shown to the user.
  std::wstring printer_name_;

  // Printer device name as opened by the OS.
  std::wstring device_name_;

  // Page setup in centimillimeter (0.01 mm) units.
  PageSetup page_setup_cmm_;

  // Page setup in pixel units, dpi adjusted.
  PageSetup page_setup_pixels_;

  // Printer's device effective dots per inch in both axis.
  int dpi_;

  // Is the orientation landscape or portrait.
  bool landscape_;
};

}  // namespace printing

#endif  // _PRINTING_PRINT_SETTINGS_H

