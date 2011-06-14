// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CEFCLIENT_H
#define _CEFCLIENT_H

#include "include/cef.h"

// Returns the main browser window instance.
CefRefPtr<CefBrowser> AppGetBrowser();

// Returns the main application window handle.
CefWindowHandle AppGetMainHwnd();

// Returns the application working directory.
std::string AppGetWorkingDirectory();

// Implementations for various tests.
void RunGetSourceTest(CefRefPtr<CefBrowser> browser);
void RunGetTextTest(CefRefPtr<CefBrowser> browser);
void RunRequestTest(CefRefPtr<CefBrowser> browser);
void RunJavaScriptExecuteTest(CefRefPtr<CefBrowser> browser);
void RunPopupTest(CefRefPtr<CefBrowser> browser);
void RunLocalStorageTest(CefRefPtr<CefBrowser> browser);
void RunAccelerated2DCanvasTest(CefRefPtr<CefBrowser> browser);
void RunAcceleratedLayersTest(CefRefPtr<CefBrowser> browser);
void RunWebGLTest(CefRefPtr<CefBrowser> browser);
void RunHTML5VideoTest(CefRefPtr<CefBrowser> browser);
void RunXMLHTTPRequestTest(CefRefPtr<CefBrowser> browser);
void RunWebURLRequestTest(CefRefPtr<CefBrowser> browser);
void RunDOMAccessTest(CefRefPtr<CefBrowser> browser);
void RunDragDropTest(CefRefPtr<CefBrowser> browser);
void RunModalDialogTest(CefRefPtr<CefBrowser> browser);

#endif // _CEFCLIENT_H
