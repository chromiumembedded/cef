// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#include "tests/ceftests/os_rendering_unittest_mac.h"

#import <Cocoa/Cocoa.h>

#include <algorithm>

#include "tests/ceftests/os_rendering_unittest_shared_texture.h"
#include "tests/ceftests/osr_renderer_test_util_mac.h"
#include "tests/shared/browser/osr_renderer_metal.h"

namespace osr_unittests {

CefWindowHandle GetFakeView() {
  NSScreen* mainScreen = [NSScreen mainScreen];
  NSRect screenRect = [mainScreen visibleFrame];
  NSView* fakeView = [[NSView alloc] initWithFrame:screenRect];
  return CAST_NSVIEW_TO_CEF_WINDOW_HANDLE(fakeView);
}

bool ReadMetalTexture(id<MTLTexture> texture, std::vector<uint32_t>* pixels) {
  @autoreleasepool {
    if (!texture || !texture.width || !texture.height ||
        texture.pixelFormat != MTLPixelFormatBGRA8Unorm) {
      return false;
    }
    const size_t width = texture.width;
    const size_t height = texture.height;
    const size_t row_bytes = (width * 4 + 255) & ~size_t(255);
    id<MTLDevice> device = texture.device;
    id<MTLBuffer> readback =
        [device newBufferWithLength:row_bytes * height
                            options:MTLResourceStorageModeShared];
    id<MTLCommandQueue> queue = [device newCommandQueue];
    id<MTLCommandBuffer> copy = [queue commandBuffer];
    id<MTLBlitCommandEncoder> encoder =
        readback ? [copy blitCommandEncoder] : nil;
    bool success = readback && encoder;
    if (success) {
      [encoder copyFromTexture:texture
                       sourceSlice:0
                       sourceLevel:0
                      sourceOrigin:MTLOriginMake(0, 0, 0)
                        sourceSize:MTLSizeMake(width, height, 1)
                          toBuffer:readback
                 destinationOffset:0
            destinationBytesPerRow:row_bytes
          destinationBytesPerImage:row_bytes * height];
      [encoder endEncoding];
      [copy commit];
      [copy waitUntilCompleted];
      success =
          copy.status == MTLCommandBufferStatusCompleted && readback.contents;
      if (success) {
        pixels->resize(width * height);
        for (size_t y = 0; y < height; ++y) {
          const auto* row = reinterpret_cast<const uint32_t*>(
              static_cast<const uint8_t*>(readback.contents) + y * row_bytes);
          std::copy_n(row, width, pixels->begin() + y * width);
        }
      }
    }
#if !__has_feature(objc_arc)
    [readback release];
    [queue release];
#endif
    return success;
  }
}

namespace {

class NativeSharedTexturePixelReader : public SharedTexturePixelReader {
 public:
  bool Initialize() override { return renderer_.Initialize(); }
  void Cleanup() override { renderer_.Cleanup(); }

  bool Read(const CefAcceleratedPaintInfo& info,
            uint32_t* marker,
            uint32_t* timestamp) override {
    @autoreleasepool {
      if (!renderer_.OnAcceleratedPaint(PET_VIEW, {}, info)) {
        return false;
      }
      const CefRect visible = info.extra.visible_rect;
      auto* descriptor = [MTLTextureDescriptor
          texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                       width:visible.width
                                      height:visible.height
                                   mipmapped:NO];
      descriptor.storageMode = MTLStorageModePrivate;
      descriptor.usage = MTLTextureUsageRenderTarget;
      id<MTLTexture> target =
          [renderer_.device() newTextureWithDescriptor:descriptor];
      id<MTLCommandBuffer> render = renderer_.Render(target);
      [render waitUntilCompleted];
      std::vector<uint32_t> pixels;
      const bool success = render &&
                           render.status == MTLCommandBufferStatusCompleted &&
                           ReadMetalTexture(target, &pixels);
      if (success) {
        *marker = pixels[0];
        *timestamp = pixels[visible.width / 2];
      }
#if !__has_feature(objc_arc)
      [target release];
#endif
      return success;
    }
  }

 private:
  client::OsrRendererMetal renderer_{0};
};

}  // namespace

std::unique_ptr<SharedTexturePixelReader> CreateSharedTexturePixelReader() {
  return std::make_unique<NativeSharedTexturePixelReader>();
}

ScopedTimingActivity::ScopedTimingActivity() {
  id activity = [[NSProcessInfo processInfo]
      beginActivityWithOptions:NSActivityUserInitiatedAllowingIdleSystemSleep |
                               NSActivityLatencyCritical
                        reason:@"CEF external BeginFrame timing test"];
#if __has_feature(objc_arc)
  activity_ = (__bridge_retained void*)activity;
#else
  activity_ = [activity retain];
#endif
}

ScopedTimingActivity::~ScopedTimingActivity() {
#if __has_feature(objc_arc)
  id activity = (__bridge_transfer id)activity_;
#else
  id activity = static_cast<id>(activity_);
#endif
  [[NSProcessInfo processInfo] endActivity:activity];
#if !__has_feature(objc_arc)
  [activity release];
#endif
}

}  // namespace osr_unittests
