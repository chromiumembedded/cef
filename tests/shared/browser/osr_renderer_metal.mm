// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/osr_renderer_metal.h"

#import <IOSurface/IOSurface.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "include/base/cef_logging.h"

namespace client {
namespace {

// Keep the sample and binary distribution independent of a metallib build step.
const char kShaders[] = R"(
#include <metal_stdlib>
using namespace metal;
struct Vertex {
  float2 position;
  float2 uv;
  float4 color;
};
struct Varying {
  float4 position [[position]];
  float2 uv;
  float4 color;
};
vertex Varying vertex_main(const device Vertex* vertices [[buffer(0)]],
                          uint index [[vertex_id]]) {
  Vertex v = vertices[index];
  return {float4(v.position, 0, 1), v.uv, v.color};
}
fragment float4 fragment_texture(Varying v [[stage_in]],
                                texture2d<float> image [[texture(0)]]) {
  constexpr sampler s(coord::normalized, address::clamp_to_edge,
                      filter::linear);
  return image.sample(s, v.uv) * v.color;
}
fragment float4 fragment_color(Varying v [[stage_in]]) {
  return v.color;
}
)";

// The distributed sample supports both ARC and manual reference counting.
template <typename T>
void Release(T& object) {
#if !__has_feature(objc_arc)
  [object release];
#endif
  object = nil;
}

template <typename T>
T Retain(T object) {
#if !__has_feature(objc_arc)
  [object retain];
#endif
  return object;
}

bool Completed(id<MTLCommandBuffer> command) {
  [command waitUntilCompleted];
  if (command.status != MTLCommandBufferStatusCompleted) {
    LOG(ERROR) << "Metal command failed: "
               << command.error.localizedDescription.UTF8String;
    return false;
  }
  return true;
}

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

}  // namespace

class OsrRendererMetal::Impl {
 public:
  ~Impl() {
    // Commands retain the resources they reference. Drain before releasing our
    // last references, including upload buffers that the CPU will otherwise
    // use.
    if (last_command) {
      Completed(last_command);
    }
    for (auto& upload : uploads) {
      Release(upload.command);
      Release(upload.buffer);
    }
    Release(last_command);
    Release(view.texture);
    Release(popup.texture);
    Release(texture_pipeline);
    Release(color_pipeline);
    Release(queue);
    Release(device);
  }

  struct Image {
    id<MTLTexture> texture = nil;
    bool valid = false;
  };

  struct Upload {
    id<MTLBuffer> buffer = nil;
    id<MTLCommandBuffer> command = nil;
  };

  bool EnsureTexture(Image& image,
                     int width,
                     int height,
                     MTLPixelFormat format) {
    if (width <= 0 || height <= 0) {
      return false;
    }
    if (image.texture &&
        image.texture.width == static_cast<NSUInteger>(width) &&
        image.texture.height == static_cast<NSUInteger>(height) &&
        image.texture.pixelFormat == format) {
      return true;
    }
    image.valid = false;
    Release(image.texture);
    auto* descriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                           width:width
                                                          height:height
                                                       mipmapped:NO];
    descriptor.storageMode = MTLStorageModePrivate;
    descriptor.usage = MTLTextureUsageShaderRead;
    image.texture = [device newTextureWithDescriptor:descriptor];
    return image.texture != nil;
  }

  void Commit(id<MTLCommandBuffer> command) {
    [command commit];
    Release(last_command);
    last_command = Retain(command);
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

  void Draw(id<MTLRenderCommandEncoder> encoder,
            float left,
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
    [encoder setVertexBytes:vertices length:sizeof(vertices) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                vertexStart:0
                vertexCount:6];
  }

  id<MTLDevice> device = nil;
  id<MTLCommandQueue> queue = nil;
  id<MTLRenderPipelineState> texture_pipeline = nil;
  id<MTLRenderPipelineState> color_pipeline = nil;
  id<MTLCommandBuffer> last_command = nil;
  std::array<Upload, 3> uploads;
  size_t next_upload = 0;
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
};

OsrRendererMetal::OsrRendererMetal(cef_color_t background_color,
                                   bool show_update_rect)
    : background_color_(background_color),
      show_update_rect_(show_update_rect),
      impl_(std::make_unique<Impl>()) {}

