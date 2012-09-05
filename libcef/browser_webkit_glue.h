// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"

#if defined(OS_WIN)
#include <windows.h>
#endif
#include <string>

#include "base/string_piece.h"
#include "v8/include/v8.h"

namespace WebKit {
class WebFrame;
class WebView;
}
namespace webkit {
struct WebPluginInfo;
}

class FilePath;

namespace webkit_glue {

#if defined(OS_WIN)
// Capture a bitmap of the web view.
void CaptureWebViewBitmap(HWND mainWnd, WebKit::WebView* webview,
                          HBITMAP& bitmap, SIZE& size);

// Save a bitmap image to file, providing optional alternative data in |lpBits|
BOOL SaveBitmapToFile(HBITMAP hBmp, HDC hDC, LPCTSTR file, LPBYTE lpBits);
#endif
 
FilePath GetResourcesFilePath();
void InitializeResourceBundle(const std::string& locale);
void CleanupResourceBundle();

string16 GetLocalizedString(int message_id);
base::StringPiece GetDataResource(int resource_id);

// Text encoding objects must be initialized on the main thread.
void InitializeTextEncoding();

// Retrieve the V8 context associated with the frame.
v8::Handle<v8::Context> GetV8Context(WebKit::WebFrame* frame);

// Clear all cached data.
void ClearCache();

// Returns true if the request represents a download based on
// the supplied Content-Type and Content-Disposition headers.
bool ShouldDownload(const std::string& content_disposition,
                    const std::string& mime_type);

// Checks whether a plugin is enabled either by the user or by policy.
bool IsPluginEnabled(const webkit::WebPluginInfo& plugin);

}  // namespace webkit_glue
