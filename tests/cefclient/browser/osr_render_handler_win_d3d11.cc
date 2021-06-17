// Copyright 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/osr_render_handler_win_d3d11.h"

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/shared/browser/util_win.h"

namespace client {

BrowserLayer::BrowserLayer(const std::shared_ptr<d3d11::Device>& device)
    : d3d11::Layer(device, true /* flip */) {
  frame_buffer_ = std::make_shared<d3d11::FrameBuffer>(device_);
}

void BrowserLayer::render(const std::shared_ptr<d3d11::Context>& ctx) {
  // Use the base class method to draw our texture.
  render_texture(ctx, frame_buffer_->texture());
}

void BrowserLayer::on_paint(void* share_handle) {
  frame_buffer_->on_paint(share_handle);
}

std::pair<uint32_t, uint32_t> BrowserLayer::texture_size() const {
  const auto texture = frame_buffer_->texture();
  return std::make_pair(texture->width(), texture->height());
}

PopupLayer::PopupLayer(const std::shared_ptr<d3d11::Device>& device)
    : BrowserLayer(device) {}

void PopupLayer::set_bounds(const CefRect& bounds) {
  const auto comp = composition();
  if (!comp)
    return;

  const auto outer_width = comp->width();
  const auto outer_height = comp->height();
  if (outer_width == 0 || outer_height == 0)
    return;

  original_bounds_ = bounds;
  bounds_ = bounds;

  // If x or y are negative, move them to 0.
  if (bounds_.x < 0)
    bounds_.x = 0;
  if (bounds_.y < 0)
    bounds_.y = 0;
  // If popup goes outside the view, try to reposition origin
  if (bounds_.x + bounds_.width > outer_width)
    bounds_.x = outer_width - bounds_.width;
  if (bounds_.y + bounds_.height > outer_height)
    bounds_.y = outer_height - bounds_.height;
  // If x or y became negative, move them to 0 again.
  if (bounds_.x < 0)
    bounds_.x = 0;
  if (bounds_.y < 0)
    bounds_.y = 0;

  const auto x = bounds_.x / float(outer_width);
  const auto y = bounds_.y / float(outer_height);
  const auto w = bounds_.width / float(outer_width);
  const auto h = bounds_.height / float(outer_height);
  move(x, y, w, h);
}

OsrRenderHandlerWinD3D11::OsrRenderHandlerWinD3D11(
    const OsrRendererSettings& settings,
    HWND hwnd)
    : OsrRenderHandlerWin(settings, hwnd), start_time_(0) {}

bool OsrRenderHandlerWinD3D11::Initialize(CefRefPtr<CefBrowser> browser,
                                          int width,
                                          int height) {
  CEF_REQUIRE_UI_THREAD();

  // Create a D3D11 device instance.
  device_ = d3d11::Device::create();
  DCHECK(device_);
  if (!device_)
    return false;

  // Create a D3D11 swapchain for the window.
  swap_chain_ = device_->create_swapchain(hwnd());
  DCHECK(swap_chain_);
  if (!swap_chain_)
    return false;

  // Create the browser layer.
  browser_layer_ = std::make_shared<BrowserLayer>(device_);

  // Set up the composition.
  composition_ = std::make_shared<d3d11::Composition>(device_, width, height);
  composition_->add_layer(browser_layer_);

  // Size to the whole composition.
  browser_layer_->move(0.0f, 0.0f, 1.0f, 1.0f);

  start_time_ = GetTimeNow();

  SetBrowser(browser);
  return true;
}

void OsrRenderHandlerWinD3D11::SetSpin(float spinX, float spinY) {
  CEF_REQUIRE_UI_THREAD();
  // Spin support is not implemented.
}

void OsrRenderHandlerWinD3D11::IncrementSpin(float spinDX, float spinDY) {
  CEF_REQUIRE_UI_THREAD();
  // Spin support is not implemented.
}

bool OsrRenderHandlerWinD3D11::IsOverPopupWidget(int x, int y) const {
  CEF_REQUIRE_UI_THREAD();
  return popup_layer_ && popup_layer_->contains(x, y);
}

int OsrRenderHandlerWinD3D11::GetPopupXOffset() const {
  CEF_REQUIRE_UI_THREAD();
  if (popup_layer_)
    return popup_layer_->xoffset();
  return 0;
}

int OsrRenderHandlerWinD3D11::GetPopupYOffset() const {
  CEF_REQUIRE_UI_THREAD();
  if (popup_layer_)
    return popup_layer_->yoffset();
  return 0;
}

void OsrRenderHandlerWinD3D11::OnPopupShow(CefRefPtr<CefBrowser> browser,
                                           bool show) {
  CEF_REQUIRE_UI_THREAD();

  if (show) {
    DCHECK(!popup_layer_);

    // Create a new layer.
    popup_layer_ = std::make_shared<PopupLayer>(device_);
    composition_->add_layer(popup_layer_);
  } else {
    DCHECK(popup_layer_);

    composition_->remove_layer(popup_layer_);
    popup_layer_ = nullptr;

    Render();
  }
}

void OsrRenderHandlerWinD3D11::OnPopupSize(CefRefPtr<CefBrowser> browser,
                                           const CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();
  popup_layer_->set_bounds(rect);
}

void OsrRenderHandlerWinD3D11::OnPaint(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirtyRects,
    const void* buffer,
    int width,
    int height) {
  // Not used with this implementation.
  NOTREACHED();
}

void OsrRenderHandlerWinD3D11::OnAcceleratedPaint(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirtyRects,
    void* share_handle) {
  CEF_REQUIRE_UI_THREAD();

  if (type == PET_POPUP) {
    popup_layer_->on_paint(share_handle);
  } else {
    browser_layer_->on_paint(share_handle);
  }

  Render();
}

void OsrRenderHandlerWinD3D11::Render() {
  // Update composition + layers based on time.
  const auto t = (GetTimeNow() - start_time_) / 1000000.0;
  composition_->tick(t);

  auto ctx = device_->immedidate_context();
  swap_chain_->bind(ctx);

  const auto texture_size = browser_layer_->texture_size();

  // Resize the composition and swap chain to match the texture if necessary.
  composition_->resize(!send_begin_frame(), texture_size.first,
                       texture_size.second);
  swap_chain_->resize(texture_size.first, texture_size.second);

  // Clear the render target.
  swap_chain_->clear(0.0f, 0.0f, 1.0f, 1.0f);

  // Render the scene.
  composition_->render(ctx);

  // Present to window.
  swap_chain_->present(send_begin_frame() ? 0 : 1);
}

}  // namespace client
