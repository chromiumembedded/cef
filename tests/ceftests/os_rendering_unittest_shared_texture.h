// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_OS_RENDERING_UNITTEST_SHARED_TEXTURE_H_
#define CEF_TESTS_CEFTESTS_OS_RENDERING_UNITTEST_SHARED_TEXTURE_H_
#pragma once

#include <memory>

#include "include/cef_render_handler.h"

namespace osr_unittests {

// Initialize, Read and Cleanup run on the UI thread. Read reopens the resource
// and finishes copying the two BGRA pixels before the paint callback returns.
// Only client-owned readback resources may be cached.
class SharedTexturePixelReader {
 public:
  virtual ~SharedTexturePixelReader() = default;
  virtual bool Initialize() = 0;
  virtual void Cleanup() = 0;
  virtual bool Read(const CefAcceleratedPaintInfo& info,
                    uint32_t* marker,
                    uint32_t* timestamp) = 0;
};

std::unique_ptr<SharedTexturePixelReader> CreateSharedTexturePixelReader();

}  // namespace osr_unittests

#endif  // CEF_TESTS_CEFTESTS_OS_RENDERING_UNITTEST_SHARED_TEXTURE_H_
