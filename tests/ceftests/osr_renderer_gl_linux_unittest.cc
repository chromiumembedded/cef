// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/osr_renderer_gl_linux.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/osr_gl_linux.h"

namespace {

namespace gl = client::gl;

constexpr uint32_t kRed = 0xffff0000;
constexpr uint32_t kGreen = 0xff00ff00;
constexpr uint32_t kBlue = 0xff0000ff;
constexpr uint32_t kWhite = 0xffffffff;

// Minimal libgbm interface, loaded at runtime.
struct Gbm {
  void* (*create_device)(int);
  void (*device_destroy)(void*);
  void* (*bo_create)(void*, uint32_t, uint32_t, uint32_t, uint32_t);
  void (*bo_destroy)(void*);
  int (*bo_get_fd)(void*);
  uint32_t (*bo_get_stride)(void*);
  uint32_t (*bo_get_offset)(void*, int);
  uint64_t (*bo_get_modifier)(void*);
  int (*bo_get_plane_count)(void*);
  void* (*bo_map)(void*,
                  uint32_t,
                  uint32_t,
                  uint32_t,
                  uint32_t,
                  uint32_t,
                  uint32_t*,
                  void**);
  void (*bo_unmap)(void*, void*);

  static const Gbm* Get() {
    static const Gbm* gbm = []() -> const Gbm* {
      void* library = dlopen("libgbm.so.1", RTLD_NOW | RTLD_LOCAL);
      if (!library) {
        return nullptr;
      }
      auto* result = new Gbm();
#define LOAD(field, name)                                              \
  result->field =                                                      \
      reinterpret_cast<decltype(result->field)>(dlsym(library, name)); \
  if (!result->field) {                                                \
    return nullptr;                                                    \
  }
      LOAD(create_device, "gbm_create_device");
      LOAD(device_destroy, "gbm_device_destroy");
      LOAD(bo_create, "gbm_bo_create");
      LOAD(bo_destroy, "gbm_bo_destroy");
      LOAD(bo_get_fd, "gbm_bo_get_fd");
      LOAD(bo_get_stride, "gbm_bo_get_stride");
      LOAD(bo_get_offset, "gbm_bo_get_offset");
      LOAD(bo_get_modifier, "gbm_bo_get_modifier");
      LOAD(bo_get_plane_count, "gbm_bo_get_plane_count");
      LOAD(bo_map, "gbm_bo_map");
      LOAD(bo_unmap, "gbm_bo_unmap");
#undef LOAD
      return result;
    }();
    return gbm;
  }
};

constexpr uint32_t kGbmBoUseRendering = 1 << 2;
constexpr uint32_t kGbmBoTransferWrite = 1 << 1;

// Returns the DRM render node used by |display|, or an empty string.
std::string GetRenderNode(const gl::Api* api, gl::EGLDisplay display) {
  auto* query_display = reinterpret_cast<gl::EGLBoolean (*)(
      gl::EGLDisplay, gl::EGLint, gl::EGLAttrib*)>(
      api->eglGetProcAddress("eglQueryDisplayAttribEXT"));
  auto* query_device = reinterpret_cast<const char* (*)(void*, gl::EGLint)>(
      api->eglGetProcAddress("eglQueryDeviceStringEXT"));
  gl::EGLAttrib device = 0;
  if (!query_display || !query_device ||
      !query_display(display, gl::kEglDeviceExt, &device) || !device) {
    return std::string();
  }
  const char* node = query_device(reinterpret_cast<void*>(device),
                                  gl::kEglDrmRenderNodeFileExt);
  if (!node) {
    node =
        query_device(reinterpret_cast<void*>(device), gl::kEglDrmDeviceFileExt);
  }
  return node ? node : std::string();
}

// A DMA-BUF for testing, allocated via GBM (as Chromium does) or exported from
// a GL texture. Pixels are written as raw memory values (BGRA byte order for
// 0xAARRGGBB constants) regardless of the buffer format.
class DmaBuf {
 public:
  ~DmaBuf() {
    for (int i = 0; i < plane_count_; ++i) {
      if (fds_[i] >= 0) {
        close(fds_[i]);
      }
    }
    if (image_) {
      api_->eglDestroyImageKHR(display_, image_);
    }
    if (texture_) {
      api_->glDeleteTextures(1, &texture_);
    }
    if (bo_) {
      gbm_->bo_destroy(bo_);
    }
    if (device_) {
      gbm_->device_destroy(device_);
    }
    if (device_fd_ >= 0) {
      close(device_fd_);
    }
  }

