// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/osr_renderer_gl_linux.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "include/base/cef_logging.h"
#include "tests/shared/browser/osr_gl_linux.h"

namespace client {

namespace {

const char kVertexShader[] = R"(#version 330 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 color;
out vec2 v_uv;
out vec4 v_color;
void main() {
  gl_Position = vec4(position, 0.0, 1.0);
  v_uv = uv;
  v_color = color;
}
)";

const char kFragmentShader[] = R"(#version 330 core
in vec2 v_uv;
in vec4 v_color;
out vec4 frag_color;
uniform sampler2D image;
uniform bool use_texture;
void main() {
  frag_color = use_texture ? texture(image, v_uv) * v_color : v_color;
}
)";

CefRect Intersect(const CefRect& a, const CefRect& b) {
  const int x = std::max(a.x, b.x);
  const int y = std::max(a.y, b.y);
  const int right = std::min(a.x + a.width, b.x + b.width);
  const int bottom = std::min(a.y + a.height, b.y + b.height);
  return CefRect(x, y, std::max(0, right - x), std::max(0, bottom - y));
}

struct Vertex {
  float position[2];
  float uv[2];
  float color[4];
};

gl::GLuint CompileShader(const gl::Api& api,
                         gl::GLenum type,
                         const char* source) {
  gl::GLuint shader = api.glCreateShader(type);
  api.glShaderSource(shader, 1, &source, nullptr);
  api.glCompileShader(shader);
  gl::GLint success = 0;
  api.glGetShaderiv(shader, gl::kGlCompileStatus, &success);
  if (!success) {
    char log[512] = {};
    api.glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    LOG(ERROR) << "OSR: Shader compile error: " << log;
    api.glDeleteShader(shader);
    return 0;
  }
  return shader;
}

}  // namespace

class OsrRendererGl::Impl {
 public:
  struct Image {
    gl::GLuint texture = 0;
    int width = 0;
    int height = 0;
    bool valid = false;
  };

  explicit Impl(const gl::Api* api) : api(api) {}

  ~Impl() {
    if (program || view.texture || popup.texture) {
      LOG(WARNING) << "OSR: OsrRendererGl destroyed without Cleanup()";
    }
  }

  bool EnsureTexture(Image& image, int width, int height) {
    if (width <= 0 || height <= 0) {
      return false;
    }
    if (image.texture && image.width == width && image.height == height) {
      return true;
    }
    image.valid = false;
    if (!image.texture) {
      api->glGenTextures(1, &image.texture);
    }
    api->glBindTexture(gl::kGlTexture2D, image.texture);
    api->glTexParameteri(gl::kGlTexture2D, gl::kGlTextureMinFilter,
                         gl::kGlLinear);
    api->glTexParameteri(gl::kGlTexture2D, gl::kGlTextureMagFilter,
                         gl::kGlLinear);
    api->glTexParameteri(gl::kGlTexture2D, gl::kGlTextureWrapS,
                         gl::kGlClampToEdge);
    api->glTexParameteri(gl::kGlTexture2D, gl::kGlTextureWrapT,
                         gl::kGlClampToEdge);
    api->glTexImage2D(gl::kGlTexture2D, 0, gl::kGlRgba8, width, height, 0,
                      gl::kGlBgra, gl::kGlUnsignedInt8888Rev, nullptr);
    api->glBindTexture(gl::kGlTexture2D, 0);
    if (api->glGetError() != gl::kGlNoError) {
      return false;
    }
    image.width = width;
    image.height = height;
    return true;
  }

