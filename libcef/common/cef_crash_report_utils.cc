// Copyright 2018 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/cef_crash_report_utils.h"

#include "base/strings/string_split.h"

namespace crash_report_utils {

ParameterMap FilterParameters(const ParameterMap& parameters) {
  ParameterMap in_map = parameters;

  // Extract the key map, if any. Must match the logic in
  // CefCrashReporterClient::ReadCrashConfigFile.
  std::string key_map;
  for (size_t i = 0; true; ++i) {
    const std::string& key = "K-" + std::string(1, 'A' + i);
    ParameterMap::iterator it = in_map.find(key);
    if (it == in_map.end()) {
      break;
    }
    key_map += it->second;
    in_map.erase(it);
  }

  if (key_map.empty()) {
    // Nothing to substitute.
    return parameters;
  }

  // Parse |key_map|.
  base::StringPairs kv_pairs;
  if (!base::SplitStringIntoKeyValuePairs(key_map, '=', ',', &kv_pairs)) {
    return parameters;
  }

  ParameterMap subs;
  for (const auto& pairs : kv_pairs) {
    subs.insert(std::make_pair(pairs.first, pairs.second));
  }

  ParameterMap out_map;

  // Perform key substitutions.
  for (const auto& params : in_map) {
    std::string key = params.first;
    ParameterMap::const_iterator subs_it = subs.find(params.first);
    if (subs_it != subs.end()) {
      key = subs_it->second;
    }
    out_map.insert(std::make_pair(key, params.second));
  }

  return out_map;
}

}  // namespace crash_report_utils
