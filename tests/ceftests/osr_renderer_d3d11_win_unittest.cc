// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/osr_renderer_d3d11_win.h"

#include <dxgi1_2.h>
#include <wrl/client.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "tests/gtest/include/gtest/gtest.h"

namespace {

using Microsoft::WRL::ComPtr;
constexpr uint32_t kRed = 0xffff0000;
constexpr uint32_t kGreen = 0xff00ff00;
constexpr uint32_t kBlue = 0xff0000ff;

class SharedImage {
 public:
  ~SharedImage() {
    if (handle) {
      CloseHandle(handle);
    }
  }
  ComPtr<ID3D11Texture2D> texture;
  HANDLE handle = nullptr;
  CefAcceleratedPaintInfo info;
};

// Run the production shaders, uploads, shared-resource imports and blending
// with both hardware and WARP. No HWND or browser process is needed.
class OsrRendererD3D11Test : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    renderer_ = std::make_unique<client::OsrRendererD3D11>(0);
    if (GetParam()) {
      ComPtr<ID3D11Device> warp;
      ASSERT_TRUE(SUCCEEDED(
          D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr,
                            0, D3D11_SDK_VERSION, &warp, nullptr, nullptr)));
      ASSERT_TRUE(renderer_->Initialize(warp.Get()));
    } else {
      ASSERT_TRUE(renderer_->Initialize());
    }
    renderer_->device()->GetImmediateContext(&context_);
  }

  bool Paint(int width,
             int height,
             const std::vector<uint32_t>& pixels,
             const CefRenderHandler::RectList& dirty,
             CefRenderHandler::PaintElementType type = PET_VIEW) {
    return renderer_->OnPaint(type, dirty, pixels.data(), width, height);
  }

  ComPtr<ID3D11Texture2D> Target(
      int width,
      int height,
      DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> texture;
    EXPECT_TRUE(SUCCEEDED(
        renderer_->device()->CreateTexture2D(&desc, nullptr, &texture)));
    return texture;
  }

  std::vector<uint32_t> Render(
      int width,
      int height,
      DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM) {
    const auto target = Target(width, height, format);
    if (!target) {
      return {};
    }
    if (!renderer_->Render(target.Get())) {
      ADD_FAILURE() << "Could not render to readback target";
      return {};
    }
    D3D11_TEXTURE2D_DESC desc = {};
    target->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;
    if (FAILED(
            renderer_->device()->CreateTexture2D(&desc, nullptr, &readback))) {
      ADD_FAILURE() << "Could not create readback texture";
      return {};
    }
    context_->CopyResource(readback.Get(), target.Get());
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context_->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
      ADD_FAILURE() << "Could not read rendered pixels";
      return {};
    }
    std::vector<uint32_t> pixels(width * height);
    for (int y = 0; y < height; ++y) {
      const auto* row = reinterpret_cast<const uint32_t*>(
          static_cast<const uint8_t*>(mapped.pData) + y * mapped.RowPitch);
      std::copy_n(row, width, pixels.begin() + y * width);
    }
    context_->Unmap(readback.Get(), 0);
    return pixels;
  }

  std::unique_ptr<SharedImage> Shared(int width,
                                      int height,
                                      const std::vector<uint32_t>& pixels,
                                      bool rgba = false,
                                      ID3D11Device* producer = nullptr) {
    auto image = std::make_unique<SharedImage>();
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format =
        rgba ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags =
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;
    D3D11_SUBRESOURCE_DATA data = {pixels.data(), static_cast<UINT>(width * 4),
                                   0};
    if (!producer) {
      producer = renderer_->device();
    }
    if (FAILED(producer->CreateTexture2D(&desc, &data, &image->texture))) {
      ADD_FAILURE() << "Could not create shared texture";
      return nullptr;
    }
    ComPtr<IDXGIResource1> resource;
    if (FAILED(image->texture.As(&resource)) ||
        FAILED(resource->CreateSharedHandle(
            nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
            nullptr, &image->handle))) {
      ADD_FAILURE() << "Could not create NT shared handle";
      return nullptr;
    }
    image->info.shared_texture_handle = image->handle;
    image->info.format =
        rgba ? CEF_COLOR_TYPE_RGBA_8888 : CEF_COLOR_TYPE_BGRA_8888;
    image->info.extra.coded_size = {width, height};
    image->info.extra.visible_rect = {0, 0, width, height};
    image->info.extra.content_rect = image->info.extra.visible_rect;
    return image;
  }

 protected:
  std::unique_ptr<client::OsrRendererD3D11> renderer_;
  ComPtr<ID3D11DeviceContext> context_;
};

