// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_CLIENTPLUGIN_H_
#define CEF_TESTS_CEFCLIENT_CLIENTPLUGIN_H_
#pragma once

#include "include/internal/cef_types.h"

#if defined(OS_WIN)

#include "include/cef_nplugin.h"

NPError API_CALL NP_ClientGetEntryPoints(NPPluginFuncs* pFuncs);
NPError API_CALL NP_ClientInitialize(NPNetscapeFuncs* pFuncs);
NPError API_CALL NP_ClientShutdown(void);

#endif  // OS_WIN

#endif  // CEF_TESTS_CEFCLIENT_CLIENTPLUGIN_H_