  // Imports the DMA-BUF described by |info| as a texture. Returns false on
  // failure. The import is released after each paint; CEF's buffers cannot be
  // reliably identified across callbacks, and EGL retains the buffer for as
  // long as the image exists.
  bool ImportTexture(gl::EGLDisplay display,
                     const CefAcceleratedPaintInfo& info,
                     uint32_t fourcc,
                     gl::EGLImage* image,
                     gl::GLuint* texture) {
    std::vector<gl::EGLint> attributes = {
        gl::kEglWidth,          info.extra.coded_size.width,
        gl::kEglHeight,         info.extra.coded_size.height,
        gl::kEglLinuxDrmFourcc, static_cast<gl::EGLint>(fourcc)};
    // Modifier attributes are only valid with
    // EGL_EXT_image_dma_buf_import_modifiers. Without it, the driver uses the
    // buffer's implicit layout.
    if (display != modifiers_display) {
      modifiers_display = display;
      supports_modifiers =
          gl::HasExtension(display, "EGL_EXT_image_dma_buf_import_modifiers");
    }
    const bool with_modifier =
        supports_modifiers && info.modifier != gl::kDrmFormatModInvalid;
    for (int i = 0; i < info.plane_count; ++i) {
      attributes.insert(attributes.end(),
                        {gl::kEglDmaBufPlaneFd[i], info.planes[i].fd,
                         gl::kEglDmaBufPlaneOffset[i],
                         static_cast<gl::EGLint>(info.planes[i].offset),
                         gl::kEglDmaBufPlanePitch[i],
                         static_cast<gl::EGLint>(info.planes[i].stride)});
      if (with_modifier) {
        attributes.insert(attributes.end(),
                          {gl::kEglDmaBufPlaneModifierLo[i],
                           static_cast<gl::EGLint>(info.modifier & 0xffffffff),
                           gl::kEglDmaBufPlaneModifierHi[i],
                           static_cast<gl::EGLint>(info.modifier >> 32)});
      }
    }
    attributes.push_back(gl::kEglNone);

    // EGL keeps its own reference to the buffer; the fds are not retained.
    *image =
        api->eglCreateImageKHR(display, gl::kNoContext, gl::kEglLinuxDmaBuf,
                               nullptr, attributes.data());
    if (!*image) {
      LOG(ERROR) << "OSR: Failed to import DMA-BUF: 0x" << std::hex
                 << api->eglGetError();
      return false;
    }
    api->glGenTextures(1, texture);
    api->glBindTexture(gl::kGlTexture2D, *texture);
    api->glTexParameteri(gl::kGlTexture2D, gl::kGlTextureMinFilter,
                         gl::kGlNearest);
    api->glTexParameteri(gl::kGlTexture2D, gl::kGlTextureMagFilter,
                         gl::kGlNearest);
    api->glEGLImageTargetTexture2DOES(gl::kGlTexture2D, *image);
    api->glBindTexture(gl::kGlTexture2D, 0);
    if (api->glGetError() != gl::kGlNoError) {
      LOG(ERROR) << "OSR: Failed to bind imported DMA-BUF";
      ReleaseImport(display, *image, *texture);
      return false;
    }
    return true;
  }

  void ReleaseImport(gl::EGLDisplay display,
                     gl::EGLImage image,
                     gl::GLuint texture) {
    if (texture) {
      api->glDeleteTextures(1, &texture);
    }
    if (image) {
      api->eglDestroyImageKHR(display, image);
    }
  }

  // Copies |source| from |source_texture| (of |source_width| x
  // |source_height|) into |dest|, waiting for the copy to complete. Samples
  // the imported texture in a shader, which is the most widely supported way
  // to read DMA-BUF imports.
  bool Copy(gl::GLuint source_texture,
            int source_width,
            int source_height,
            const CefRect& source,
            Image& dest) {
    gl::GLint framebuffer_binding = 0;
    gl::GLint viewport[4] = {};
    api->glGetIntegerv(gl::kGlFramebufferBinding, &framebuffer_binding);
    api->glGetIntegerv(gl::kGlViewport, viewport);

    gl::GLuint framebuffer = 0;
    api->glGenFramebuffers(1, &framebuffer);
    api->glBindFramebuffer(gl::kGlFramebuffer, framebuffer);
    api->glFramebufferTexture2D(gl::kGlFramebuffer, gl::kGlColorAttachment0,
                                gl::kGlTexture2D, dest.texture, 0);
    const bool success = api->glCheckFramebufferStatus(gl::kGlFramebuffer) ==
                         gl::kGlFramebufferComplete;
    if (success) {
      api->glViewport(0, 0, source.width, source.height);
      api->glDisable(gl::kGlScissorTest);
      api->glDisable(gl::kGlBlend);
      api->glUseProgram(program);
      api->glBindVertexArray(vertex_array);
      api->glBindBuffer(gl::kGlArrayBuffer, vertex_buffer);
      api->glActiveTexture(gl::kGlTexture0);
      UseTexture(source_texture);
      // Framebuffer row 0 (NDC y = -1) is the first (top) row of the image.
      const float u0 = source.x / static_cast<float>(source_width);
      const float u1 =
          (source.x + source.width) / static_cast<float>(source_width);
      const float v0 = source.y / static_cast<float>(source_height);
      const float v1 =
          (source.y + source.height) / static_cast<float>(source_height);
      Vertex vertices[] = {
          {{-1, -1}, {u0, v0}, {1, 1, 1, 1}}, {{-1, 1}, {u0, v1}, {1, 1, 1, 1}},
          {{1, -1}, {u1, v0}, {1, 1, 1, 1}},  {{1, -1}, {u1, v0}, {1, 1, 1, 1}},
          {{-1, 1}, {u0, v1}, {1, 1, 1, 1}},  {{1, 1}, {u1, v1}, {1, 1, 1, 1}},
      };
      api->glBufferData(gl::kGlArrayBuffer, sizeof(vertices), vertices,
                        gl::kGlStreamDraw);
      api->glDrawArrays(gl::kGlTriangles, 0, 6);
      api->glBindTexture(gl::kGlTexture2D, 0);
      api->glBindBuffer(gl::kGlArrayBuffer, 0);
      api->glBindVertexArray(0);
      api->glUseProgram(0);

      // CEF reuses the buffer after the callback returns.
      gl::GLsync sync = api->glFenceSync(gl::kGlSyncGpuCommandsComplete, 0);
      const gl::GLenum result =
          sync ? api->glClientWaitSync(sync, gl::kGlSyncFlushCommandsBit,
                                       1000000000ULL)
               : gl::kGlWaitFailed;
      if (sync) {
        api->glDeleteSync(sync);
      }
      if (result == gl::kGlTimeoutExpired || result == gl::kGlWaitFailed) {
        api->glFinish();
      }
    } else {
      LOG(ERROR) << "OSR: Incomplete framebuffer for DMA-BUF copy";
    }

    api->glBindFramebuffer(gl::kGlFramebuffer, framebuffer_binding);
    api->glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    api->glDeleteFramebuffers(1, &framebuffer);
    return success && api->glGetError() == gl::kGlNoError;
  }