TEST_P(OsrRendererD3D11Test, DirtyRectanglesAndUploadReuse) {
  const int width = 17, height = 9;
  std::vector<uint32_t> pixels(width * height, kBlue);
  ASSERT_TRUE(Paint(width, height, pixels, {}));
  EXPECT_EQ(pixels, Render(width, height));
  auto expected = pixels;
  for (int i = 1; i <= 8; ++i) {
    std::fill(pixels.begin(), pixels.end(), kRed);
    expected[2 * width + i] = pixels[2 * width + i] = kGreen;
    expected[6 * width + i + 1] = pixels[6 * width + i + 1] = kGreen;
    ASSERT_TRUE(Paint(width, height, pixels,
                      {CefRect(i, 2, 1, 1), CefRect(i + 1, 6, 1, 1),
                       CefRect(-4, -4, 2, 2), CefRect(100, 100, 2, 2)}));
  }
  // Callback memory may be reused as soon as OnPaint returns.
  std::fill(pixels.begin(), pixels.end(), kBlue);
  EXPECT_EQ(expected, Render(width, height));
}

TEST_P(OsrRendererD3D11Test, ResizeInitializesWholeTexture) {
  ASSERT_TRUE(Paint(8, 8, std::vector<uint32_t>(64, kRed), {}));
  std::vector<uint32_t> pixels(13 * 7, kGreen);
  pixels.front() = kBlue;
  pixels.back() = kRed;
  ASSERT_TRUE(Paint(13, 7, pixels, {CefRect(3, 3, 1, 1)}));
  EXPECT_EQ(pixels, Render(13, 7));
}

TEST_P(OsrRendererD3D11Test, FailedResizePreservesLastImage) {
  ASSERT_TRUE(Paint(8, 8, std::vector<uint32_t>(64, kGreen), {}));
  const int width = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION + 1;
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = width;
  desc.Height = desc.MipLevels = desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  ComPtr<ID3D11Texture2D> probe;
  // Drivers may accept dimensions beyond the documented hardware
  // maximum. Only use this descriptor to force failure when the driver rejects
  // it, and still supply a complete callback buffer rather than assuming
  // failure.
  if (SUCCEEDED(renderer_->device()->CreateTexture2D(&desc, nullptr, &probe))) {
    GTEST_SKIP()
        << "Driver accepts oversized texture; cannot force allocation failure";
  }
  EXPECT_FALSE(Paint(width, 1, std::vector<uint32_t>(width, kRed), {}));
  EXPECT_EQ(CefSize(8, 8), renderer_->view_size());
  EXPECT_EQ(std::vector<uint32_t>(64, kGreen), Render(8, 8));
}

TEST_P(OsrRendererD3D11Test, ExtremeDirtyRectanglesAreClipped) {
  ASSERT_TRUE(Paint(8, 8, std::vector<uint32_t>(64, kBlue), {}));
  const int largest = std::numeric_limits<int>::max();
  const int smallest = std::numeric_limits<int>::min();
  ASSERT_TRUE(Paint(8, 8, std::vector<uint32_t>(64, kGreen),
                    {CefRect(1, 0, largest, 8), CefRect(0, 1, 8, largest),
                     CefRect(0, 0, smallest, 1), CefRect(0, 0, 1, smallest)}));
  std::vector<uint32_t> expected(64, kGreen);
  expected.front() = kBlue;
  EXPECT_EQ(expected, Render(8, 8));
}

TEST_P(OsrRendererD3D11Test, EmptyPopupBoundsClearCachedPopup) {
  ASSERT_TRUE(Paint(8, 8, std::vector<uint32_t>(64, kBlue), {}));
  const int smallest = std::numeric_limits<int>::min();
  for (const auto& bounds :
       {CefRect(1, 1, 0, 2), CefRect(1, 1, 2, 0), CefRect(1, 1, smallest, 1),
        CefRect(1, 1, 1, smallest)}) {
    renderer_->OnPopupShow(true);
    renderer_->OnPopupSize(CefRect(1, 1, 2, 2));
    ASSERT_TRUE(Paint(2, 2, std::vector<uint32_t>(4, kRed), {}, PET_POPUP));
    renderer_->OnPopupSize(bounds);
    EXPECT_EQ(CefRect(), renderer_->original_popup_rect());
    EXPECT_EQ(CefRect(), renderer_->popup_rect());
    EXPECT_EQ(std::vector<uint32_t>(64, kBlue), Render(8, 8));
  }
}

