// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_SHARED_BROWSER_OSR_GL_LINUX_H_
#define CEF_TESTS_SHARED_BROWSER_OSR_GL_LINUX_H_
#pragma once

#include <cstddef>
#include <cstdint>

// Minimal EGL and OpenGL declarations for off-screen rendering on Linux. EGL is
// loaded at runtime and all OpenGL functions are resolved via
// eglGetProcAddress, so no EGL/GL headers or link-time libraries are required.

namespace client {
namespace gl {

using EGLBoolean = unsigned int;
using EGLenum = unsigned int;
using EGLint = int32_t;
using EGLAttrib = intptr_t;
using EGLDisplay = void*;
using EGLConfig = void*;
using EGLContext = void*;
using EGLSurface = void*;
using EGLImage = void*;

constexpr EGLDisplay kNoDisplay = nullptr;
constexpr EGLContext kNoContext = nullptr;
constexpr EGLSurface kNoSurface = nullptr;
constexpr EGLImage kNoImage = nullptr;

constexpr EGLint kEglNone = 0x3038;
constexpr EGLint kEglSuccess = 0x3000;
constexpr EGLint kEglVendor = 0x3053;
constexpr EGLint kEglExtensions = 0x3055;
constexpr EGLint kEglRedSize = 0x3024;
constexpr EGLint kEglGreenSize = 0x3023;
constexpr EGLint kEglBlueSize = 0x3022;
constexpr EGLint kEglAlphaSize = 0x3021;
constexpr EGLint kEglNativeVisualId = 0x302E;
constexpr EGLint kEglSurfaceType = 0x3033;
constexpr EGLint kEglWindowBit = 0x0004;
constexpr EGLint kEglRenderableType = 0x3040;
constexpr EGLint kEglOpenGLBit = 0x0008;
constexpr EGLint kEglWidth = 0x3057;
constexpr EGLint kEglHeight = 0x3056;
constexpr EGLenum kEglOpenGLApi = 0x30A2;
constexpr EGLint kEglContextMajorVersion = 0x3098;
constexpr EGLint kEglContextMinorVersion = 0x30FB;
constexpr EGLint kEglContextOpenGLProfileMask = 0x30FD;
constexpr EGLint kEglContextOpenGLCoreProfileBit = 0x0001;
constexpr EGLenum kEglPlatformX11 = 0x31D5;
constexpr EGLenum kEglPlatformSurfacelessMesa = 0x31DD;
constexpr EGLenum kEglPlatformDevice = 0x313F;
constexpr EGLint kEglDeviceExt = 0x322C;
constexpr EGLint kEglDrmDeviceFileExt = 0x3233;
constexpr EGLint kEglDrmRenderNodeFileExt = 0x3377;
constexpr EGLenum kEglGLTexture2D = 0x30B1;
constexpr EGLenum kEglLinuxDmaBuf = 0x3270;
constexpr EGLint kEglLinuxDrmFourcc = 0x3271;
constexpr EGLint kEglDmaBufPlaneFd[] = {0x3272, 0x3275, 0x3278, 0x3440};
constexpr EGLint kEglDmaBufPlaneOffset[] = {0x3273, 0x3276, 0x3279, 0x3441};
constexpr EGLint kEglDmaBufPlanePitch[] = {0x3274, 0x3277, 0x327A, 0x3442};
constexpr EGLint kEglDmaBufPlaneModifierLo[] = {0x3443, 0x3445, 0x3447, 0x3449};
constexpr EGLint kEglDmaBufPlaneModifierHi[] = {0x3444, 0x3446, 0x3448, 0x344A};

// DRM formats and modifiers (drm_fourcc.h).
constexpr uint32_t kDrmFormatArgb8888 = 0x34325241;  // B,G,R,A in memory.
constexpr uint32_t kDrmFormatAbgr8888 = 0x34324241;  // R,G,B,A in memory.
constexpr uint64_t kDrmFormatModLinear = 0;
constexpr uint64_t kDrmFormatModInvalid = 0x00ffffffffffffffULL;

using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLfloat = float;
using GLchar = char;
using GLsizeiptr = intptr_t;
using GLuint64 = uint64_t;
struct GLsyncObject;
using GLsync = GLsyncObject*;

constexpr GLenum kGlNoError = 0;
constexpr GLenum kGlTexture2D = 0x0DE1;
constexpr GLenum kGlTexture0 = 0x84C0;
constexpr GLenum kGlTextureMinFilter = 0x2801;
constexpr GLenum kGlTextureMagFilter = 0x2800;
constexpr GLenum kGlTextureWrapS = 0x2802;
constexpr GLenum kGlTextureWrapT = 0x2803;
constexpr GLenum kGlLinear = 0x2601;
constexpr GLenum kGlNearest = 0x2600;
constexpr GLenum kGlClampToEdge = 0x812F;
constexpr GLenum kGlRgba = 0x1908;
constexpr GLenum kGlRgba8 = 0x8058;
constexpr GLenum kGlBgra = 0x80E1;
constexpr GLenum kGlUnsignedByte = 0x1401;
constexpr GLenum kGlUnsignedInt8888Rev = 0x8367;
constexpr GLenum kGlUnpackRowLength = 0x0CF2;
constexpr GLenum kGlUnpackSkipRows = 0x0CF3;
constexpr GLenum kGlUnpackSkipPixels = 0x0CF4;
constexpr GLenum kGlUnpackAlignment = 0x0CF5;
constexpr GLenum kGlPackAlignment = 0x0D05;
constexpr GLenum kGlFramebuffer = 0x8D40;
constexpr GLenum kGlReadFramebuffer = 0x8CA8;
constexpr GLenum kGlDrawFramebuffer = 0x8CA9;
constexpr GLenum kGlFramebufferBinding = 0x8CA6;
constexpr GLenum kGlReadFramebufferBinding = 0x8CAA;
constexpr GLenum kGlColorAttachment0 = 0x8CE0;
constexpr GLenum kGlFramebufferComplete = 0x8CD5;
constexpr GLbitfield kGlColorBufferBit = 0x00004000;
constexpr GLenum kGlBlend = 0x0BE2;
constexpr GLenum kGlScissorTest = 0x0C11;
constexpr GLenum kGlViewport = 0x0BA2;
constexpr GLenum kGlOne = 1;
constexpr GLenum kGlOneMinusSrcAlpha = 0x0303;
constexpr GLenum kGlTriangles = 0x0004;
constexpr GLenum kGlFloat = 0x1406;
constexpr GLenum kGlArrayBuffer = 0x8892;
constexpr GLenum kGlStreamDraw = 0x88E0;
constexpr GLenum kGlVertexShader = 0x8B31;
constexpr GLenum kGlFragmentShader = 0x8B30;
constexpr GLenum kGlCompileStatus = 0x8B81;
constexpr GLenum kGlLinkStatus = 0x8B82;
constexpr GLenum kGlVersion = 0x1F02;
constexpr GLenum kGlRenderer = 0x1F01;
constexpr GLenum kGlSyncGpuCommandsComplete = 0x9117;
constexpr GLbitfield kGlSyncFlushCommandsBit = 0x00000001;
constexpr GLenum kGlTimeoutExpired = 0x911B;
constexpr GLenum kGlWaitFailed = 0x911D;

// Function table. Members are null if unavailable.
struct Api {
  // EGL.
  void* (*eglGetProcAddress)(const char*);
  EGLDisplay (*eglGetPlatformDisplay)(EGLenum, void*, const EGLAttrib*);
  EGLBoolean (*eglInitialize)(EGLDisplay, EGLint*, EGLint*);
  const char* (*eglQueryString)(EGLDisplay, EGLint);
  EGLBoolean (*eglBindAPI)(EGLenum);
  EGLBoolean (
      *eglChooseConfig)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
  EGLBoolean (*eglGetConfigAttrib)(EGLDisplay, EGLConfig, EGLint, EGLint*);
  EGLContext (*eglCreateContext)(EGLDisplay,
                                 EGLConfig,
                                 EGLContext,
                                 const EGLint*);
  EGLBoolean (*eglDestroyContext)(EGLDisplay, EGLContext);
  EGLSurface (*eglCreateWindowSurface)(EGLDisplay,
                                       EGLConfig,
                                       unsigned long,
                                       const EGLint*);
  EGLBoolean (*eglDestroySurface)(EGLDisplay, EGLSurface);
  EGLBoolean (*eglMakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
  EGLBoolean (*eglSwapBuffers)(EGLDisplay, EGLSurface);
  EGLBoolean (*eglSwapInterval)(EGLDisplay, EGLint);
  EGLContext (*eglGetCurrentContext)();
  EGLDisplay (*eglGetCurrentDisplay)();
  EGLint (*eglGetError)();
  EGLImage (*eglCreateImageKHR)(EGLDisplay,
                                EGLContext,
                                EGLenum,
                                void*,
                                const EGLint*);
  EGLBoolean (*eglDestroyImageKHR)(EGLDisplay, EGLImage);
  // EGL_MESA_image_dma_buf_export (optional, used by tests).
  EGLBoolean (*eglExportDMABUFImageQueryMESA)(EGLDisplay,
                                              EGLImage,
                                              int*,
                                              int*,
                                              uint64_t*);
  EGLBoolean (
      *eglExportDMABUFImageMESA)(EGLDisplay, EGLImage, int*, EGLint*, EGLint*);

  // OpenGL (desktop 3.3 core profile).
  GLenum (*glGetError)();
  const unsigned char* (*glGetString)(GLenum);
  void (*glGetIntegerv)(GLenum, GLint*);
  void (*glGenTextures)(GLsizei, GLuint*);
  void (*glDeleteTextures)(GLsizei, const GLuint*);
  void (*glBindTexture)(GLenum, GLuint);
  void (*glActiveTexture)(GLenum);
  void (*glTexParameteri)(GLenum, GLenum, GLint);
  void (*glTexImage2D)(GLenum,
                       GLint,
                       GLint,
                       GLsizei,
                       GLsizei,
                       GLint,
                       GLenum,
                       GLenum,
                       const void*);
  void (*glTexSubImage2D)(GLenum,
                          GLint,
                          GLint,
                          GLint,
                          GLsizei,
                          GLsizei,
                          GLenum,
                          GLenum,
                          const void*);
  void (*glPixelStorei)(GLenum, GLint);
  void (*glGenFramebuffers)(GLsizei, GLuint*);
  void (*glDeleteFramebuffers)(GLsizei, const GLuint*);
  void (*glBindFramebuffer)(GLenum, GLuint);
  void (*glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
  GLenum (*glCheckFramebufferStatus)(GLenum);
  void (*glBlitFramebuffer)(GLint,
                            GLint,
                            GLint,
                            GLint,
                            GLint,
                            GLint,
                            GLint,
                            GLint,
                            GLbitfield,
                            GLenum);
  void (*glReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
  void (*glViewport)(GLint, GLint, GLsizei, GLsizei);
  void (*glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
  void (*glClear)(GLbitfield);
  void (*glEnable)(GLenum);
  void (*glDisable)(GLenum);
  void (*glBlendFunc)(GLenum, GLenum);
  GLuint (*glCreateShader)(GLenum);
  void (*glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
  void (*glCompileShader)(GLuint);
  void (*glGetShaderiv)(GLuint, GLenum, GLint*);
  void (*glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
  void (*glDeleteShader)(GLuint);
  GLuint (*glCreateProgram)();
  void (*glAttachShader)(GLuint, GLuint);
  void (*glLinkProgram)(GLuint);
  void (*glGetProgramiv)(GLuint, GLenum, GLint*);
  void (*glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
  void (*glDeleteProgram)(GLuint);
  void (*glUseProgram)(GLuint);
  GLint (*glGetUniformLocation)(GLuint, const GLchar*);
  void (*glUniform1i)(GLint, GLint);
  void (*glGenVertexArrays)(GLsizei, GLuint*);
  void (*glDeleteVertexArrays)(GLsizei, const GLuint*);
  void (*glBindVertexArray)(GLuint);
  void (*glGenBuffers)(GLsizei, GLuint*);
  void (*glDeleteBuffers)(GLsizei, const GLuint*);
  void (*glBindBuffer)(GLenum, GLuint);
  void (*glBufferData)(GLenum, GLsizeiptr, const void*, GLenum);
  void (*glVertexAttribPointer)(GLuint,
                                GLint,
                                GLenum,
                                GLboolean,
                                GLsizei,
                                const void*);
  void (*glEnableVertexAttribArray)(GLuint);
  void (*glDrawArrays)(GLenum, GLint, GLsizei);
  void (*glFlush)();
  void (*glFinish)();
  GLsync (*glFenceSync)(GLenum, GLbitfield);
  GLenum (*glClientWaitSync)(GLsync, GLbitfield, GLuint64);
  void (*glDeleteSync)(GLsync);
  // OES_EGL_image.
  void (*glEGLImageTargetTexture2DOES)(GLenum, void*);
};

// Returns the function table, loading libEGL on first use. Returns nullptr if
// libEGL or a required function is unavailable. Thread safe.
const Api* GetApi();

// Returns true if |display| (or the client extensions if |display| is
// kNoDisplay) supports |extension|.
bool HasExtension(EGLDisplay display, const char* extension);

// Returns true if |display| supports importing DMA-BUFs.
bool SupportsDmaBufImport(EGLDisplay display);

// Creates a desktop OpenGL 3.3 core profile context for |display| and
// |config| (which may be null with EGL_KHR_no_config_context). Returns
// kNoContext on failure.
EGLContext CreateContext(EGLDisplay display, EGLConfig config);

}  // namespace gl
}  // namespace client

#endif  // CEF_TESTS_SHARED_BROWSER_OSR_GL_LINUX_H_