  void UpdateViewSize(int width, int height) {
    if (view_width != width || view_height != height) {
      view_width = width;
      view_height = height;
      UpdatePopupRect();
    }
  }

  void UpdatePopupRect() {
    popup_rect = original_popup_rect;
    popup_rect.x =
        std::max(0, std::min(popup_rect.x, view_width - popup_rect.width));
    popup_rect.y =
        std::max(0, std::min(popup_rect.y, view_height - popup_rect.height));
  }

  void Draw(float left,
            float top,
            float right,
            float bottom,
            float u,
            float v,
            bool rotate,
            bool gradient = false,
            bool red = false) {
    const float top_red = gradient ? 0.f : 1.f;
    const float bottom_blue = gradient ? 0.f : 1.f;
    const float green = (gradient || red) ? 0.f : 1.f;
    const float blue = red ? 0.f : 1.f;
    Vertex vertices[] = {
        {{left, top}, {0, 0}, {top_red, green, blue, 1}},
        {{left, bottom}, {0, v}, {1, green, red ? 0.f : bottom_blue, 1}},
        {{right, top}, {u, 0}, {top_red, green, blue, 1}},
        {{right, top}, {u, 0}, {top_red, green, blue, 1}},
        {{left, bottom}, {0, v}, {1, green, red ? 0.f : bottom_blue, 1}},
        {{right, bottom}, {u, v}, {1, green, red ? 0.f : bottom_blue, 1}},
    };
    if (rotate) {
      const float rx = -spin_x * 3.14159265358979323846f / 180.f;
      const float ry = -spin_y * 3.14159265358979323846f / 180.f;
      for (auto& vertex : vertices) {
        const float x = vertex.position[0];
        const float y = vertex.position[1];
        vertex.position[0] = x * std::cos(ry);
        vertex.position[1] = y * std::cos(rx) + x * std::sin(ry) * std::sin(rx);
      }
    }
    api->glBufferData(gl::kGlArrayBuffer, sizeof(vertices), vertices,
                      gl::kGlStreamDraw);
    api->glDrawArrays(gl::kGlTriangles, 0, 6);
  }

  void UseTexture(gl::GLuint texture) {
    api->glUniform1i(use_texture_location, texture ? 1 : 0);
    api->glBindTexture(gl::kGlTexture2D, texture);
  }

  const gl::Api* const api;
  gl::GLuint program = 0;
  gl::GLuint vertex_array = 0;
  gl::GLuint vertex_buffer = 0;
  gl::GLint use_texture_location = -1;
  Image view;
  Image popup;
  int view_width = 0;
  int view_height = 0;
  bool popup_visible = false;
  CefRect original_popup_rect;
  CefRect popup_rect;
  CefRenderHandler::RectList update_rects;
  float spin_x = 0;
  float spin_y = 0;
  gl::EGLDisplay modifiers_display = gl::kNoDisplay;
  bool supports_modifiers = false;
};

