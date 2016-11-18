// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/image_util.h"

#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/resource_util.h"

namespace image_util {

void LoadImage(CefRefPtr<CefImage> image,
               double scale_factor,
               const std::string& name,
               const CefSize& expected_size) {
  std::string image_str;

  std::string name_str;
  if (scale_factor == 1.0f)
    name_str = name + ".1x.png";
  else if (scale_factor == 2.0f)
    name_str = name + ".2x.png";

  EXPECT_TRUE(client::LoadBinaryResource(name_str.c_str(), image_str));
  EXPECT_TRUE(image->AddPNG(scale_factor, image_str.c_str(), image_str.size()));

  EXPECT_FALSE(image->IsEmpty());
  EXPECT_EQ(expected_size.width, static_cast<int>(image->GetWidth()));
  EXPECT_EQ(expected_size.height, static_cast<int>(image->GetHeight()));
}

void LoadIconImage(CefRefPtr<CefImage> image,
                   double scale_factor,
                   const std::string& name) {
  LoadImage(image, scale_factor, name, CefSize(16, 16));
}

}  // namespace image_util
