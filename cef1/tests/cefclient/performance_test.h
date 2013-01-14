// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_PERFORMANCE_TEST_H_
#define CEF_TESTS_CEFCLIENT_PERFORMANCE_TEST_H_
#pragma once

#include "include/cef_base.h"

class CefBrowser;
class CefFrame;
class CefV8Value;

namespace performance_test {

extern const char kTestUrl[];

void InitTest(CefRefPtr<CefBrowser> browser,
              CefRefPtr<CefFrame> frame,
              CefRefPtr<CefV8Value> object);

// Run the test.
void RunTest(CefRefPtr<CefBrowser> browser);

}  // namespace performance_test

#endif  // CEF_TESTS_CEFCLIENT_PERFORMANCE_TEST_H_
