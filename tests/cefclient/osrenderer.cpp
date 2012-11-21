// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#include "cefclient/osrenderer.h"
#include "cefclient/util.h"

#if defined(OS_WIN)
#include <gl/gl.h>
#include <gl/glu.h>
#elif defined(OS_MACOSX)
#include <OpenGL/gl.h>
#else
#error Platform is not supported.
#endif

#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif


ClientOSRenderer::ClientOSRenderer(bool transparent)
    : transparent_(transparent),
      initialized_(false),
      texture_id_(0),
      view_width_(0),
      view_height_(0),
      spin_x_(0),
      spin_y_(0) {
}

ClientOSRenderer::~ClientOSRenderer() {
  Cleanup();
}

void ClientOSRenderer::Initialize() {
  if (initialized_)
    return;

  glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

  // Necessary for non-power-of-2 textures to render correctly.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  // Create the texture.
  glGenTextures(1, &texture_id_);
  ASSERT(texture_id_ != 0);

  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  initialized_ = true;
}

void ClientOSRenderer::Cleanup() {
  if (texture_id_ != 0)
    glDeleteTextures(1, &texture_id_);
}

void ClientOSRenderer::Render() {
  if (view_width_ == 0 || view_height_ == 0)
    return;

  ASSERT(initialized_);

  struct {
    float tu, tv;
    float x, y, z;
  } static vertices[] = {
    {0.0f, 1.0f, -1.0f, -1.0f, 0.0f},
    {1.0f, 1.0f,  1.0f, -1.0f, 0.0f},
    {1.0f, 0.0f,  1.0f,  1.0f, 0.0f},
    {0.0f, 0.0f, -1.0f,  1.0f, 0.0f}
  };

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Match GL units to screen coordinates.
  glViewport(0, 0, view_width_, view_height_);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, 0, view_width_, view_height_, 0.1, 100.0);

  // Draw the background gradient.
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glBegin(GL_QUADS);
  glColor4f(1.0, 0.0, 0.0, 1.0);  // red
  glVertex2f(-1.0, -1.0);
  glVertex2f(1.0, -1.0);
  glColor4f(0.0, 0.0, 1.0, 1.0);  // blue
  glVertex2f(1.0, 1.0);
  glVertex2f(-1.0, 1.0);
  glEnd();
  glPopAttrib();

  // Rotate the view based on the mouse spin.
  if (spin_x_ != 0)
    glRotatef(-spin_x_, 1.0f, 0.0f, 0.0f);
  if (spin_y_ != 0)
    glRotatef(-spin_y_, 0.0f, 1.0f, 0.0f);

  if (transparent_) {
    // Alpha blending style. Texture values have premultiplied alpha.
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // Enable alpha blending.
    glEnable(GL_BLEND);
  }

  // Enable 2D textures.
  glEnable(GL_TEXTURE_2D);

  // Draw the facets with the texture.
  ASSERT(texture_id_ != 0);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glInterleavedArrays(GL_T2F_V3F, 0, vertices);
  glDrawArrays(GL_QUADS, 0, 4);

  // Disable 2D textures.
  glDisable(GL_TEXTURE_2D);

  if (transparent_) {
    // Disable alpha blending.
    glDisable(GL_BLEND);
  }
}

void ClientOSRenderer::OnPopupShow(CefRefPtr<CefBrowser> browser,
                                   bool show) {
  if (!show) {
    // Invalidate the previous popup rectangle so that it will be repainted.
    browser->GetHost()->Invalidate(popup_rect_);

    // Clear the popup rectangle.
    popup_rect_.Set(0, 0, 0, 0);
  }
}

void ClientOSRenderer::OnPopupSize(CefRefPtr<CefBrowser> browser,
                                   const CefRect& rect) {
  if (rect.width > 0 && rect.height > 0)
    popup_rect_ = rect;
}

void ClientOSRenderer::OnPaint(CefRefPtr<CefBrowser> browser,
                               CefRenderHandler::PaintElementType type,
                               const CefRenderHandler::RectList& dirtyRects,
                               const void* buffer, int width, int height) {
  if (!initialized_)
    Initialize();

  if (transparent_) {
    // Enable alpha blending.
    glEnable(GL_BLEND);
  }

  // Enable 2D textures.
  glEnable(GL_TEXTURE_2D);

  ASSERT(texture_id_ != 0);
  glBindTexture(GL_TEXTURE_2D, texture_id_);

  if (type == PET_VIEW) {
    int old_width = view_width_;
    int old_height = view_height_;

    view_width_ = width;
    view_height_ = height;

    glPixelStorei(GL_UNPACK_ROW_LENGTH, view_width_);

    if (old_width != view_width_ || old_height != view_height_) {
      // Update/resize the whole texture.
      glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
      glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, view_width_, view_height_, 0,
                   GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer);
    } else {
      // Update just the dirty rectangles.
      CefRenderHandler::RectList::const_iterator i = dirtyRects.begin();
      for (; i != dirtyRects.end(); ++i) {
        const CefRect& rect = *i;
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, rect.x);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, rect.y);
        glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, rect.width,
                        rect.height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                        buffer);
      }
    }
  } else if (type == PET_POPUP && popup_rect_.width > 0 &&
             popup_rect_.height > 0) {
    int skip_pixels = 0, x = popup_rect_.x;
    int skip_rows = 0, y = popup_rect_.y;
    int width = popup_rect_.width;
    int height = popup_rect_.height;

    // Adjust the popup to fit inside the view.
    if (x < 0) {
      skip_pixels = -x;
      x = 0;
    }
    if (y < 0) {
      skip_rows = -y;
      y = 0;
    }
    if (x + width > view_width_)
      width -= x + width - view_width_;
    if (y + height > view_height_)
      height -= y + height - view_height_;

    // Update the popup rectangle.
    glPixelStorei(GL_UNPACK_ROW_LENGTH, popup_rect_.width);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, skip_pixels);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, skip_rows);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_BGRA,
                    GL_UNSIGNED_INT_8_8_8_8_REV, buffer);
  }

  // Disable 2D textures.
  glDisable(GL_TEXTURE_2D);

  if (transparent_) {
    // Disable alpha blending.
    glDisable(GL_BLEND);
  }
}

void ClientOSRenderer::SetSpin(float spinX, float spinY) {
  spin_x_ = spinX;
  spin_y_ = spinY;
}

void ClientOSRenderer::IncrementSpin(float spinDX, float spinDY) {
  spin_x_ -= spinDX;
  spin_y_ -= spinDY;
}
