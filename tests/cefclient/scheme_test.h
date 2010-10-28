// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CEFCLIENT_SCHEME_TEST
#define _CEFCLIENT_SCHEME_TEST

#include "include/cef.h"

// Register the scheme handler.
void InitSchemeTest();

// Run the test.
void RunSchemeTest(CefRefPtr<CefBrowser> browser);

#endif // _CEFCLIENT_SCHEME_TEST
