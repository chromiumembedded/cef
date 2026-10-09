// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/osr_renderer_d3d11_win.h"

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>

#include "include/base/cef_logging.h"

namespace client {
namespace {

using Microsoft::WRL::ComPtr;

const char kShaders[] = R"(
struct Vertex {
  float2 position : POSITION;
  float2 uv : TEXCOORD;
  float4 color : COLOR;
};

struct Varying {
  float4 position : SV_POSITION;
  float2 uv : TEXCOORD;
  float4 color : COLOR;
};
Varying vertex_main(Vertex v) {
  Varying result;
  result.position = float4(v.position, 0, 1);
  result.uv = v.uv;
  result.color = v.color;
  return result;
}
Texture2D image : register(t0);
SamplerState image_sampler : register(s0);
float4 fragment_texture(Varying v) : SV_TARGET {
  return image.Sample(image_sampler, v.uv) * v.color;
}
float4 fragment_color(Varying v) : SV_TARGET {
  return v.color;
}
)";

CefRect Intersect(const CefRect& a, const CefRect& b) {
  if (a.IsEmpty() || b.IsEmpty()) {
    return CefRect();
  }
  const int x = std::max(a.x, b.x);
  const int y = std::max(a.y, b.y);
  const int64_t right = std::min(static_cast<int64_t>(a.x) + a.width,
                                 static_cast<int64_t>(b.x) + b.width);
  const int64_t bottom = std::min(static_cast<int64_t>(a.y) + a.height,
                                  static_cast<int64_t>(b.y) + b.height);
  if (right <= x || bottom <= y) {
    return CefRect();
  }
  // Each intersection extent is bounded by the positive input extents.
  return CefRect(x, y, static_cast<int>(right - x),
                 static_cast<int>(bottom - y));
}

struct Vertex {
  float position[2];
  float uv[2];
  float color[4];
};

struct ScopedCompiler {
  HMODULE module = LoadLibraryW(L"d3dcompiler_47.dll");
  ~ScopedCompiler() {
    if (module) {
      FreeLibrary(module);
    }
  }
};

}  // namespace

class OsrRendererD3D11::Impl {
 public:
  ~Impl() {
    if (context) {
      Wait();
      context->ClearState();
    }
  }

  struct Image {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> view;
    int width = 0;
    int height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool valid = false;
  };

  bool Wait() {
    if (!completion) {
      return false;
    }
    context->End(completion.Get());
    context->Flush();
    HRESULT result;
    while ((result = context->GetData(completion.Get(), nullptr, 0,
                                      D3D11_ASYNC_GETDATA_DONOTFLUSH)) ==
           S_FALSE) {
      if (FAILED(device->GetDeviceRemovedReason())) {
        return false;
      }
      SwitchToThread();
    }
    return result == S_OK;
  }

  bool EnsureTexture(Image& image, int width, int height, DXGI_FORMAT format) {
    if (image.texture && image.width == width && image.height == height &&
        image.format == format) {
      return true;
    }
    Image replacement;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &replacement.texture)) ||
        FAILED(device->CreateShaderResourceView(replacement.texture.Get(),
                                                nullptr, &replacement.view))) {
      return false;
    }
    replacement.width = width;
    replacement.height = height;
    replacement.format = format;
    // Keep the last complete image available for cached redraw if either
    // allocation fails during a resize or format change.
    image = std::move(replacement);
    return true;
  }

  void UpdateViewSize(int width, int height) {
    view_width = width;
    view_height = height;
    UpdatePopupRect();
  }

  void UpdatePopupRect() {
    popup_rect = original_popup_rect;
    popup_rect.x =
        std::max(0, std::min(popup_rect.x, view_width - popup_rect.width));
    popup_rect.y =
        std::max(0, std::min(popup_rect.y, view_height - popup_rect.height));
  }

  bool Draw(float left,
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
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(vertex_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                            &mapped))) {
      return false;
    }
    std::memcpy(mapped.pData, vertices, sizeof(vertices));
    context->Unmap(vertex_buffer.Get(), 0);
    context->Draw(6, 0);
    return true;
  }

  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11Device1> device1;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3D11Query> completion;
  ComPtr<ID3D11VertexShader> vertex_shader;
  ComPtr<ID3D11PixelShader> texture_shader;
  ComPtr<ID3D11PixelShader> color_shader;
  ComPtr<ID3D11InputLayout> layout;
  ComPtr<ID3D11Buffer> vertex_buffer;
  ComPtr<ID3D11SamplerState> sampler;
  ComPtr<ID3D11BlendState> blend;
  ComPtr<ID3D11RasterizerState> rasterizer;
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

