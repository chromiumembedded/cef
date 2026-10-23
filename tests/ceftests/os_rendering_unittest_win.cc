// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <wrl/client.h>

#include <algorithm>

#include "tests/ceftests/os_rendering_unittest_shared_texture.h"
#include "tests/ceftests/osr_renderer_test_util_win.h"
#include "tests/shared/browser/osr_renderer_d3d11_win.h"

namespace osr_unittests {

bool ReadD3D11Texture(ID3D11Texture2D* texture, std::vector<uint32_t>* pixels) {
  using Microsoft::WRL::ComPtr;
  if (!texture) {
    return false;
  }
  ComPtr<ID3D11Device> device;
  texture->GetDevice(&device);
  ComPtr<ID3D11DeviceContext> context;
  device->GetImmediateContext(&context);
  D3D11_TEXTURE2D_DESC desc = {};
  texture->GetDesc(&desc);
  if ((desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
       desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM) ||
      desc.SampleDesc.Count != 1 || desc.ArraySize != 1) {
    return false;
  }
  desc.Usage = D3D11_USAGE_STAGING;
  desc.BindFlags = desc.MiscFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  ComPtr<ID3D11Texture2D> readback;
  if (FAILED(device->CreateTexture2D(&desc, nullptr, &readback))) {
    return false;
  }
  context->CopyResource(readback.Get(), texture);
  D3D11_MAPPED_SUBRESOURCE mapped = {};
  // Map completes the GPU copy before returning the pixels to the caller.
  if (FAILED(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
    return false;
  }
  pixels->resize(static_cast<size_t>(desc.Width) * desc.Height);
  for (UINT y = 0; y < desc.Height; ++y) {
    const auto* row = reinterpret_cast<const uint32_t*>(
        static_cast<const uint8_t*>(mapped.pData) + y * mapped.RowPitch);
    std::copy_n(row, desc.Width, pixels->begin() + y * desc.Width);
  }
  context->Unmap(readback.Get(), 0);
  return true;
}

namespace {

class NativeSharedTexturePixelReader : public SharedTexturePixelReader {
 public:
  bool Initialize() override { return renderer_.Initialize(); }

  void Cleanup() override { renderer_.Cleanup(); }

  bool Read(const CefAcceleratedPaintInfo& info,
            std::vector<uint32_t>* pixels) override {
    if (!renderer_.OnAcceleratedPaint(PET_VIEW, {}, info)) {
      return false;
    }
    const CefRect visible = info.extra.visible_rect;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = visible.width;
    desc.Height = visible.height;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> target;
    return SUCCEEDED(
               renderer_.device()->CreateTexture2D(&desc, nullptr, &target)) &&
           renderer_.Render(target.Get()) &&
           ReadD3D11Texture(target.Get(), pixels);
  }

 private:
  client::OsrRendererD3D11 renderer_{0};
};

}  // namespace

std::unique_ptr<SharedTexturePixelReader> CreateSharedTexturePixelReader() {
  return std::make_unique<NativeSharedTexturePixelReader>();
}

}  // namespace osr_unittests
