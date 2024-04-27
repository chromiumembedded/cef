// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#include "tests/cefclient/browser/osr_renderer.h"

#if defined(__clang__)
// Begin disable NSOpenGL deprecation warnings.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#if USE_SHADERS
// Expose prototypes for OpenGL shader functions.
#define GL_GLEXT_PROTOTYPES
#endif

#include <math.h>

#if defined(OS_WIN)
#include <gl/gl.h>
#elif defined(OS_MAC)
#if USE_SHADERS
#include <OpenGL/gl3.h>
#else
#include <OpenGL/gl.h>
#endif
#elif defined(OS_LINUX)
#include <GL/gl.h>
#else
#error Platform is not supported.
#endif

#include "include/base/cef_logging.h"
#include "include/wrapper/cef_helpers.h"

#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif

// DCHECK on gl errors.
#if DCHECK_IS_ON()
#define VERIFY_NO_ERROR                                                      \
  {                                                                          \
    int _gl_error = glGetError();                                            \
    DCHECK(_gl_error == GL_NO_ERROR) << "glGetError returned " << _gl_error; \
  }
#else
#define VERIFY_NO_ERROR
#endif

namespace client {

#if USE_SHADERS
namespace {

const char* kScreenVertexShader =
    "#version 330 core\n"
    "out vec2 texCoord;\n"
    "uniform mat4 transform;\n"
    "void main() {\n"
    "\tfloat x = float(((uint(gl_VertexID) + 2u) / 3u)\%2u);\n"
    "\tfloat y = float(((uint(gl_VertexID) + 1u) / 3u)\%2u);\n"
    "\tvec4 pos = vec4(-1.0f + x*2.0f, -1.0f + y*2.0f, 0.0f, 1.0f);\n"
    "\tgl_Position = transform * pos;\n"
    "\ttexCoord = vec2(x, -y);\n"
    "}";

const char* kScreenFragmentShader =
    "#version 330 core\n"
    "out vec4 fColor;\n"
    "in vec2 texCoord;\n"
    "uniform sampler2D texture;\n"
    "void main() {\n"
    "\tfColor = texture2D(texture, texCoord);\n"
    "}";

const char* kUpdateRectVertexShader =
    "#version 330 core\n"
    "layout (location = 0) in vec2 pos;\n"
    "layout (location = 1) in vec3 color;\n"
    "out vec4 vColor;\n"
    "uniform mat4 transform;\n"
    "void main() {\n"
    "\tgl_Position = transform * vec4(pos, 0.0f, 1.0f);\n"
    "\tvColor = vec4(color, 1.0f);\n"
    "}";

const char* kUpdateRectFragmentShader =
    "#version 330 core\n"
    "out vec4 fColor;\n"
    "in vec4 vColor;\n"
    "void main() {\n"
    "\tfColor = vColor;\n"
    "}";

// clang-format off
constexpr float kLineVertices[] = {
    // pos        // color
    0.0f, 0.0f,   1.0f, 0.0f, 0.0f,
    0.0f, 0.0f,   1.0f, 0.0f, 0.0f,
    0.0f, 0.0f,   1.0f, 0.0f, 0.0f,
    0.0f, 0.0f,   1.0f, 0.0f, 0.0f,
    0.0f, 0.0f,   1.0f, 0.0f, 0.0f
};
// clang-format on

using mat4x4_t = float[16];

void mat4x4_identity(mat4x4_t& matrix) {
  // row 0
  matrix[0] = 1.0f;
  matrix[1] = 0.0f;
  matrix[2] = 0.0f;
  matrix[3] = 0.0f;

  // row 1
  matrix[4] = 0.0f;
  matrix[5] = 1.0f;
  matrix[6] = 0.0f;
  matrix[7] = 0.0f;

  // row 2
  matrix[8] = 0.0f;
  matrix[9] = 0.0f;
  matrix[10] = 1.0f;
  matrix[11] = 0.0f;

  // row 3
  matrix[12] = 0.0f;
  matrix[13] = 0.0f;
  matrix[14] = 0.0f;
  matrix[15] = 1.0f;
}

void mat4x4_rotate(mat4x4_t& matrix, float angle, float x, float y, float z) {
  float c, s, t;
  float length, theta;

  // degrees to radians
  theta = angle * (M_PI / 180.0f);

  // normalize
  length = sqrtf(x * x + y * y + z * z);

  // too close to 0, can't make normalized vector
  if (length < 0.0001f) {
    return;
  }

  x /= length;
  y /= length;
  z /= length;

  c = cosf(theta);
  s = sinf(theta);
  t = 1.0f - c;

  // row 0
  matrix[0] = t * x * x + c;
  matrix[1] = t * x * y - s * z;
  matrix[2] = t * x * z + s * y;
  matrix[3] = 0.0f;

  // row 1
  matrix[4] = t * y * x + s * z;
  matrix[5] = t * y * y + c;
  matrix[6] = t * y * z - s * x;
  matrix[7] = 0.0f;

  // row 2
  matrix[8] = t * x * z - s * y;
  matrix[9] = t * y * z + s * x;
  matrix[10] = t * z * z + c;
  matrix[11] = 0.0f;

  // row 3
  matrix[12] = 0.0f;
  matrix[13] = 0.0f;
  matrix[14] = 0.0f;
  matrix[15] = 1.0f;
}

void mat4x4_ortho(mat4x4_t& matrix,
                  float left,
                  float right,
                  float bottom,
                  float top,
                  float near,
                  float far) {
  // row 0
  matrix[0] = 2.0f / (right - left);
  matrix[1] = 0.0f;
  matrix[2] = 0.0f;
  matrix[3] = 0.0f;

  // row 1
  matrix[4] = 0.0f;
  matrix[5] = 2.0f / (top - bottom);
  matrix[6] = 0.0f;
  matrix[7] = 0.0f;

  // row 2
  matrix[8] = 0.0f;
  matrix[9] = 0.0f;
  matrix[10] = -2.0f / (far - near);
  matrix[11] = 0.0f;

  // row 3
  matrix[12] = -(right + left) / (right - left);
  matrix[13] = -(top + bottom) / (top - bottom);
  matrix[14] = -(far + near) / (far - near);
  matrix[15] = 1.0f;
}

void mat4x4_multiply(mat4x4_t& c, const mat4x4_t& a, const mat4x4_t& b) {
  // row 0
  c[0] = a[0] * b[0] + a[1] * b[4] + a[2] * b[8];
  c[1] = a[0] * b[1] + a[1] * b[5] + a[2] * b[9];
  c[2] = a[0] * b[2] + a[1] * b[6] + a[2] * b[10];
  c[3] = a[0] * b[3] + a[1] * b[7] + a[2] * b[11] + a[3];

  // row 1
  c[4] = a[4] * b[0] + a[5] * b[4] + a[6] * b[8];
  c[5] = a[4] * b[1] + a[5] * b[5] + a[6] * b[9];
  c[6] = a[4] * b[2] + a[5] * b[6] + a[6] * b[10];
  c[7] = a[4] * b[3] + a[5] * b[7] + a[6] * b[11] + a[7];

  // row 2
  c[8] = a[8] * b[0] + a[9] * b[4] + a[10] * b[8];
  c[9] = a[8] * b[1] + a[9] * b[5] + a[10] * b[9];
  c[10] = a[8] * b[2] + a[9] * b[6] + a[10] * b[10];
  c[11] = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11];