  // Returns nullptr if a single-plane 32-bit RGBA DMA-BUF cannot be created
  // and imported on the current display.
  static std::unique_ptr<DmaBuf> Create(
      int width,
      int height,
      cef_color_type_t format = CEF_COLOR_TYPE_BGRA_8888) {
    const gl::Api* api = gl::GetApi();
    gl::EGLDisplay display = api->eglGetCurrentDisplay();
    if (!gl::SupportsDmaBufImport(display)) {
      return nullptr;
    }
    const uint32_t fourcc = format == CEF_COLOR_TYPE_RGBA_8888
                                ? gl::kDrmFormatAbgr8888
                                : gl::kDrmFormatArgb8888;
    auto buffer = CreateWithGbm(api, display, width, height, fourcc);
    if (!buffer) {
      buffer = CreateWithExport(api, display, width, height, fourcc);
    }
    if (!buffer || !buffer->ContentsVisibleToImports(fourcc)) {
      return nullptr;
    }
    return buffer;
  }

  void Fill(const std::vector<uint32_t>& pixels) {
    if (bo_) {
      // Write through GBM, which handles any tiled layout.
      uint32_t stride = 0;
      void* map_data = nullptr;
      auto* data = static_cast<uint8_t*>(gbm_->bo_map(
          bo_, 0, 0, width_, height_, kGbmBoTransferWrite, &stride, &map_data));
      ASSERT_NE(nullptr, data);
      for (int y = 0; y < height_; ++y) {
        memcpy(data + y * stride, pixels.data() + y * width_, width_ * 4);
      }
      gbm_->bo_unmap(bo_, map_data);
      return;
    }
    // Render (rather than upload) into the buffer, as Chromium does. The
    // staging upload format matches the buffer's memory order so that memory
    // contains |pixels|.
    gl::GLuint staging = 0;
    api_->glGenTextures(1, &staging);
    api_->glBindTexture(gl::kGlTexture2D, staging);
    api_->glTexImage2D(gl::kGlTexture2D, 0, gl::kGlRgba8, width_, height_, 0,
                       fill_format_, gl::kGlUnsignedByte, pixels.data());
    api_->glBindTexture(gl::kGlTexture2D, 0);
    gl::GLuint framebuffers[2] = {};
    api_->glGenFramebuffers(2, framebuffers);
    api_->glBindFramebuffer(gl::kGlReadFramebuffer, framebuffers[0]);
    api_->glFramebufferTexture2D(gl::kGlReadFramebuffer,
                                 gl::kGlColorAttachment0, gl::kGlTexture2D,
                                 staging, 0);
    api_->glBindFramebuffer(gl::kGlDrawFramebuffer, framebuffers[1]);
    api_->glFramebufferTexture2D(gl::kGlDrawFramebuffer,
                                 gl::kGlColorAttachment0, gl::kGlTexture2D,
                                 texture_, 0);
    api_->glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_,
                            gl::kGlColorBufferBit, gl::kGlNearest);
    api_->glBindFramebuffer(gl::kGlFramebuffer, 0);
    api_->glDeleteFramebuffers(2, framebuffers);
    api_->glDeleteTextures(1, &staging);
    api_->glFinish();
  }

  // Returns paint info for this buffer as CEF would provide it.
  CefAcceleratedPaintInfo Info(cef_color_type_t format,
                               const CefRect& visible) const {
    CefAcceleratedPaintInfo info;
    info.plane_count = plane_count_;
    for (int i = 0; i < plane_count_; ++i) {
      info.planes[i].fd = fds_[i];
      info.planes[i].stride = strides_[i];
      info.planes[i].offset = offsets_[i];
      info.planes[i].size = static_cast<uint64_t>(strides_[i]) * height_;
    }
    info.modifier = modifier_;
    info.format = format;
    info.extra.coded_size = {width_, height_};
    info.extra.visible_rect = {visible.x, visible.y, visible.width,
                               visible.height};
    info.extra.content_rect = info.extra.visible_rect;
    return info;
  }

