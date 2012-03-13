// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cefclient/osrplugin.h"

#if defined(OS_WIN)

#include <windows.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <sstream>
#include <string>
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "cefclient/cefclient.h"
#include "cefclient/client_popup_handler.h"
#include "cefclient/resource.h"
#include "cefclient/resource_util.h"
#include "cefclient/string_util.h"
#include "cefclient/util.h"

// Initialized in NP_Initialize.
NPNetscapeFuncs* g_osrbrowser = NULL;

namespace {

GLuint g_textureID = -1;
float g_spinX = 0.0f;
float g_spinY = 0.0f;
int g_width = -1, g_height = -1;
CefRefPtr<CefBrowser> g_offscreenBrowser;

// If set to true alpha transparency will be used.
bool g_offscreenTransparent = false;

#define GL_IMAGE_FORMAT (g_offscreenTransparent?GL_RGBA:GL_RGB)
#define GL_BYTE_COUNT (g_offscreenTransparent?4:3)

// Class holding pointers for the client plugin window.
class ClientPlugin {
 public:
  ClientPlugin()  {
    hWnd = NULL;
    hDC = NULL;
    hRC = NULL;
  }

  HWND hWnd;
  HDC hDC;
  HGLRC hRC;
};

// Handler for off-screen rendering windows.
class ClientOSRHandler : public CefClient,
                         public CefLifeSpanHandler,
                         public CefLoadHandler,
                         public CefRequestHandler,
                         public CefDisplayHandler,
                         public CefRenderHandler {
 public:
  explicit ClientOSRHandler(ClientPlugin* plugin)
    : plugin_(plugin),
      view_buffer_(NULL),
      view_buffer_size_(0),
      popup_buffer_(NULL),
      popup_buffer_size_(0) {
  }
  ~ClientOSRHandler() {
    if (view_buffer_)
      delete [] view_buffer_;
    if (popup_buffer_)
      delete [] popup_buffer_;
  }

  // CefClient methods
  virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefLoadHandler> GetLoadHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefRequestHandler> GetRequestHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefRenderHandler> GetRenderHandler() OVERRIDE {
    return this;
  }

  // CefLifeSpanHandler methods

  virtual bool OnBeforePopup(CefRefPtr<CefBrowser> parentBrowser,
                             const CefPopupFeatures& popupFeatures,
                             CefWindowInfo& windowInfo,
                             const CefString& url,
                             CefRefPtr<CefClient>& client,
                             CefBrowserSettings& settings) OVERRIDE {
    REQUIRE_UI_THREAD();

    windowInfo.m_bWindowRenderingDisabled = TRUE;
    client = new ClientPopupHandler(g_offscreenBrowser);
    return false;
  }

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE {
    REQUIRE_UI_THREAD();

    // Set the view size to match the plugin window size.
    browser->SetSize(PET_VIEW, g_width, g_height);

    g_offscreenBrowser = browser;
  }

  virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser)  OVERRIDE {
    g_offscreenBrowser = NULL;
  }

  // CefLoadHandler methods

  virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame) OVERRIDE {
    REQUIRE_UI_THREAD();

    if (!browser->IsPopup() && frame->IsMain()) {
      // We've just started loading a page
      SetLoading(true);
    }
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    REQUIRE_UI_THREAD();

    if (!browser->IsPopup() && frame->IsMain()) {
      // We've just finished loading a page
      SetLoading(false);
    }
  }

  // CefRequestHandler methods

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefRequest> request,
                                    CefString& redirectUrl,
                                    CefRefPtr<CefStreamReader>& resourceStream,
                                    CefRefPtr<CefResponse> response,
                                    int loadFlags) OVERRIDE {
    REQUIRE_IO_THREAD();

    std::string url = request->GetURL();
    if (url == "http://tests/transparency") {
      resourceStream = GetBinaryResourceReader(IDS_TRANSPARENCY);
      response->SetMimeType("text/html");
      response->SetStatus(200);
    }

    return false;
  }

  // CefDisplayHandler methods

  virtual void OnNavStateChange(CefRefPtr<CefBrowser> browser,
                                bool canGoBack,
                                bool canGoForward) OVERRIDE {
    REQUIRE_UI_THREAD();

    // Set the "back" and "forward" button state in the HTML.
    std::stringstream ss;
    ss << "document.getElementById('back').disabled = "
       << (canGoBack?"false":"true") << ";";
    ss << "document.getElementById('forward').disabled = "
       << (canGoForward?"false":"true") << ";";
    AppGetBrowser()->GetMainFrame()->ExecuteJavaScript(ss.str(), "", 0);
  }

  virtual void OnAddressChange(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               const CefString& url) OVERRIDE {
    REQUIRE_UI_THREAD();

    // Set the "url" value in the HTML.
    std::stringstream ss;
    std::string urlStr = url;
    StringReplace(urlStr, "'", "\\'");
    ss << "document.getElementById('url').value = '" << urlStr.c_str() << "'";
    AppGetBrowser()->GetMainFrame()->ExecuteJavaScript(ss.str(), "", 0);
  }

  virtual void OnTitleChange(CefRefPtr<CefBrowser> browser,
                             const CefString& title) OVERRIDE {
    REQUIRE_UI_THREAD();

    // Set the "title" value in the HTML.
    std::stringstream ss;
    std::string titleStr = title;
    StringReplace(titleStr, "'", "\\'");
    ss << "document.getElementById('title').innerHTML = '" <<
        titleStr.c_str() << "'";
    AppGetBrowser()->GetMainFrame()->ExecuteJavaScript(ss.str(), "", 0);
  }

  // CefRenderHandler methods

  virtual bool GetViewRect(CefRefPtr<CefBrowser> browser,
                           CefRect& rect) OVERRIDE {
    REQUIRE_UI_THREAD();

    // The simulated screen and view rectangle are the same. This is necessary
    // for popup menus to be located and sized inside the view.
    rect.x = rect.y = 0;
    rect.width = g_width;
    rect.height = g_height;
    return true;
  }

  virtual bool GetScreenRect(CefRefPtr<CefBrowser> browser,
                             CefRect& rect) OVERRIDE {
    return GetViewRect(browser, rect);
  }

  virtual bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                              int viewX,
                              int viewY,
                              int& screenX,
                              int& screenY) OVERRIDE {
    REQUIRE_UI_THREAD();

    // Convert the point from view coordinates to actual screen coordinates.
    POINT screen_pt = {viewX, viewY};
    MapWindowPoints(plugin_->hWnd, HWND_DESKTOP, &screen_pt, 1);
    screenX = screen_pt.x;
    screenY = screen_pt.y;
    return true;
  }

  virtual void OnPopupShow(CefRefPtr<CefBrowser> browser,
                           bool show) OVERRIDE {
    REQUIRE_UI_THREAD();

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

  virtual void OnPopupSize(CefRefPtr<CefBrowser> browser,
                           const CefRect& rect) OVERRIDE {
    REQUIRE_UI_THREAD();

    if (rect.width > 0) {
      // Update the popup rectange. It will always be inside the view due to
      // HandleGetRect().
      ASSERT(rect.x + rect.width < g_width &&
             rect.y + rect.height < g_height);
      popup_rect_ = rect;
    }
  }

  virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                       PaintElementType type,
                       const RectList& dirtyRects,
                       const void* buffer) OVERRIDE {
    REQUIRE_UI_THREAD();

    wglMakeCurrent(plugin_->hDC, plugin_->hRC);

    if (g_offscreenTransparent) {
      // Enable alpha blending.
      glEnable(GL_BLEND);
    }

    // Enable 2D textures.
    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, g_textureID);

    if (type == PET_VIEW) {
      SetBufferSize(g_width, g_height, true);
      // Paint the view.
      if (g_offscreenTransparent) {
        RectList::const_iterator i = dirtyRects.begin();
        for (; i != dirtyRects.end(); ++i) {
          ConvertToRGBARect(*i, (unsigned char*)buffer, view_buffer_, g_width,
                            g_height);
        }
      } else {
        RectList::const_iterator i = dirtyRects.begin();
        for (i; i != dirtyRects.end(); ++i) {
          ConvertToRGBRect(*i, (unsigned char*)buffer, view_buffer_, g_width,
                           g_height);
        }
      }

      // Update the whole texture. This is done for simplicity instead of
      // updating just the dirty region.
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_width, g_height,
          GL_IMAGE_FORMAT, GL_UNSIGNED_BYTE, view_buffer_);
    }

    if (popup_rect_.width > 0) {
      if (type == PET_POPUP) {
        // Paint the popup.
        if (g_offscreenTransparent)
          SetRGBA(buffer, popup_rect_.width, popup_rect_.height, false);
        else
          SetRGB(buffer, popup_rect_.width, popup_rect_.height, false);
      }

      if (popup_buffer_) {
        // Update the popup region.
        glTexSubImage2D(GL_TEXTURE_2D, 0, popup_rect_.x,
            g_height-popup_rect_.y-popup_rect_.height, popup_rect_.width,
            popup_rect_.height, GL_IMAGE_FORMAT, GL_UNSIGNED_BYTE,
            popup_buffer_);
      }
    }

    // Disable 2D textures.
    glDisable(GL_TEXTURE_2D);

    if (g_offscreenTransparent) {
      // Disable alpha blending.
      glDisable(GL_BLEND);
    }
  }

  virtual void OnCursorChange(CefRefPtr<CefBrowser> browser,
                              CefCursorHandle cursor) OVERRIDE {
    REQUIRE_UI_THREAD();

    // Change the plugin window's cursor.
    SetClassLong(plugin_->hWnd, GCL_HCURSOR,
        static_cast<LONG>(reinterpret_cast<LONG_PTR>(cursor)));
    SetCursor(cursor);
  }

 private:
  void SetLoading(bool isLoading) {
    // Set the "stop" and "reload" button state in the HTML.
    std::stringstream ss;
    ss << "document.getElementById('stop').disabled = "
       << (isLoading?"false":"true") << ";"
       << "document.getElementById('reload').disabled = "
       << (isLoading?"true":"false") << ";";
    AppGetBrowser()->GetMainFrame()->ExecuteJavaScript(ss.str(), "", 0);
  }

  // Size the RGB buffer.
  void SetBufferSize(int width, int height, bool view) {
    int dst_size = width * height * GL_BYTE_COUNT;

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
  void SetRGBA(const void* src, int width, int height, bool view) {
    SetBufferSize(width, height, view);
    ConvertToRGBA((unsigned char*)src, view?view_buffer_:popup_buffer_, width,
        height);
  }

  // Convert from BGRA to RGBA format and from upper-left to lower-left origin.
  static void ConvertToRGBA(const unsigned char* src, unsigned char* dst,
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

  static void ConvertToRGBARect(const CefRect& clipRect,
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

  // Set the contents of the RGB buffer.
  void SetRGB(const void* src, int width, int height, bool view) {
    SetBufferSize(width, height, view);
    ConvertToRGB((unsigned char*)src, view?view_buffer_:popup_buffer_, width,
        height);
  }

  // Convert from BGRA to RGB format and from upper-left to lower-left origin.
  static void ConvertToRGB(const unsigned char* src, unsigned char* dst,
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

  static void ConvertToRGBRect(const CefRect& clipRect,
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

  ClientPlugin* plugin_;
  unsigned char* view_buffer_;
  int view_buffer_size_;
  unsigned char* popup_buffer_;
  int popup_buffer_size_;
  CefRect popup_rect_;

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(ClientOSRPlugin);
};

// Forward declarations of functions included in this code module:
LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT message, WPARAM wParam,
                               LPARAM lParam);

// Enable GL.
void EnableGL(HWND hWnd, HDC * hDC, HGLRC * hRC) {
  PIXELFORMATDESCRIPTOR pfd;
  int format;

  // Get the device context.
  *hDC = GetDC(hWnd);

  // Set the pixel format for the DC.
  ZeroMemory(&pfd, sizeof(pfd));
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 24;
  pfd.cDepthBits = 16;
  pfd.iLayerType = PFD_MAIN_PLANE;
  format = ChoosePixelFormat(*hDC, &pfd);
  SetPixelFormat(*hDC, format, &pfd);

  // Create and enable the render context.
  *hRC = wglCreateContext(*hDC);
  wglMakeCurrent(*hDC, *hRC);

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

  // Necessary for non-power-of-2 textures to render correctly.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  if (g_offscreenTransparent) {
    // Alpha blending style.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }
}

// Disable GL.
void DisableGL(HWND hWnd, HDC hDC, HGLRC hRC) {
  // Delete the texture.
  if (g_textureID != -1)
    glDeleteTextures(1, &g_textureID);

  wglMakeCurrent(NULL, NULL);
  wglDeleteContext(hRC);
  ReleaseDC(hWnd, hDC);
}

// Size the GL view.
void SizeGL(ClientPlugin* plugin, int width, int height) {
  g_width = width;
  g_height = height;

  wglMakeCurrent(plugin->hDC, plugin->hRC);

  // Match GL units to screen coordinates.
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, 0, width, height, 0.1, 100.0);

  if (g_offscreenTransparent) {
    // Enable alpha blending.
    glEnable(GL_BLEND);
  }

  // Enable 2D textures.
  glEnable(GL_TEXTURE_2D);

  // Delete the existing exture.
  if (g_textureID != -1)
    glDeleteTextures(1, &g_textureID);

  // Create a new texture.
  glGenTextures(1, &g_textureID);
  glBindTexture(GL_TEXTURE_2D, g_textureID);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Start with all white contents.
  int size = width * height * GL_BYTE_COUNT;
  unsigned char* buffer = new unsigned char[size];
  memset(buffer, 255, size);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
      GL_IMAGE_FORMAT, GL_UNSIGNED_BYTE, buffer);

  // Disable 2D textures.
  glDisable(GL_TEXTURE_2D);

  if (g_offscreenTransparent) {
    // Disable alpha blending.
    glDisable(GL_BLEND);
  }

  delete [] buffer;

  if (g_offscreenBrowser.get())
    g_offscreenBrowser->SetSize(PET_VIEW, width, height);
}

// Render the view contents.
void RenderGL(ClientPlugin* plugin) {
  wglMakeCurrent(plugin->hDC, plugin->hRC);

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
  // glTranslatef(0.0f, 0.0f, -3.0f);

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
  glRotatef(-g_spinX, 1.0f, 0.0f, 0.0f);
  glRotatef(-g_spinY, 0.0f, 1.0f, 0.0f);

  if (g_offscreenTransparent) {
    // Enable alpha blending.
    glEnable(GL_BLEND);
  }

  // Enable 2D textures.
  glEnable(GL_TEXTURE_2D);

  // Draw the facets with the texture.
  glBindTexture(GL_TEXTURE_2D, g_textureID);
  glInterleavedArrays(GL_T2F_V3F, 0, vertices);
  glDrawArrays(GL_QUADS, 0, 4);

  // Disable 2D textures.
  glDisable(GL_TEXTURE_2D);

  if (g_offscreenTransparent) {
    // Disable alpha blending.
    glDisable(GL_BLEND);
  }

  SwapBuffers(plugin->hDC);
}

NPError NPP_NewImpl(NPMIMEType plugin_type, NPP instance, uint16 mode,
                    int16 argc, char* argn[], char* argv[],
                    NPSavedData* saved) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  ClientPlugin* plugin = new ClientPlugin;
  instance->pdata = reinterpret_cast<void*>(plugin);

  return NPERR_NO_ERROR;
}

