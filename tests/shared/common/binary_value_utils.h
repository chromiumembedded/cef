// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_SHARED_COMMON_BINARY_VALUE_UTILS
#define CEF_TESTS_SHARED_COMMON_BINARY_VALUE_UTILS
#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

#include "include/cef_values.h"

namespace bv_utils {

extern const char kTestSendProcessMessage[];
extern const char kTestSendSMRProcessMessage[];

using TimePoint = std::chrono::high_resolution_clock::time_point;
using Duration = std::chrono::high_resolution_clock::duration;

struct RendererMessage {
  int test_id;
  TimePoint start_time;
};

struct BrowserMessage {
  int test_id;
  Duration duration;
  TimePoint start_time;
};

TimePoint Now();

CefRefPtr<CefBinaryValue> CreateCefBinaryValue(
    const std::vector<uint8_t>& data);

void CopyDataIntoMemory(const std::vector<uint8_t>& data, void* dst);

RendererMessage GetRendererMsgFromBinary(
    const CefRefPtr<CefBinaryValue>& value);

BrowserMessage GetBrowserMsgFromBinary(const CefRefPtr<CefBinaryValue>& value);

std::string ToMicroSecString(const Duration& duration);

}  // namespace bv_utils

#endif  // CEF_TESTS_SHARED_COMMON_BINARY_VALUE_UTILS