  // row 3
  c[12] = 0.0f;
  c[13] = 0.0f;
  c[14] = 0.0f;
  c[15] = 1.0f;
}

}  // namespace
#endif  // USE_SHADERS

OsrRenderer::OsrRenderer(const OsrRendererSettings& settings)
    : settings_(settings)
#if USE_SHADERS
      ,
      line_vertices_(kLineVertices, kLineVertices + std::size(kLineVertices))
#endif
{
}

OsrRenderer::~OsrRenderer() {
  Cleanup();
}

void OsrRenderer::Initialize() {
  if (initialized_) {
    return;
  }

  glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
  VERIFY_NO_ERROR;

  if (IsTransparent()) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    VERIFY_NO_ERROR;
  } else {
    glClearColor(float(CefColorGetR(settings_.background_color)) / 255.0f,
                 float(CefColorGetG(settings_.background_color)) / 255.0f,
                 float(CefColorGetB(settings_.background_color)) / 255.0f,
                 1.0f);
    VERIFY_NO_ERROR;
  }

#if USE_SHADERS
  glGenVertexArrays(1, &vao_id_);
  VERIFY_NO_ERROR;
  glGenBuffers(1, &vbo_id_);
  VERIFY_NO_ERROR;

  glBindVertexArray(vao_id_);
  VERIFY_NO_ERROR;

  glBindBuffer(GL_ARRAY_BUFFER, vbo_id_);
  VERIFY_NO_ERROR;
  glBufferData(GL_ARRAY_BUFFER, line_vertices_.size() * sizeof(float), nullptr,
               GL_STATIC_DRAW);
  VERIFY_NO_ERROR;

  int success;
  unsigned int vertex_shader_id;
  unsigned int fragment_shader_id;
  char infoLog[512];

  // create & compile screen vertex shader program
  vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
  VERIFY_NO_ERROR;
  glShaderSource(vertex_shader_id, 1, &kScreenVertexShader, NULL);
  VERIFY_NO_ERROR;
  glCompileShader(vertex_shader_id);
  VERIFY_NO_ERROR;
  glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &success);
  VERIFY_NO_ERROR;
  if (!success) {
    glGetShaderInfoLog(vertex_shader_id, 512, NULL, infoLog);
    VERIFY_NO_ERROR;
    LOG(ERROR) << "Vertex shader compile error: " << infoLog;
  }

  // create & compile screen fragment shader program
  fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
  VERIFY_NO_ERROR;
  glShaderSource(fragment_shader_id, 1, &kScreenFragmentShader, NULL);
  VERIFY_NO_ERROR;
  glCompileShader(fragment_shader_id);
  VERIFY_NO_ERROR;
  glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &success);
  VERIFY_NO_ERROR;
  if (!success) {
    glGetShaderInfoLog(fragment_shader_id, 512, NULL, infoLog);
    VERIFY_NO_ERROR;
    LOG(ERROR) << "Fragment shader compile error: " << infoLog;
  }

  // create final shader program
  screen_shader_program_id_ = glCreateProgram();
  VERIFY_NO_ERROR;
  glAttachShader(screen_shader_program_id_, vertex_shader_id);
  VERIFY_NO_ERROR;
  glAttachShader(screen_shader_program_id_, fragment_shader_id);
  VERIFY_NO_ERROR;
  glLinkProgram(screen_shader_program_id_);
  VERIFY_NO_ERROR;
  glGetProgramiv(screen_shader_program_id_, GL_LINK_STATUS, &success);
  VERIFY_NO_ERROR;
  if (!success) {
    glGetProgramInfoLog(screen_shader_program_id_, 512, NULL, infoLog);
    VERIFY_NO_ERROR;
    LOG(ERROR) << "Shader program link error: " << infoLog;
  }

  // delete the shader's as they're linked into our program now
  glDeleteShader(vertex_shader_id);
  VERIFY_NO_ERROR;
  glDeleteShader(fragment_shader_id);
  VERIFY_NO_ERROR;

  // create & compile update rect vertex shader program
  vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
  VERIFY_NO_ERROR;
  glShaderSource(vertex_shader_id, 1, &kUpdateRectVertexShader, NULL);
  VERIFY_NO_ERROR;
  glCompileShader(vertex_shader_id);
  VERIFY_NO_ERROR;
  glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &success);
  VERIFY_NO_ERROR;
  if (!success) {
    glGetShaderInfoLog(vertex_shader_id, 512, NULL, infoLog);
    VERIFY_NO_ERROR;
    LOG(ERROR) << "Vertex shader compile error: " << infoLog;
  }

  // create & compile update rect fragment shader program
  fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
  VERIFY_NO_ERROR;
  glShaderSource(fragment_shader_id, 1, &kUpdateRectFragmentShader, NULL);
  VERIFY_NO_ERROR;
  glCompileShader(fragment_shader_id);
  VERIFY_NO_ERROR;
  glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &success);
  VERIFY_NO_ERROR;
  if (!success) {
    glGetShaderInfoLog(fragment_shader_id, 512, NULL, infoLog);
    VERIFY_NO_ERROR;
    LOG(ERROR) << "Fragment shader compile error: " << infoLog;
  }

  // create final shader program
  update_rect_shader_program_id_ = glCreateProgram();
  VERIFY_NO_ERROR;
  glAttachShader(update_rect_shader_program_id_, vertex_shader_id);
  VERIFY_NO_ERROR;
  glAttachShader(update_rect_shader_program_id_, fragment_shader_id);
  VERIFY_NO_ERROR;
  glLinkProgram(update_rect_shader_program_id_);
  VERIFY_NO_ERROR;
  glGetProgramiv(update_rect_shader_program_id_, GL_LINK_STATUS, &success);
  VERIFY_NO_ERROR;
  if (!success) {
    glGetProgramInfoLog(update_rect_shader_program_id_, 512, NULL, infoLog);
    VERIFY_NO_ERROR;
    LOG(ERROR) << "Shader program link error: " << infoLog;
  }

  // delete the shader's as they're linked into our program now
  glDeleteShader(vertex_shader_id);
  VERIFY_NO_ERROR;
  glDeleteShader(fragment_shader_id);
  VERIFY_NO_ERROR;
