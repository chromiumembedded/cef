// Copyright (c) 2008 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <string>

class WebFrame;
class WebView;

namespace webkit_glue {

// Return the HTML contents of a web frame as a string
std::string GetDocumentString(WebFrame* frame);

#if defined(OS_WIN)
// Capture a bitmap of the web view.
void CaptureWebViewBitmap(HWND mainWnd, WebView* webview, HBITMAP& bitmap,
                          SIZE& size);

// Save a bitmap image to file, providing optional alternative data in |lpBits|
BOOL SaveBitmapToFile(HBITMAP hBmp, HDC hDC, LPCTSTR file, LPBYTE lpBits);
#endif

}  // namespace webkit_glue