TEST_P(OsrRendererD3D11Test, CachedRedrawWhileWaitingForResizedPaint) {
  std::vector<uint32_t> pixels(16 * 12, kGreen);
  ASSERT_TRUE(Paint(16, 12, pixels, {}));
  // Native exposure/resize must redraw owned pixels without requesting CEF to
  // read the producer's framebuffer, which may be cleared for its next frame.
  std::fill(pixels.begin(), pixels.end(), kBlue);
  for (const auto& size : {CefSize(16, 12), CefSize(32, 24), CefSize(7, 3),
                           CefSize(24, 18), CefSize(16, 12)}) {
    EXPECT_EQ(std::vector<uint32_t>(size.width * size.height, kGreen),
              Render(size.width, size.height));
    EXPECT_EQ(CefSize(16, 12), renderer_->view_size());
  }
  // A completed resized paint then replaces the cached image normally.
  ASSERT_TRUE(Paint(24, 18, std::vector<uint32_t>(24 * 18, kRed), {}));
  EXPECT_EQ(std::vector<uint32_t>(24 * 18, kRed), Render(24, 18));
  EXPECT_EQ(CefSize(24, 18), renderer_->view_size());
}

TEST_P(OsrRendererD3D11Test, PopupHideRestoresUpdatedView) {
  ASSERT_TRUE(Paint(8, 8, std::vector<uint32_t>(64, kBlue), {}));
  renderer_->OnPopupShow(true);
  renderer_->OnPopupSize(CefRect(6, 7, 3, 2));
  EXPECT_EQ(CefRect(5, 6, 3, 2), renderer_->popup_rect());
  EXPECT_EQ(CefRect(6, 7, 3, 2), renderer_->original_popup_rect());
  ASSERT_TRUE(Paint(3, 2, std::vector<uint32_t>(6, kRed), {}, PET_POPUP));
  auto pixels = Render(8, 8);
  ASSERT_EQ(64u, pixels.size());
  EXPECT_EQ(kRed, pixels[6 * 8 + 5]);
  EXPECT_EQ(kBlue, pixels[6 * 8 + 4]);
  ASSERT_TRUE(
      Paint(8, 8, std::vector<uint32_t>(64, kGreen), {CefRect(0, 0, 8, 8)}));
  pixels = Render(8, 8);
  ASSERT_EQ(64u, pixels.size());
  EXPECT_EQ(kRed, pixels[7 * 8 + 7]);
  EXPECT_EQ(kGreen, pixels[0]);
  renderer_->OnPopupShow(false);
  EXPECT_EQ(std::vector<uint32_t>(64, kGreen), Render(8, 8));
  renderer_->OnPopupShow(true);
  renderer_->OnPopupSize(CefRect(0, 0, 3, 2));
  EXPECT_EQ(std::vector<uint32_t>(64, kGreen), Render(8, 8));
}

TEST_P(OsrRendererD3D11Test, OversizedPopupAndViewResize) {
  ASSERT_TRUE(Paint(4, 4, std::vector<uint32_t>(16, kBlue), {}));
  renderer_->OnPopupShow(true);
  renderer_->OnPopupSize(CefRect(-3, -2, 6, 5));
  std::vector<uint32_t> popup(30, kRed);
  for (int y = 0; y < 5; ++y) {
    popup[y * 6 + 5] = kGreen;
  }
  ASSERT_TRUE(Paint(6, 5, popup, {}, PET_POPUP));
  EXPECT_EQ(std::vector<uint32_t>(16, kRed), Render(4, 4));
  renderer_->OnPopupSize(CefRect(5, 5, 6, 5));
  ASSERT_TRUE(Paint(12, 12, std::vector<uint32_t>(144, kBlue), {}));
  EXPECT_EQ(CefRect(5, 5, 6, 5), renderer_->popup_rect());
}

