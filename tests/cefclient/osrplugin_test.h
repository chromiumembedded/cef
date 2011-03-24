// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CEFCLIENT_OSRPLUGIN_TEST_H
#define _CEFCLIENT_OSRPLUGIN_TEST_H

#include "include/cef.h"

// Register the internal client plugin and V8 extension.
void InitOSRPluginTest();

// Run the test.
void RunOSRPluginTest(CefRefPtr<CefBrowser> browser);

#endif // _CEFCLIENT_OSRPLUGIN_TEST_H
