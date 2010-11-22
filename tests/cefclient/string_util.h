// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CEFCLIENT_STRING_UTIL_H
#define _CEFCLIENT_STRING_UTIL_H

#include "include/cef.h"

// Dump the contents of the request into a string.
void DumpRequestContents(CefRefPtr<CefRequest> request, std::string& str);

// Replace all instances of |from| with |to| in |str|.
std::string StringReplace(const std::string& str, const std::string& from,
                          const std::string& to);

#endif // _CEFCLIENT_STRING_UTIL_H
