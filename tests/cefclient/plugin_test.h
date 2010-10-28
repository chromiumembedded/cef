// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CEFCLIENT_PLUGIN_TEST_H
#define _CEFCLIENT_PLUGIN_TEST_H

#include "include/cef.h"

// Register the internal client plugin.
void InitPluginTest();

// Run the test.
void RunPluginTest(CefRefPtr<CefBrowser> browser);

#endif // _CEFCLIENT_PLUGIN_TEST_H