#endif  // USE_SHADERS

  // Necessary for non-power-of-2 textures to render correctly.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  VERIFY_NO_ERROR;

  // Create the texture.
  glGenTextures(1, &texture_id_);
  VERIFY_NO_ERROR;
  DCHECK_NE(texture_id_, 0U);
  VERIFY_NO_ERROR;

  glBindTexture(GL_TEXTURE_2D, texture_id_);
  VERIFY_NO_ERROR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  VERIFY_NO_ERROR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  VERIFY_NO_ERROR;
#if USE_SHADERS
  glUseProgram(screen_shader_program_id_);
  VERIFY_NO_ERROR;
  glUniform1i(glGetUniformLocation(screen_shader_program_id_, "texture"), 0);
  VERIFY_NO_ERROR;
#else
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  VERIFY_NO_ERROR;
#endif

  initialized_ = true;
}

void OsrRenderer::Cleanup() {
  if (texture_id_ != 0) {
    glDeleteTextures(1, &texture_id_);
  }
#if USE_SHADERS
  if (vao_id_ != 0) {
    glDeleteVertexArrays(1, &vao_id_);
  }
  if (screen_shader_program_id_ != 0) {
    glDeleteProgram(screen_shader_program_id_);
  }
  if (update_rect_shader_program_id_ != 0) {
    glDeleteProgram(screen_shader_program_id_);
  }
#endif
}

