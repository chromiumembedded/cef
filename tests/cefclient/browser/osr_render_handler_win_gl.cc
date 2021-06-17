// Copyright 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/osr_render_handler_win_gl.h"

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/shared/browser/util_win.h"

namespace client {

namespace {

// Helper that calls wglMakeCurrent.
class ScopedGLContext {
 public:
  ScopedGLContext(HDC hdc, HGLRC hglrc, bool swap_buffers)
      : hdc_(hdc), swap_buffers_(swap_buffers) {
    BOOL result = wglMakeCurrent(hdc, hglrc);
    ALLOW_UNUSED_LOCAL(result);
    DCHECK(result);
  }
  ~ScopedGLContext() {
    BOOL result = wglMakeCurrent(nullptr, nullptr);
    DCHECK(result);
    if (swap_buffers_) {
      result = SwapBuffers(hdc_);
      DCHECK(result);
    }
  }

 private:
  const HDC hdc_;
  const bool swap_buffers_;
};

}  // namespace

OsrRenderHandlerWinGL::OsrRenderHandlerWinGL(
    const OsrRendererSettings& settings,
    HWND hwnd)
    : OsrRenderHandlerWin(settings, hwnd),
      renderer_(settings),
      hdc_(nullptr),
      hrc_(nullptr),
      painting_popup_(false) {}

void OsrRenderHandlerWinGL::Initialize(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  SetBrowser(browser);
}

OsrRenderHandlerWinGL::~OsrRenderHandlerWinGL() {
  CEF_REQUIRE_UI_THREAD();
  DisableGL();
}

void OsrRenderHandlerWinGL::SetSpin(float spinX, float spinY) {
  CEF_REQUIRE_UI_THREAD();
  renderer_.SetSpin(spinX, spinY);
  Invalidate();
}

void OsrRenderHandlerWinGL::IncrementSpin(float spinDX, float spinDY) {
  CEF_REQUIRE_UI_THREAD();
  renderer_.IncrementSpin(spinDX, spinDY);
  Invalidate();
}

bool OsrRenderHandlerWinGL::IsOverPopupWidget(int x, int y) const {
  CEF_REQUIRE_UI_THREAD();
  const CefRect& rc = renderer_.popup_rect();
  int popup_right = rc.x + rc.width;
  int popup_bottom = rc.y + rc.height;
  return (x >= rc.x) && (x < popup_right) && (y >= rc.y) && (y < popup_bottom);
}

int OsrRenderHandlerWinGL::GetPopupXOffset() const {
  CEF_REQUIRE_UI_THREAD();
  return renderer_.original_popup_rect().x - renderer_.popup_rect().x;
}

int OsrRenderHandlerWinGL::GetPopupYOffset() const {
  CEF_REQUIRE_UI_THREAD();
  return renderer_.original_popup_rect().y - renderer_.popup_rect().y;
}

void OsrRenderHandlerWinGL::OnPopupShow(CefRefPtr<CefBrowser> browser,
                                        bool show) {
  CEF_REQUIRE_UI_THREAD();

  if (!show) {
    renderer_.ClearPopupRects();
    browser->GetHost()->Invalidate(PET_VIEW);
  }

  renderer_.OnPopupShow(browser, show);
}

void OsrRenderHandlerWinGL::OnPopupSize(CefRefPtr<CefBrowser> browser,
                                        const CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();
  renderer_.OnPopupSize(browser, rect);
}

void OsrRenderHandlerWinGL::OnPaint(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirtyRects,
    const void* buffer,
    int width,
    int height) {
  CEF_REQUIRE_UI_THREAD();

  if (painting_popup_) {
    renderer_.OnPaint(browser, type, dirtyRects, buffer, width, height);
    return;
  }
  if (!hdc_) {
    EnableGL();
  }

  ScopedGLContext scoped_gl_context(hdc_, hrc_, true);
  renderer_.OnPaint(browser, type, dirtyRects, buffer, width, height);
  if (type == PET_VIEW && !renderer_.popup_rect().IsEmpty()) {
    painting_popup_ = true;
    browser->GetHost()->Invalidate(PET_POPUP);
    painting_popup_ = false;
  }
  renderer_.Render();
}

void OsrRenderHandlerWinGL::OnAcceleratedPaint(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirtyRects,
    void* share_handle) {
  // Not used with this implementation.
  NOTREACHED();
}

void OsrRenderHandlerWinGL::Render() {
  if (!hdc_) {
    EnableGL();
  }

  ScopedGLContext scoped_gl_context(hdc_, hrc_, true);
  renderer_.Render();
}

void OsrRenderHandlerWinGL::EnableGL() {
  PIXELFORMATDESCRIPTOR pfd;
  int format;

  // Get the device context.
  hdc_ = GetDC(hwnd());

  // Set the pixel format for the DC.
  ZeroMemory(&pfd, sizeof(pfd));
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 24;
  pfd.cDepthBits = 16;
  pfd.iLayerType = PFD_MAIN_PLANE;
  format = ChoosePixelFormat(hdc_, &pfd);
  SetPixelFormat(hdc_, format, &pfd);

  // Create and enable the render context.
  hrc_ = wglCreateContext(hdc_);

  ScopedGLContext scoped_gl_context(hdc_, hrc_, false);
  renderer_.Initialize();
}

void OsrRenderHandlerWinGL::DisableGL() {
  if (!hdc_) {
    return;
  }

  {
    ScopedGLContext scoped_gl_context(hdc_, hrc_, false);
    renderer_.Cleanup();
  }

  if (IsWindow(hwnd())) {
    // wglDeleteContext will make the context not current before deleting it.
    BOOL result = wglDeleteContext(hrc_);
    ALLOW_UNUSED_LOCAL(result);
    DCHECK(result);
    ReleaseDC(hwnd(), hdc_);
  }

  hdc_ = nullptr;
  hrc_ = nullptr;
}

}  // namespace client
