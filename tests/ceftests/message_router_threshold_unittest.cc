// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/message_router_unittest_utils.h"

namespace {

constexpr int kSingleQueryErrorCode = 5;
constexpr size_t kMsgSizeThresholdInBytes = 16000;
constexpr auto kCharSize = sizeof(CefString::char_type);

enum class TestType { SUCCESS, FAILURE };

CefString GenerateResponse(size_t count, char ch) {
  return std::string(count, ch);
}

CefString GenerateResponse(size_t count, wchar_t ch) {
  return std::wstring(count, ch);
}

template <class CharType>
class ThresholdTestHandler final : public SingleLoadTestHandler {
 public:
  ThresholdTestHandler(TestType type, size_t string_length, CharType symbol)
      : test_type_(type), string_length_(string_length), symbol_(symbol) {}

  std::string GetMainHTML() override {
    const std::string& errorCodeStr = std::to_string(kSingleQueryErrorCode);

    std::string html =
        "<html><body><script>\n"
        // Send the query.
        "var request_id = window." +
        std::string(kJSQueryFunc) + "({\n request: '" +
        std::to_string(string_length_) +
        "',\n persistent: false,\n"
        "  onSuccess: function(response) {\n"
        "      window.mrtNotify(response);\n"
        "  },\n"
        "  onFailure: function(error_code, error_message) {\n"
        "    if (error_code == " +
        errorCodeStr +
        ")\n"
        "      window.mrtNotify(error_message);\n"
        "    else\n"
        "      window.mrtNotify('error-onFailure');\n"
        "  }\n"
        "});\n</script></body></html>";

    return html;
  }

  void OnNotify(CefRefPtr<CefBrowser> browser,
                CefRefPtr<CefFrame> frame,
                const std::string& message) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);

    // OnNotify only be called once.
    EXPECT_FALSE(got_notify_);
    got_notify_.yes();

    auto expected = GenerateResponse(string_length_, symbol_);

    switch (test_type_) {
      case TestType::SUCCESS:
        EXPECT_EQ(expected, message);
        break;
      case TestType::FAILURE:
        EXPECT_EQ(expected, message);
        break;
      default:
        ADD_FAILURE();
        break;
    }

    DestroyTest();
  }

  void ExecuteCallback(size_t response_size) {
    auto response = GenerateResponse(response_size, symbol_);

    EXPECT_TRUE(callback_.get());
    switch (test_type_) {
      case TestType::SUCCESS:
        callback_->Success(response);
        break;
      case TestType::FAILURE:
        callback_->Failure(kSingleQueryErrorCode, response);
        break;
      default:
        ADD_FAILURE();
        break;
    }
    callback_ = nullptr;
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);
    EXPECT_NE(0, query_id);
    EXPECT_FALSE(persistent);
    EXPECT_EQ(std::to_string(string_length_), request.ToString());

    const size_t message_size =
        static_cast<size_t>(std::stoi(request.ToString()));
    got_on_query_.yes();

    callback_ = callback;
    ExecuteCallback(message_size);

    return true;
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_notify_);
    EXPECT_TRUE(got_on_query_);
    EXPECT_FALSE(callback_.get());

    TestHandler::DestroyTest();
  }

 private:
  const TestType test_type_;
  const size_t string_length_;
  const CharType symbol_;

  CefRefPtr<Callback> callback_;

  TrackCallback got_on_query_;
  TrackCallback got_notify_;
};

using CharTestHandler = ThresholdTestHandler<char>;
using CharTestHandlerPtr = CefRefPtr<CharTestHandler>;

using WCharTestHandler = ThresholdTestHandler<wchar_t>;
using WCharTestHandlerPtr = CefRefPtr<WCharTestHandler>;

}  // namespace

TEST(MessageRouterTest, ThresholdMessageUnderSuccessCallback) {
  const auto under_threshold = kMsgSizeThresholdInBytes - kCharSize;
  const auto string_length = under_threshold / kCharSize;
  CharTestHandlerPtr handler =
      new CharTestHandler(TestType::SUCCESS, string_length, 'A');
  handler->SetMessageSizeThreshold(kMsgSizeThresholdInBytes);

  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);
}

TEST(MessageRouterTest, ThresholdMessageExactSuccessCallback) {
  const auto string_length = kMsgSizeThresholdInBytes / kCharSize;
  CharTestHandlerPtr handler =
      new CharTestHandler(TestType::SUCCESS, string_length, 'A');
  handler->SetMessageSizeThreshold(kMsgSizeThresholdInBytes);

  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);
}

TEST(MessageRouterTest, ThresholdMessageOverSuccessCallback) {
  const auto over_threshold = kMsgSizeThresholdInBytes + kCharSize;
  const auto string_length = over_threshold / kCharSize;
  CharTestHandlerPtr handler =
      new CharTestHandler(TestType::SUCCESS, string_length, 'A');
  handler->SetMessageSizeThreshold(kMsgSizeThresholdInBytes);

  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);
}

TEST(MessageRouterTest, ThresholdMessageUnderFailureCallback) {
  const auto under_threshold = kMsgSizeThresholdInBytes - kCharSize;
  const auto string_length = under_threshold / kCharSize;
  CharTestHandlerPtr handler =
      new CharTestHandler(TestType::FAILURE, string_length, 'A');
  handler->SetMessageSizeThreshold(kMsgSizeThresholdInBytes);

  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);
}

TEST(MessageRouterTest, ThresholdMessageOverFailureCallback) {
  const auto over_threshold = kMsgSizeThresholdInBytes + kCharSize;
  const auto string_length = over_threshold / kCharSize;
  CharTestHandlerPtr handler =
      new CharTestHandler(TestType::FAILURE, string_length, 'A');
  handler->SetMessageSizeThreshold(kMsgSizeThresholdInBytes);

  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);
}

TEST(MessageRouterTest, ThresholdUtf8MessageUnderSuccessCallback) {
  const auto under_threshold = kMsgSizeThresholdInBytes - kCharSize;
  const auto string_length = under_threshold / kCharSize;
  WCharTestHandlerPtr handler =
      new WCharTestHandler(TestType::SUCCESS, string_length, L'\u304B');
  handler->SetMessageSizeThreshold(kMsgSizeThresholdInBytes);

  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);
}

TEST(MessageRouterTest, ThresholdUtf8MessageOverSuccessCallback) {
  const auto over_threshold = kMsgSizeThresholdInBytes + kCharSize;
  const auto string_length = over_threshold / kCharSize;
  WCharTestHandlerPtr handler =
      new WCharTestHandler(TestType::SUCCESS, string_length, L'\u304B');
  handler->SetMessageSizeThreshold(kMsgSizeThresholdInBytes);

  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);
}