void OsrRenderer::Render() {
  if (view_width_ == 0 || view_height_ == 0) {
    return;
  }

  DCHECK(initialized_);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  VERIFY_NO_ERROR;
  // Match GL units to screen coordinates.
  glViewport(0, 0, view_width_, view_height_);
  VERIFY_NO_ERROR;

#if USE_SHADERS
  mat4x4_t transformation;
  mat4x4_t rotX, rotY;

  mat4x4_identity(transformation);
  mat4x4_identity(rotX);
  mat4x4_identity(rotY);

  // Rotate the view based on the mouse spin.
  if (spin_x_ != 0) {
    mat4x4_rotate(rotX, -spin_x_, 1.0f, 0.0f, 0.0f);
  }
  if (spin_y_ != 0) {
    mat4x4_rotate(rotY, -spin_y_, 0.0f, 1.0f, 0.0f);
  }
  mat4x4_multiply(transformation, rotX, rotY);

  if (IsTransparent()) {
    // Alpha blending style. Texture values have premultiplied alpha.
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    VERIFY_NO_ERROR;

    // Enable alpha blending.
    glEnable(GL_BLEND);
    VERIFY_NO_ERROR;
  }
  glActiveTexture(GL_TEXTURE0);
  VERIFY_NO_ERROR;
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  VERIFY_NO_ERROR;

  glUseProgram(screen_shader_program_id_);
  VERIFY_NO_ERROR;
  glUniformMatrix4fv(
      glGetUniformLocation(screen_shader_program_id_, "transform"), 1, GL_FALSE,
      transformation);
  VERIFY_NO_ERROR;
  glDrawArrays(GL_TRIANGLES, 0, 6);
  VERIFY_NO_ERROR;

  if (IsTransparent()) {
    // Disable alpha blending.
    glDisable(GL_BLEND);
    VERIFY_NO_ERROR;
  }
  // Draw a rectangle around the update region.
  if (settings_.show_update_rect && !update_rect_.IsEmpty()) {
    mat4x4_t projection;
    int left = update_rect_.x;
    int right = update_rect_.x + update_rect_.width;
    int top = update_rect_.y;
    int bottom = update_rect_.y + update_rect_.height;
    float* vertices = line_vertices_.data();

#if defined(OS_LINUX)
    // Shrink the box so that top & right sides are drawn.
    top += 1;
    right -= 1;
#else
    // Shrink the box so that left & bottom sides are drawn.
    left += 1;
    bottom -= 1;
#endif

    mat4x4_ortho(projection, 0.0f, view_width_, view_height_, 0.0f, 0.0f, 1.0f);

    // v0
    vertices[0] = left;
    vertices[1] = top;

    // v1
    vertices[5] = right;
    vertices[6] = top;

    // v2
    vertices[10] = right;
    vertices[11] = bottom;

    // v3
    vertices[15] = left;
    vertices[16] = bottom;

    // v4
    vertices[20] = left;
    vertices[21] = top;

    void* ptr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    VERIFY_NO_ERROR;
    memcpy(ptr, line_vertices_.data(), line_vertices_.size() * sizeof(float));
    glUnmapBuffer(GL_ARRAY_BUFFER);
    VERIFY_NO_ERROR;

    glLineWidth(1.0f);
    VERIFY_NO_ERROR;

    glUseProgram(update_rect_shader_program_id_);
    VERIFY_NO_ERROR;
    glUniformMatrix4fv(
        glGetUniformLocation(update_rect_shader_program_id_, "transform"), 1,
        GL_FALSE, projection);
    VERIFY_NO_ERROR;
    glEnableVertexAttribArray(0);
    VERIFY_NO_ERROR;
    glEnableVertexAttribArray(1);
    VERIFY_NO_ERROR;
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)0);
    VERIFY_NO_ERROR;
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    VERIFY_NO_ERROR;
    glDrawArrays(GL_LINE_LOOP, 0, 5);
    VERIFY_NO_ERROR;
    glDisableVertexAttribArray(0);
    VERIFY_NO_ERROR;
    glDisableVertexAttribArray(1);
    VERIFY_NO_ERROR;
  }