OsrRendererMetal::~OsrRendererMetal() = default;

bool OsrRendererMetal::Initialize() {
  auto& state = *impl_;
  if (state.texture_pipeline && state.color_pipeline) {
    return true;
  }
  state.device = MTLCreateSystemDefaultDevice();
  state.queue = [state.device newCommandQueue];
  NSError* error = nil;
  id<MTLLibrary> library = [state.device
      newLibraryWithSource:[NSString stringWithUTF8String:kShaders]
                   options:nil
                     error:&error];
  if (!library || !state.queue) {
    LOG(ERROR) << "Cannot initialize Metal OSR: "
               << error.localizedDescription.UTF8String;
    Release(library);
    Cleanup();
    return false;
  }
  auto* descriptor = [[MTLRenderPipelineDescriptor alloc] init];
  id<MTLFunction> vertex = [library newFunctionWithName:@"vertex_main"];
  descriptor.vertexFunction = vertex;
  Release(vertex);
  auto* attachment = descriptor.colorAttachments[0];
  attachment.pixelFormat = MTLPixelFormatBGRA8Unorm;
  attachment.blendingEnabled = YES;
  attachment.sourceRGBBlendFactor = MTLBlendFactorOne;
  attachment.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
  attachment.sourceAlphaBlendFactor = MTLBlendFactorOne;
  attachment.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
  id<MTLFunction> fragment = [library newFunctionWithName:@"fragment_texture"];
  descriptor.fragmentFunction = fragment;
  Release(fragment);
  state.texture_pipeline =
      [state.device newRenderPipelineStateWithDescriptor:descriptor
                                                   error:&error];
  fragment = [library newFunctionWithName:@"fragment_color"];
  descriptor.fragmentFunction = fragment;
  Release(fragment);
  state.color_pipeline =
      [state.device newRenderPipelineStateWithDescriptor:descriptor
                                                   error:&error];
  Release(descriptor);
  Release(library);
  if (!state.texture_pipeline || !state.color_pipeline) {
    LOG(ERROR) << "Cannot create Metal OSR pipelines: "
               << error.localizedDescription.UTF8String;
    Cleanup();
    return false;
  }
  return true;
}

void OsrRendererMetal::Cleanup() {
  impl_ = std::make_unique<Impl>();
}

id<MTLDevice> OsrRendererMetal::device() const {
  return impl_->device;
}

bool OsrRendererMetal::OnPaint(CefRenderHandler::PaintElementType type,
                               const CefRenderHandler::RectList& dirty_rects,
                               const void* buffer,
                               int width,
                               int height) {
  auto& state = *impl_;
  if (!state.queue || !buffer || width <= 0 || height <= 0 ||
      (type == PET_POPUP && !state.popup_visible)) {
    return false;
  }
  auto& image = type == PET_VIEW ? state.view : state.popup;
  if (!state.EnsureTexture(image, width, height, MTLPixelFormatBGRA8Unorm)) {
    return false;
  }
  const CefRect bounds(0, 0, width, height);
  CefRenderHandler::RectList updates = dirty_rects;
  if (!image.valid) {
    updates = {bounds};
  }

  auto& upload = state.uploads[state.next_upload++ % state.uploads.size()];
  if (upload.command) {
    const bool success = Completed(upload.command);
    Release(upload.command);
    if (!success) {
      state.view.valid = state.popup.valid = false;
      return false;
    }
  }
  // Align each row for buffer-to-texture copies. Copy from the callback's full
  // image stride, even when only a small subrectangle is dirty.
  const size_t row_bytes =
      (static_cast<size_t>(width) * 4 + 255) & ~size_t(255);
  const size_t length = row_bytes * height;
  if (!upload.buffer || upload.buffer.length < length) {
    Release(upload.buffer);
    upload.buffer =
        [state.device newBufferWithLength:length
                                  options:MTLResourceStorageModeShared];
  }
  if (!upload.buffer) {
    image.valid = false;
    return false;
  }
  id<MTLCommandBuffer> command = [state.queue commandBuffer];
  id<MTLBlitCommandEncoder> encoder = [command blitCommandEncoder];
  if (!encoder) {
    image.valid = false;
    return false;
  }
  for (const auto& dirty : updates) {
    const auto rect = Intersect(dirty, bounds);
    if (rect.IsEmpty()) {
      continue;
    }
    // Preserve full-image coordinates in staging, including nonzero origins.
    for (int y = rect.y; y < rect.y + rect.height; ++y) {
      std::memcpy(static_cast<uint8_t*>(upload.buffer.contents) +
                      y * row_bytes + rect.x * 4,
                  static_cast<const uint8_t*>(buffer) +
                      (static_cast<size_t>(y) * width + rect.x) * 4,
                  rect.width * 4);
    }
    [encoder copyFromBuffer:upload.buffer
               sourceOffset:rect.y * row_bytes + rect.x * 4
          sourceBytesPerRow:row_bytes
        sourceBytesPerImage:length
                 sourceSize:MTLSizeMake(rect.width, rect.height, 1)
                  toTexture:image.texture
           destinationSlice:0
           destinationLevel:0
          destinationOrigin:MTLOriginMake(rect.x, rect.y, 0)];
  }
  [encoder endEncoding];
  state.Commit(command);
  upload.command = Retain(command);
  image.valid = true;
  if (type == PET_VIEW) {
    state.UpdateViewSize(width, height);
    state.update_rects = dirty_rects;
  }
  return true;
}