  int fd() const { return fds_[0]; }

 private:
  DmaBuf(const gl::Api* api, gl::EGLDisplay display, int width, int height)
      : api_(api), display_(display), width_(width), height_(height) {}

  static std::unique_ptr<DmaBuf> CreateWithGbm(const gl::Api* api,
                                               gl::EGLDisplay display,
                                               int width,
                                               int height,
                                               uint32_t fourcc) {
    const Gbm* gbm = Gbm::Get();
    const std::string node = GetRenderNode(api, display);
    if (!gbm || node.empty()) {
      return nullptr;
    }
    auto buffer =
        std::unique_ptr<DmaBuf>(new DmaBuf(api, display, width, height));
    buffer->gbm_ = gbm;
    buffer->device_fd_ = open(node.c_str(), O_RDWR | O_CLOEXEC);
    if (buffer->device_fd_ < 0 ||
        !(buffer->device_ = gbm->create_device(buffer->device_fd_)) ||
        !(buffer->bo_ = gbm->bo_create(buffer->device_, width, height, fourcc,
                                       kGbmBoUseRendering)) ||
        gbm->bo_get_plane_count(buffer->bo_) != 1) {
      return nullptr;
    }
    buffer->plane_count_ = 1;
    buffer->fds_[0] = gbm->bo_get_fd(buffer->bo_);
    buffer->strides_[0] = gbm->bo_get_stride(buffer->bo_);
    buffer->offsets_[0] = gbm->bo_get_offset(buffer->bo_, 0);
    buffer->modifier_ = gbm->bo_get_modifier(buffer->bo_);
    if (buffer->fds_[0] < 0) {
      return nullptr;
    }
    return buffer;
  }

  static std::unique_ptr<DmaBuf> CreateWithExport(const gl::Api* api,
                                                  gl::EGLDisplay display,
                                                  int width,
                                                  int height,
                                                  uint32_t requested_fourcc) {
    if (!api->eglExportDMABUFImageQueryMESA ||
        !gl::HasExtension(display, "EGL_MESA_image_dma_buf_export") ||
        !gl::HasExtension(display, "EGL_KHR_gl_texture_2D_image")) {
      return nullptr;
    }
    auto buffer =
        std::unique_ptr<DmaBuf>(new DmaBuf(api, display, width, height));
    api->glGenTextures(1, &buffer->texture_);
    api->glBindTexture(gl::kGlTexture2D, buffer->texture_);
    api->glTexImage2D(gl::kGlTexture2D, 0, gl::kGlRgba8, width, height, 0,
                      gl::kGlRgba, gl::kGlUnsignedByte, nullptr);
    api->glBindTexture(gl::kGlTexture2D, 0);
    const gl::EGLint attributes[] = {gl::kEglNone};
    buffer->image_ = api->eglCreateImageKHR(
        display, api->eglGetCurrentContext(), gl::kEglGLTexture2D,
        reinterpret_cast<void*>(static_cast<uintptr_t>(buffer->texture_)),
        attributes);
    int fourcc = 0;
    int plane_count = 0;
    gl::EGLint strides[4] = {};
    gl::EGLint offsets[4] = {};
    if (!buffer->image_ ||
        !api->eglExportDMABUFImageQueryMESA(display, buffer->image_, &fourcc,
                                            &plane_count, &buffer->modifier_) ||
        plane_count != 1 || fourcc != static_cast<int>(requested_fourcc) ||
        !api->eglExportDMABUFImageMESA(display, buffer->image_, buffer->fds_,
                                       strides, offsets)) {
      return nullptr;
    }
    buffer->plane_count_ = 1;
    buffer->strides_[0] = static_cast<uint32_t>(strides[0]);
    buffer->offsets_[0] = static_cast<uint32_t>(offsets[0]);
    buffer->fill_format_ = fourcc == static_cast<int>(gl::kDrmFormatArgb8888)
                               ? gl::kGlBgra
                               : gl::kGlRgba;

    return buffer;
  }