#else  // !USE_SHADERS
  struct {
    float tu, tv;
    float x, y, z;
  } static vertices[] = {{0.0f, 1.0f, -1.0f, -1.0f, 0.0f},
                         {1.0f, 1.0f, 1.0f, -1.0f, 0.0f},
                         {1.0f, 0.0f, 1.0f, 1.0f, 0.0f},
                         {0.0f, 0.0f, -1.0f, 1.0f, 0.0f}};

  glMatrixMode(GL_MODELVIEW);
  VERIFY_NO_ERROR;
  glLoadIdentity();
  VERIFY_NO_ERROR;

  glMatrixMode(GL_PROJECTION);
  VERIFY_NO_ERROR;
  glLoadIdentity();
  VERIFY_NO_ERROR;

  // Draw the background gradient.
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  VERIFY_NO_ERROR;
  // Don't check for errors until glEnd().
  glBegin(GL_QUADS);
  glColor4f(1.0, 0.0, 0.0, 1.0);  // red
  glVertex2f(-1.0, -1.0);
  glVertex2f(1.0, -1.0);
  glColor4f(0.0, 0.0, 1.0, 1.0);  // blue
  glVertex2f(1.0, 1.0);
  glVertex2f(-1.0, 1.0);
  glEnd();
  VERIFY_NO_ERROR;
  glPopAttrib();
  VERIFY_NO_ERROR;

  // Rotate the view based on the mouse spin.
  if (spin_x_ != 0) {
    glRotatef(-spin_x_, 1.0f, 0.0f, 0.0f);
    VERIFY_NO_ERROR;
  }
  if (spin_y_ != 0) {
    glRotatef(-spin_y_, 0.0f, 1.0f, 0.0f);
    VERIFY_NO_ERROR;
  }

  if (IsTransparent()) {
    // Alpha blending style. Texture values have premultiplied alpha.
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    VERIFY_NO_ERROR;

    // Enable alpha blending.
    glEnable(GL_BLEND);
    VERIFY_NO_ERROR;
  }

  // Enable 2D textures.
  glEnable(GL_TEXTURE_2D);
  VERIFY_NO_ERROR;

  // Draw the facets with the texture.
  DCHECK_NE(texture_id_, 0U);
  VERIFY_NO_ERROR;
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  VERIFY_NO_ERROR;
  glInterleavedArrays(GL_T2F_V3F, 0, vertices);
  VERIFY_NO_ERROR;
  glDrawArrays(GL_QUADS, 0, 4);
  VERIFY_NO_ERROR;

  // Disable 2D textures.
  glDisable(GL_TEXTURE_2D);
  VERIFY_NO_ERROR;

  if (IsTransparent()) {
    // Disable alpha blending.
    glDisable(GL_BLEND);
    VERIFY_NO_ERROR;
  }

  // Draw a rectangle around the update region.
  if (settings_.show_update_rect && !update_rect_.IsEmpty()) {
    int left = update_rect_.x;
    int right = update_rect_.x + update_rect_.width;
    int top = update_rect_.y;
    int bottom = update_rect_.y + update_rect_.height;

#if defined(OS_LINUX)
    // Shrink the box so that top & right sides are drawn.
    top += 1;
    right -= 1;
#else
    // Shrink the box so that left & bottom sides are drawn.
    left += 1;
    bottom -= 1;
#endif

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    VERIFY_NO_ERROR
    glMatrixMode(GL_PROJECTION);
    VERIFY_NO_ERROR;
    glPushMatrix();
    VERIFY_NO_ERROR;
    glLoadIdentity();
    VERIFY_NO_ERROR;
    glOrtho(0, view_width_, view_height_, 0, 0, 1);
    VERIFY_NO_ERROR;

    glLineWidth(1);
    VERIFY_NO_ERROR;
    glColor3f(1.0f, 0.0f, 0.0f);
    VERIFY_NO_ERROR;
    // Don't check for errors until glEnd().
    glBegin(GL_LINE_STRIP);
    glVertex2i(left, top);
    glVertex2i(right, top);
    glVertex2i(right, bottom);
    glVertex2i(left, bottom);
    glVertex2i(left, top);
    glEnd();
    VERIFY_NO_ERROR;

    glPopMatrix();
    VERIFY_NO_ERROR;
    glPopAttrib();
    VERIFY_NO_ERROR;
  }
