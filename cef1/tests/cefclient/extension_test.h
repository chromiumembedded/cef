// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_EXTENSION_TEST_H_
#define CEF_TESTS_CEFCLIENT_EXTENSION_TEST_H_
#pragma once

#include "include/cef_base.h"

class CefBrowser;

// Register the V8 extension handler.
void InitExtensionTest();

// Run the test.
void RunExtensionTest(CefRefPtr<CefBrowser> browser);
void RunExtensionPerfTest(CefRefPtr<CefBrowser> browser);

#endif  // CEF_TESTS_CEFCLIENT_EXTENSION_TEST_H_