TEST_P(OsrRendererD3D11Test, PremultipliedAlphaAndTopLeftOrigin) {
  std::vector<uint32_t> pixels(16, 0x80008000);
  pixels[0] = kRed;
  pixels[15] = kBlue;
  ASSERT_TRUE(Paint(4, 4, pixels, {}));
  const auto result = Render(4, 4);
  ASSERT_EQ(16u, result.size());
  EXPECT_EQ(kRed, result[0]);
  EXPECT_EQ(kBlue, result[15]);
  EXPECT_EQ(128u, (result[5] >> 8) & 0xff);
  EXPECT_EQ(255u, result[5] >> 24);
  EXPECT_GT(result[5] & 0xff, 0u);
  EXPECT_GT((result[5] >> 16) & 0xff, 0u);
}

TEST_P(OsrRendererD3D11Test, SpinRedrawWithoutPaint) {
  ASSERT_TRUE(Paint(16, 16, std::vector<uint32_t>(256, kGreen), {}));
  renderer_->SetSpin(0, 60);
  const auto rotated = Render(16, 16);
  ASSERT_EQ(256u, rotated.size());
  EXPECT_NE(kGreen, rotated[0]);
  EXPECT_EQ(kGreen, rotated[8 * 16 + 8]);
  renderer_->IncrementSpin(0, 60);
  EXPECT_EQ(std::vector<uint32_t>(256, kGreen), Render(16, 16));
}

TEST_P(OsrRendererD3D11Test, AcceleratedCopyOwnsPixelsAndHonorsFormat) {
  for (bool rgba : {false, true}) {
    std::vector<uint32_t> pixels(64, kGreen);
    for (int y = 1; y < 7; ++y) {
      std::fill_n(pixels.begin() + y * 8 + 2, 5, rgba ? kBlue : kRed);
    }
    auto image = Shared(8, 8, pixels, rgba);
    ASSERT_NE(nullptr, image);
    image->info.extra.visible_rect = {2, 1, 5, 6};
    image->info.extra.content_rect = image->info.extra.visible_rect;
    ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, image->info));
    std::fill(pixels.begin(), pixels.end(), kGreen);
    context_->UpdateSubresource(image->texture.Get(), 0, nullptr, pixels.data(),
                                8 * 4, 0);
    EXPECT_EQ(std::vector<uint32_t>(30, kRed), Render(5, 6));
    renderer_->OnPopupShow(true);
    renderer_->OnPopupSize(CefRect(3, 4, 2, 2));
    image->info.extra.visible_rect = {2, 1, 2, 2};
    image->info.extra.content_rect = image->info.extra.visible_rect;
    ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_POPUP, {}, image->info));
    // Closing the producer's handle and releasing its texture must not affect
    // either the last view image or the popup after the callback returns.
    image.reset();
    const auto with_popup = Render(5, 6);
    ASSERT_EQ(30u, with_popup.size());
    EXPECT_EQ(kRed, with_popup[0]);
    EXPECT_EQ(kGreen, with_popup[4 * 5 + 3]);
    EXPECT_EQ(kGreen, with_popup[5 * 5 + 4]);
    renderer_->OnPopupShow(false);
    EXPECT_EQ(std::vector<uint32_t>(30, kRed), Render(5, 6));
  }
}

TEST_P(OsrRendererD3D11Test, AcceleratedReusedAndAlternatingHandles) {
  auto first = Shared(8, 8, std::vector<uint32_t>(64, kRed));
  auto second = Shared(8, 8, std::vector<uint32_t>(64, kBlue));
  ASSERT_NE(nullptr, first);
  ASSERT_NE(nullptr, second);
  for (auto* image : {first.get(), second.get(), first.get()}) {
    // A tiny damage rectangle cannot describe changes across pooled handles.
    ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_VIEW, {CefRect(2, 2, 1, 1)},
                                              image->info));
    EXPECT_EQ(std::vector<uint32_t>(64, image == first.get() ? kRed : kBlue),
              Render(8, 8));
  }
  const std::vector<uint32_t> changed(64, kGreen);
  context_->UpdateSubresource(first->texture.Get(), 0, nullptr, changed.data(),
                              8 * 4, 0);
  ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_VIEW, {CefRect(2, 2, 1, 1)},
                                            first->info));
  EXPECT_EQ(changed, Render(8, 8));
}