#endif  // !USE_SHADERS
}

void OsrRenderer::OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) {
  if (!show) {
    // Clear the popup rectangle.
    ClearPopupRects();
  }
}

void OsrRenderer::OnPopupSize(CefRefPtr<CefBrowser> browser,
                              const CefRect& rect) {
  if (rect.width <= 0 || rect.height <= 0) {
    return;
  }
  original_popup_rect_ = rect;
  popup_rect_ = GetPopupRectInWebView(original_popup_rect_);
}

CefRect OsrRenderer::GetPopupRectInWebView(const CefRect& original_rect) {
  CefRect rc(original_rect);
  // if x or y are negative, move them to 0.
  if (rc.x < 0) {
    rc.x = 0;
  }
  if (rc.y < 0) {
    rc.y = 0;
  }
  // if popup goes outside the view, try to reposition origin
  if (rc.x + rc.width > view_width_) {
    rc.x = view_width_ - rc.width;
  }
  if (rc.y + rc.height > view_height_) {
    rc.y = view_height_ - rc.height;
  }
  // if x or y became negative, move them to 0 again.
  if (rc.x < 0) {
    rc.x = 0;
  }
  if (rc.y < 0) {
    rc.y = 0;
  }
  return rc;
}

void OsrRenderer::ClearPopupRects() {
  popup_rect_.Set(0, 0, 0, 0);
  original_popup_rect_.Set(0, 0, 0, 0);
}

