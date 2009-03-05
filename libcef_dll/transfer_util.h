// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _TRANSFER_UTIL_H
#define _TRANSFER_UTIL_H

#include "cef_string.h"
#include "cef_string_map.h"

// Copy contents from one map type to another.
void transfer_string_map_contents(cef_string_map_t fromMap,
                                  std::map<std::wstring, std::wstring>& toMap);
void transfer_string_map_contents(const std::map<std::wstring, std::wstring>& fromMap,
                                  cef_string_map_t toMap);

// Copy the contents from one string type to another.
void transfer_string_contents(const std::wstring& fromString,
                              cef_string_t* toString);
void transfer_string_contents(cef_string_t fromString, std::wstring& toString,
                              bool freeFromString);

#endif // _TRANSFER_UTIL_H
