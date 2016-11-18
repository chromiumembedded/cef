// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_UNITTESTS_IMAGE_UTIL_H_
#define CEF_TESTS_UNITTESTS_IMAGE_UTIL_H_
#pragma once

#include "include/cef_image.h"

namespace image_util {

// Load an PNG image. Tests that the size is |expected_size| in DIPs. Call
// multiple times to load the same image at different scale factors.
void LoadImage(CefRefPtr<CefImage> image,
               double scale_factor,
               const std::string& name,
               const CefSize& expected_size);

// Load an icon image. Expected size is 16x16 DIPs.
void LoadIconImage(CefRefPtr<CefImage> image,
                   double scale_factor,
                   const std::string& name = "window_icon");

}  // namespace image_util

#endif  // CEF_TESTS_UNITTESTS_IMAGE_UTIL_H_
