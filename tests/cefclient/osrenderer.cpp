// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#include "osrenderer.h"
#include "util.h"

#if defined(OS_WIN)
#include <gl/gl.h>
#include <gl/glu.h>
#elif defined(OS_MACOSX)
#include <OpenGL/gl.h>
#else
#error Platform is not supported.
#endif

namespace {

// Convert from BGRA to RGBA format and from upper-left to lower-left origin.
void ConvertToRGBA(const unsigned char* src, unsigned char* dst,
                   int width, int height) {
  int sp = 0, dp = (height-1) * width * 4;
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++, dp += 4, sp += 4) {
      dst[dp] = src[sp+2];  // R
      dst[dp+1] = src[sp+1];  // G
      dst[dp+2] = src[sp];  // B
      dst[dp+3] = src[sp+3];  // A
    }
    dp -= width * 8;
  }
}

void ConvertToRGBARect(const CefRect& clipRect,
                       const unsigned char* src, unsigned char* dst,
                       int width, int height) {
  int sp = 0, dp = (height-1) * width * 4;
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++, dp += 4, sp += 4) {
      if ((clipRect.x <= j) && (clipRect.x + clipRect.width > j) &&
          (clipRect.y <= i) && (clipRect.y + clipRect.height > i)) {
        dst[dp] = src[sp+2];  // R
        dst[dp+1] = src[sp+1];  // G
        dst[dp+2] = src[sp];  // B
        dst[dp+3] = src[sp+3];  // A
      }
    }
    dp -= width * 8;
  }
}

// Convert from BGRA to RGB format and from upper-left to lower-left origin.
void ConvertToRGB(const unsigned char* src, unsigned char* dst,
                  int width, int height) {
  int sp = 0, dp = (height-1) * width * 3;
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++, dp += 3, sp += 4) {
      dst[dp] = src[sp+2];  // R
      dst[dp+1] = src[sp+1];  // G
      dst[dp+2] = src[sp];  // B
    }
    dp -= width * 6;
  }
}

void ConvertToRGBRect(const CefRect& clipRect,
                      const unsigned char* src, unsigned char* dst,
                      int width, int height) {
  int sp = 0, dp = (height-1) * width * 3;
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++, dp += 3, sp += 4) {
      if ((clipRect.x <= j) && (clipRect.x + clipRect.width > j) &&
          (clipRect.y <= i) && (clipRect.y + clipRect.height > i)) {
        dst[dp] = src[sp+2];  // R
        dst[dp+1] = src[sp+1];  // G
        dst[dp+2] = src[sp];  // B
      }
    }
    dp -= width * 6;
  }
}

}  // namespace

ClientOSRenderer::ClientOSRenderer(bool transparent)
  : transparent_(transparent),
    initialized_(false),
    texture_id_(0),
    view_buffer_(NULL),
    view_buffer_size_(0),
    popup_buffer_(NULL),
    popup_buffer_size_(0),
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

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

  // Necessary for non-power-of-2 textures to render correctly.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  if (transparent_) {
    // Alpha blending style. Texture values have premultiplied alpha.
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }

  initialized_ = true;
}

void ClientOSRenderer::Cleanup() {
  if (view_buffer_) {
    delete [] view_buffer_;
    view_buffer_ = NULL;
    view_buffer_size_ = 0;
  }
  if (popup_buffer_) {
    delete [] popup_buffer_;
    popup_buffer_ = NULL;
    popup_buffer_size_ = 0;
  }

  if (texture_id_ != 0)
    glDeleteTextures(1, &texture_id_);
}

void ClientOSRenderer::SetSize(int width, int height) {
  if (!initialized_)
    Initialize();

  // Match GL units to screen coordinates.
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, 0, width, height, 0.1, 100.0);

  if (transparent_) {
    // Enable alpha blending.
    glEnable(GL_BLEND);
  }

  // Enable 2D textures.
  glEnable(GL_TEXTURE_2D);

  GLuint new_texture_id = 0;

  // Create a new texture.
  glGenTextures(1, &new_texture_id);
  ASSERT(new_texture_id != 0);
  glBindTexture(GL_TEXTURE_2D, new_texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Start with all white contents.
  {
    int size = width * height * (transparent_?4:3);
    unsigned char* buffer = new unsigned char[size];
    memset(buffer, 255, size);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
        transparent_?GL_RGBA:GL_RGB, GL_UNSIGNED_BYTE, buffer);

    delete [] buffer;
  }

  if (texture_id_ != 0) {
    // Draw the existing view buffer to the new texture.
    DrawViewBuffer(width, height);

    // Delete the old texture.
    glDeleteTextures(1, &texture_id_);
  }

  texture_id_ = new_texture_id;

  // Disable 2D textures.
  glDisable(GL_TEXTURE_2D);

  if (transparent_) {
    // Disable alpha blending.
    glDisable(GL_BLEND);
  }
}

