// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/osr_renderer_metal.h"

#import <IOSurface/IOSurface.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "tests/gtest/include/gtest/gtest.h"

namespace {

constexpr uint32_t kRed = 0xffff0000;
constexpr uint32_t kGreen = 0xff00ff00;
constexpr uint32_t kBlue = 0xff0000ff;
constexpr uint32_t kWhite = 0xffffffff;

class OsrRendererMetalTest : public testing::Test {
 public:
  void SetUp() override {
    renderer_ = std::make_unique<client::OsrRendererMetal>(0);
    ASSERT_TRUE(renderer_->Initialize());
  }

  bool Paint(int width,
             int height,
             const std::vector<uint32_t>& pixels,
             const CefRenderHandler::RectList& dirty,
             CefRenderHandler::PaintElementType type = PET_VIEW) {
    return renderer_->OnPaint(type, dirty, pixels.data(), width, height);
  }

  // Read pixels from the production render path, including GPU uploads,
  // sampling and blending. No NSWindow or CPU rendering substitute is involved.
  std::vector<uint32_t> Render(int width, int height) {
    @autoreleasepool {
      id<MTLDevice> device = renderer_->device();
      auto* descriptor = [MTLTextureDescriptor
          texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                       width:width
                                      height:height
                                   mipmapped:NO];
      descriptor.storageMode = MTLStorageModePrivate;
      descriptor.usage = MTLTextureUsageRenderTarget;
      id<MTLTexture> target = [device newTextureWithDescriptor:descriptor];
      id<MTLCommandBuffer> render = renderer_->Render(target);
      EXPECT_NE(nil, render);
      [render waitUntilCompleted];
      EXPECT_EQ(MTLCommandBufferStatusCompleted, render.status);

      const size_t row_bytes = (width * 4 + 255) & ~size_t(255);
      id<MTLBuffer> readback =
          [device newBufferWithLength:row_bytes * height
                              options:MTLResourceStorageModeShared];
      id<MTLCommandQueue> queue = [device newCommandQueue];
      id<MTLCommandBuffer> copy = [queue commandBuffer];
      id<MTLBlitCommandEncoder> encoder = [copy blitCommandEncoder];
      [encoder copyFromTexture:target
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
      EXPECT_EQ(MTLCommandBufferStatusCompleted, copy.status);
      std::vector<uint32_t> pixels(width * height);
      for (int y = 0; y < height; ++y) {
        const auto* row = reinterpret_cast<const uint32_t*>(
            static_cast<const uint8_t*>(readback.contents) + y * row_bytes);
        std::copy_n(row, width, pixels.begin() + y * width);
      }
#if !__has_feature(objc_arc)
      [target release];
      [readback release];
      [queue release];
#endif
      return pixels;
    }
  }

 protected:
  std::unique_ptr<client::OsrRendererMetal> renderer_;
};

TEST_F(OsrRendererMetalTest, DirtyRectanglesAndUploadReuse) {
  const int width = 17;
  const int height = 9;
  std::vector<uint32_t> pixels(width * height, kBlue);
  ASSERT_TRUE(Paint(width, height, pixels, {CefRect(0, 0, width, height)}));
  EXPECT_EQ(pixels, Render(width, height));

  // Cycle past the staging pool size, updating nonzero, unaligned origins.
  // Changes outside the dirty rectangles must not reach the destination.
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

TEST_F(OsrRendererMetalTest, ResizeInitializesWholeTexture) {
  ASSERT_TRUE(Paint(8, 8, std::vector<uint32_t>(64, kRed), {}));
  std::vector<uint32_t> resized(13 * 7, kGreen);
  resized.front() = kBlue;
  resized.back() = kRed;
  ASSERT_TRUE(Paint(13, 7, resized, {CefRect(3, 3, 1, 1)}));
  EXPECT_EQ(resized, Render(13, 7));
}

TEST_F(OsrRendererMetalTest, PopupHideRestoresUpdatedView) {
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

TEST_F(OsrRendererMetalTest, OversizedPopupAndViewResize) {
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

TEST_F(OsrRendererMetalTest, PremultipliedAlphaAndTopLeftOrigin) {
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

TEST_F(OsrRendererMetalTest, SpinRedrawWithoutPaint) {
  ASSERT_TRUE(Paint(16, 16, std::vector<uint32_t>(256, kGreen), {}));
  renderer_->SetSpin(0, 60);
  const auto rotated = Render(16, 16);
  EXPECT_NE(kGreen, rotated[0]);
  EXPECT_EQ(kGreen, rotated[8 * 16 + 8]);
  renderer_->SetSpin(0, 0);
  EXPECT_EQ(std::vector<uint32_t>(256, kGreen), Render(16, 16));
}

TEST_F(OsrRendererMetalTest, AcceleratedCopyOwnsPixelsAndHonorsFormat) {
  for (bool rgba : {false, true}) {
    @autoreleasepool {
      NSDictionary* properties = @{
        (id)kIOSurfaceWidth : @8,
        (id)kIOSurfaceHeight : @8,
        (id)kIOSurfaceBytesPerElement : @4,
        (id)kIOSurfacePixelFormat : @(rgba ? 'RGBA' : 'BGRA'),
      };
      IOSurfaceRef surface =
          IOSurfaceCreate((__bridge CFDictionaryRef)properties);
      ASSERT_NE(nullptr, surface);
      ASSERT_EQ(kIOReturnSuccess, IOSurfaceLock(surface, 0, nullptr));
      const size_t stride = IOSurfaceGetBytesPerRow(surface);
      auto* bytes = static_cast<uint8_t*>(IOSurfaceGetBaseAddress(surface));
      for (int y = 0; y < 8; ++y) {
        auto* row = reinterpret_cast<uint32_t*>(bytes + y * stride);
        std::fill_n(row, 8, kGreen);
        if (y >= 1 && y < 7) {
          // Red inside the visible aperture, green in the surrounding padding.
          std::fill_n(row + 2, 5, rgba ? kBlue : kRed);
        }
      }
      ASSERT_EQ(kIOReturnSuccess, IOSurfaceUnlock(surface, 0, nullptr));
      CefAcceleratedPaintInfo info;
      info.shared_texture_io_surface = surface;
      info.format = rgba ? CEF_COLOR_TYPE_RGBA_8888 : CEF_COLOR_TYPE_BGRA_8888;
      info.extra.coded_size = {8, 8};
      info.extra.visible_rect = {2, 1, 5, 6};
      info.extra.content_rect = info.extra.visible_rect;
      ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, info));

      // Simulate immediate reuse by CEF's pool before we render anything.
      ASSERT_EQ(kIOReturnSuccess, IOSurfaceLock(surface, 0, nullptr));
      for (int y = 0; y < 8; ++y) {
        std::fill_n(reinterpret_cast<uint32_t*>(bytes + y * stride), 8, kGreen);
      }
      ASSERT_EQ(kIOReturnSuccess, IOSurfaceUnlock(surface, 0, nullptr));
      EXPECT_EQ(std::vector<uint32_t>(30, kRed), Render(5, 6));

      renderer_->OnPopupShow(true);
      renderer_->OnPopupSize(CefRect(3, 4, 2, 2));
      info.extra.visible_rect = {2, 1, 2, 2};
      info.extra.content_rect = info.extra.visible_rect;
      ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_POPUP, {}, info));
      CFRelease(surface);
      const auto with_popup = Render(5, 6);
      EXPECT_EQ(kRed, with_popup[0]);
      EXPECT_EQ(kGreen, with_popup[4 * 5 + 3]);
      EXPECT_EQ(kGreen, with_popup[5 * 5 + 4]);
      renderer_->OnPopupShow(false);
      EXPECT_EQ(std::vector<uint32_t>(30, kRed), Render(5, 6));
    }
  }
}

TEST_F(OsrRendererMetalTest, CleanupWithUploadsInFlight) {
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

}  // namespace