NPError NPP_DestroyImpl(NPP instance, NPSavedData** save) {
  ClientPlugin* plugin = reinterpret_cast<ClientPlugin*>(instance->pdata);

  if (plugin) {
    if (plugin->hWnd) {
      DestroyWindow(plugin->hWnd);
      DisableGL(plugin->hWnd, plugin->hDC, plugin->hRC);
    }
    delete plugin;
  }

  return NPERR_NO_ERROR;
}

NPError NPP_SetWindowImpl(NPP instance, NPWindow* window_info) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if (window_info == NULL)
    return NPERR_GENERIC_ERROR;

  ClientPlugin* plugin = reinterpret_cast<ClientPlugin*>(instance->pdata);
  HWND parent_hwnd = reinterpret_cast<HWND>(window_info->window);

  if (plugin->hWnd == NULL) {
    WNDCLASS wc;
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // Register the window class.
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = PluginWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"ClientOSRPlugin";
    RegisterClass(&wc);

    // Create the main window.
    plugin->hWnd = CreateWindow(L"ClientOSRPlugin", L"Client OSR Plugin",
        WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0, 0, 0, parent_hwnd, NULL,
        hInstance, NULL);

    SetWindowLongPtr(plugin->hWnd, GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(plugin));

    // Enable GL drawing for the window.
    EnableGL(plugin->hWnd, &(plugin->hDC), &(plugin->hRC));

    // Create the off-screen rendering window.
    CefWindowInfo windowInfo;
    CefBrowserSettings settings;
    windowInfo.SetAsOffScreen(plugin->hWnd);
    if (g_offscreenTransparent)
      windowInfo.SetTransparentPainting(TRUE);
    CefBrowser::CreateBrowser(windowInfo, new ClientOSRHandler(plugin),
        "http://www.google.com", settings);
  }

  // Position the plugin window and make sure it's visible.
  RECT parent_rect;
  GetClientRect(parent_hwnd, &parent_rect);
  SetWindowPos(plugin->hWnd, NULL, parent_rect.left, parent_rect.top,
      parent_rect.right - parent_rect.left,
      parent_rect.bottom - parent_rect.top, SWP_SHOWWINDOW);

  UpdateWindow(plugin->hWnd);
  ShowWindow(plugin->hWnd, SW_SHOW);

  return NPERR_NO_ERROR;
}

