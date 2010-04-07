// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#pragma once

#include "include/cef.h"
#include <string>


// Convert a std::string to a std::wstring
std::wstring StringToWString(const std::string& s);

// Convert a std::wstring to a std::string
std::string WStringToString(const std::wstring& s);

// Dump the contents of the request into a string.
void DumpRequestContents(CefRefPtr<CefRequest> request, std::wstring& str);

// Replace all instances of |from| with |to| in |str|.
std::wstring StringReplace(const std::wstring& str, const std::wstring& from,
                           const std::wstring& to);