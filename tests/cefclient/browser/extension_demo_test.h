// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_EXTENSION_DEMO_TEST_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_EXTENSION_DEMO_TEST_H_
#pragma once

#include "include/cef_command_line.h"
#include "tests/cefclient/browser/test_runner.h"

namespace client::extension_demo_test {

// Returns true if --extensions-demo is present on the command line.
bool IsEnabled(CefRefPtr<CefCommandLine> command_line);

// Launch the extension demo window. Call from OnContextInitialized.
void Launch(CefRefPtr<CefCommandLine> command_line);

// Register the message router handler(s) for the demo page.
void CreateMessageHandlers(test_runner::MessageHandlerSet& handlers);

}  // namespace client::extension_demo_test

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_EXTENSION_DEMO_TEST_H_
