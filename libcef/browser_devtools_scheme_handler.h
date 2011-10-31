// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef _CEF_BROWSER_DEVTOOLS_SCHEME_HANDLER_H
#define _CEF_BROWSER_DEVTOOLS_SCHEME_HANDLER_H

extern const char kChromeDevToolsScheme[];
extern const char kChromeDevToolsHost[];
extern const char kChromeDevToolsURL[];

// Register the DevTools scheme handler.
void RegisterDevToolsSchemeHandler(bool firstTime);

#endif // _CEF_BROWSER_DEVTOOLS_SCHEME_HANDLER_H