void OsrRenderer::OnAcceleratedPaint(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirtyRects,
    unsigned int io_surface_tex,
    int width,
    int height) {
  if (!initialized_) {
    Initialize();
  }

#if !defined(OS_WIN)
  if (width != view_width_ || height != view_height_) {
    // Width or height has changed, so proceed to update the texture
    view_width_ = width;
    view_height_ = height;

    // Rebind texture_id as the active texture
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    VERIFY_NO_ERROR;

    // Allocate a new storage for texture_id with the new dimensions
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA,
                 GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    VERIFY_NO_ERROR;
  }

  if (IsTransparent()) {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    VERIFY_NO_ERROR;

    // Enable alpha blending.
    glEnable(GL_BLEND);
    VERIFY_NO_ERROR;
  }

  GLuint framebuffer;
  glGenFramebuffers(1, &framebuffer);
  VERIFY_NO_ERROR;
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  VERIFY_NO_ERROR;
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture_id_, 0);
  VERIFY_NO_ERROR;

  // Check framebuffer status
  DCHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

  glPushAttrib(GL_ALL_ATTRIB_BITS);
  VERIFY_NO_ERROR;
  glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
  VERIFY_NO_ERROR;

  // Setup OpenGL states
  glViewport(0, 0, width, height);
  VERIFY_NO_ERROR;

  // Bind the GL_TEXTURE_RECTANGLE_ARB texture
  glActiveTexture(GL_TEXTURE0);
  VERIFY_NO_ERROR;

  glClientActiveTexture(GL_TEXTURE0);
  VERIFY_NO_ERROR;

  glMatrixMode(GL_TEXTURE);
  VERIFY_NO_ERROR;
  glPushMatrix();
  VERIFY_NO_ERROR;
  glLoadIdentity();
  VERIFY_NO_ERROR;

  glMatrixMode(GL_PROJECTION);
  VERIFY_NO_ERROR;
  glPushMatrix();
  VERIFY_NO_ERROR;
  glLoadIdentity();
  VERIFY_NO_ERROR;
  glOrtho(0, width, 0, height, -1, 1);
  VERIFY_NO_ERROR;

  glMatrixMode(GL_MODELVIEW);
  VERIFY_NO_ERROR;
  glPushMatrix();
  VERIFY_NO_ERROR;
  glLoadIdentity();
  VERIFY_NO_ERROR;

  // rectangleTexture is the GL_TEXTURE_RECTANGLE_ARB
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, io_surface_tex);
  VERIFY_NO_ERROR;

  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  VERIFY_NO_ERROR;
  glColor4f(1.0, 1.0, 1.0, 1.0);
  VERIFY_NO_ERROR;

  bool isFlipped = false;

  GLfloat tex_coords[8];

  GLfloat texOriginX = 0;
  GLfloat texOriginY = 0;
  GLfloat texExtentX = width;
  GLfloat texExtentY = height;

  // X
  tex_coords[0] = texOriginX;
  tex_coords[2] = texOriginX;
  tex_coords[4] = texExtentX;
  tex_coords[6] = texExtentX;

  // Y
  if (!isFlipped) {
    tex_coords[1] = texOriginY;
    tex_coords[3] = texExtentY;
    tex_coords[5] = texExtentY;
    tex_coords[7] = texOriginY;
  } else {
    tex_coords[1] = texExtentY;
    tex_coords[3] = texOriginY;
    tex_coords[5] = texOriginY;
    tex_coords[7] = texExtentY;
  }

  GLfloat verts[] = {
      0.0f,         0.0f,          0.0f,         (float)height,
      (float)width, (float)height, (float)width, 0.0f,
  };

  // Ought to cache the GL_ARRAY_BUFFER_BINDING,
  // GL_ELEMENT_ARRAY_BUFFER_BINDING, set buffer to 0, and reset
  GLint arrayBuffer, elementArrayBuffer;
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementArrayBuffer);
  VERIFY_NO_ERROR;
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
  VERIFY_NO_ERROR;

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  VERIFY_NO_ERROR;
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  VERIFY_NO_ERROR;

  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  VERIFY_NO_ERROR;
  glTexCoordPointer(2, GL_FLOAT, 0, tex_coords);
  VERIFY_NO_ERROR;
  glEnableClientState(GL_VERTEX_ARRAY);
  VERIFY_NO_ERROR;
  glVertexPointer(2, GL_FLOAT, 0, verts);
  VERIFY_NO_ERROR;
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  VERIFY_NO_ERROR;

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementArrayBuffer);
  VERIFY_NO_ERROR;
  glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
  VERIFY_NO_ERROR;

  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
  VERIFY_NO_ERROR;

  // Restore OpenGL states
  glMatrixMode(GL_MODELVIEW);
  VERIFY_NO_ERROR;
  glPopMatrix();
  VERIFY_NO_ERROR;

  glMatrixMode(GL_PROJECTION);
  VERIFY_NO_ERROR;
  glPopMatrix();
  VERIFY_NO_ERROR;

  glMatrixMode(GL_TEXTURE);
  VERIFY_NO_ERROR;
  glPopMatrix();
  VERIFY_NO_ERROR;

  glPopClientAttrib();
  VERIFY_NO_ERROR;
  glPopAttrib();
  VERIFY_NO_ERROR;

  if (IsTransparent()) {
    // Enable alpha blending.
    glDisable(GL_BLEND);
    VERIFY_NO_ERROR;
  }

  // 0 unbinds the FBO, reverting to the default buffer
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  VERIFY_NO_ERROR;

  glDeleteFramebuffers(1, &framebuffer);
  VERIFY_NO_ERROR;

  // Delete the rectangle texture
  glDeleteTextures(1, &io_surface_tex);
  VERIFY_NO_ERROR;

  glDisable(GL_TEXTURE_RECTANGLE_ARB);
  VERIFY_NO_ERROR;