OsrRendererGl::OsrRendererGl(cef_color_t background_color,
                             bool show_update_rect)
    : background_color_(background_color),
      show_update_rect_(show_update_rect),
      impl_(std::make_unique<Impl>(gl::GetApi())) {}

OsrRendererGl::~OsrRendererGl() = default;

bool OsrRendererGl::Initialize() {
  auto& state = *impl_;
  if (state.program) {
    return true;
  }
  const gl::Api* api = state.api;
  if (!api) {
    LOG(ERROR) << "OSR: EGL/OpenGL is unavailable";
    return false;
  }
  if (!api->eglGetCurrentContext()) {
    LOG(ERROR) << "OSR: No current EGL context";
    return false;
  }

  const gl::GLuint vertex_shader =
      CompileShader(*api, gl::kGlVertexShader, kVertexShader);
  const gl::GLuint fragment_shader =
      CompileShader(*api, gl::kGlFragmentShader, kFragmentShader);
  if (!vertex_shader || !fragment_shader) {
    api->glDeleteShader(vertex_shader);
    api->glDeleteShader(fragment_shader);
    return false;
  }
  const gl::GLuint program = api->glCreateProgram();
  api->glAttachShader(program, vertex_shader);
  api->glAttachShader(program, fragment_shader);
  api->glLinkProgram(program);
  api->glDeleteShader(vertex_shader);
  api->glDeleteShader(fragment_shader);
  gl::GLint success = 0;
  api->glGetProgramiv(program, gl::kGlLinkStatus, &success);
  if (!success) {
    char log[512] = {};
    api->glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    LOG(ERROR) << "OSR: Shader program link error: " << log;
    api->glDeleteProgram(program);
    return false;
  }
  state.program = program;
  state.use_texture_location =
      api->glGetUniformLocation(program, "use_texture");
  api->glUseProgram(program);
  api->glUniform1i(api->glGetUniformLocation(program, "image"), 0);
  api->glUseProgram(0);

  api->glGenVertexArrays(1, &state.vertex_array);
  api->glGenBuffers(1, &state.vertex_buffer);
  api->glBindVertexArray(state.vertex_array);
  api->glBindBuffer(gl::kGlArrayBuffer, state.vertex_buffer);
  api->glVertexAttribPointer(
      0, 2, gl::kGlFloat, 0, sizeof(Vertex),
      reinterpret_cast<void*>(offsetof(Vertex, position)));
  api->glVertexAttribPointer(1, 2, gl::kGlFloat, 0, sizeof(Vertex),
                             reinterpret_cast<void*>(offsetof(Vertex, uv)));
  api->glVertexAttribPointer(2, 4, gl::kGlFloat, 0, sizeof(Vertex),
                             reinterpret_cast<void*>(offsetof(Vertex, color)));
  api->glEnableVertexAttribArray(0);
  api->glEnableVertexAttribArray(1);
  api->glEnableVertexAttribArray(2);
  api->glBindVertexArray(0);
  api->glBindBuffer(gl::kGlArrayBuffer, 0);

  if (api->glGetError() != gl::kGlNoError) {
    Cleanup();
    return false;
  }
  return true;
}

void OsrRendererGl::Cleanup() {
  auto& state = *impl_;
  const gl::Api* api = state.api;
  if (api && api->eglGetCurrentContext()) {
    for (auto* image : {&state.view, &state.popup}) {
      if (image->texture) {
        api->glDeleteTextures(1, &image->texture);
      }
    }
    if (state.vertex_buffer) {
      api->glDeleteBuffers(1, &state.vertex_buffer);
    }
    if (state.vertex_array) {
      api->glDeleteVertexArrays(1, &state.vertex_array);
    }
    if (state.program) {
      api->glDeleteProgram(state.program);
    }
  } else if (state.program) {
    LOG(ERROR) << "OSR: OsrRendererGl::Cleanup() without a current context";
  }
  // Reset all state, including sizes and popups.
  impl_ = std::make_unique<Impl>(api);
}

