// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef_process_ui_thread.h"
#include "browser_impl.h"
#include <commctrl.h>
#include <Objbase.h>

void CefProcessUIThread::PlatformInit() {
  HRESULT res;

  // Initialize common controls
  res = CoInitialize(NULL);
  DCHECK(SUCCEEDED(res));
  INITCOMMONCONTROLSEX InitCtrlEx;
  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC  = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&InitCtrlEx);

  // Start COM stuff
  res = OleInitialize(NULL);
  DCHECK(SUCCEEDED(res));

  // Register the window class
  WNDCLASSEX wcex = {
    /* cbSize = */ sizeof(WNDCLASSEX),
    /* style = */ CS_HREDRAW | CS_VREDRAW,
    /* lpfnWndProc = */ CefBrowserImpl::WndProc,
    /* cbClsExtra = */ 0,
    /* cbWndExtra = */ 0,
    /* hInstance = */ ::GetModuleHandle(NULL),
    /* hIcon = */ NULL,
    /* hCursor = */ LoadCursor(NULL, IDC_ARROW),
    /* hbrBackground = */ 0,
    /* lpszMenuName = */ NULL,
    /* lpszClassName = */ CefBrowserImpl::GetWndClass(),
    /* hIconSm = */ NULL,
  };
  RegisterClassEx(&wcex);
}

void CefProcessUIThread::PlatformCleanUp() {
  // Uninitialize COM stuff
  OleUninitialize();

  // Closes the COM library on the current thread. CoInitialize must
  // be balanced by a corresponding call to CoUninitialize.
  CoUninitialize();
}