// Plugin window procedure.
LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT message, WPARAM wParam,
                               LPARAM lParam) {
  static POINT lastMousePos, curMousePos;
  static bool mouseRotation = false;
  static bool mouseTracking = false;

  ClientPlugin* plugin =
      reinterpret_cast<ClientPlugin*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

  switch (message) {
  case WM_CREATE:
    // Start the timer that's used for redrawing.
    SetTimer(hWnd, 1, 20, NULL);
    return 0;

  case WM_DESTROY:
    // Stop the timer that's used for redrawing.
    KillTimer(hWnd, 1);

    // Explicitly close the offscreen browser and release the reference.
    g_offscreenBrowser->CloseBrowser();
    g_offscreenBrowser = NULL;
    return 0;

  case WM_TIMER:
    RenderGL(plugin);
    break;

  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
    SetCapture(hWnd);
    SetFocus(hWnd);
    if (wParam & MK_SHIFT) {
      // Start rotation effect.
      lastMousePos.x = curMousePos.x = LOWORD(lParam);
      lastMousePos.y = curMousePos.y = HIWORD(lParam);
      mouseRotation = true;
    } else {
      if (g_offscreenBrowser.get()) {
        g_offscreenBrowser->SendMouseClickEvent(LOWORD(lParam), HIWORD(lParam),
            (message == WM_LBUTTONDOWN?MBT_LEFT:MBT_RIGHT), false, 1);
      }
    }
    break;

  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
    if (GetCapture() == hWnd)
      ReleaseCapture();
    if (mouseRotation) {
      // End rotation effect.
      mouseRotation = false;
      g_spinX = 0;
      g_spinY = 0;
    } else {
      if (g_offscreenBrowser.get()) {
        g_offscreenBrowser->SendMouseClickEvent(LOWORD(lParam), HIWORD(lParam),
            (message == WM_LBUTTONUP?MBT_LEFT:MBT_RIGHT), true, 1);
      }
    }
    break;

  case WM_MOUSEMOVE:
    if (mouseRotation) {
      // Apply rotation effect.
      curMousePos.x = LOWORD(lParam);
      curMousePos.y = HIWORD(lParam);
      g_spinX -= (curMousePos.x - lastMousePos.x);
      g_spinY -= (curMousePos.y - lastMousePos.y);
      lastMousePos.x = curMousePos.x;
      lastMousePos.y = curMousePos.y;
    } else {
      if (!mouseTracking) {
        // Start tracking mouse leave. Required for the WM_MOUSELEAVE event to
        // be generated.
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hWnd;
        TrackMouseEvent(&tme);
        mouseTracking = true;
      }
      if (g_offscreenBrowser.get()) {
        g_offscreenBrowser->SendMouseMoveEvent(LOWORD(lParam), HIWORD(lParam),
            false);
      }
    }
    break;

  case WM_MOUSELEAVE:
    if (mouseTracking) {
      // Stop tracking mouse leave.
      TRACKMOUSEEVENT tme;
      tme.cbSize = sizeof(TRACKMOUSEEVENT);
      tme.dwFlags = TME_LEAVE & TME_CANCEL;
      tme.hwndTrack = hWnd;
      TrackMouseEvent(&tme);
      mouseTracking = false;
    }
    if (g_offscreenBrowser.get())
      g_offscreenBrowser->SendMouseMoveEvent(0, 0, true);
    break;

  case WM_MOUSEWHEEL:
    if (g_offscreenBrowser.get()) {
      g_offscreenBrowser->SendMouseWheelEvent(LOWORD(lParam), HIWORD(lParam),
          GET_WHEEL_DELTA_WPARAM(wParam));
    }
    break;

  case WM_SIZE: {
    int width  = LOWORD(lParam);
    int height = HIWORD(lParam);
    if (width > 0 && height > 0)
      SizeGL(plugin, width, height);
    break;
  }

  case WM_SETFOCUS:
  case WM_KILLFOCUS:
    if (g_offscreenBrowser.get())
      g_offscreenBrowser->SendFocusEvent(message == WM_SETFOCUS);
    break;

  case WM_CAPTURECHANGED:
  case WM_CANCELMODE:
    if (!mouseRotation) {
      if (g_offscreenBrowser.get())
        g_offscreenBrowser->SendCaptureLostEvent();
    }
    break;

  case WM_KEYDOWN:
  case WM_KEYUP:
  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_CHAR:
  case WM_SYSCHAR:
  case WM_IME_CHAR:
    if (g_offscreenBrowser.get()) {
      CefBrowser::KeyType type = KT_CHAR;
      bool sysChar = false, imeChar = false;

      if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
        type = KT_KEYDOWN;
      else if (message == WM_KEYUP || message == WM_SYSKEYUP)
        type = KT_KEYUP;

      if (message == WM_SYSKEYDOWN || message == WM_SYSKEYUP ||
          message == WM_SYSCHAR)
        sysChar = true;

      if (message == WM_IME_CHAR)
        imeChar = true;

       g_offscreenBrowser->SendKeyEvent(type, wParam, lParam, sysChar, imeChar);
    }
    break;

  case WM_PAINT: {
    PAINTSTRUCT ps;
    BeginPaint(hWnd, &ps);
    EndPaint(hWnd, &ps);
    return 0;
  }

  case WM_ERASEBKGND:
    return 0;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}

}  // namespace

NPError API_CALL NP_OSRGetEntryPoints(NPPluginFuncs* pFuncs) {
  pFuncs->newp = NPP_NewImpl;
  pFuncs->destroy = NPP_DestroyImpl;
  pFuncs->setwindow = NPP_SetWindowImpl;
  return NPERR_NO_ERROR;
}

NPError API_CALL NP_OSRInitialize(NPNetscapeFuncs* pFuncs) {
  g_osrbrowser = pFuncs;
  return NPERR_NO_ERROR;
}

NPError API_CALL NP_OSRShutdown(void) {
  g_osrbrowser = NULL;
  return NPERR_NO_ERROR;
}

CefRefPtr<CefBrowser> GetOffScreenBrowser() {
  return g_offscreenBrowser;
}

void SetOffScreenTransparent(bool transparent) {
  g_offscreenTransparent = transparent;
}

#endif  // OS_WIN
