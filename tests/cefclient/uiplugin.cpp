// Copyright (c) 2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cefclient/uiplugin.h"

#if defined(OS_WIN)

#include <windows.h>
#include <gl/gl.h>
#include <sstream>
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "cefclient/cefclient.h"

// Initialized in NP_Initialize.
NPNetscapeFuncs* g_uibrowser = NULL;

namespace {

// Global values.
float g_rotationspeed = 0.0f;
float g_theta = 0.0f;

// Class holding pointers for the client plugin window.
class ClientPlugin {
 public:
  ClientPlugin() {
    hWnd = NULL;
    hDC = NULL;
    hRC = NULL;
  }

  HWND hWnd;
  HDC hDC;
  HGLRC hRC;
};

// Forward declarations of functions included in this code module:
LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT message, WPARAM wParam,
                               LPARAM lParam);
void EnableOpenGL(HWND hWnd, HDC * hDC, HGLRC * hRC);
void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC);

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
      DisableOpenGL(plugin->hWnd, plugin->hDC, plugin->hRC);
    }
    delete plugin;
    g_rotationspeed = 0.0f;
    g_theta = 0.0f;
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
    wc.lpszClassName = L"ClientUIPlugin";
    RegisterClass(&wc);

    // Create the main window.
    plugin->hWnd = CreateWindow(L"ClientUIPlugin", L"Client UI Plugin",
        WS_CHILD, 0, 0, 0, 0, parent_hwnd, NULL, hInstance, NULL);

    SetWindowLongPtr(plugin->hWnd, GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(plugin));

    // Enable OpenGL drawing for the window.
    EnableOpenGL(plugin->hWnd, &(plugin->hDC), &(plugin->hRC));
  }

  // Position the window and make sure it's visible.
  RECT parent_rect;
  GetClientRect(parent_hwnd, &parent_rect);
  SetWindowPos(plugin->hWnd, NULL, parent_rect.left, parent_rect.top,
      parent_rect.right - parent_rect.left,
      parent_rect.bottom - parent_rect.top, SWP_SHOWWINDOW);

  UpdateWindow(plugin->hWnd);
  ShowWindow(plugin->hWnd, SW_SHOW);

  return NPERR_NO_ERROR;
}

// Send the notification to the browser as a JavaScript function call.
static void NotifyNewRotation(float value) {
  std::stringstream buf;
  buf << "notifyNewRotation(" << value << ");";
  AppGetBrowser()->GetMainFrame()->ExecuteJavaScript(buf.str(), CefString(),
      0);
}

// Nice little fly polygon borrowed from the OpenGL Red Book.
const GLubyte fly[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x80, 0x01, 0xC0, 0x06, 0xC0, 0x03, 0x60,
    0x04, 0x60, 0x06, 0x20, 0x04, 0x30, 0x0C, 0x20,
    0x04, 0x18, 0x18, 0x20, 0x04, 0x0C, 0x30, 0x20,
    0x04, 0x06, 0x60, 0x20, 0x44, 0x03, 0xC0, 0x22,
    0x44, 0x01, 0x80, 0x22, 0x44, 0x01, 0x80, 0x22,
    0x44, 0x01, 0x80, 0x22, 0x44, 0x01, 0x80, 0x22,
    0x44, 0x01, 0x80, 0x22, 0x44, 0x01, 0x80, 0x22,
    0x66, 0x01, 0x80, 0x66, 0x33, 0x01, 0x80, 0xCC,
    0x19, 0x81, 0x81, 0x98, 0x0C, 0xC1, 0x83, 0x30,
    0x07, 0xe1, 0x87, 0xe0, 0x03, 0x3f, 0xfc, 0xc0,
    0x03, 0x31, 0x8c, 0xc0, 0x03, 0x33, 0xcc, 0xc0,
    0x06, 0x64, 0x26, 0x60, 0x0c, 0xcc, 0x33, 0x30,
    0x18, 0xcc, 0x33, 0x18, 0x10, 0xc4, 0x23, 0x08,
    0x10, 0x63, 0xC6, 0x08, 0x10, 0x30, 0x0c, 0x08,
    0x10, 0x18, 0x18, 0x08, 0x10, 0x00, 0x00, 0x08};


// Plugin window procedure.
LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT message, WPARAM wParam,
                               LPARAM lParam) {
  ClientPlugin* plugin =
      reinterpret_cast<ClientPlugin*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

  switch (message) {
  case WM_CREATE:
    // Start the timer that's used for redrawing.
    SetTimer(hWnd, 1, 1, NULL);
    return 0;

  case WM_DESTROY:
    // Stop the timer that's used for redrawing.
    KillTimer(hWnd, 1);
    return 0;

  case WM_LBUTTONDOWN:
    // Decrement rotation speed.
    ModifyRotation(-2.0f);
    return 0;

  case WM_RBUTTONDOWN:
    // Increment rotation speed.
    ModifyRotation(2.0f);
    return 0;

  case WM_SIZE:
    if (plugin) {
      // Resize the OpenGL viewport to match the window size.
      int width = LOWORD(lParam);
      int height = HIWORD(lParam);

      wglMakeCurrent(plugin->hDC, plugin->hRC);
      glViewport(0, 0, width, height);
    }
    break;

  case WM_ERASEBKGND:
    return 0;

  case WM_TIMER:
    wglMakeCurrent(plugin->hDC, plugin->hRC);

    // Adjust the theta value and redraw the display when the timer fires.
    glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glPushMatrix();
    glEnable(GL_POLYGON_STIPPLE);
    glPolygonStipple(fly);

    glRotatef(g_theta, 0.0f, 0.0f, 1.0f);
    glBegin(GL_QUADS);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex2f(0.7f, 0.7f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex2f(0.7f, -0.7f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex2f(-0.7f, -0.7f);
    glColor3f(1.0f, 0.0f, 1.0f);
    glVertex2f(-0.7f, 0.7f);
    glEnd();

    glDisable(GL_POLYGON_STIPPLE);
    glPopMatrix();

    SwapBuffers(plugin->hDC);

    g_theta -= g_rotationspeed;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}

// Enable OpenGL.
void EnableOpenGL(HWND hWnd, HDC * hDC, HGLRC * hRC) {
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

  // Create and enable the render contex.
  *hRC = wglCreateContext(*hDC);
}

// Disable OpenGL.
void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC) {
  wglMakeCurrent(NULL, NULL);
  wglDeleteContext(hRC);
  ReleaseDC(hWnd, hDC);
}

}  // namespace

NPError API_CALL NP_UIGetEntryPoints(NPPluginFuncs* pFuncs) {
  pFuncs->newp = NPP_NewImpl;
  pFuncs->destroy = NPP_DestroyImpl;
  pFuncs->setwindow = NPP_SetWindowImpl;
  return NPERR_NO_ERROR;
}

NPError API_CALL NP_UIInitialize(NPNetscapeFuncs* pFuncs) {
  g_uibrowser = pFuncs;
  return NPERR_NO_ERROR;
}

NPError API_CALL NP_UIShutdown(void) {
  g_uibrowser = NULL;
  return NPERR_NO_ERROR;
}

void ModifyRotation(float value) {
  g_rotationspeed += value;
  NotifyNewRotation(g_rotationspeed);
}

void ResetRotation() {
  g_rotationspeed = 0.0;
  NotifyNewRotation(g_rotationspeed);
}

#endif  // OS_WIN