  // Returns true if contents written by Fill() are visible to an independent
  // import of the buffer, as they are for buffers written by another process.
  bool ContentsVisibleToImports(uint32_t fourcc) {
    std::vector<uint32_t> pattern(width_ * height_, 0xff336699);
    Fill(pattern);
    const auto info =
        Info(CEF_COLOR_TYPE_BGRA_8888, CefRect(0, 0, width_, height_));
    std::vector<gl::EGLint> attributes = {
        gl::kEglWidth,
        width_,
        gl::kEglHeight,
        height_,
        gl::kEglLinuxDrmFourcc,
        static_cast<gl::EGLint>(fourcc),
        gl::kEglDmaBufPlaneFd[0],
        info.planes[0].fd,
        gl::kEglDmaBufPlaneOffset[0],
        static_cast<gl::EGLint>(info.planes[0].offset),
        gl::kEglDmaBufPlanePitch[0],
        static_cast<gl::EGLint>(info.planes[0].stride)};
    if (modifier_ != gl::kDrmFormatModInvalid &&
        gl::HasExtension(display_, "EGL_EXT_image_dma_buf_import_modifiers")) {
      attributes.insert(attributes.end(),
                        {gl::kEglDmaBufPlaneModifierLo[0],
                         static_cast<gl::EGLint>(modifier_ & 0xffffffff),
                         gl::kEglDmaBufPlaneModifierHi[0],
                         static_cast<gl::EGLint>(modifier_ >> 32)});
    }
    attributes.push_back(gl::kEglNone);
    gl::EGLImage image =
        api_->eglCreateImageKHR(display_, gl::kNoContext, gl::kEglLinuxDmaBuf,
                                nullptr, attributes.data());
    if (!image) {
      return false;
    }
    gl::GLuint texture = 0;
    gl::GLuint framebuffer = 0;
    api_->glGenTextures(1, &texture);
    api_->glBindTexture(gl::kGlTexture2D, texture);
    api_->glEGLImageTargetTexture2DOES(gl::kGlTexture2D, image);
    api_->glBindTexture(gl::kGlTexture2D, 0);
    api_->glGenFramebuffers(1, &framebuffer);
    api_->glBindFramebuffer(gl::kGlFramebuffer, framebuffer);
    api_->glFramebufferTexture2D(gl::kGlFramebuffer, gl::kGlColorAttachment0,
                                 gl::kGlTexture2D, texture, 0);
    // Read back as raw memory order for the buffer's format.
    uint32_t pixel = 0;
    api_->glReadPixels(
        0, 0, 1, 1,
        fourcc == gl::kDrmFormatArgb8888 ? gl::kGlBgra : gl::kGlRgba,
        gl::kGlUnsignedByte, &pixel);
    api_->glBindFramebuffer(gl::kGlFramebuffer, 0);
    api_->glDeleteFramebuffers(1, &framebuffer);
    api_->glDeleteTextures(1, &texture);
    api_->eglDestroyImageKHR(display_, image);
    return api_->glGetError() == gl::kGlNoError && pixel == pattern[0];
  }

  const gl::Api* const api_;
  const gl::EGLDisplay display_;
  const int width_;
  const int height_;
  const Gbm* gbm_ = nullptr;
  int device_fd_ = -1;
  void* device_ = nullptr;
  void* bo_ = nullptr;
  gl::GLuint texture_ = 0;
  gl::EGLImage image_ = gl::kNoImage;
  gl::GLenum fill_format_ = gl::kGlBgra;
  uint64_t modifier_ = gl::kDrmFormatModInvalid;
  int plane_count_ = 0;
  int fds_[4] = {-1, -1, -1, -1};
  uint32_t strides_[4] = {};
  uint32_t offsets_[4] = {};
};

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

class OsrRendererGlTest : public testing::Test {
 public:
  void SetUp() override {
    api_ = gl::GetApi();
    if (!api_) {
      GTEST_SKIP() << "EGL is unavailable";
    }
    display_ = CreateHeadlessDisplay(api_);
    if (!display_ ||
        !gl::HasExtension(display_, "EGL_KHR_surfaceless_context") ||
        !gl::HasExtension(display_, "EGL_KHR_no_config_context")) {
      GTEST_SKIP() << "EGL surfaceless contexts are unavailable";
    }
    context_ = gl::CreateContext(display_, nullptr);
    if (!context_ || !api_->eglMakeCurrent(display_, gl::kNoSurface,
                                           gl::kNoSurface, context_)) {
      GTEST_SKIP() << "OpenGL 3.3 core profile is unavailable";
    }
    renderer_ = std::make_unique<client::OsrRendererGl>(0);
    ASSERT_TRUE(renderer_->Initialize());
  }

