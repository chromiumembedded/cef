// Copyright 2018 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.
//
// Portions Copyright (c) 2018 Daktronics with the following MIT License:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_D3D11_WIN_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_D3D11_WIN_H_
#pragma once

#include <d3d11_1.h>

#include <memory>
#include <string>
#include <vector>

#include "include/base/cef_macros.h"

namespace client::d3d11 {

class Composition;
class Context;
class Effect;
class Geometry;
class SwapChain;
class Texture2D;

// Basic rect for floats.
struct Rect {
  float x;
  float y;
  float width;
  float height;
};

template <class T>
class ScopedBinder {
 public:
  ScopedBinder(const std::shared_ptr<Context>& ctx,
               const std::shared_ptr<T>& target)
      : target_(target) {
    if (target_) {
      target_->bind(ctx);
    }
  }
  ~ScopedBinder() {
    if (target_) {
      target_->unbind();
    }
  }

 private:
  const std::shared_ptr<T> target_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBinder);
};

class Context {
 public:
  explicit Context(ID3D11DeviceContext*);

  void flush();

  explicit operator ID3D11DeviceContext*() { return ctx_.get(); }

 private:
  const std::shared_ptr<ID3D11DeviceContext> ctx_;
};

// Encapsulate a D3D11 Device object.
class Device {
 public:
  Device(ID3D11Device*, ID3D11DeviceContext*);

  static std::shared_ptr<Device> create();

  std::string adapter_name() const;

  explicit operator ID3D11Device*() { return device_.get(); }

  std::shared_ptr<Context> immedidate_context();

  std::shared_ptr<SwapChain> create_swapchain(HWND,
                                              int width = 0,
                                              int height = 0);

  std::shared_ptr<Geometry> create_quad(float x,
                                        float y,
                                        float width,
                                        float height,
                                        bool flip = false);

  std::shared_ptr<Texture2D> create_texture(int width,
                                            int height,
                                            DXGI_FORMAT format,
                                            const void* data,
                                            size_t row_stride);

  std::shared_ptr<Texture2D> open_shared_texture(void*);

  // Create some basic shaders so we can draw a textured-quad.
  std::shared_ptr<Effect> create_default_effect();

  std::shared_ptr<Effect> create_effect(const std::string& vertex_code,
                                        const std::string& vertex_entry,
                                        const std::string& vertex_model,
                                        const std::string& pixel_code,
                                        const std::string& pixel_entry,
                                        const std::string& pixel_model);

 private:
  std::shared_ptr<ID3DBlob> compile_shader(const std::string& source_code,
                                           const std::string& entry_point,
                                           const std::string& model);

  HMODULE lib_compiler_;

  const std::shared_ptr<ID3D11Device> device_;
  const std::shared_ptr<Context> ctx_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

// Encapsulate a DXGI swapchain for a window.
class SwapChain {
 public:
  SwapChain(IDXGISwapChain*,
            ID3D11RenderTargetView*,
            ID3D11SamplerState*,
            ID3D11BlendState*);

  void bind(const std::shared_ptr<Context>& ctx);
  void unbind();

  void clear(float red, float green, float blue, float alpha);

  void present(int sync_interval);
  void resize(int width, int height);

  int width() const { return width_; }
  int height() const { return height_; }

 private:
  const std::shared_ptr<ID3D11SamplerState> sampler_;
  const std::shared_ptr<ID3D11BlendState> blender_;
  const std::shared_ptr<IDXGISwapChain> swapchain_;
  std::shared_ptr<ID3D11RenderTargetView> rtv_;
  std::shared_ptr<Context> ctx_;
  int width_ = 0;
  int height_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SwapChain);
};

class Texture2D {
 public:
  Texture2D(ID3D11Texture2D* tex, ID3D11ShaderResourceView* srv);

  void bind(std::shared_ptr<Context> const& ctx);
  void unbind();

  uint32_t width() const;
  uint32_t height() const;
  DXGI_FORMAT format() const;

  bool has_mutex() const;

  bool lock_key(uint64_t key, uint32_t timeout_ms);
  void unlock_key(uint64_t key);

  void* share_handle() const;

  void copy_from(const std::shared_ptr<Texture2D>&);

