// Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>

#include "include/cef_print_settings.h"
#include "tests/gtest/include/gtest/gtest.h"

// Verify Set/Get methods for CefPrintSettings.
TEST(PrintTest, SettingsSetGet) {
  // CefRequest CreateRequest
  CefRefPtr<CefPrintSettings> settings(CefPrintSettings::Create());
  EXPECT_TRUE(settings.get() != nullptr);
  EXPECT_TRUE(settings->IsValid());
  EXPECT_FALSE(settings->IsReadOnly());

  bool landscape = true;
  settings->SetOrientation(landscape);
  EXPECT_EQ(landscape, settings->IsLandscape());
  landscape = false;
  settings->SetOrientation(landscape);
  EXPECT_EQ(landscape, settings->IsLandscape());

  const char device_name[] = "my_device_name";
  settings->SetDeviceName(device_name);
  EXPECT_STREQ(device_name, settings->GetDeviceName().ToString().c_str());

  int dpi = 25;
  settings->SetDPI(dpi);
  EXPECT_EQ(dpi, settings->GetDPI());

  CefPrintSettings::PageRangeList page_ranges;
  page_ranges.push_back(CefRange(1, 3));
  page_ranges.push_back(CefRange(5, 6));
  settings->SetPageRanges(page_ranges);
  EXPECT_EQ(page_ranges.size(), settings->GetPageRangesCount());
  CefPrintSettings::PageRangeList page_ranges2;
  settings->GetPageRanges(page_ranges2);
  EXPECT_EQ(page_ranges.size(), page_ranges2.size());
  for (size_t i = 0; i < page_ranges.size(); ++i)
    EXPECT_EQ(page_ranges[i], page_ranges2[i]);

  bool selection_only = true;
  settings->SetSelectionOnly(selection_only);
  EXPECT_EQ(selection_only, settings->IsSelectionOnly());
  selection_only = false;
  settings->SetSelectionOnly(selection_only);
  EXPECT_EQ(selection_only, settings->IsSelectionOnly());

  bool collate = true;
  settings->SetCollate(collate);
  EXPECT_EQ(collate, settings->WillCollate());
  collate = false;
  settings->SetCollate(collate);
  EXPECT_EQ(collate, settings->WillCollate());

  CefPrintSettings::ColorModel color_model = COLOR_MODEL_CMYK;
  settings->SetColorModel(color_model);
  EXPECT_EQ(color_model, settings->GetColorModel());

  int copies = 3;
  settings->SetCopies(copies);
  EXPECT_EQ(copies, settings->GetCopies());

  CefPrintSettings::DuplexMode duplex_mode = DUPLEX_MODE_SIMPLEX;
  settings->SetDuplexMode(duplex_mode);
  EXPECT_EQ(duplex_mode, settings->GetDuplexMode());
}