  void TearDown() override {
    if (renderer_) {
      renderer_->Cleanup();
      renderer_.reset();
    }
    if (context_) {
      api_->eglMakeCurrent(display_, gl::kNoSurface, gl::kNoSurface,
                           gl::kNoContext);
      api_->eglDestroyContext(display_, context_);
    }
  }

  bool Paint(int width,
             int height,
             const std::vector<uint32_t>& pixels,
             const CefRenderHandler::RectList& dirty,
             CefRenderHandler::PaintElementType type = PET_VIEW) {
    return renderer_->OnPaint(type, dirty, pixels.data(), width, height);
  }

  // Skips the test if DMA-BUF export or import is unavailable.
  std::unique_ptr<DmaBuf> CreateDmaBuf(
      int width,
      int height,
      cef_color_type_t format = CEF_COLOR_TYPE_BGRA_8888) {
    return DmaBuf::Create(width, height, format);
  }

  // Read pixels with a top-left origin from the production render path,
  // including texture uploads, DMA-BUF copies, sampling and blending.
  std::vector<uint32_t> Render(int width, int height) {
    gl::GLuint texture = 0;
    gl::GLuint framebuffer = 0;
    api_->glGenTextures(1, &texture);
    api_->glBindTexture(gl::kGlTexture2D, texture);
    api_->glTexImage2D(gl::kGlTexture2D, 0, gl::kGlRgba8, width, height, 0,
                       gl::kGlBgra, gl::kGlUnsignedInt8888Rev, nullptr);
    api_->glBindTexture(gl::kGlTexture2D, 0);
    api_->glGenFramebuffers(1, &framebuffer);
    api_->glBindFramebuffer(gl::kGlFramebuffer, framebuffer);
    api_->glFramebufferTexture2D(gl::kGlFramebuffer, gl::kGlColorAttachment0,
                                 gl::kGlTexture2D, texture, 0);
    EXPECT_EQ(gl::kGlFramebufferComplete,
              api_->glCheckFramebufferStatus(gl::kGlFramebuffer));
    EXPECT_TRUE(renderer_->Render(framebuffer, width, height));

    std::vector<uint32_t> bottom_up(width * height);
    api_->glBindFramebuffer(gl::kGlFramebuffer, framebuffer);
    api_->glPixelStorei(gl::kGlPackAlignment, 4);
    api_->glReadPixels(0, 0, width, height, gl::kGlBgra,
                       gl::kGlUnsignedInt8888Rev, bottom_up.data());
    api_->glBindFramebuffer(gl::kGlFramebuffer, 0);
    api_->glDeleteFramebuffers(1, &framebuffer);
    api_->glDeleteTextures(1, &texture);
    EXPECT_EQ(gl::kGlNoError, api_->glGetError());

    std::vector<uint32_t> pixels(width * height);
    for (int y = 0; y < height; ++y) {
      std::copy_n(bottom_up.begin() + (height - 1 - y) * width, width,
                  pixels.begin() + y * width);
    }
    return pixels;
  }

 protected:
  const gl::Api* api_ = nullptr;
  gl::EGLDisplay display_ = gl::kNoDisplay;
  gl::EGLContext context_ = gl::kNoContext;
  std::unique_ptr<client::OsrRendererGl> renderer_;
};

#define SKIP_IF_NO_DMA_BUF(buffer)                        \
  if (!(buffer)) {                                        \
    GTEST_SKIP() << "Shareable DMA-BUFs are unavailable"; \
  }

TEST_F(OsrRendererGlTest, DirtyRectanglesAndUploadReuse) {
  const int width = 17;
  const int height = 9;
  std::vector<uint32_t> pixels(width * height, kBlue);
  ASSERT_TRUE(Paint(width, height, pixels, {CefRect(0, 0, width, height)}));
  EXPECT_EQ(pixels, Render(width, height));

  // Update nonzero, unaligned origins repeatedly. Changes outside the dirty
  // rectangles must not reach the destination.
  std::vector<uint32_t> expected = pixels;
  for (int i = 1; i <= 8; ++i) {
    std::fill(pixels.begin(), pixels.end(), kRed);
    const uint32_t color = i % 2 ? kGreen : kWhite;
    pixels[2 * width + i] = color;
    pixels[6 * width + i + 1] = color;
    expected[2 * width + i] = color;
    expected[6 * width + i + 1] = color;
    ASSERT_TRUE(Paint(width, height, pixels,
                      {CefRect(i, 2, 1, 1), CefRect(i + 1, 6, 1, 1)}));
  }
  EXPECT_EQ(expected, Render(width, height));
}

