// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_PLUGIN_TEST_H_
#define CEF_TESTS_CEFCLIENT_PLUGIN_TEST_H_
#pragma once

#include "include/cef_base.h"

class CefBrowser;

// Register the internal client plugin.
void InitPluginTest();

// Run the test.
void RunPluginTest(CefRefPtr<CefBrowser> browser);

#endif  // CEF_TESTS_CEFCLIENT_PLUGIN_TEST_H_
