// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/clientplugin.h"

#if defined(OS_WIN)

#include <windows.h>

namespace {

// Client plugin window implementation.
class ClientPlugin {
 public:
  ClientPlugin()
      : hwnd_(NULL) {
  }

  ~ClientPlugin() {
    if (IsWindow(hwnd_))
      DestroyWindow(hwnd_);
  }

  void Initialize(HWND parent_hwnd) {
    if (hwnd_ != NULL)
      return;

    HINSTANCE hInstance = GetModuleHandle(NULL);

    static bool class_registered = false;
    if (!class_registered) {
      // Register the window class.
      WNDCLASS wc = {0};
      wc.style = CS_OWNDC;
      wc.lpfnWndProc = PluginWndProc;
      wc.cbClsExtra = 0;
      wc.cbWndExtra = 0;
      wc.hInstance = hInstance;
      wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
      wc.hCursor = LoadCursor(NULL, IDC_ARROW);
      wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
      wc.lpszMenuName = NULL;
      wc.lpszClassName = L"ClientPlugin";
      RegisterClass(&wc);

      class_registered = true;
    }

    // Create the main window.
    hwnd_ = CreateWindow(L"ClientPlugin", L"Client Plugin",
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 0, 0, parent_hwnd,
        NULL, hInstance, NULL);

    SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Size and display the plugin window.
    RECT parent_rect;
    GetClientRect(parent_hwnd, &parent_rect);
    SetWindowPos(hwnd_, NULL, parent_rect.left, parent_rect.top,
        parent_rect.right - parent_rect.left,
        parent_rect.bottom - parent_rect.top, SWP_SHOWWINDOW);
  }

  void EraseBackground(HDC hdc) {
    RECT erase_rect;
    GetClipBox(hdc, &erase_rect);
    HBRUSH brush = CreateSolidBrush(RGB(0, 255, 0));
    FillRect(hdc, &erase_rect, brush);
    DeleteObject(brush);
  }

  void Paint(HDC hdc) {
    static LPCWSTR text = L"Left click in the green area for a message box!";

    RECT client_rect;
    GetClientRect(hwnd_, &client_rect);

    int old_mode = SetBkMode(hdc, TRANSPARENT);
    COLORREF old_color = SetTextColor(hdc, RGB(0, 0, 255));

    RECT text_rect = client_rect;
    DrawText(hdc, text, -1, &text_rect, DT_CENTER | DT_CALCRECT);

    client_rect.top = ((client_rect.bottom - client_rect.top) -
                      (text_rect.bottom - text_rect.top)) / 2;
    DrawText(hdc, text, -1, &client_rect, DT_CENTER);

    SetBkMode(hdc, old_mode);
    SetTextColor(hdc, old_color);
  }

  HWND GetWindow() const { return hwnd_; }

  // Plugin window procedure.
  static LRESULT CALLBACK PluginWndProc(HWND hwnd_, UINT message, WPARAM wParam,
                                        LPARAM lParam) {
    ClientPlugin* plugin =
        reinterpret_cast<ClientPlugin*>(GetWindowLongPtr(hwnd_, GWLP_USERDATA));

    switch (message) {
      case WM_DESTROY:
        return 0;

      case WM_LBUTTONDOWN:
        MessageBox(plugin->GetWindow(),
            L"You clicked on the client plugin!", L"Client Plugin", MB_OK);
        return 0;

      case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd_, &ps);
        plugin->Paint(ps.hdc);
        EndPaint(hwnd_, &ps);
        return 0;
      }

      case WM_PRINTCLIENT:
        plugin->Paint(reinterpret_cast<HDC>(wParam));
        return 0;

      case WM_ERASEBKGND:
        plugin->EraseBackground(reinterpret_cast<HDC>(wParam));
        return 1;
    }

    return DefWindowProc(hwnd_, message, wParam, lParam);
  }

 private:
  HWND hwnd_;
};

NPError NPP_NewImpl(NPMIMEType plugin_type, NPP instance, uint16 mode,
                    int16 argc, char* argn[], char* argv[],
                    NPSavedData* saved) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  ClientPlugin* plugin = new ClientPlugin();
  instance->pdata = reinterpret_cast<void*>(plugin);

  return NPERR_NO_ERROR;
}

NPError NPP_DestroyImpl(NPP instance, NPSavedData** save) {
  ClientPlugin* plugin = reinterpret_cast<ClientPlugin*>(instance->pdata);
  if (plugin)
    delete plugin;

  return NPERR_NO_ERROR;
}

NPError NPP_SetWindowImpl(NPP instance, NPWindow* window_info) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if (window_info == NULL)
    return NPERR_GENERIC_ERROR;

  ClientPlugin* plugin = reinterpret_cast<ClientPlugin*>(instance->pdata);
  HWND parent_hwnd = reinterpret_cast<HWND>(window_info->window);
  plugin->Initialize(parent_hwnd);

  return NPERR_NO_ERROR;
}

}  // namespace

NPError API_CALL NP_ClientGetEntryPoints(NPPluginFuncs* pFuncs) {
  pFuncs->newp = NPP_NewImpl;
  pFuncs->destroy = NPP_DestroyImpl;
  pFuncs->setwindow = NPP_SetWindowImpl;
  return NPERR_NO_ERROR;
}

NPError API_CALL NP_ClientInitialize(NPNetscapeFuncs* pFuncs) {
  return NPERR_NO_ERROR;
}

NPError API_CALL NP_ClientShutdown(void) {
  return NPERR_NO_ERROR;
}

#endif  // OS_WIN