OsrRendererD3D11::OsrRendererD3D11(cef_color_t background_color,
                                   bool show_update_rect)
    : background_color_(background_color),
      show_update_rect_(show_update_rect),
      impl_(std::make_unique<Impl>()) {}

OsrRendererD3D11::~OsrRendererD3D11() = default;

bool OsrRendererD3D11::Initialize(ID3D11Device* device) {
  if (impl_->vertex_shader) {
    return true;
  }
  // Build state separately so a failed initialization leaves no partial state.
  auto state = std::make_unique<Impl>();
  if (device) {
    state->device = device;
    device->GetImmediateContext(&state->context);
  } else {
    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                   0, levels, 4, D3D11_SDK_VERSION,
                                   &state->device, nullptr, &state->context);
    if (hr == E_INVALIDARG) {
      hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                             levels + 1, 3, D3D11_SDK_VERSION, &state->device,
                             nullptr, &state->context);
    }
    if (FAILED(hr)) {
      return false;
    }
  }
  // Device1 is only needed for accelerated paints (NT shared handles).
  state->device.As(&state->device1);
  D3D11_QUERY_DESC query = {D3D11_QUERY_EVENT, 0};
  if (FAILED(state->device->CreateQuery(&query, &state->completion))) {
    return false;
  }

  // Compile at runtime, like Metal/EGL, without requiring a generated shader
  // artifact or an additional import library in the binary distribution.
  ScopedCompiler compiler;
  if (!compiler.module) {
    return false;
  }
  const auto compile = reinterpret_cast<pD3DCompile>(
      GetProcAddress(compiler.module, "D3DCompile"));
  ComPtr<ID3DBlob> vertex_code, texture_code, color_code, errors;
  const auto build = [&](const char* entry, const char* profile,
                         ComPtr<ID3DBlob>& code) {
    return compile &&
           SUCCEEDED(compile(kShaders, sizeof(kShaders) - 1, nullptr, nullptr,
                             nullptr, entry, profile,
                             D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &errors));
  };
  const bool compiled = build("vertex_main", "vs_4_0", vertex_code) &&
                        build("fragment_texture", "ps_4_0", texture_code) &&
                        build("fragment_color", "ps_4_0", color_code);
  if (!compiled && errors) {
    LOG(ERROR) << "D3D11 OSR shader compilation failed: "
               << static_cast<const char*>(errors->GetBufferPointer());
  }
  if (!compiled) {
    return false;
  }
  const D3D11_INPUT_ELEMENT_DESC elements[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
  };
  if (FAILED(state->device->CreateVertexShader(
          vertex_code->GetBufferPointer(), vertex_code->GetBufferSize(),
          nullptr, &state->vertex_shader)) ||
      FAILED(state->device->CreatePixelShader(
          texture_code->GetBufferPointer(), texture_code->GetBufferSize(),
          nullptr, &state->texture_shader)) ||
      FAILED(state->device->CreatePixelShader(color_code->GetBufferPointer(),
                                              color_code->GetBufferSize(),
                                              nullptr, &state->color_shader)) ||
      FAILED(state->device->CreateInputLayout(
          elements, 3, vertex_code->GetBufferPointer(),
          vertex_code->GetBufferSize(), &state->layout))) {
    return false;
  }
  D3D11_BUFFER_DESC buffer = {};
  buffer.ByteWidth = sizeof(Vertex) * 6;
  buffer.Usage = D3D11_USAGE_DYNAMIC;
  buffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  buffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  D3D11_SAMPLER_DESC sampler = {};
  sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampler.AddressU = sampler.AddressV = sampler.AddressW =
      D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler.MaxLOD = D3D11_FLOAT32_MAX;
  D3D11_BLEND_DESC blend = {};
  auto& target = blend.RenderTarget[0];
  target.BlendEnable = TRUE;
  target.SrcBlend = target.SrcBlendAlpha = D3D11_BLEND_ONE;
  target.DestBlend = target.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
  target.BlendOp = target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
  target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  D3D11_RASTERIZER_DESC rasterizer = {};
  rasterizer.FillMode = D3D11_FILL_SOLID;
  rasterizer.CullMode = D3D11_CULL_NONE;
  rasterizer.DepthClipEnable = TRUE;
  if (FAILED(state->device->CreateBuffer(&buffer, nullptr,
                                         &state->vertex_buffer)) ||
      FAILED(state->device->CreateSamplerState(&sampler, &state->sampler)) ||
      FAILED(state->device->CreateBlendState(&blend, &state->blend)) ||
      FAILED(state->device->CreateRasterizerState(&rasterizer,
                                                  &state->rasterizer))) {
    return false;
  }
  impl_ = std::move(state);
  return true;
}