TEST_P(OsrRendererD3D11Test, AcceleratedCopyCompletesBeforeProducerReuse) {
  // Use a separate immediate context, like Chromium's GPU producer. An update
  // on the renderer's own context would be ordered after its copy even if the
  // callback returned before the GPU finished reading the borrowed resource.
  ComPtr<IDXGIDevice> dxgi_device;
  ComPtr<IDXGIAdapter> adapter;
  ASSERT_TRUE(SUCCEEDED(
      renderer_->device()->QueryInterface(IID_PPV_ARGS(&dxgi_device))));
  ASSERT_TRUE(SUCCEEDED(dxgi_device->GetAdapter(&adapter)));
  ComPtr<ID3D11Device> producer;
  ComPtr<ID3D11DeviceContext> producer_context;
  ASSERT_TRUE(SUCCEEDED(D3D11CreateDevice(
      adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0,
      D3D11_SDK_VERSION, &producer, nullptr, &producer_context)));
  D3D11_QUERY_DESC desc = {D3D11_QUERY_EVENT, 0};
  ComPtr<ID3D11Query> completed;
  ASSERT_TRUE(SUCCEEDED(producer->CreateQuery(&desc, &completed)));
  const auto finish = [&]() {
    producer_context->End(completed.Get());
    producer_context->Flush();
    HRESULT hr;
    while ((hr = producer_context->GetData(completed.Get(), nullptr, 0, 0)) ==
           S_FALSE) {
      if (FAILED(producer->GetDeviceRemovedReason())) {
        return false;
      }
      SwitchToThread();
    }
    return hr == S_OK;
  };
  const std::vector<uint32_t> pixels(256 * 256, kRed);
  auto image = Shared(256, 256, pixels, false, producer.Get());
  ASSERT_NE(nullptr, image);
  ASSERT_TRUE(finish());
  ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, image->info));
  const std::vector<uint32_t> reused(256 * 256, kGreen);
  producer_context->UpdateSubresource(image->texture.Get(), 0, nullptr,
                                      reused.data(), 256 * 4, 0);
  ASSERT_TRUE(finish());
  image.reset();
  EXPECT_EQ(pixels, Render(256, 256));
}

TEST_P(OsrRendererD3D11Test, AcceleratedRejectsInvalidInputPreservesLastImage) {
  auto image = Shared(8, 8, std::vector<uint32_t>(64, kRed));
  ASSERT_NE(nullptr, image);
  ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, image->info));
  auto invalid = image->info;
  invalid.shared_texture_handle = nullptr;
  EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, invalid));
  invalid = image->info;
  invalid.format = CEF_COLOR_TYPE_RGBA_8888;  // Resource is BGRA.
  EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, invalid));
  invalid = image->info;
  invalid.extra.visible_rect = {-1, 0, 8, 8};
  EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, invalid));
  invalid.extra.visible_rect = {0, 0, 9, 8};
  EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, invalid));
  invalid.extra.visible_rect = {};
  EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, invalid));
  invalid.extra.visible_rect = {1, 1, std::numeric_limits<int>::max(), 2};
  EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, invalid));
  invalid.extra.visible_rect = {1, 1, 2, std::numeric_limits<int>::max()};
  EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, invalid));
  invalid = image->info;
  invalid.extra.coded_size = {9, 8};
  EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, invalid));
  EXPECT_FALSE(renderer_->OnAcceleratedPaint(PET_POPUP, {}, image->info));
  EXPECT_EQ(std::vector<uint32_t>(64, kRed), Render(8, 8));
}

TEST_P(OsrRendererD3D11Test, PaintAndAcceleratedPaintInterleave) {
  ASSERT_TRUE(Paint(8, 8, std::vector<uint32_t>(64, kBlue), {}));
  auto image = Shared(8, 8, std::vector<uint32_t>(64, kRed), true);
  ASSERT_NE(nullptr, image);
  ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, image->info));
  EXPECT_EQ(std::vector<uint32_t>(64, kBlue), Render(8, 8));
  // Format change must initialize the whole new BGRA image despite small
  // damage.
  ASSERT_TRUE(
      Paint(8, 8, std::vector<uint32_t>(64, kGreen), {CefRect(2, 2, 1, 1)}));
  EXPECT_EQ(std::vector<uint32_t>(64, kGreen), Render(8, 8));
  ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, image->info));
  EXPECT_EQ(std::vector<uint32_t>(64, kBlue), Render(8, 8));
}

