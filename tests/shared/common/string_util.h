// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_SHARED_COMMON_STRING_UTIL_H_
#define CEF_TESTS_SHARED_COMMON_STRING_UTIL_H_
#pragma once

#include <string>
#include <vector>

namespace client {

// Convert |str| to lowercase.
std::string AsciiStrToLower(const std::string& str);

// Replace all instances of |from| with |to| in |str|.
std::string AsciiStrReplace(const std::string& str,
                            const std::string& from,
                            const std::string& to);

// Split |str| at character |delim|.
std::vector<std::string> AsciiStrSplit(const std::string& str, char delim);

}  // namespace client

#endif  // CEF_TESTS_SHARED_COMMON_STRING_UTIL_H_
