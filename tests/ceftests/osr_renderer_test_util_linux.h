// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_OSR_RENDERER_TEST_UTIL_LINUX_H_
#define CEF_TESTS_CEFTESTS_OSR_RENDERER_TEST_UTIL_LINUX_H_
#pragma once

#include <cstdint>
#include <vector>

#include "tests/shared/browser/osr_gl_linux.h"
#include "tests/shared/browser/osr_renderer_gl_linux.h"

namespace osr_unittests {

// Returns an initialized display that does not require a window system.
client::gl::EGLDisplay CreateHeadlessDisplay(const client::gl::Api* api);

// Synchronous readback used by renderer unit tests and browser timing tests.
// Requires the renderer's OpenGL context to be current.
bool RenderGlPixels(client::OsrRendererGl* renderer,
                    int width,
                    int height,
                    std::vector<uint32_t>* pixels);

}  // namespace osr_unittests

#endif  // CEF_TESTS_CEFTESTS_OSR_RENDERER_TEST_UTIL_LINUX_H_
