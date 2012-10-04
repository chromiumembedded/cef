// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_OSRPLUGIN_TEST_H_
#define CEF_TESTS_CEFCLIENT_OSRPLUGIN_TEST_H_
#pragma once

#include "include/cef_base.h"

class CefBrowser;

// Register the internal client plugin and V8 extension.
void InitOSRPluginTest();

// Run the test.
void RunOSRPluginTest(CefRefPtr<CefBrowser> browser, bool transparent);

#endif  // CEF_TESTS_CEFCLIENT_OSRPLUGIN_TEST_H_