TEST_F(OsrRendererGlTest, ResizeInitializesWholeTexture) {
  ASSERT_TRUE(Paint(8, 8, std::vector<uint32_t>(64, kRed), {}));
  std::vector<uint32_t> resized(13 * 7, kGreen);
  resized.front() = kBlue;
  resized.back() = kRed;
  ASSERT_TRUE(Paint(13, 7, resized, {CefRect(3, 3, 1, 1)}));
  EXPECT_EQ(resized, Render(13, 7));
}

TEST_F(OsrRendererGlTest, PopupHideRestoresUpdatedView) {
  ASSERT_TRUE(Paint(8, 8, std::vector<uint32_t>(64, kBlue), {}));
  renderer_->OnPopupShow(true);
  renderer_->OnPopupSize(CefRect(6, 7, 3, 2));
  EXPECT_EQ(CefRect(5, 6, 3, 2), renderer_->popup_rect());
  EXPECT_EQ(CefRect(6, 7, 3, 2), renderer_->original_popup_rect());
  ASSERT_TRUE(Paint(3, 2, std::vector<uint32_t>(6, kRed), {}, PET_POPUP));
  auto pixels = Render(8, 8);
  EXPECT_EQ(kRed, pixels[6 * 8 + 5]);
  EXPECT_EQ(kBlue, pixels[6 * 8 + 4]);

  ASSERT_TRUE(
      Paint(8, 8, std::vector<uint32_t>(64, kGreen), {CefRect(0, 0, 8, 8)}));
  pixels = Render(8, 8);
  EXPECT_EQ(kRed, pixels[7 * 8 + 7]);
  EXPECT_EQ(kGreen, pixels[0]);
  renderer_->OnPopupShow(false);
  EXPECT_EQ(std::vector<uint32_t>(64, kGreen), Render(8, 8));

  renderer_->OnPopupShow(true);
  renderer_->OnPopupSize(CefRect(0, 0, 3, 2));
  // No old popup contents before the new popup's first paint.
  EXPECT_EQ(std::vector<uint32_t>(64, kGreen), Render(8, 8));
}

TEST_F(OsrRendererGlTest, OversizedPopupAndViewResize) {
  ASSERT_TRUE(Paint(4, 4, std::vector<uint32_t>(16, kBlue), {}));
  renderer_->OnPopupShow(true);
  renderer_->OnPopupSize(CefRect(-3, -2, 6, 5));
  std::vector<uint32_t> popup(30, kRed);
  // The clipped right/bottom pixels must not be squeezed into the view.
  for (int y = 0; y < 5; ++y) {
    popup[y * 6 + 5] = kGreen;
  }
  ASSERT_TRUE(Paint(6, 5, popup, {}, PET_POPUP));
  EXPECT_EQ(std::vector<uint32_t>(16, kRed), Render(4, 4));
  renderer_->OnPopupSize(CefRect(5, 5, 6, 5));
  ASSERT_TRUE(Paint(12, 12, std::vector<uint32_t>(144, kBlue), {}));
  EXPECT_EQ(CefRect(5, 5, 6, 5), renderer_->popup_rect());
}

TEST_F(OsrRendererGlTest, PremultipliedAlphaAndTopLeftOrigin) {
  std::vector<uint32_t> pixels(16,
                               0x80008000);  // Half-alpha premultiplied green.
  pixels[0] = kRed;
  pixels[15] = kBlue;
  ASSERT_TRUE(Paint(4, 4, pixels, {}));
  const auto result = Render(4, 4);
  EXPECT_EQ(kRed, result[0]);
  EXPECT_EQ(kBlue, result[15]);
  EXPECT_EQ(128u, (result[5] >> 8) & 0xff);
  EXPECT_EQ(255u, result[5] >> 24);
  // Background gradient contributes through alpha instead of multiplying
  // the already-premultiplied green by alpha a second time.
  EXPECT_GT(result[5] & 0xff, 0u);
  EXPECT_GT((result[5] >> 16) & 0xff, 0u);
}