bool OsrRendererMetal::OnAcceleratedPaint(
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirty_rects,
    const CefAcceleratedPaintInfo& info) {
  auto& state = *impl_;
  auto surface = static_cast<IOSurfaceRef>(info.shared_texture_io_surface);
  if (!state.queue || !surface || (type == PET_POPUP && !state.popup_visible)) {
    return false;
  }
  MTLPixelFormat format;
  switch (info.format) {
    case CEF_COLOR_TYPE_BGRA_8888:
      format = MTLPixelFormatBGRA8Unorm;
      break;
    case CEF_COLOR_TYPE_RGBA_8888:
      format = MTLPixelFormatRGBA8Unorm;
      break;
    default:
      LOG(ERROR) << "Unsupported Metal OSR color format: " << info.format;
      return false;
  }
  const int width = static_cast<int>(IOSurfaceGetWidth(surface));
  const int height = static_cast<int>(IOSurfaceGetHeight(surface));
  const CefRect bounds(0, 0, width, height);
  const CefRect visible = info.extra.visible_rect;
  if (visible.IsEmpty() || Intersect(visible, bounds) != visible ||
      info.extra.coded_size.width > width ||
      info.extra.coded_size.height > height) {
    LOG(ERROR) << "Invalid Metal OSR IOSurface dimensions";
    return false;
  }
  auto* descriptor =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                         width:width
                                                        height:height
                                                     mipmapped:NO];
  descriptor.storageMode = state.device.hasUnifiedMemory
                               ? MTLStorageModeShared
                               : MTLStorageModeManaged;
  descriptor.usage = MTLTextureUsageShaderRead;
  id<MTLTexture> source = [state.device newTextureWithDescriptor:descriptor
                                                       iosurface:surface
                                                           plane:0];
  auto& image = type == PET_VIEW ? state.view : state.popup;
  if (!source ||
      !state.EnsureTexture(image, visible.width, visible.height, format)) {
    Release(source);
    return false;
  }
  id<MTLCommandBuffer> command = [state.queue commandBuffer];
  id<MTLBlitCommandEncoder> encoder = [command blitCommandEncoder];
  if (!encoder) {
    Release(source);
    image.valid = false;
    return false;
  }
  // Copy the entire visible image. This also handles skipped capture counters
  // and surfaces recycled by CEF's frame pool without stale dirty regions.
  [encoder copyFromTexture:source
               sourceSlice:0
               sourceLevel:0
              sourceOrigin:MTLOriginMake(visible.x, visible.y, 0)
                sourceSize:MTLSizeMake(visible.width, visible.height, 1)
                 toTexture:image.texture
          destinationSlice:0
          destinationLevel:0
         destinationOrigin:MTLOriginMake(0, 0, 0)];
  [encoder endEncoding];
  state.Commit(command);
  // CEF returns this surface to its pool when the callback returns. Retaining
  // source does not prevent reuse of its contents. Finish ALL reads here.
  image.valid = Completed(command);
  // This copy has drained prior work on the queue. Do not keep a command that
  // references the borrowed surface as the renderer's last submitted command.
  Release(state.last_command);
  Release(source);
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