 private:
  HANDLE share_handle_;

  const std::shared_ptr<ID3D11Texture2D> texture_;
  const std::shared_ptr<ID3D11ShaderResourceView> srv_;
  std::shared_ptr<IDXGIKeyedMutex> keyed_mutex_;
  std::shared_ptr<Context> ctx_;

  DISALLOW_COPY_AND_ASSIGN(Texture2D);
};

class Effect {
 public:
  Effect(ID3D11VertexShader* vsh,
         ID3D11PixelShader* psh,
         ID3D11InputLayout* layout);

  void bind(const std::shared_ptr<Context>& ctx);
  void unbind();

 private:
  const std::shared_ptr<ID3D11VertexShader> vsh_;
  const std::shared_ptr<ID3D11PixelShader> psh_;
  const std::shared_ptr<ID3D11InputLayout> layout_;
  std::shared_ptr<Context> ctx_;

  DISALLOW_COPY_AND_ASSIGN(Effect);
};

class Geometry {
 public:
  Geometry(D3D_PRIMITIVE_TOPOLOGY primitive,
           uint32_t vertices,
           uint32_t stride,
           ID3D11Buffer*);

  void bind(const std::shared_ptr<Context>& ctx);
  void unbind();

  void draw();

 private:
  D3D_PRIMITIVE_TOPOLOGY primitive_;
  uint32_t vertices_;
  uint32_t stride_;
  const std::shared_ptr<ID3D11Buffer> buffer_;
  std::shared_ptr<Context> ctx_;

  DISALLOW_COPY_AND_ASSIGN(Geometry);
};

// Abstraction for a 2D layer within a composition.
class Layer {
 public:
  Layer(const std::shared_ptr<Device>& device, bool flip);
  virtual ~Layer();

  void attach(const std::shared_ptr<Composition>&);

  // Uses normalized 0-1.0 coordinates.
  virtual void move(float x, float y, float width, float height);

  virtual void tick(double t);
  virtual void render(const std::shared_ptr<Context>& ctx) = 0;

  Rect bounds() const;

  std::shared_ptr<Composition> composition() const;

 protected:
  // Helper method for derived classes to draw a textured-quad.
  void render_texture(const std::shared_ptr<Context>& ctx,
                      const std::shared_ptr<Texture2D>& texture);

  const std::shared_ptr<Device> device_;
  const bool flip_;

  Rect bounds_;
  std::shared_ptr<Geometry> geometry_;
  std::shared_ptr<Effect> effect_;

 private:
  std::weak_ptr<Composition> composition_;

  DISALLOW_COPY_AND_ASSIGN(Layer);
};

// A collection of layers. Will render 1-N layers to a D3D11 device.
class Composition : public std::enable_shared_from_this<Composition> {
 public:
  explicit Composition(const std::shared_ptr<Device>& device,
                       int width = 0,
                       int height = 0);

  int width() const { return width_; }
  int height() const { return height_; }

  double fps() const;
  double time() const;

  bool is_vsync() const;

  void tick(double);
  void render(const std::shared_ptr<Context>&);

  void add_layer(const std::shared_ptr<Layer>& layer);
  bool remove_layer(const std::shared_ptr<Layer>& layer);
  void resize(bool vsync, int width, int height);

 private:
  int width_;
  int height_;
  uint32_t frame_;
  int64_t fps_start_;
  double fps_;
  double time_;
  bool vsync_ = true;

  const std::shared_ptr<Device> device_;
  std::vector<std::shared_ptr<Layer>> layers_;

  DISALLOW_COPY_AND_ASSIGN(Composition);
};

class FrameBuffer {
 public:
  explicit FrameBuffer(const std::shared_ptr<Device>& device);

  // Called in response to CEF's OnAcceleratedPaint notification.
  void on_paint(void* shared_handle);

  // Returns what should be considered the front buffer.
  std::shared_ptr<Texture2D> texture() const { return shared_buffer_; }

 private:
  const std::shared_ptr<Device> device_;
  std::shared_ptr<Texture2D> shared_buffer_;

  DISALLOW_COPY_AND_ASSIGN(FrameBuffer);
};

}  // namespace client::d3d11

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_D3D11_WIN_H_
