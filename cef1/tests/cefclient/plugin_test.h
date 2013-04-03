// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_PLUGIN_TEST_H_
#define CEF_TESTS_CEFCLIENT_PLUGIN_TEST_H_
#pragma once

#include "include/cef_browser.h"
#include "cefclient/client_handler.h"

namespace plugin_test {

// Register the internal client plugin.
void InitTest();

// Delegate creation. Called from ClientHandler.
void CreateRequestDelegates(ClientHandler::RequestDelegateSet& delegates);

// Run the test.
void RunTest(CefRefPtr<CefBrowser> browser);

}  // namespace plugin_test

#endif  // CEF_TESTS_CEFCLIENT_PLUGIN_TEST_H_
