// Copyright 2018 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/gpu/external_texture_manager.h"

#include "gpu/command_buffer/service/service_utils.h"
#include "third_party/khronos/EGL/egl.h"
#include "third_party/khronos/EGL/eglext.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_WIN)
#include <d3d11_1.h>
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_image_dxgi.h"
#endif

#ifndef EGL_ANGLE_d3d_texture_client_buffer
#define EGL_ANGLE_d3d_texture_client_buffer 1
#define EGL_D3D_TEXTURE_ANGLE 0x33A3
#endif

namespace gpu {
namespace gles2 {

namespace {

#if BUILDFLAG(IS_WIN)

class GLImageDXGISharedHandle : public gl::GLImageDXGI {
 public:
  GLImageDXGISharedHandle(const gfx::Size& size)
      : GLImageDXGI(size, nullptr),
        handle_((HANDLE)0),
        surface_(EGL_NO_SURFACE),
        texture_id_(0) {}

  void* share_handle() const { return handle_; }

  bool Initialize() {
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        gl::QueryD3D11DeviceObjectFromANGLE();
    if (!d3d11_device) {
      return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Device1> d3d11_device1;
    HRESULT hr = d3d11_device.As(&d3d11_device1);
    if (FAILED(hr)) {
      return false;
    }

    D3D11_TEXTURE2D_DESC td = {0};
    td.ArraySize = 1;
    td.CPUAccessFlags = 0;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.Width = GetSize().width();
    td.Height = GetSize().height();
    td.MipLevels = 1;
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.MiscFlags = 0;

    hr = d3d11_device1->CreateTexture2D(&td, nullptr, texture_.GetAddressOf());
    if (FAILED(hr)) {
      return false;
    }

    // Create a staging texture that will not be a render-target, but will be
    // shared.  We could make the render target directly shareable, but the
    // staged copy is safer for synchronization and less problematic
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    hr = d3d11_device1->CreateTexture2D(&td, nullptr,
                                        staging_texture_.GetAddressOf());
    if (FAILED(hr)) {
      return false;
    }

    // If using a staging texture ... then we need the shared handle for that
    Microsoft::WRL::ComPtr<IDXGIResource> dxgi_res;
    if (staging_texture_.Get()) {
      hr = staging_texture_.As(&dxgi_res);
    } else {
      hr = texture_.As(&dxgi_res);
    }
    if (SUCCEEDED(hr)) {
      dxgi_res->GetSharedHandle(&handle_);
    }

    return true;
  }

  void Lock() {
    // In the future a keyed mutex could be utilized here.
  }

  void Unlock() {
    if (staging_texture_.Get() && texture_.Get()) {
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
      staging_texture_->GetDevice(&d3d11_device);
      if (d3d11_device.Get()) {
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_ctx;
        d3d11_device->GetImmediateContext(&d3d11_ctx);
        if (d3d11_ctx.Get()) {
          d3d11_ctx->CopyResource(staging_texture_.Get(), texture_.Get());
        }
      }
    }
  }

  void SetSurface(EGLSurface surface, GLuint texture_id) {
    surface_ = surface;
    texture_id_ = texture_id;
  }

  EGLSurface surface() const { return surface_; }

  GLuint texture_id() const { return texture_id_; }

 protected:
  ~GLImageDXGISharedHandle() override {}

 private:
  HANDLE handle_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;
  EGLSurface surface_;
  GLuint texture_id_;
};

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

ExternalTextureManager::ExternalTextureManager() {}

ExternalTextureManager::~ExternalTextureManager() {}

void* ExternalTextureManager::CreateTexture(GLuint texture_id,
                                            uint32_t width,
                                            uint32_t height,
                                            TextureManager* tex_man) {
  void* share_handle = nullptr;

#if BUILDFLAG(IS_WIN)
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  if (egl_display == EGL_NO_DISPLAY) {
    return nullptr;
  }

  EGLContext curContext = eglGetCurrentContext();
  if (curContext == EGL_NO_CONTEXT) {
    return nullptr;
  }

  gfx::Size size(width, height);
  scoped_refptr<gl::GLImage> image;
  void* texture = nullptr;

  GLImageDXGISharedHandle* dxgi_image = new GLImageDXGISharedHandle(size);
  if (!dxgi_image->Initialize()) {
    return nullptr;
  }
  image = dxgi_image;
  share_handle = dxgi_image->share_handle();
  texture = dxgi_image->texture().Get();

  if (!image) {  // this check seems unnecessary
    return nullptr;
  }

  EGLint numConfigs = 0;
  EGLint configAttrs[] = {
      EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES3_BIT,  // must remain in this position for ES2 fallback
      EGL_SURFACE_TYPE,
      EGL_PBUFFER_BIT,
      EGL_BUFFER_SIZE,
      32,
      EGL_RED_SIZE,
      8,
      EGL_GREEN_SIZE,
      8,
      EGL_BLUE_SIZE,
      8,
      EGL_ALPHA_SIZE,
      8,
      EGL_DEPTH_SIZE,
      0,
      EGL_STENCIL_SIZE,
      0,
      EGL_SAMPLE_BUFFERS,
      0,
      EGL_NONE};

  EGLConfig config = nullptr;
  if (eglChooseConfig(egl_display, configAttrs, &config, 1, &numConfigs) !=
      EGL_TRUE) {
    return nullptr;
  }

  EGLSurface surface = EGL_NO_SURFACE;
  EGLint surfAttrs[] = {EGL_WIDTH,
                        width,
                        EGL_HEIGHT,
                        height,
                        EGL_TEXTURE_TARGET,
                        EGL_TEXTURE_2D,
                        EGL_TEXTURE_FORMAT,
                        EGL_TEXTURE_RGBA,
                        EGL_NONE};

  surface = eglCreatePbufferFromClientBuffer(egl_display, EGL_D3D_TEXTURE_ANGLE,
                                             texture, config, surfAttrs);
  if (surface == EGL_NO_SURFACE) {
    // fallback to ES2 - it could be that we're running on older hardware
    // and ES3 isn't available

    // EGL_RENDERABLE_TYPE is the bit at configAttrs[0]
    configAttrs[1] = EGL_OPENGL_ES2_BIT;
    config = nullptr;
    if (eglChooseConfig(egl_display, configAttrs, &config, 1, &numConfigs) ==
        EGL_TRUE) {
      surface = eglCreatePbufferFromClientBuffer(
          egl_display, EGL_D3D_TEXTURE_ANGLE, texture, config, surfAttrs);
    }

    // still no surface? we're done
    if (surface == EGL_NO_SURFACE) {
      return nullptr;
    }
  }

  dxgi_image->SetSurface(surface, texture_id);

  surfaceMap_[share_handle] = image;

  EGLSurface drawSurface = eglGetCurrentSurface(EGL_DRAW);
  EGLSurface readSurface = eglGetCurrentSurface(EGL_READ);

  eglMakeCurrent(egl_display, surface, surface, curContext);

  if (eglBindTexImage(egl_display, surface, EGL_BACK_BUFFER)) {
    if (tex_man) {
      TextureRef* texture_ref = tex_man->GetTexture(texture_id);
      tex_man->SetLevelInfo(texture_ref, GL_TEXTURE_2D, 0, GL_BGRA_EXT, width,
                            height, 1, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
                            gfx::Rect(size));
      tex_man->SetLevelImage(texture_ref, GL_TEXTURE_2D, 0, image.get(),
                             Texture::BOUND);
    }
  }

  eglMakeCurrent(egl_display, drawSurface, readSurface, curContext);

#endif  // BUILDFLAG(IS_WIN)

  return share_handle;
}

void ExternalTextureManager::LockTexture(void* handle) {
#if BUILDFLAG(IS_WIN)
  auto const img = surfaceMap_.find(handle);
  if (img != surfaceMap_.end()) {
    GLImageDXGISharedHandle* dxgi_image =
        reinterpret_cast<GLImageDXGISharedHandle*>(img->second.get());
    dxgi_image->Lock();
  }
#endif  // BUILDFLAG(IS_WIN)
}

void ExternalTextureManager::UnlockTexture(void* handle) {
#if BUILDFLAG(IS_WIN)
  auto const img = surfaceMap_.find(handle);
  if (img != surfaceMap_.end()) {
    GLImageDXGISharedHandle* dxgi_image =
        reinterpret_cast<GLImageDXGISharedHandle*>(img->second.get());
    dxgi_image->Unlock();
  }
#endif  // BUILDFLAG(IS_WIN)
}

void ExternalTextureManager::DeleteTexture(void* handle,
                                           TextureManager* tex_man) {
#if BUILDFLAG(IS_WIN)
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  if (egl_display == EGL_NO_DISPLAY) {
    return;
  }
  auto const img = surfaceMap_.find(handle);
  if (img == surfaceMap_.end()) {
    return;
  }

  EGLSurface surface = EGL_NO_SURFACE;
  GLuint texture_id = 0;

  GLImageDXGISharedHandle* dxgi_image =
      reinterpret_cast<GLImageDXGISharedHandle*>(img->second.get());
  surface = dxgi_image->surface();
  texture_id = dxgi_image->texture_id();

  if (surface != EGL_NO_SURFACE) {
    EGLContext curContext = eglGetCurrentContext();
    if (curContext != EGL_NO_CONTEXT) {
      EGLSurface drawSurface = eglGetCurrentSurface(EGL_DRAW);
      EGLSurface readSurface = eglGetCurrentSurface(EGL_READ);

      eglMakeCurrent(egl_display, surface, surface, curContext);

      TextureRef* texture_ref = nullptr;
      if (tex_man) {
        texture_ref = tex_man->GetTexture(texture_id);
      }

      eglReleaseTexImage(egl_display, surface, EGL_BACK_BUFFER);

      if (tex_man && texture_ref) {
        tex_man->SetLevelInfo(texture_ref, GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 1,
                              0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
        tex_man->SetLevelImage(texture_ref, GL_TEXTURE_2D, 0, nullptr,
                               Texture::UNBOUND);
      }

      eglMakeCurrent(egl_display, drawSurface, readSurface, curContext);

      eglDestroySurface(egl_display, surface);
    }
  }
  surfaceMap_.erase(img);
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace gles2
}  // namespace gpu