void ClientOSRenderer::Render() {
  ASSERT(initialized_);

  struct {
    float tu, tv;
    float x, y, z;
  } static vertices[] = {
    {0.0f, 0.0f, -1.0f, -1.0f, 0.0f},
    {1.0f, 0.0f,  1.0f, -1.0f, 0.0f},
    {1.0f, 1.0f,  1.0f,  1.0f, 0.0f},
    {0.0f, 1.0f, -1.0f,  1.0f, 0.0f}
  };

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

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
  glRotatef(-spin_x_, 1.0f, 0.0f, 0.0f);
  glRotatef(-spin_y_, 0.0f, 1.0f, 0.0f);

  if (transparent_) {
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

  glFlush();
}

void ClientOSRenderer::OnPopupShow(CefRefPtr<CefBrowser> browser,
                                   bool show) {
  if (!show) {
    // Clear the popup buffer.
    popup_rect_.Set(0, 0, 0, 0);
    if (popup_buffer_) {
      delete [] popup_buffer_;
      popup_buffer_ = NULL;
      popup_buffer_size_ = 0;
    }
  }
}

void ClientOSRenderer::OnPopupSize(CefRefPtr<CefBrowser> browser,
                                   const CefRect& rect) {
  if (rect.width > 0) {
    // Update the popup rectange. It should always be inside the view.
    ASSERT(rect.x + rect.width < view_width_ &&
          rect.y + rect.height < view_height_);
    popup_rect_ = rect;
  }
}

void ClientOSRenderer::OnPaint(CefRefPtr<CefBrowser> browser,
                               CefRenderHandler::PaintElementType type,
                               const CefRenderHandler::RectList& dirtyRects,
                               const void* buffer) {
  ASSERT(initialized_);

  // Retrieve the current size of the browser view.
  browser->GetSize(type, view_width_, view_height_);

  if (transparent_) {
    // Enable alpha blending.
    glEnable(GL_BLEND);
  }

  // Enable 2D textures.
  glEnable(GL_TEXTURE_2D);

  ASSERT(texture_id_ != 0);
  glBindTexture(GL_TEXTURE_2D, texture_id_);

  if (type == PET_VIEW) {
    SetBufferSize(view_width_, view_height_, true);

    // Paint the view.
    if (transparent_) {
      CefRenderHandler::RectList::const_iterator i = dirtyRects.begin();
      for (; i != dirtyRects.end(); ++i) {
        ConvertToRGBARect(*i, (unsigned char*)buffer, view_buffer_,
                          view_width_, view_height_);
      }
    } else {
      CefRenderHandler::RectList::const_iterator i = dirtyRects.begin();
      for (; i != dirtyRects.end(); ++i) {
        ConvertToRGBRect(*i, (unsigned char*)buffer, view_buffer_,
                          view_width_, view_height_);
      }
    }

    // Update the whole texture. This is done for simplicity instead of
    // updating just the dirty region.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, view_width_, view_height_,
      transparent_?GL_RGBA:GL_RGB, GL_UNSIGNED_BYTE, view_buffer_);
  }

  if (popup_rect_.width > 0) {
    if (type == PET_POPUP) {
      // Paint the popup.
      if (transparent_)
        SetRGBA(buffer, popup_rect_.width, popup_rect_.height, false);
      else
        SetRGB(buffer, popup_rect_.width, popup_rect_.height, false);
    }

    if (popup_buffer_) {
      // Update the popup region.
      glTexSubImage2D(GL_TEXTURE_2D, 0, popup_rect_.x,
          view_height_ - popup_rect_.y - popup_rect_.height,
          popup_rect_.width, popup_rect_.height,
          transparent_?GL_RGBA:GL_RGB,
          GL_UNSIGNED_BYTE, popup_buffer_);
    }
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

bool ClientOSRenderer::GetPixelValue(int x, int y, unsigned char& r,
                                     unsigned char& g, unsigned char& b,
                                     unsigned char& a) {
  if (!view_buffer_)
    return false;

  ASSERT(x >= 0 && x < view_width_);
  ASSERT(y >= 0 && y < view_height_);
                                       
  int pixel_bytes = transparent_ ? 4 : 3;
  int y_flipped = view_height_ - y;
  int index = (view_width_ * y_flipped * pixel_bytes) + (x * pixel_bytes);

  r = view_buffer_[index];
  g = view_buffer_[index+1];
  b = view_buffer_[index+2];
  if (transparent_)
    a = view_buffer_[index+3];

  return true;
}

void ClientOSRenderer::SetBufferSize(int width, int height, bool view) {
  int dst_size = width * height * (transparent_?4:3);

  // Allocate a new buffer if necesary.
  if (view) {
    if (dst_size > view_buffer_size_) {
      if (view_buffer_)
        delete [] view_buffer_;
      view_buffer_ = new unsigned char[dst_size];
      view_buffer_size_ = dst_size;
    }
  } else {
    if (dst_size > popup_buffer_size_) {
      if (popup_buffer_)
        delete [] popup_buffer_;
      popup_buffer_ = new unsigned char[dst_size];
      popup_buffer_size_ = dst_size;
    }
  }
}

// Set the contents of the RGBA buffer.
void ClientOSRenderer::SetRGBA(const void* src, int width, int height,
                               bool view) {
  SetBufferSize(width, height, view);
  ConvertToRGBA((unsigned char*)src, view?view_buffer_:popup_buffer_, width,
      height);
}

// Set the contents of the RGB buffer.
void ClientOSRenderer::SetRGB(const void* src, int width, int height,
                              bool view) {
  SetBufferSize(width, height, view);
  ConvertToRGB((unsigned char*)src, view?view_buffer_:popup_buffer_, width,
      height);
}

void ClientOSRenderer::DrawViewBuffer(int max_width, int max_height) {
  if (max_width < view_width_ || max_height < view_height_) {
    // The requested max size is smaller than the current view buffer.
    int width = std::min(max_width, view_width_);
    int height = std::min(max_height, view_height_);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, view_width_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    transparent_?GL_RGBA:GL_RGB, GL_UNSIGNED_BYTE,
                    view_buffer_);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  } else {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, view_width_, view_height_,
        transparent_?GL_RGBA:GL_RGB, GL_UNSIGNED_BYTE, view_buffer_);
  }
}
