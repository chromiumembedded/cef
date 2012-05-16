// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cefclient/osrplugin.h"
#include "cefclient/osrenderer.h"

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

CefRefPtr<CefBrowser> g_offscreenBrowser;

// If set to true alpha transparency will be used.
bool g_offscreenTransparent = false;

// Class holding pointers for the client plugin window.
class ClientPlugin {
 public:
  ClientPlugin(bool transparent)
    : renderer(transparent),
      hWnd(NULL),
      hDC(NULL),
      hRC(NULL) {
  }

  ClientOSRenderer renderer;
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
    : plugin_(plugin) {
  }
  ~ClientOSRHandler() {
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

    g_offscreenBrowser = browser;

    // Set the off-screen browser size to match the plugin window size.
    RECT clientRect;
    ::GetClientRect(plugin_->hWnd, &clientRect);
    g_offscreenBrowser->SetSize(PET_VIEW, clientRect.right, clientRect.bottom);
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
    RECT clientRect;
    ::GetClientRect(plugin_->hWnd, &clientRect);

    rect.x = rect.y = 0;
    rect.width = clientRect.right;
    rect.height = clientRect.bottom;
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
    plugin_->renderer.OnPopupShow(browser, show);
  }

  virtual void OnPopupSize(CefRefPtr<CefBrowser> browser,
                           const CefRect& rect) OVERRIDE {
    REQUIRE_UI_THREAD();
    plugin_->renderer.OnPopupSize(browser, rect);
  }

  virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                       PaintElementType type,
                       const RectList& dirtyRects,
                       const void* buffer) OVERRIDE {
    REQUIRE_UI_THREAD();

    wglMakeCurrent(plugin_->hDC, plugin_->hRC);
    plugin_->renderer.OnPaint(browser, type, dirtyRects, buffer);
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

  ClientPlugin* plugin_;

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(ClientOSRPlugin);
};

// Forward declarations of functions included in this code module:
LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT message, WPARAM wParam,
                               LPARAM lParam);

// Enable GL.
void EnableGL(ClientPlugin* plugin) {
  PIXELFORMATDESCRIPTOR pfd;
  int format;

  // Get the device context.
  plugin->hDC = GetDC(plugin->hWnd);

  // Set the pixel format for the DC.
  ZeroMemory(&pfd, sizeof(pfd));
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 24;
  pfd.cDepthBits = 16;
  pfd.iLayerType = PFD_MAIN_PLANE;
  format = ChoosePixelFormat(plugin->hDC, &pfd);
  SetPixelFormat(plugin->hDC, format, &pfd);

  // Create and enable the render context.
  plugin->hRC = wglCreateContext(plugin->hDC);
  wglMakeCurrent(plugin->hDC, plugin->hRC);

  plugin->renderer.Initialize();
}

// Disable GL.
void DisableGL(ClientPlugin* plugin) {
  plugin->renderer.Cleanup();

  wglMakeCurrent(NULL, NULL);
  wglDeleteContext(plugin->hRC);
  ReleaseDC(plugin->hWnd, plugin->hDC);
}

// Size the GL view.
void SizeGL(ClientPlugin* plugin, int width, int height) {
  wglMakeCurrent(plugin->hDC, plugin->hRC);

  plugin->renderer.SetSize(width, height);

  if (g_offscreenBrowser.get())
    g_offscreenBrowser->SetSize(PET_VIEW, width, height);
}

// Render the view contents.
void RenderGL(ClientPlugin* plugin) {
  wglMakeCurrent(plugin->hDC, plugin->hRC);

  plugin->renderer.Render();

  SwapBuffers(plugin->hDC);
}

NPError NPP_NewImpl(NPMIMEType plugin_type, NPP instance, uint16 mode,
                    int16 argc, char* argn[], char* argv[],
                    NPSavedData* saved) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  ClientPlugin* plugin = new ClientPlugin(g_offscreenTransparent);
  instance->pdata = reinterpret_cast<void*>(plugin);

  return NPERR_NO_ERROR;
}

NPError NPP_DestroyImpl(NPP instance, NPSavedData** save) {
  ClientPlugin* plugin = reinterpret_cast<ClientPlugin*>(instance->pdata);

  if (plugin) {
    if (plugin->hWnd) {
      DestroyWindow(plugin->hWnd);
      DisableGL(plugin);
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
    EnableGL(plugin);

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
      plugin->renderer.SetSpin(0, 0);
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
      plugin->renderer.IncrementSpin((curMousePos.x - lastMousePos.x),
                                     (curMousePos.y - lastMousePos.y));
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
          0, GET_WHEEL_DELTA_WPARAM(wParam));
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
      CefKeyInfo keyInfo;
      keyInfo.key = wParam;

      if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
        type = KT_KEYDOWN;
      else if (message == WM_KEYUP || message == WM_SYSKEYUP)
        type = KT_KEYUP;

      if (message == WM_SYSKEYDOWN || message == WM_SYSKEYUP ||
          message == WM_SYSCHAR)
        keyInfo.sysChar = true;

      if (message == WM_IME_CHAR)
        keyInfo.imeChar = true;

       g_offscreenBrowser->SendKeyEvent(type, keyInfo, lParam);
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
