// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/osr_gl_linux.h"

#include <dlfcn.h>

#include <cstring>
#include <mutex>

#include "include/base/cef_logging.h"

namespace client {
namespace gl {

namespace {

// Resolves |name| into |out|. EGL core functions are exported by libEGL;
// everything else is resolved via eglGetProcAddress.
template <typename T>
bool Resolve(void* library,
             void* (*get_proc_address)(const char*),
             const char* name,
             T& out,
             bool required = true) {
  void* address = dlsym(library, name);
  if (!address) {
    address = get_proc_address(name);
  }
  out = reinterpret_cast<T>(address);
  if (!out && required) {
    LOG(ERROR) << "OSR: Missing required EGL/GL function " << name;
  }
  return out || !required;
}

bool Load(Api& api) {
  void* library = dlopen("libEGL.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!library) {
    LOG(ERROR) << "OSR: Failed to load libEGL.so.1: " << dlerror();
    return false;
  }
  api.eglGetProcAddress = reinterpret_cast<void* (*)(const char*)>(
      dlsym(library, "eglGetProcAddress"));
  if (!api.eglGetProcAddress) {
    LOG(ERROR) << "OSR: Missing eglGetProcAddress";
    return false;
  }

  auto* gpa = api.eglGetProcAddress;
#define RESOLVE(name) Resolve(library, gpa, #name, api.name)
#define RESOLVE_OPTIONAL(name) Resolve(library, gpa, #name, api.name, false)
  const bool ok =
      RESOLVE_OPTIONAL(eglGetPlatformDisplay) && RESOLVE(eglInitialize) &&
      RESOLVE(eglQueryString) && RESOLVE(eglBindAPI) &&
      RESOLVE(eglChooseConfig) && RESOLVE(eglGetConfigAttrib) &&
      RESOLVE(eglCreateContext) && RESOLVE(eglDestroyContext) &&
      RESOLVE(eglCreateWindowSurface) && RESOLVE(eglDestroySurface) &&
      RESOLVE(eglMakeCurrent) && RESOLVE(eglSwapBuffers) &&
      RESOLVE(eglSwapInterval) && RESOLVE(eglGetCurrentContext) &&
      RESOLVE(eglGetCurrentDisplay) && RESOLVE(eglGetError) &&
      RESOLVE(eglCreateImageKHR) && RESOLVE(eglDestroyImageKHR) &&
      RESOLVE_OPTIONAL(eglExportDMABUFImageQueryMESA) &&
      RESOLVE_OPTIONAL(eglExportDMABUFImageMESA) && RESOLVE(glGetError) &&
      RESOLVE(glGetString) && RESOLVE(glGetIntegerv) &&
      RESOLVE(glGenTextures) && RESOLVE(glDeleteTextures) &&
      RESOLVE(glBindTexture) && RESOLVE(glActiveTexture) &&
      RESOLVE(glTexParameteri) && RESOLVE(glTexImage2D) &&
      RESOLVE(glTexSubImage2D) && RESOLVE(glPixelStorei) &&
      RESOLVE(glGenFramebuffers) && RESOLVE(glDeleteFramebuffers) &&
      RESOLVE(glBindFramebuffer) && RESOLVE(glFramebufferTexture2D) &&
      RESOLVE(glCheckFramebufferStatus) && RESOLVE(glBlitFramebuffer) &&
      RESOLVE(glReadPixels) && RESOLVE(glViewport) && RESOLVE(glClearColor) &&
      RESOLVE(glClear) && RESOLVE(glEnable) && RESOLVE(glDisable) &&
      RESOLVE(glBlendFunc) && RESOLVE(glCreateShader) &&
      RESOLVE(glShaderSource) && RESOLVE(glCompileShader) &&
      RESOLVE(glGetShaderiv) && RESOLVE(glGetShaderInfoLog) &&
      RESOLVE(glDeleteShader) && RESOLVE(glCreateProgram) &&
      RESOLVE(glAttachShader) && RESOLVE(glLinkProgram) &&
      RESOLVE(glGetProgramiv) && RESOLVE(glGetProgramInfoLog) &&
      RESOLVE(glDeleteProgram) && RESOLVE(glUseProgram) &&
      RESOLVE(glGetUniformLocation) && RESOLVE(glUniform1i) &&
      RESOLVE(glGenVertexArrays) && RESOLVE(glDeleteVertexArrays) &&
      RESOLVE(glBindVertexArray) && RESOLVE(glGenBuffers) &&
      RESOLVE(glDeleteBuffers) && RESOLVE(glBindBuffer) &&
      RESOLVE(glBufferData) && RESOLVE(glVertexAttribPointer) &&
      RESOLVE(glEnableVertexAttribArray) && RESOLVE(glDrawArrays) &&
      RESOLVE(glFlush) && RESOLVE(glFinish) && RESOLVE(glFenceSync) &&
      RESOLVE(glClientWaitSync) && RESOLVE(glDeleteSync) &&
      RESOLVE(glEGLImageTargetTexture2DOES);
#undef RESOLVE
#undef RESOLVE_OPTIONAL
  if (ok && !api.eglGetPlatformDisplay) {
    // EGL 1.4 with EGL_EXT_platform_base. The attribute list types differ
    // (EGLint vs EGLAttrib) but no attributes are passed.
    api.eglGetPlatformDisplay =
        reinterpret_cast<decltype(api.eglGetPlatformDisplay)>(
            gpa("eglGetPlatformDisplayEXT"));
    if (!api.eglGetPlatformDisplay) {
      LOG(ERROR) << "OSR: Missing eglGetPlatformDisplay";
      return false;
    }
  }
  // |library| remains loaded for the lifetime of the process.
  return ok;
}

}  // namespace

const Api* GetApi() {
  static Api* api = nullptr;
  static std::once_flag once;
  std::call_once(once, []() {
    auto* loaded = new Api();
    if (Load(*loaded)) {
      api = loaded;
    } else {
      delete loaded;
    }
  });
  return api;
}

bool HasExtension(EGLDisplay display, const char* extension) {
  const Api* api = GetApi();
  if (!api) {
    return false;
  }
  const char* extensions = api->eglQueryString(display, kEglExtensions);
  if (!extensions) {
    return false;
  }
  const size_t length = strlen(extension);
  for (const char* p = extensions; (p = strstr(p, extension)); p += length) {
    if ((p == extensions || p[-1] == ' ') &&
        (p[length] == ' ' || p[length] == '\0')) {
      return true;
    }
  }
  return false;
}

bool SupportsDmaBufImport(EGLDisplay display) {
  return HasExtension(display, "EGL_EXT_image_dma_buf_import") &&
         HasExtension(display, "EGL_KHR_image_base");
}

EGLContext CreateContext(EGLDisplay display, EGLConfig config) {
  const Api* api = GetApi();
  if (!api || !api->eglBindAPI(kEglOpenGLApi)) {
    return kNoContext;
  }
  const EGLint attributes[] = {kEglContextMajorVersion,
                               3,
                               kEglContextMinorVersion,
                               3,
                               kEglContextOpenGLProfileMask,
                               kEglContextOpenGLCoreProfileBit,
                               kEglNone};
  EGLContext context =
      api->eglCreateContext(display, config, kNoContext, attributes);
  if (!context) {
    LOG(ERROR) << "OSR: eglCreateContext failed: 0x" << std::hex
               << api->eglGetError();
  }
  return context;
}

}  // namespace gl
}  // namespace client
