// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BINDING_TEST_H_
#define CEF_TESTS_CEFCLIENT_BINDING_TEST_H_
#pragma once

#include "include/cef_base.h"

class CefBrowser;
class CefFrame;
class CefV8Value;

// Add the V8 bindings.
void InitBindingTest(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     CefRefPtr<CefV8Value> object);

// Run the test.
void RunBindingTest(CefRefPtr<CefBrowser> browser);

#endif  // CEF_TESTS_CEFCLIENT_BINDING_TEST_H_