bool OsrRendererGl::OnPaint(CefRenderHandler::PaintElementType type,
                            const CefRenderHandler::RectList& dirty_rects,
                            const void* buffer,
                            int width,
                            int height) {
  auto& state = *impl_;
  if (!state.program || !buffer || width <= 0 || height <= 0 ||
      (type == PET_POPUP && !state.popup_visible)) {
    return false;
  }
  auto& image = type == PET_VIEW ? state.view : state.popup;
  if (!state.EnsureTexture(image, width, height)) {
    return false;
  }
  const CefRect bounds(0, 0, width, height);
  CefRenderHandler::RectList updates = dirty_rects;
  if (!image.valid) {
    updates = {bounds};
  }

  const gl::Api* api = state.api;
  api->glBindTexture(gl::kGlTexture2D, image.texture);
  api->glPixelStorei(gl::kGlUnpackAlignment, 4);
  api->glPixelStorei(gl::kGlUnpackRowLength, width);
  for (const auto& dirty : updates) {
    const auto rect = Intersect(dirty, bounds);
    if (rect.IsEmpty()) {
      continue;
    }
    api->glPixelStorei(gl::kGlUnpackSkipPixels, rect.x);
    api->glPixelStorei(gl::kGlUnpackSkipRows, rect.y);
    api->glTexSubImage2D(gl::kGlTexture2D, 0, rect.x, rect.y, rect.width,
                         rect.height, gl::kGlBgra, gl::kGlUnsignedInt8888Rev,
                         buffer);
  }
  api->glPixelStorei(gl::kGlUnpackRowLength, 0);
  api->glPixelStorei(gl::kGlUnpackSkipPixels, 0);
  api->glPixelStorei(gl::kGlUnpackSkipRows, 0);
  api->glBindTexture(gl::kGlTexture2D, 0);
  image.valid = api->glGetError() == gl::kGlNoError;
  if (type == PET_VIEW && image.valid) {
    state.UpdateViewSize(width, height);
    state.update_rects = dirty_rects;
  }
  return image.valid;
}

bool OsrRendererGl::OnAcceleratedPaint(
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirty_rects,
    const CefAcceleratedPaintInfo& info) {
  auto& state = *impl_;
  const gl::Api* api = state.api;
  if (!state.program || (type == PET_POPUP && !state.popup_visible)) {
    return false;
  }
  uint32_t fourcc;
  switch (info.format) {
    case CEF_COLOR_TYPE_BGRA_8888:
      fourcc = gl::kDrmFormatArgb8888;
      break;
    case CEF_COLOR_TYPE_RGBA_8888:
      fourcc = gl::kDrmFormatAbgr8888;
      break;
    default:
      LOG(ERROR) << "OSR: Unsupported DMA-BUF color format: " << info.format;
      return false;
  }
  if (info.plane_count < 1 || info.plane_count > kAcceleratedPaintMaxPlanes) {
    LOG(ERROR) << "OSR: Invalid DMA-BUF plane count: " << info.plane_count;
    return false;
  }
  for (int i = 0; i < info.plane_count; ++i) {
    if (info.planes[i].fd < 0) {
      LOG(ERROR) << "OSR: Invalid DMA-BUF file descriptor";
      return false;
    }
  }
  const int width = info.extra.coded_size.width;
  const int height = info.extra.coded_size.height;
  const CefRect visible = info.extra.visible_rect;
  if (width <= 0 || height <= 0 || visible.IsEmpty() ||
      Intersect(visible, CefRect(0, 0, width, height)) != visible) {
    LOG(ERROR) << "OSR: Invalid DMA-BUF dimensions";
    return false;
  }
  const gl::EGLDisplay display = api->eglGetCurrentDisplay();
  if (!display) {
    LOG(ERROR) << "OSR: No current EGL display";
    return false;
  }

  auto& image = type == PET_VIEW ? state.view : state.popup;
  if (!state.EnsureTexture(image, visible.width, visible.height)) {
    return false;
  }
  gl::EGLImage source_image = gl::kNoImage;
  gl::GLuint source = 0;
  if (!state.ImportTexture(display, info, fourcc, &source_image, &source)) {
    return false;
  }
  // Copy the entire visible image. This also handles skipped capture counters
  // and buffers recycled by CEF's frame pool without stale dirty regions.
  image.valid = state.Copy(source, width, height, visible, image);
  state.ReleaseImport(display, source_image, source);
  if (type == PET_VIEW && image.valid) {
    state.UpdateViewSize(visible.width, visible.height);
    state.update_rects.clear();
    for (const auto& dirty : dirty_rects) {
      const auto rect = Intersect(dirty, visible);
      if (!rect.IsEmpty()) {
        state.update_rects.emplace_back(rect.x - visible.x, rect.y - visible.y,
                                        rect.width, rect.height);
      }
    }
  }
  return image.valid;
}

