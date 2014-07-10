// Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PRINT_SETTINGS_IMPL_H_
#define CEF_LIBCEF_BROWSER_PRINT_SETTINGS_IMPL_H_
#pragma once

#include "include/cef_print_settings.h"
#include "libcef/common/value_base.h"

#include "printing/print_settings.h"

// CefPrintSettings implementation
class CefPrintSettingsImpl
    : public CefValueBase<CefPrintSettings, printing::PrintSettings> {
 public:
  CefPrintSettingsImpl(printing::PrintSettings* value,
                       bool will_delete,
                       bool read_only);

  // CefPrintSettings methods.
  virtual bool IsValid() OVERRIDE;
  virtual bool IsReadOnly() OVERRIDE;
  virtual CefRefPtr<CefPrintSettings> Copy() OVERRIDE;
  virtual void SetOrientation(bool landscape) OVERRIDE;
  virtual bool IsLandscape() OVERRIDE;
  virtual void SetPrinterPrintableArea(
      const CefSize& physical_size_device_units,
      const CefRect& printable_area_device_units,
      bool landscape_needs_flip) OVERRIDE;
  virtual void SetDeviceName(const CefString& name) OVERRIDE;
  virtual CefString GetDeviceName() OVERRIDE;
  virtual void SetDPI(int dpi) OVERRIDE;
  virtual int GetDPI() OVERRIDE;
  virtual void SetPageRanges(const PageRangeList& ranges) OVERRIDE;
  virtual size_t GetPageRangesCount() OVERRIDE;
  virtual void GetPageRanges(PageRangeList& ranges) OVERRIDE;
  virtual void SetSelectionOnly(bool selection_only) OVERRIDE;
  virtual bool IsSelectionOnly() OVERRIDE;
  virtual void SetCollate(bool collate) OVERRIDE;
  virtual bool WillCollate() OVERRIDE;
  virtual void SetColorModel(ColorModel model) OVERRIDE;
  virtual ColorModel GetColorModel() OVERRIDE;
  virtual void SetCopies(int copies) OVERRIDE;
  virtual int GetCopies() OVERRIDE;
  virtual void SetDuplexMode(DuplexMode mode) OVERRIDE;
  virtual DuplexMode GetDuplexMode() OVERRIDE;

  // Must hold the controller lock while using this value.
  const printing::PrintSettings& print_settings() { return const_value(); }

  DISALLOW_COPY_AND_ASSIGN(CefPrintSettingsImpl);
};

#endif  // CEF_LIBCEF_BROWSER_PRINT_SETTINGS_IMPL_H_
