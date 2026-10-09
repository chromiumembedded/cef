// Copyright 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/osr_render_handler_win.h"

#include <algorithm>

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/shared/browser/util_win.h"

namespace client {
namespace {
using Microsoft::WRL::ComPtr;
}  // namespace

OsrRenderHandlerWin::OsrRenderHandlerWin(const OsrRendererSettings& settings,
                                         HWND hwnd)
    : settings_(settings),
      hwnd_(hwnd),
      renderer_(settings.background_color, settings.show_update_rect),
      weak_factory_(this) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(hwnd_);
}

OsrRenderHandlerWin::~OsrRenderHandlerWin() {
  CEF_REQUIRE_UI_THREAD();
  weak_factory_.InvalidateWeakPtrs();
  renderer_.Cleanup();
}

void OsrRenderHandlerWin::SetBrowser(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = browser;
  if (browser_ && settings_.external_begin_frame_enabled) {
    // Start the BeginFrame timer.
    Invalidate();
  }
}

void OsrRenderHandlerWin::Invalidate() {
  CEF_REQUIRE_UI_THREAD();
  if (begin_frame_pending_) {
    // The timer is already running.
    return;
  }

  // Trigger the BeginFrame timer.
  CHECK_GT(settings_.begin_frame_rate, 0);
  const float delay_us = (1.0 / double(settings_.begin_frame_rate)) * 1000000.0;
  TriggerBeginFrame(0, delay_us);
}

void OsrRenderHandlerWin::TriggerBeginFrame(uint64_t last_time_us,
                                            float delay_us) {
  if (begin_frame_pending_ && !settings_.external_begin_frame_enabled) {
    // Render immediately and then wait for the next call to Invalidate() or
    // On[Accelerated]Paint().
    begin_frame_pending_ = false;
    Render();
    return;
  }

  const auto now = GetTimeNow();
  float offset = now - last_time_us;
  if (offset > delay_us) {
    offset = delay_us;
  }

  if (!begin_frame_pending_) {
    begin_frame_pending_ = true;
  }

  // Trigger again after the necessary delay to maintain the desired frame rate.
  CefPostDelayedTask(TID_UI,
                     base::BindOnce(&OsrRenderHandlerWin::TriggerBeginFrame,
                                    weak_factory_.GetWeakPtr(), now, delay_us),
                     static_cast<int64_t>(offset / 1000.0));

  if (settings_.external_begin_frame_enabled && browser_) {
    // We're running the BeginFrame timer. Trigger rendering via
    // On[Accelerated]Paint().
    browser_->GetHost()->SendExternalBeginFrame();
  }
}

bool OsrRenderHandlerWin::Initialize(CefRefPtr<CefBrowser> browser,
                                     int width,
                                     int height) {
  CEF_REQUIRE_UI_THREAD();
  if (!renderer_.Initialize()) {
    return false;
  }
  ComPtr<IDXGIDevice> device;
  ComPtr<IDXGIAdapter> adapter;
  ComPtr<IDXGIFactory> factory;
  if (FAILED(renderer_.device()->QueryInterface(IID_PPV_ARGS(&device))) ||
      FAILED(device->GetAdapter(&adapter)) ||
      FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) {
    return false;
  }
  width_ = std::max(1, width);
  height_ = std::max(1, height);
  DXGI_SWAP_CHAIN_DESC desc = {};
  desc.BufferDesc.Width = width_;
  desc.BufferDesc.Height = height_;
  desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferDesc.RefreshRate.Numerator = 60;
  desc.BufferDesc.RefreshRate.Denominator = 1;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 1;
  desc.OutputWindow = hwnd();
  desc.Windowed = TRUE;
  // Preserve the existing single-buffer, discard presentation model.
  if (FAILED(
          factory->CreateSwapChain(renderer_.device(), &desc, &swap_chain_))) {
    return false;
  }
  factory->MakeWindowAssociation(hwnd(), DXGI_MWA_NO_ALT_ENTER);
  SetBrowser(browser);
  return true;
}

void OsrRenderHandlerWin::SetSpin(float spinX, float spinY) {
  CEF_REQUIRE_UI_THREAD();
  renderer_.SetSpin(spinX, spinY);
  Invalidate();
}

void OsrRenderHandlerWin::IncrementSpin(float spinDX, float spinDY) {
  CEF_REQUIRE_UI_THREAD();
  renderer_.IncrementSpin(spinDX, spinDY);
  Invalidate();
}

bool OsrRenderHandlerWin::IsOverPopupWidget(int x, int y) const {
  CEF_REQUIRE_UI_THREAD();
  return renderer_.popup_rect().Contains(x, y);
}

int OsrRenderHandlerWin::GetPopupXOffset() const {
  CEF_REQUIRE_UI_THREAD();
  return renderer_.original_popup_rect().x - renderer_.popup_rect().x;
}

int OsrRenderHandlerWin::GetPopupYOffset() const {
  CEF_REQUIRE_UI_THREAD();
  return renderer_.original_popup_rect().y - renderer_.popup_rect().y;
}

void OsrRenderHandlerWin::OnPopupShow(CefRefPtr<CefBrowser> browser,
                                      bool show) {
  CEF_REQUIRE_UI_THREAD();
  renderer_.OnPopupShow(show);
  if (!show) {
    Render();
  }
}

void OsrRenderHandlerWin::OnPopupSize(CefRefPtr<CefBrowser> browser,
                                      const CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();
  renderer_.OnPopupSize(rect);
}

void OsrRenderHandlerWin::OnPaint(CefRefPtr<CefBrowser> browser,
                                  CefRenderHandler::PaintElementType type,
                                  const CefRenderHandler::RectList& dirtyRects,
                                  const void* buffer,
                                  int width,
                                  int height) {
  CEF_REQUIRE_UI_THREAD();
  if (renderer_.OnPaint(type, dirtyRects, buffer, width, height)) {
    Render();
  }
}

void OsrRenderHandlerWin::OnAcceleratedPaint(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirtyRects,
    const CefAcceleratedPaintInfo& info) {
  CEF_REQUIRE_UI_THREAD();
  if (renderer_.OnAcceleratedPaint(type, dirtyRects, info)) {
    Render();
  }
}

void OsrRenderHandlerWin::Render() {
  CEF_REQUIRE_UI_THREAD();
  const auto size = renderer_.view_size();
  if (!swap_chain_ || size.width <= 0 || size.height <= 0) {
    return;
  }
  if (width_ != size.width || height_ != size.height) {
    if (FAILED(swap_chain_->ResizeBuffers(0, size.width, size.height,
                                          DXGI_FORMAT_UNKNOWN, 0))) {
      LOG(ERROR) << "D3D11 OSR swap chain resize failed";
      return;
    }
    width_ = size.width;
    height_ = size.height;
  }
  ComPtr<ID3D11Texture2D> target;
  if (SUCCEEDED(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&target))) &&
      renderer_.Render(target.Get())) {
    // Preserve immediate presentation with external begin frames, otherwise
    // synchronize to the display as before.
    swap_chain_->Present(send_begin_frame() ? 0 : 1, 0);
  }
}

}  // namespace client
