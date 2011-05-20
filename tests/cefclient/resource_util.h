// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CEFCLIENT_RESOURCE_UTIL
#define _CEFCLIENT_RESOURCE_UTIL

#include "include/cef.h"

#if defined(OS_WIN)

// Load a resource of type BINARY
bool LoadBinaryResource(int binaryId, DWORD &dwSize, LPBYTE &pBytes);
CefRefPtr<CefStreamReader> GetBinaryResourceReader(int binaryId);

#elif defined(OS_MACOSX)

// Load the resource with the specified name.
bool LoadBinaryResource(const char* resource_name, std::string& resource_data);
CefRefPtr<CefStreamReader> GetBinaryResourceReader(const char* resource_name);

#endif

#endif // _CEFCLIENT_RESOURCE_UTIL