TEST_F(OsrRendererGlTest, SpinRedrawWithoutPaint) {
  ASSERT_TRUE(Paint(16, 16, std::vector<uint32_t>(256, kGreen), {}));
  renderer_->SetSpin(0, 60);
  const auto rotated = Render(16, 16);
  EXPECT_NE(kGreen, rotated[0]);
  EXPECT_EQ(kGreen, rotated[8 * 16 + 8]);
  renderer_->SetSpin(0, 0);
  EXPECT_EQ(std::vector<uint32_t>(256, kGreen), Render(16, 16));
}

TEST_F(OsrRendererGlTest, AcceleratedCopyOwnsPixelsAndHonorsFormat) {
  for (bool rgba : {false, true}) {
    const cef_color_type_t format =
        rgba ? CEF_COLOR_TYPE_RGBA_8888 : CEF_COLOR_TYPE_BGRA_8888;
    auto buffer = CreateDmaBuf(8, 8, format);
    SKIP_IF_NO_DMA_BUF(buffer);
    std::vector<uint32_t> pixels(64, kGreen);
    for (int y = 1; y < 7; ++y) {
      // Red inside the visible aperture, green in the surrounding padding.
      std::fill_n(pixels.begin() + y * 8 + 2, 5, rgba ? kBlue : kRed);
    }
    buffer->Fill(pixels);
    ASSERT_TRUE(renderer_->OnAcceleratedPaint(
        PET_VIEW, {}, buffer->Info(format, CefRect(2, 1, 5, 6))));

    // Simulate immediate reuse by CEF's pool before we render anything.
    buffer->Fill(std::vector<uint32_t>(64, kGreen));
    EXPECT_EQ(std::vector<uint32_t>(30, kRed), Render(5, 6));

    renderer_->OnPopupShow(true);
    renderer_->OnPopupSize(CefRect(3, 4, 2, 2));
    ASSERT_TRUE(renderer_->OnAcceleratedPaint(
        PET_POPUP, {}, buffer->Info(format, CefRect(2, 1, 2, 2))));
    buffer.reset();
    const auto with_popup = Render(5, 6);
    EXPECT_EQ(kRed, with_popup[0]);
    EXPECT_EQ(kGreen, with_popup[4 * 5 + 3]);
    EXPECT_EQ(kGreen, with_popup[5 * 5 + 4]);
    renderer_->OnPopupShow(false);
    EXPECT_EQ(std::vector<uint32_t>(30, kRed), Render(5, 6));
  }
}

TEST_F(OsrRendererGlTest, CleanupWithUploadsInFlight) {
  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(Paint(256, 256, std::vector<uint32_t>(256 * 256, kBlue),
                      {CefRect(0, 0, 256, 256)}));
  }
  renderer_->Cleanup();
  renderer_->Cleanup();
  ASSERT_TRUE(renderer_->Initialize());
  ASSERT_TRUE(Paint(4, 4, std::vector<uint32_t>(16, kRed), {}));
  EXPECT_EQ(std::vector<uint32_t>(16, kRed), Render(4, 4));
}

TEST_F(OsrRendererGlTest, AcceleratedReusedBufferShowsNewContents) {
  auto first = CreateDmaBuf(4, 4);
  SKIP_IF_NO_DMA_BUF(first);
  auto second = CreateDmaBuf(4, 4);
  SKIP_IF_NO_DMA_BUF(second);
  const CefRect visible(0, 0, 4, 4);

  first->Fill(std::vector<uint32_t>(16, kRed));
  ASSERT_TRUE(renderer_->OnAcceleratedPaint(
      PET_VIEW, {}, first->Info(CEF_COLOR_TYPE_BGRA_8888, visible)));
  EXPECT_EQ(std::vector<uint32_t>(16, kRed), Render(4, 4));

  second->Fill(std::vector<uint32_t>(16, kBlue));
  ASSERT_TRUE(renderer_->OnAcceleratedPaint(
      PET_VIEW, {}, second->Info(CEF_COLOR_TYPE_BGRA_8888, visible)));
  EXPECT_EQ(std::vector<uint32_t>(16, kBlue), Render(4, 4));

  // CEF's pool reuses buffers, possibly with a different fd for the same
  // buffer. New contents must always be displayed.
  auto info = first->Info(CEF_COLOR_TYPE_BGRA_8888, visible);
  info.planes[0].fd = dup(first->fd());
  ASSERT_GE(info.planes[0].fd, 0);
  first->Fill(std::vector<uint32_t>(16, kGreen));
  ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, info));
  close(info.planes[0].fd);
  EXPECT_EQ(std::vector<uint32_t>(16, kGreen), Render(4, 4));
}