void OsrRendererGl::OnPopupShow(bool show) {
  impl_->popup_visible = show;
  // A newly shown popup must not display pixels from the previous popup.
  impl_->popup.valid = false;
  if (!show) {
    impl_->original_popup_rect = CefRect();
    impl_->popup_rect = CefRect();
  }
}

void OsrRendererGl::OnPopupSize(const CefRect& rect) {
  if (rect.width != impl_->original_popup_rect.width ||
      rect.height != impl_->original_popup_rect.height) {
    impl_->popup.valid = false;
  }
  impl_->original_popup_rect = rect;
  impl_->UpdatePopupRect();
}

CefRect OsrRendererGl::popup_rect() const {
  return impl_->popup_rect;
}

CefRect OsrRendererGl::original_popup_rect() const {
  return impl_->original_popup_rect;
}

void OsrRendererGl::SetSpin(float spin_x, float spin_y) {
  impl_->spin_x = spin_x;
  impl_->spin_y = spin_y;
}

void OsrRendererGl::IncrementSpin(float spin_dx, float spin_dy) {
  impl_->spin_x -= spin_dx;
  impl_->spin_y -= spin_dy;
}

bool OsrRendererGl::Render(unsigned int framebuffer, int width, int height) {
  auto& state = *impl_;
  const gl::Api* api = state.api;
  if (!state.program || width <= 0 || height <= 0) {
    return false;
  }
  api->glBindFramebuffer(gl::kGlFramebuffer, framebuffer);
  api->glViewport(0, 0, width, height);
  api->glDisable(gl::kGlScissorTest);
  api->glClearColor(CefColorGetR(background_color_) / 255.f,
                    CefColorGetG(background_color_) / 255.f,
                    CefColorGetB(background_color_) / 255.f, 1.f);
  api->glClear(gl::kGlColorBufferBit);

  if (state.view.valid) {
    api->glUseProgram(state.program);
    api->glBindVertexArray(state.vertex_array);
    api->glBindBuffer(gl::kGlArrayBuffer, state.vertex_buffer);
    api->glActiveTexture(gl::kGlTexture0);
    // Texture values have premultiplied alpha.
    api->glEnable(gl::kGlBlend);
    api->glBlendFunc(gl::kGlOne, gl::kGlOneMinusSrcAlpha);

    // Preserve cefclient's gradient behind transparent/rotated browser content.
    state.UseTexture(0);
    state.Draw(-1, 1, 1, -1, 1, 1, false, true);
    state.UseTexture(state.view.texture);
    state.Draw(-1, 1, 1, -1, 1, 1, true);

    const float sx = 2.f / state.view_width;
    const float sy = 2.f / state.view_height;
    if (state.popup_visible && state.popup.valid) {
      CefRect rect = state.popup_rect;
      rect.width = std::min(rect.width, state.popup.width);
      rect.height = std::min(rect.height, state.popup.height);
      rect =
          Intersect(rect, CefRect(0, 0, state.view_width, state.view_height));
      if (!rect.IsEmpty()) {
        state.UseTexture(state.popup.texture);
        state.Draw(rect.x * sx - 1, 1 - rect.y * sy,
                   (rect.x + rect.width) * sx - 1,
                   1 - (rect.y + rect.height) * sy,
                   rect.width / static_cast<float>(state.popup.width),
                   rect.height / static_cast<float>(state.popup.height), true);
      }
    }
    if (show_update_rect_) {
      state.UseTexture(0);
      for (const auto& dirty : state.update_rects) {
        const auto rect = Intersect(
            dirty, CefRect(0, 0, state.view_width, state.view_height));
        if (rect.IsEmpty()) {
          continue;
        }
        const float l = rect.x * sx - 1;
        const float r = (rect.x + rect.width) * sx - 1;
        const float t = 1 - rect.y * sy;
        const float b = 1 - (rect.y + rect.height) * sy;
        state.Draw(l, t, r, t - sy, 1, 1, false, false, true);
        state.Draw(l, b + sy, r, b, 1, 1, false, false, true);
        state.Draw(l, t, l + sx, b, 1, 1, false, false, true);
        state.Draw(r - sx, t, r, b, 1, 1, false, false, true);
      }
    }

    api->glDisable(gl::kGlBlend);
    api->glBindTexture(gl::kGlTexture2D, 0);
    api->glBindBuffer(gl::kGlArrayBuffer, 0);
    api->glBindVertexArray(0);
    api->glUseProgram(0);
  }
  return api->glGetError() == gl::kGlNoError;
}

}  // namespace client
