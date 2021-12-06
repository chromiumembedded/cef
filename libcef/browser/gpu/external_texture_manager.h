// Copyright 2018 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_GPU_EXTERNAL_TEXTURE_MANAGER_H_
#define CEF_LIBCEF_BROWSER_GPU_EXTERNAL_TEXTURE_MANAGER_H_
#pragma once

#include <map>

#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/gpu_export.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {
class GLImage;
}

namespace gpu {
namespace gles2 {

class GPU_GLES2_EXPORT ExternalTextureManager {
 public:
  ExternalTextureManager();
  ~ExternalTextureManager();

  void* CreateTexture(GLuint texture_id,
                      uint32_t width,
                      uint32_t height,
                      TextureManager* tex_man);

  void LockTexture(void* handle);
  void UnlockTexture(void* handle);

  void DeleteTexture(void* handle, TextureManager* tex_man);

 private:
  using ExternalSurfaceMap = std::map<void*, scoped_refptr<gl::GLImage>>;
  ExternalSurfaceMap surfaceMap_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // CEF_LIBCEF_BROWSER_GPU_EXTERNAL_TEXTURE_MANAGER_H_