TEST_P(OsrRendererD3D11Test, AcceleratedMatchesPaintComposition) {
  // Selecting a callback must not change the sample's transparency background,
  // origin, premultiplied blending, rotation or popup composition. These are
  // the same user-facing renderer features exercised by Metal/EGL tests.
  std::vector<uint32_t> pixels(16 * 16, 0x80008000);
  pixels.front() = kRed;
  pixels.back() = kBlue;
  ASSERT_TRUE(Paint(16, 16, pixels, {}));
  const auto software = Render(16, 16);
  ASSERT_EQ(256u, software.size());
  auto image = Shared(16, 16, pixels);
  ASSERT_NE(nullptr, image);
  ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_VIEW, {}, image->info));
  EXPECT_EQ(software, Render(16, 16));
  renderer_->SetSpin(0, 60);
  EXPECT_NE(software, Render(16, 16));
  renderer_->SetSpin(0, 0);
  EXPECT_EQ(software, Render(16, 16));
  renderer_->OnPopupShow(true);
  renderer_->OnPopupSize(CefRect(14, 15, 3, 2));
  ASSERT_TRUE(Paint(3, 2, std::vector<uint32_t>(6, kRed), {}, PET_POPUP));
  const auto software_popup = Render(16, 16);
  ASSERT_EQ(256u, software_popup.size());
  auto popup = Shared(3, 2, std::vector<uint32_t>(6, kRed));
  ASSERT_NE(nullptr, popup);
  ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_POPUP, {}, popup->info));
  EXPECT_EQ(software_popup, Render(16, 16));
  renderer_->OnPopupShow(false);
  EXPECT_EQ(software, Render(16, 16));
}

TEST_P(OsrRendererD3D11Test, UpdateBordersUseVisibleCoordinates) {
  auto device = ComPtr<ID3D11Device>(renderer_->device());
  renderer_ = std::make_unique<client::OsrRendererD3D11>(0, true);
  ASSERT_TRUE(renderer_->Initialize(device.Get()));
  auto image = Shared(12, 12, std::vector<uint32_t>(144, kBlue));
  ASSERT_NE(nullptr, image);
  image->info.extra.visible_rect = {2, 3, 8, 8};
  ASSERT_TRUE(renderer_->OnAcceleratedPaint(PET_VIEW, {CefRect(3, 4, 4, 4)},
                                            image->info));
  const auto pixels = Render(8, 8);
  ASSERT_EQ(64u, pixels.size());
  EXPECT_EQ(kBlue, pixels[0]);
  EXPECT_EQ(kRed, pixels[1 * 8 + 1]);
  EXPECT_EQ(kBlue, pixels[2 * 8 + 2]);
  EXPECT_EQ(kRed, pixels[4 * 8 + 4]);
}

TEST_P(OsrRendererD3D11Test, CleanupWithUploadsInFlight) {
  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(Paint(256, 256, std::vector<uint32_t>(256 * 256, kBlue),
                      {CefRect(0, 0, 256, 256)}));
    auto target = Target(256, 256);
    ASSERT_TRUE(renderer_->Render(target.Get()));
  }
  auto device = ComPtr<ID3D11Device>(renderer_->device());
  renderer_->Cleanup();
  renderer_->Cleanup();
  EXPECT_FALSE(renderer_->OnPaint(PET_VIEW, {}, nullptr, 4, 4));
  ASSERT_TRUE(renderer_->Initialize(device.Get()));
  ASSERT_TRUE(renderer_->Initialize(device.Get()));
  ASSERT_TRUE(Paint(4, 4, std::vector<uint32_t>(16, kRed), {}));
  EXPECT_EQ(std::vector<uint32_t>(16, kRed), Render(4, 4));
}

TEST_P(OsrRendererD3D11Test, RgbaPresentationTarget) {
  ASSERT_TRUE(Paint(8, 8, std::vector<uint32_t>(64, kRed), {}));
  EXPECT_EQ(std::vector<uint32_t>(64, kBlue),
            Render(8, 8, DXGI_FORMAT_R8G8B8A8_UNORM));
}

INSTANTIATE_TEST_SUITE_P(HardwareAndWarp,
                         OsrRendererD3D11Test,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Warp" : "Hardware";
                         });

}  // namespace