void OsrRendererD3D11::Cleanup() {
  impl_ = std::make_unique<Impl>();
}

ID3D11Device* OsrRendererD3D11::device() const {
  return impl_->device.Get();
}

CefSize OsrRendererD3D11::view_size() const {
  return CefSize(impl_->view_width, impl_->view_height);
}

bool OsrRendererD3D11::OnPaint(CefRenderHandler::PaintElementType type,
                               const CefRenderHandler::RectList& dirty_rects,
                               const void* buffer,
                               int width,
                               int height) {
  auto& state = *impl_;
  if (!state.vertex_shader || !buffer || width <= 0 || height <= 0 ||
      (type != PET_VIEW && type != PET_POPUP) ||
      (type == PET_POPUP && !state.popup_visible)) {
    return false;
  }
  auto& image = type == PET_VIEW ? state.view : state.popup;
  if (!state.EnsureTexture(image, width, height, DXGI_FORMAT_B8G8R8A8_UNORM)) {
    return false;
  }
  const CefRect bounds(0, 0, width, height);
  const auto updates =
      image.valid ? dirty_rects : CefRenderHandler::RectList{bounds};
  for (const auto& dirty : updates) {
    const auto rect = Intersect(dirty, bounds);
    if (rect.IsEmpty()) {
      continue;
    }
    const D3D11_BOX box = {static_cast<UINT>(rect.x),
                           static_cast<UINT>(rect.y),
                           0,
                           static_cast<UINT>(rect.x + rect.width),
                           static_cast<UINT>(rect.y + rect.height),
                           1};
    const auto* pixels = static_cast<const uint8_t*>(buffer) +
                         (static_cast<size_t>(rect.y) * width + rect.x) * 4;
    // UpdateSubresource snapshots client memory before returning. Its source
    // stride is the full callback image, even for a small dirty rectangle.
    state.context->UpdateSubresource(image.texture.Get(), 0, &box, pixels,
                                     width * 4, 0);
  }
  image.valid = SUCCEEDED(state.device->GetDeviceRemovedReason());
  if (type == PET_VIEW && image.valid) {
    state.UpdateViewSize(width, height);
    state.update_rects = dirty_rects;
  }
  return image.valid;
}

