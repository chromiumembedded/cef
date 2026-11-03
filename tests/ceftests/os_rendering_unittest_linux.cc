// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>

#include "tests/ceftests/os_rendering_unittest_shared_texture.h"
#include "tests/ceftests/osr_renderer_test_util_linux.h"
#include "tests/shared/browser/osr_gl_linux.h"
#include "tests/shared/browser/osr_renderer_gl_linux.h"

namespace osr_unittests {

namespace gl = client::gl;

// Returns an initialized display that does not require a window system.
gl::EGLDisplay CreateHeadlessDisplay(const gl::Api* api) {
  if (gl::HasExtension(gl::kNoDisplay, "EGL_MESA_platform_surfaceless")) {
    gl::EGLDisplay display = api->eglGetPlatformDisplay(
        gl::kEglPlatformSurfacelessMesa, nullptr, nullptr);
    if (display && api->eglInitialize(display, nullptr, nullptr)) {
      return display;
    }
  }
  // Supported by Mesa and NVIDIA.
  if (gl::HasExtension(gl::kNoDisplay, "EGL_EXT_platform_device") &&
      gl::HasExtension(gl::kNoDisplay, "EGL_EXT_device_enumeration")) {
    auto* query_devices =
        reinterpret_cast<gl::EGLBoolean (*)(gl::EGLint, void**, gl::EGLint*)>(
            api->eglGetProcAddress("eglQueryDevicesEXT"));
    void* devices[16] = {};
    gl::EGLint count = 0;
    if (query_devices && query_devices(16, devices, &count)) {
      for (gl::EGLint i = 0; i < count; ++i) {
        gl::EGLDisplay display = api->eglGetPlatformDisplay(
            gl::kEglPlatformDevice, devices[i], nullptr);
        if (display && api->eglInitialize(display, nullptr, nullptr)) {
          return display;
        }
      }
    }
  }
  return gl::kNoDisplay;
}

bool RenderGlPixels(client::OsrRendererGl* renderer,
                    int width,
                    int height,
                    std::vector<uint32_t>* pixels) {
  const auto* api = client::gl::GetApi();
  if (!api || width <= 0 || height <= 0) {
    return false;
  }
  client::gl::GLuint texture = 0;
  client::gl::GLuint framebuffer = 0;
  api->glGenTextures(1, &texture);
  api->glBindTexture(client::gl::kGlTexture2D, texture);
  api->glTexImage2D(client::gl::kGlTexture2D, 0, client::gl::kGlRgba8, width,
                    height, 0, client::gl::kGlBgra,
                    client::gl::kGlUnsignedInt8888Rev, nullptr);
  api->glBindTexture(client::gl::kGlTexture2D, 0);
  api->glGenFramebuffers(1, &framebuffer);
  api->glBindFramebuffer(client::gl::kGlFramebuffer, framebuffer);
  api->glFramebufferTexture2D(client::gl::kGlFramebuffer,
                              client::gl::kGlColorAttachment0,
                              client::gl::kGlTexture2D, texture, 0);
  bool success = api->glCheckFramebufferStatus(client::gl::kGlFramebuffer) ==
                     client::gl::kGlFramebufferComplete &&
                 renderer->Render(framebuffer, width, height);
  std::vector<uint32_t> bottom_up(width * height);
  if (success) {
    api->glBindFramebuffer(client::gl::kGlFramebuffer, framebuffer);
    api->glPixelStorei(client::gl::kGlPackAlignment, 4);
    api->glReadPixels(0, 0, width, height, client::gl::kGlBgra,
                      client::gl::kGlUnsignedInt8888Rev, bottom_up.data());
    success = api->glGetError() == client::gl::kGlNoError;
  }
  api->glBindFramebuffer(client::gl::kGlFramebuffer, 0);
  api->glDeleteFramebuffers(1, &framebuffer);
  api->glDeleteTextures(1, &texture);
  if (success) {
    pixels->resize(width * height);
    for (int y = 0; y < height; ++y) {
      std::copy_n(bottom_up.begin() + (height - 1 - y) * width, width,
                  pixels->begin() + y * width);
    }
  }
  return success;
}

namespace {

class NativeSharedTexturePixelReader : public SharedTexturePixelReader {
 public:
  bool Initialize() override {
    api_ = client::gl::GetApi();
    if (!api_) {
      return false;
    }
    display_ = CreateHeadlessDisplay(api_);
    if (!display_ ||
        !client::gl::HasExtension(display_, "EGL_KHR_surfaceless_context") ||
        !client::gl::HasExtension(display_, "EGL_KHR_no_config_context") ||
        !client::gl::SupportsDmaBufImport(display_)) {
      return false;
    }
    context_ = client::gl::CreateContext(display_, nullptr);
    if (!context_ || !MakeCurrent()) {
      return false;
    }
    renderer_ = std::make_unique<client::OsrRendererGl>(0);
    const bool initialized = renderer_->Initialize();
    ReleaseCurrent();
    return initialized;
  }

  void Cleanup() override {
    if (context_) {
      if (MakeCurrent()) {
        if (renderer_) {
          renderer_->Cleanup();
        }
        ReleaseCurrent();
      }
      renderer_.reset();
      api_->eglDestroyContext(display_, context_);
      context_ = nullptr;
    }
  }

  bool Read(const CefAcceleratedPaintInfo& info,
            std::vector<uint32_t>* pixels) override {
    if (!MakeCurrent()) {
      return false;
    }
    const CefRect visible = info.extra.visible_rect;
    const bool success =
        renderer_->OnAcceleratedPaint(PET_VIEW, {}, info) &&
        RenderGlPixels(renderer_.get(), visible.width, visible.height, pixels);
    ReleaseCurrent();
    return success;
  }

 private:
  bool MakeCurrent() {
    return api_->eglMakeCurrent(display_, client::gl::kNoSurface,
                                client::gl::kNoSurface, context_);
  }
  void ReleaseCurrent() {
    api_->eglMakeCurrent(display_, client::gl::kNoSurface,
                         client::gl::kNoSurface, client::gl::kNoContext);
  }
  const client::gl::Api* api_ = nullptr;
  client::gl::EGLDisplay display_ = nullptr;
  client::gl::EGLContext context_ = nullptr;
  std::unique_ptr<client::OsrRendererGl> renderer_;
};

}  // namespace

std::unique_ptr<SharedTexturePixelReader> CreateSharedTexturePixelReader() {
  return std::make_unique<NativeSharedTexturePixelReader>();
}

}  // namespace osr_unittests
