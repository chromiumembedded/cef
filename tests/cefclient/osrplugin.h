// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions of this implementation are borrowed from webkit\default_plugin\
// plugin_impl.h

#ifndef _CEFCLIENT_OSRPLUGIN_H
#define _CEFCLIENT_OSRPLUGIN_H

#include "include/cef.h"
#include "include/cef_nplugin.h"

#if defined(OS_WIN)

extern NPNetscapeFuncs* g_osrbrowser;

NPError API_CALL NP_OSRGetEntryPoints(NPPluginFuncs* pFuncs);
NPError API_CALL NP_OSRInitialize(NPNetscapeFuncs* pFuncs);
NPError API_CALL NP_OSRShutdown(void);

CefRefPtr<CefBrowser> GetOffScreenBrowser();

#endif // OS_WIN

#endif // _CEFCLIENT_OSRPLUGIN_H
