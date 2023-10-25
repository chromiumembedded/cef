// Copyright (c) 2023 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/message_router_unittest_utils.h"

namespace {

constexpr size_t kMsgSizeThresholdInBytes = 16000;

class BinaryTestHandler final : public SingleLoadTestHandler {
 public:
  explicit BinaryTestHandler(size_t message_size)
      : message_size_(message_size) {}

  std::string GetMainHTML() override {
    return "<html><body><script>\n"
           "function generateRandomArrayBuffer(size) {\n"
           "  const buffer = new ArrayBuffer(size);\n"
           "  const uint8Array = new Uint8Array(buffer);\n"
           "  for (let i = 0; i < uint8Array.length; i++) {\n"
           "    uint8Array[i] = Math.floor(Math.random() * 256);\n"
           "  }\n"
           "  return buffer;\n"
           "}\n"
           "function isEqualArrayBuffers(left, right) {\n"
           "  if (left.byteLength !== right.byteLength) { return false; }\n"
           "  const leftView = new DataView(left);\n"
           "  const rightView = new DataView(right);\n"
           "  for (let i = 0; i < left.byteLength; i++) {\n"
           "    if (leftView.getUint8(i) !== rightView.getUint8(i)) {\n"
           "      return false;\n"
           "    }\n"
           "  }\n"
           "  return true;\n"
           "}\n"
           "const request = generateRandomArrayBuffer(" +
           std::to_string(message_size_) +
           ")\n"
           "window." +
           std::string(kJSQueryFunc) +
           "({\n  request: request,\n"
           "  persistent: false,\n"
           "  onSuccess: (response) => {\n"
           "    if (!isEqualArrayBuffers(request, response)) {\n"
           "      window.mrtNotify('error-ArrayBuffersDiffer');\n"
           "    } else {\n"
           "      window.mrtNotify('success');\n"
           "    }\n"
           "  },\n"
           "  onFailure: (errorCode, errorMessage) => {\n"
           "    window.mrtNotify('error-onFailureCalled');\n"
           "  }\n"
           "});\n</script></body></html>";
  }

  void OnNotify(CefRefPtr<CefBrowser> browser,
                CefRefPtr<CefFrame> frame,
                const std::string& message) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);

    // OnNotify only be called once.
    EXPECT_FALSE(got_notify_);
    got_notify_.yes();

    EXPECT_EQ("success", message);

    DestroyTest();
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               CefRefPtr<const CefBinaryBuffer> request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);
    EXPECT_NE(0, query_id);
    EXPECT_FALSE(persistent);
    EXPECT_EQ(message_size_, request->GetSize());

    got_on_query_.yes();

    callback->Success(request->GetData(), request->GetSize());

    return true;
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_notify_);
    EXPECT_TRUE(got_on_query_);

    TestHandler::DestroyTest();
  }

 private:
  const size_t message_size_;
  TrackCallback got_on_query_;
  TrackCallback got_notify_;
};

using TestHandlerPtr = CefRefPtr<BinaryTestHandler>;

}  // namespace

TEST(MessageRouterTest, BinaryMessageEmpty) {
  const auto message_size = 0;
  TestHandlerPtr handler = new BinaryTestHandler(message_size);
  handler->SetMessageSizeThreshold(kMsgSizeThresholdInBytes);

  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);
}

TEST(MessageRouterTest, BinaryMessageUnderThresholdSize) {
  const auto under_threshold = kMsgSizeThresholdInBytes - 1;
  TestHandlerPtr handler = new BinaryTestHandler(under_threshold);
  handler->SetMessageSizeThreshold(kMsgSizeThresholdInBytes);

  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);
}

TEST(MessageRouterTest, BinaryMessageOverThresholdSize) {
  const auto over_threshold = kMsgSizeThresholdInBytes + 1;
  TestHandlerPtr handler = new BinaryTestHandler(over_threshold);
  handler->SetMessageSizeThreshold(kMsgSizeThresholdInBytes);

  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);
}
