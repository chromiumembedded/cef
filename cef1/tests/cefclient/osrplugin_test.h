// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_OSRPLUGIN_TEST_H_
#define CEF_TESTS_CEFCLIENT_OSRPLUGIN_TEST_H_
#pragma once

#include "include/cef_browser.h"

namespace osrplugin_test {

// Register the internal client plugin and V8 extension.
void InitTest();

// Run the test.
void RunTest(CefRefPtr<CefBrowser> browser, bool transparent);

}  // namespace osrplugin_test

#endif  // CEF_TESTS_CEFCLIENT_OSRPLUGIN_TEST_H_
