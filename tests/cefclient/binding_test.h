// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CEFCLIENT_BINDING_TEST_H
#define _CEFCLIENT_BINDING_TEST_H

#include "include/cef.h"

// Add the V8 bindings.
void InitBindingTest(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     CefRefPtr<CefV8Value> object);

// Run the test.
void RunBindingTest(CefRefPtr<CefBrowser> browser);

#endif // _CEFCLIENT_BINDING_TEST_H
