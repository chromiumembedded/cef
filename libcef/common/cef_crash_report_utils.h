// Copyright 2018 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_CEF_CRASH_REPORT_UTILS_H_
#define CEF_LIBCEF_COMMON_CEF_CRASH_REPORT_UTILS_H_

#include <map>
#include <string>

namespace crash_report_utils {

using ParameterMap = std::map<std::string, std::string>;

ParameterMap FilterParameters(const ParameterMap& parameters);

}  // namespace crash_report_utils

#endif  // CEF_LIBCEF_COMMON_CEF_CRASH_REPORT_UTILS_H_
