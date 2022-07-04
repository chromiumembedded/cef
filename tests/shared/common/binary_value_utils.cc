// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/common/binary_value_utils.h"

namespace bv_utils {

const char kTestSendProcessMessage[] = "testSendProcessMessage";
const char kTestSendSMRProcessMessage[] = "testSendSMRProcessMessage";

TimePoint Now() {
  return std::chrono::high_resolution_clock::now();
}

CefRefPtr<CefBinaryValue> CreateCefBinaryValue(
    const std::vector<uint8_t>& data) {
  return CefBinaryValue::Create(data.data(), data.size());
}

RendererMessage GetRendererMsgFromBinary(
    const CefRefPtr<CefBinaryValue>& value) {
  DCHECK_GE(value->GetSize(), sizeof(RendererMessage));
  std::vector<uint8_t> data(value->GetSize());
  value->GetData(data.data(), data.size(), 0);
  return *reinterpret_cast<const RendererMessage*>(data.data());
}

BrowserMessage GetBrowserMsgFromBinary(const CefRefPtr<CefBinaryValue>& value) {
  DCHECK_GE(value->GetSize(), sizeof(BrowserMessage));
  std::vector<uint8_t> data(value->GetSize());
  value->GetData(data.data(), data.size(), 0);
  return *reinterpret_cast<const BrowserMessage*>(data.data());
}

std::string ToMilliString(const Duration& duration) {
  const auto ms =
      std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
          duration);

  return std::to_string(ms.count());
}

}  // namespace bv_utils