TEST_F(OsrRendererGlTest, AcceleratedDoesNotTakeFdOwnership) {
  auto buffer = CreateDmaBuf(4, 4);
  SKIP_IF_NO_DMA_BUF(buffer);
  buffer->Fill(std::vector<uint32_t>(16, kBlue));
  const int fd = dup(buffer->fd());
  ASSERT_GE(fd, 0);
  auto info = buffer->Info(CEF_COLOR_TYPE_BGRA_8888, CefRect(0, 0, 4, 4));
  info.planes[0].fd = fd;
  ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, info));
  EXPECT_NE(-1, fcntl(fd, F_GETFD));

  // The copied frame does not depend on the fd or buffer after the callback.
  close(fd);
  buffer.reset();
  EXPECT_EQ(std::vector<uint32_t>(16, kBlue), Render(4, 4));
}

TEST_F(OsrRendererGlTest, AcceleratedRejectsInvalidInput) {
  auto buffer = CreateDmaBuf(4, 4);
  SKIP_IF_NO_DMA_BUF(buffer);
  buffer->Fill(std::vector<uint32_t>(16, kBlue));
  const CefRect visible(0, 0, 4, 4);
  ASSERT_TRUE(renderer_->OnAcceleratedPaint(
      PET_VIEW, {}, buffer->Info(CEF_COLOR_TYPE_BGRA_8888, visible)));
  buffer->Fill(std::vector<uint32_t>(16, kRed));

  auto info = buffer->Info(static_cast<cef_color_type_t>(-1), visible);
  EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, info));

  for (int plane_count : {0, kAcceleratedPaintMaxPlanes + 1}) {
    info = buffer->Info(CEF_COLOR_TYPE_BGRA_8888, visible);
    info.plane_count = plane_count;
    EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, info));
  }

  info = buffer->Info(CEF_COLOR_TYPE_BGRA_8888, visible);
  info.planes[0].fd = -1;
  EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, info));

  info = buffer->Info(CEF_COLOR_TYPE_BGRA_8888, CefRect(2, 2, 4, 4));
  EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, info));

  info = buffer->Info(CEF_COLOR_TYPE_BGRA_8888, CefRect());
  EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, info));

  // Popups are ignored while hidden.
  info = buffer->Info(CEF_COLOR_TYPE_BGRA_8888, visible);
  EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_POPUP, {}, info));

  // The previous frame is retained.
  EXPECT_EQ(std::vector<uint32_t>(16, kBlue), Render(4, 4));
}

TEST_F(OsrRendererGlTest, PaintAndAcceleratedPaintInterleave) {
  auto buffer = CreateDmaBuf(4, 4);
  SKIP_IF_NO_DMA_BUF(buffer);
  ASSERT_TRUE(Paint(4, 4, std::vector<uint32_t>(16, kBlue), {}));
  buffer->Fill(std::vector<uint32_t>(16, kRed));
  ASSERT_TRUE(renderer_->OnAcceleratedPaint(
      PET_VIEW, {},
      buffer->Info(CEF_COLOR_TYPE_BGRA_8888, CefRect(0, 0, 4, 4))));
  EXPECT_EQ(std::vector<uint32_t>(16, kRed), Render(4, 4));

  // A partial software update applies on top of the accelerated frame.
  std::vector<uint32_t> pixels(16, kGreen);
  ASSERT_TRUE(Paint(4, 4, pixels, {CefRect(1, 1, 1, 1)}));
  std::vector<uint32_t> expected(16, kRed);
  expected[5] = kGreen;
  EXPECT_EQ(expected, Render(4, 4));
}

}  // namespace