bool OsrRendererD3D11::OnAcceleratedPaint(
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirty_rects,
    const CefAcceleratedPaintInfo& info) {
  auto& state = *impl_;
  if (!state.vertex_shader || !state.device1 || !info.shared_texture_handle ||
      (type != PET_VIEW && type != PET_POPUP) ||
      (type == PET_POPUP && !state.popup_visible)) {
    return false;
  }
  DXGI_FORMAT format;
  switch (info.format) {
    case CEF_COLOR_TYPE_BGRA_8888:
      format = DXGI_FORMAT_B8G8R8A8_UNORM;
      break;
    case CEF_COLOR_TYPE_RGBA_8888:
      format = DXGI_FORMAT_R8G8B8A8_UNORM;
      break;
    default:
      return false;
  }
  ComPtr<ID3D11Texture2D> source;
  if (FAILED(state.device1->OpenSharedResource1(info.shared_texture_handle,
                                                IID_PPV_ARGS(&source)))) {
    return false;
  }
  D3D11_TEXTURE2D_DESC desc = {};
  source->GetDesc(&desc);
  const auto& coded = info.extra.coded_size;
  const CefRect visible = info.extra.visible_rect;
  if (coded.width <= 0 || coded.height <= 0 ||
      static_cast<UINT>(coded.width) > desc.Width ||
      static_cast<UINT>(coded.height) > desc.Height || visible.IsEmpty() ||
      Intersect(visible, CefRect(0, 0, coded.width, coded.height)) != visible ||
      desc.Format != format || desc.SampleDesc.Count != 1 ||
      desc.ArraySize != 1 || desc.MipLevels != 1) {
    return false;
  }
  auto& image = type == PET_VIEW ? state.view : state.popup;
  if (!state.EnsureTexture(image, visible.width, visible.height, format)) {
    return false;
  }
  const D3D11_BOX box = {static_cast<UINT>(visible.x),
                         static_cast<UINT>(visible.y),
                         0,
                         static_cast<UINT>(visible.x + visible.width),
                         static_cast<UINT>(visible.y + visible.height),
                         1};
  // A pooled frame's damage only describes changes from the previous capture.
  // Copy the whole visible image, including when handles alternate or frames
  // are skipped. Reopen every callback; no resource from CEF is cached.
  state.context->CopySubresourceRegion(image.texture.Get(), 0, 0, 0, 0,
                                       source.Get(), 0, &box);
  // The callback contract forbids accessing the shared resource after return.
  // Flush alone only submits work: an event query completes all reads here.
  image.valid = state.Wait();
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

void OsrRendererD3D11::OnPopupShow(bool show) {
  impl_->popup_visible = show;
  impl_->popup.valid = false;
  if (!show) {
    impl_->original_popup_rect = impl_->popup_rect = CefRect();
  }
}

void OsrRendererD3D11::OnPopupSize(const CefRect& rect) {
  if (rect.IsEmpty()) {
    impl_->popup.valid = false;
    impl_->original_popup_rect = impl_->popup_rect = CefRect();
    return;
  }
  if (rect.width != impl_->original_popup_rect.width ||
      rect.height != impl_->original_popup_rect.height) {
    impl_->popup.valid = false;
  }
  impl_->original_popup_rect = rect;
  impl_->UpdatePopupRect();
}

CefRect OsrRendererD3D11::popup_rect() const {
  return impl_->popup_rect;
}
CefRect OsrRendererD3D11::original_popup_rect() const {
  return impl_->original_popup_rect;
}

void OsrRendererD3D11::SetSpin(float spin_x, float spin_y) {
  impl_->spin_x = spin_x;
  impl_->spin_y = spin_y;
}

void OsrRendererD3D11::IncrementSpin(float spin_dx, float spin_dy) {
  impl_->spin_x -= spin_dx;
  impl_->spin_y -= spin_dy;
}

bool OsrRendererD3D11::Render(ID3D11Texture2D* target) {
  auto& state = *impl_;
  if (!state.vertex_shader || !target) {
    return false;
  }
  D3D11_TEXTURE2D_DESC desc = {};
  target->GetDesc(&desc);
  if ((desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
       desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM) ||
      desc.SampleDesc.Count != 1) {
    return false;
  }
  ComPtr<ID3D11RenderTargetView> rtv;
  if (FAILED(state.device->CreateRenderTargetView(target, nullptr, &rtv))) {
    return false;
  }
  auto* context = state.context.Get();
  ID3D11RenderTargetView* targets[] = {rtv.Get()};
  context->OMSetRenderTargets(1, targets, nullptr);
  context->OMSetBlendState(state.blend.Get(), nullptr, 0xffffffff);
  context->OMSetDepthStencilState(nullptr, 0);
  context->RSSetState(state.rasterizer.Get());
  const D3D11_VIEWPORT viewport = {
      0, 0, static_cast<float>(desc.Width), static_cast<float>(desc.Height),
      0, 1};
  context->RSSetViewports(1, &viewport);
  const float color[] = {CefColorGetR(background_color_) / 255.f,
                         CefColorGetG(background_color_) / 255.f,
                         CefColorGetB(background_color_) / 255.f, 1};
  context->ClearRenderTargetView(rtv.Get(), color);
  context->IASetInputLayout(state.layout.Get());
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ID3D11Buffer* buffers[] = {state.vertex_buffer.Get()};
  const UINT stride = sizeof(Vertex), offset = 0;
  context->IASetVertexBuffers(0, 1, buffers, &stride, &offset);
  context->VSSetShader(state.vertex_shader.Get(), nullptr, 0);
  ID3D11SamplerState* samplers[] = {state.sampler.Get()};
  context->PSSetSamplers(0, 1, samplers);
  bool success = true;
  if (state.view.valid) {
    context->PSSetShader(state.color_shader.Get(), nullptr, 0);
    success &= state.Draw(-1, 1, 1, -1, 1, 1, false, true);
    context->PSSetShader(state.texture_shader.Get(), nullptr, 0);
    ID3D11ShaderResourceView* images[] = {state.view.view.Get()};
    context->PSSetShaderResources(0, 1, images);
    success &= state.Draw(-1, 1, 1, -1, 1, 1, true);
    const float sx = 2.f / state.view_width;
    const float sy = 2.f / state.view_height;
    if (state.popup_visible && state.popup.valid) {
      CefRect rect = state.popup_rect;
      rect.width = std::min(rect.width, state.popup.width);
      rect.height = std::min(rect.height, state.popup.height);
      rect =
          Intersect(rect, CefRect(0, 0, state.view_width, state.view_height));
      if (!rect.IsEmpty()) {
        images[0] = state.popup.view.Get();
        context->PSSetShaderResources(0, 1, images);
        success &= state.Draw(
            rect.x * sx - 1, 1 - rect.y * sy, (rect.x + rect.width) * sx - 1,
            1 - (rect.y + rect.height) * sy,
            rect.width / static_cast<float>(state.popup.width),
            rect.height / static_cast<float>(state.popup.height), true);
      }
    }
    if (show_update_rect_) {
      context->PSSetShader(state.color_shader.Get(), nullptr, 0);
      for (const auto& dirty : state.update_rects) {
        const auto rect = Intersect(
            dirty, CefRect(0, 0, state.view_width, state.view_height));
        if (rect.IsEmpty()) {
          continue;
        }
        const float l = rect.x * sx - 1, r = (rect.x + rect.width) * sx - 1;
        const float t = 1 - rect.y * sy, b = 1 - (rect.y + rect.height) * sy;
        success &= state.Draw(l, t, r, t - sy, 1, 1, false, false, true);
        success &= state.Draw(l, b + sy, r, b, 1, 1, false, false, true);
        success &= state.Draw(l, t, l + sx, b, 1, 1, false, false, true);
        success &= state.Draw(r - sx, t, r, b, 1, 1, false, false, true);
      }
    }
  }
  // Do not keep references to caller-owned targets or images bound. This also
  // allows the caller to resize a swap chain immediately after rendering.
  ID3D11ShaderResourceView* empty[] = {nullptr};
  context->PSSetShaderResources(0, 1, empty);
  context->OMSetRenderTargets(0, nullptr, nullptr);
  return success && SUCCEEDED(state.device->GetDeviceRemovedReason());
}

}  // namespace client
