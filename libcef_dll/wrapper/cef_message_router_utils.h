// Copyright (c) 2023 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_WRAPPER_CEF_MESSAGE_ROUTER_UTILS_H_
#define CEF_LIBCEF_DLL_WRAPPER_CEF_MESSAGE_ROUTER_UTILS_H_
#pragma once

#include <variant>

#include "include/wrapper/cef_message_router.h"

namespace cef_message_router_utils {

///
/// This class handles the task of copying user data, such as CefString or
/// binary values like (void*, size_t), directly to an appropriate buffer based
/// on the user data type and size.
///
/// There are four specializations of this abstract class. The appropriate
/// specialization is chosen by the `CreateBrowserResponseBuilder` function,
/// based on the provided data type and size. For instance, for a "short"
/// CefString, a StringResponseBuilder specialization is used, and for an empty
/// binary value - EmptyResponseBuilder.
///
class BrowserResponseBuilder : public CefBaseRefCounted {
 public:
  ///
  /// Creates a new CefProcessMessage from the data provided to the builder.
  /// Returns nullptr for invalid instances. Invalidates this builder instance.
  ///
  virtual CefRefPtr<CefProcessMessage> Build(int context_id,
                                             int request_id) = 0;
};

struct BrowserMessage {
  int context_id;
  int request_id;
  bool is_success;
  int error_code;
  std::variant<CefString, CefRefPtr<CefBinaryBuffer>> payload;
};

struct RendererMessage {
  int context_id;
  int request_id;
  bool is_persistent;
  std::variant<CefString, CefRefPtr<const CefBinaryBuffer>> payload;
};

#ifndef CEF_V8_ENABLE_SANDBOX
class BinaryValueABRCallback final : public CefV8ArrayBufferReleaseCallback {
 public:
  explicit BinaryValueABRCallback(CefRefPtr<CefBinaryBuffer> value)
      : value_(std::move(value)) {}
  BinaryValueABRCallback(const BinaryValueABRCallback&) = delete;
  BinaryValueABRCallback& operator=(const BinaryValueABRCallback&) = delete;

  void ReleaseBuffer(void* buffer) override {}

 private:
  const CefRefPtr<CefBinaryBuffer> value_;

  IMPLEMENT_REFCOUNTING(BinaryValueABRCallback);
};
#endif

CefRefPtr<BrowserResponseBuilder> CreateBrowserResponseBuilder(
    size_t threshold,
    const std::string& name,
    const CefString& response);

CefRefPtr<BrowserResponseBuilder> CreateBrowserResponseBuilder(
    size_t threshold,
    const std::string& name,
    const void* data,
    size_t size);

CefRefPtr<CefProcessMessage> BuildRendererMsg(
    size_t threshold,
    const std::string& name,
    int context_id,
    int request_id,
    const CefRefPtr<CefV8Value>& request,
    bool persistent);

BrowserMessage ParseBrowserMessage(const CefRefPtr<CefProcessMessage>& message);

RendererMessage ParseRendererMessage(
    const CefRefPtr<CefProcessMessage>& message);

}  // namespace cef_message_router_utils

#endif  // CEF_LIBCEF_DLL_WRAPPER_CEF_MESSAGE_ROUTER_UTILS_H_
