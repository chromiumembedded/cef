// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "clientplugin.h"

// Initialized in NP_Initialize.
NPNetscapeFuncs* g_browser = NULL;

static
NPError NPP_New(NPMIMEType plugin_type, NPP instance, uint16 mode, int16 argc,
                char* argn[], char* argv[], NPSavedData* saved) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  ClientPlugin* plugin_impl = new ClientPlugin(mode);
  plugin_impl->Initialize(GetModuleHandle(NULL), instance, plugin_type, argc,
                          argn, argv);
  instance->pdata = reinterpret_cast<void*>(plugin_impl);
  return NPERR_NO_ERROR;
}

static
NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  ClientPlugin* plugin_impl = reinterpret_cast<ClientPlugin*>(instance->pdata);

  if (plugin_impl) {
    plugin_impl->Shutdown();
    delete plugin_impl;
  }

  return NPERR_NO_ERROR;
}

static
NPError NPP_SetWindow(NPP instance, NPWindow* window_info) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if (window_info == NULL)
    return NPERR_GENERIC_ERROR;

  ClientPlugin* plugin_impl = reinterpret_cast<ClientPlugin*>(instance->pdata);

  if (plugin_impl == NULL)
    return NPERR_GENERIC_ERROR;

  HWND window_handle = reinterpret_cast<HWND>(window_info->window);
  if (!plugin_impl->SetWindow(window_handle)) {
    delete plugin_impl;
    return NPERR_GENERIC_ERROR;
  }

  return NPERR_NO_ERROR;
}

NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* pFuncs)
{
  pFuncs->newp = NPP_New;
  pFuncs->destroy = NPP_Destroy;
  pFuncs->setwindow = NPP_SetWindow;
  return NPERR_NO_ERROR;
}

NPError API_CALL NP_Initialize(NPNetscapeFuncs* pFuncs)
{
  g_browser = pFuncs;
  return NPERR_NO_ERROR;
}

NPError API_CALL NP_Shutdown(void)
{
  g_browser = NULL;
  return NPERR_NO_ERROR;
}


// ClientPlugin Implementation

ClientPlugin::ClientPlugin(int16 mode)
  : mode_(mode)
{
}

ClientPlugin::~ClientPlugin()
{
}

bool ClientPlugin::Initialize(HINSTANCE module_handle, NPP instance,
                              NPMIMEType mime_type, int16 argc, char* argn[],
                              char* argv[])
{
  RefreshDisplay();
  return true;
}

bool ClientPlugin::SetWindow(HWND parent_window)
{
  if (!::IsWindow(parent_window)) {
    // No window created yet. Ignore this call.
    if (!IsWindow())
      return true;
    // Parent window has been destroyed.
    Shutdown();
    return true;
  }

  RECT parent_rect;

  if (IsWindow()) {
    ::GetClientRect(parent_window, &parent_rect);
    SetWindowPos(NULL, &parent_rect, SWP_SHOWWINDOW);
    return true;
  }
  // First time in -- no window created by plugin yet.
  ::GetClientRect(parent_window, &parent_rect);
  Create(parent_window, parent_rect, NULL, WS_CHILD | WS_BORDER);
  
  UpdateWindow();
  ShowWindow(SW_SHOW);

  return true;
}

void ClientPlugin::Shutdown()
{
  if (IsWindow()) {
    DestroyWindow();
  }
}

LRESULT ClientPlugin::OnPaint(UINT message, WPARAM wparam, LPARAM lparam,
                              BOOL& handled)
{
  static LPCWSTR text = L"Left click in the green area for a message box!";

  PAINTSTRUCT paint_struct;
  BeginPaint(&paint_struct);

  RECT client_rect;
  GetClientRect(&client_rect);

  int old_mode = SetBkMode(paint_struct.hdc, TRANSPARENT);
  COLORREF old_color = SetTextColor(paint_struct.hdc, RGB(0, 0, 255));

  RECT text_rect = client_rect;
  DrawText(paint_struct.hdc, text, -1, &text_rect, DT_CENTER | DT_CALCRECT);

  client_rect.top = ((client_rect.bottom - client_rect.top)
      - (text_rect.bottom - text_rect.top)) / 2;
  DrawText(paint_struct.hdc, text, -1, &client_rect, DT_CENTER);

  SetBkMode(paint_struct.hdc, old_mode);
  SetTextColor(paint_struct.hdc, old_color);

  EndPaint(&paint_struct);
  return 0;
}

LRESULT ClientPlugin::OnEraseBackGround(UINT message, WPARAM wparam,
                                        LPARAM lparam, BOOL& handled)
{
  HDC paint_device_context = reinterpret_cast<HDC>(wparam);
  RECT erase_rect;
  GetClipBox(paint_device_context, &erase_rect);
  HBRUSH brush = CreateSolidBrush(RGB(0, 255, 0));
  FillRect(paint_device_context, &erase_rect, brush);
  DeleteObject(brush);
  return 1;
}

LRESULT ClientPlugin::OnLButtonDown(UINT message, WPARAM wparam, LPARAM lparam,
                                    BOOL& handled)
{
  MessageBox(L"You clicked on the client plugin!", L"Client Plugin", MB_OK);
  return 0;
}

void ClientPlugin::RefreshDisplay() {
  if (!IsWindow())
    return;

  InvalidateRect(NULL, TRUE);
  UpdateWindow();
}