#endif  // !defined(OS_WIN)
}

void OsrRenderer::OnPaint(CefRefPtr<CefBrowser> browser,
                          CefRenderHandler::PaintElementType type,
                          const CefRenderHandler::RectList& dirtyRects,
                          const void* buffer,
                          int width,
                          int height) {
  if (!initialized_) {
    Initialize();
  }

  if (IsTransparent()) {
    // Enable alpha blending.
    glEnable(GL_BLEND);
    VERIFY_NO_ERROR;
  }

#if !USE_SHADERS
  // Enable 2D textures.
  glEnable(GL_TEXTURE_2D);
  VERIFY_NO_ERROR;
#endif

  DCHECK_NE(texture_id_, 0U);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  VERIFY_NO_ERROR;

  if (type == PET_VIEW) {
    int old_width = view_width_;
    int old_height = view_height_;

    view_width_ = width;
    view_height_ = height;

    if (settings_.show_update_rect) {
      update_rect_ = dirtyRects[0];
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, view_width_);
    VERIFY_NO_ERROR;

    if (old_width != view_width_ || old_height != view_height_ ||
        (dirtyRects.size() == 1 &&
         dirtyRects[0] == CefRect(0, 0, view_width_, view_height_))) {
      // Update/resize the whole texture.
      glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
      VERIFY_NO_ERROR;
      glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
      VERIFY_NO_ERROR;
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, view_width_, view_height_, 0,
                   GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer);
      VERIFY_NO_ERROR;
    } else {
      // Update just the dirty rectangles.
      CefRenderHandler::RectList::const_iterator i = dirtyRects.begin();
      for (; i != dirtyRects.end(); ++i) {
        const CefRect& rect = *i;
        DCHECK(rect.x + rect.width <= view_width_);
        DCHECK(rect.y + rect.height <= view_height_);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, rect.x);
        VERIFY_NO_ERROR;
        glPixelStorei(GL_UNPACK_SKIP_ROWS, rect.y);
        VERIFY_NO_ERROR;
        glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, rect.width,
                        rect.height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                        buffer);
        VERIFY_NO_ERROR;
      }
    }
  } else if (type == PET_POPUP && popup_rect_.width > 0 &&
             popup_rect_.height > 0) {
    int skip_pixels = 0, x = popup_rect_.x;
    int skip_rows = 0, y = popup_rect_.y;
    int w = width;
    int h = height;

    // Adjust the popup to fit inside the view.
    if (x < 0) {
      skip_pixels = -x;
      x = 0;
    }
    if (y < 0) {
      skip_rows = -y;
      y = 0;
    }
    if (x + w > view_width_) {
      w -= x + w - view_width_;
    }
    if (y + h > view_height_) {
      h -= y + h - view_height_;
    }

    // Update the popup rectangle.
    glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
    VERIFY_NO_ERROR;
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, skip_pixels);
    VERIFY_NO_ERROR;
    glPixelStorei(GL_UNPACK_SKIP_ROWS, skip_rows);
    VERIFY_NO_ERROR;
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_BGRA,
                    GL_UNSIGNED_INT_8_8_8_8_REV, buffer);
    VERIFY_NO_ERROR;
  }

#if !USE_SHADERS
  // Disable 2D textures.
  glDisable(GL_TEXTURE_2D);
  VERIFY_NO_ERROR;
#endif

  if (IsTransparent()) {
    // Disable alpha blending.
    glDisable(GL_BLEND);
    VERIFY_NO_ERROR;
  }
}

void OsrRenderer::SetSpin(float spinX, float spinY) {
  spin_x_ = spinX;
  spin_y_ = spinY;
}

void OsrRenderer::IncrementSpin(float spinDX, float spinDY) {
  spin_x_ -= spinDX;
  spin_y_ -= spinDY;
}

}  // namespace client

#if defined(__clang__)
// End disable NSOpenGL deprecation warnings.
#pragma clang diagnostic pop
#endif