void OsrRendererMetal::OnPopupShow(bool show) {
  impl_->popup_visible = show;
  // A newly shown popup must not display pixels from the previous popup.
  impl_->popup.valid = false;
  if (!show) {
    impl_->original_popup_rect = CefRect();
    impl_->popup_rect = CefRect();
  }
}

void OsrRendererMetal::OnPopupSize(const CefRect& rect) {
  if (rect.width != impl_->original_popup_rect.width ||
      rect.height != impl_->original_popup_rect.height) {
    impl_->popup.valid = false;
  }
  impl_->original_popup_rect = rect;
  impl_->UpdatePopupRect();
}

CefRect OsrRendererMetal::popup_rect() const {
  return impl_->popup_rect;
}

CefRect OsrRendererMetal::original_popup_rect() const {
  return impl_->original_popup_rect;
}

void OsrRendererMetal::SetSpin(float spin_x, float spin_y) {
  impl_->spin_x = spin_x;
  impl_->spin_y = spin_y;
}

void OsrRendererMetal::IncrementSpin(float spin_dx, float spin_dy) {
  impl_->spin_x -= spin_dx;
  impl_->spin_y -= spin_dy;
}

id<MTLCommandBuffer> OsrRendererMetal::Render(id<MTLTexture> target,
                                              id<CAMetalDrawable> drawable) {
  auto& state = *impl_;
  if (!state.queue || !target ||
      target.pixelFormat != MTLPixelFormatBGRA8Unorm) {
    return nil;
  }
  auto* pass = [MTLRenderPassDescriptor renderPassDescriptor];
  auto* attachment = pass.colorAttachments[0];
  attachment.texture = target;
  attachment.loadAction = MTLLoadActionClear;
  attachment.storeAction = MTLStoreActionStore;
  attachment.clearColor =
      MTLClearColorMake(CefColorGetR(background_color_) / 255.0,
                        CefColorGetG(background_color_) / 255.0,
                        CefColorGetB(background_color_) / 255.0, 1);
  id<MTLCommandBuffer> command = [state.queue commandBuffer];
  id<MTLRenderCommandEncoder> encoder =
      [command renderCommandEncoderWithDescriptor:pass];
  if (!encoder) {
    return nil;
  }
  if (state.view.valid) {
    // Preserve cefclient's gradient behind transparent/rotated browser content.
    [encoder setRenderPipelineState:state.color_pipeline];
    state.Draw(encoder, -1, 1, 1, -1, 1, 1, false, true);
    [encoder setRenderPipelineState:state.texture_pipeline];
    [encoder setFragmentTexture:state.view.texture atIndex:0];
    state.Draw(encoder, -1, 1, 1, -1, 1, 1, true);

    const float sx = 2.f / state.view_width;
    const float sy = 2.f / state.view_height;
    if (state.popup_visible && state.popup.valid) {
      CefRect rect = state.popup_rect;
      rect.width =
          std::min(rect.width, static_cast<int>(state.popup.texture.width));
      rect.height =
          std::min(rect.height, static_cast<int>(state.popup.texture.height));
      rect =
          Intersect(rect, CefRect(0, 0, state.view_width, state.view_height));
      if (!rect.IsEmpty()) {
        [encoder setFragmentTexture:state.popup.texture atIndex:0];
        state.Draw(
            encoder, rect.x * sx - 1, 1 - rect.y * sy,
            (rect.x + rect.width) * sx - 1, 1 - (rect.y + rect.height) * sy,
            rect.width / static_cast<float>(state.popup.texture.width),
            rect.height / static_cast<float>(state.popup.texture.height), true);
      }
    }
    if (show_update_rect_) {
      [encoder setRenderPipelineState:state.color_pipeline];
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
        state.Draw(encoder, l, t, r, t - sy, 1, 1, false, false, true);
        state.Draw(encoder, l, b + sy, r, b, 1, 1, false, false, true);
        state.Draw(encoder, l, t, l + sx, b, 1, 1, false, false, true);
        state.Draw(encoder, r - sx, t, r, b, 1, 1, false, false, true);
      }
    }
  }
  [encoder endEncoding];
  if (drawable) {
    [command presentDrawable:drawable];
  }
  state.Commit(command);
  return command;
}

}  // namespace client
