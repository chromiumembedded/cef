// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_OSR_RENDERER_TEST_UTIL_WIN_H_
#define CEF_TESTS_CEFTESTS_OSR_RENDERER_TEST_UTIL_WIN_H_
#pragma once

#include <d3d11.h>

#include <cstdint>
#include <vector>

namespace osr_unittests {

// Synchronous readback used by renderer unit tests and browser timing tests.
// The caller must finish rendering before calling.
bool ReadD3D11Texture(ID3D11Texture2D* texture, std::vector<uint32_t>* pixels);

}  // namespace osr_unittests

#endif  // CEF_TESTS_CEFTESTS_OSR_RENDERER_TEST_UTIL_WIN_H_
