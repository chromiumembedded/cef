// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <array>

#include "include/base/cef_callback.h"
#include "include/cef_shared_process_message_builder.h"
#include "include/cef_task.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/renderer/client_app_renderer.h"

using client::ClientAppRenderer;

namespace {

struct TestData {
  bool flag = true;
  int value = 1;
  double d_value = 77.77;
  std::array<size_t, 50> buffer{};
};

const char kSharedMessageUrl[] = "https://tests/SendSharedProcessMessageTest";
const char kSharedMessageName[] = "SendSharedProcessMessageTest";

CefRefPtr<CefProcessMessage> CreateTestMessage(const TestData& data) {
  auto builder =
      CefSharedProcessMessageBuilder::Create(kSharedMessageName, sizeof(data));
  std::memcpy(builder->Memory(), reinterpret_cast<const void*>(&data),
              sizeof(data));
  return builder->Build();
}

// Renderer side.
class SharedMessageRendererTest final : public ClientAppRenderer::Delegate {
 public:
  bool OnProcessMessageReceived(CefRefPtr<ClientAppRenderer> app,
                                CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    if (message->GetName() == kSharedMessageName) {
      EXPECT_TRUE(browser.get());
      EXPECT_TRUE(frame.get());
      EXPECT_EQ(PID_BROWSER, source_process);
      EXPECT_TRUE(message.get());
      EXPECT_TRUE(message->IsValid());
      EXPECT_TRUE(message->IsReadOnly());
      EXPECT_EQ(message->GetArgumentList(), nullptr);

      const std::string& url = frame->GetURL();
      if (url == kSharedMessageUrl) {
        // Echo the message back to the sender natively.
        frame->SendProcessMessage(PID_BROWSER, message);
        EXPECT_FALSE(message->IsValid());
        return true;
      }
    }

    return false;
  }

  IMPLEMENT_REFCOUNTING(SharedMessageRendererTest);
};

// Browser side.
class SharedMessageTestHandler final : public TestHandler {
 public:
  explicit SharedMessageTestHandler(cef_thread_id_t send_thread)
      : send_thread_(send_thread) {}

  void RunTest() override {
    AddResource(kSharedMessageUrl, "<html><body>TEST</body></html>",
                "text/html");
    CreateBrowser(kSharedMessageUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));

    // Send the message to the renderer process.
    if (!CefCurrentlyOn(send_thread_)) {
      CefPostTask(send_thread_,
                  base::BindOnce(&SharedMessageTestHandler::SendProcessMessage,
                                 this, browser, frame));
    } else {
      SendProcessMessage(browser, frame);
    }
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_TRUE(browser.get());
    EXPECT_TRUE(frame.get());
    EXPECT_EQ(PID_RENDERER, source_process);
    EXPECT_TRUE(message.get());
    EXPECT_TRUE(message->IsValid());
    EXPECT_TRUE(message->IsReadOnly());
    EXPECT_EQ(message->GetArgumentList(), nullptr);

    // Verify that the received message is the same as the sent message.
    auto region = message->GetSharedMemoryRegion();
    const TestData* received = static_cast<const TestData*>(region->Memory());
    EXPECT_EQ(data_.flag, received->flag);
    EXPECT_EQ(data_.value, received->value);
    EXPECT_EQ(data_.d_value, received->d_value);

    got_message_.yes();

    // Test is complete.
    DestroyTest();

    return true;
  }

 protected:
  void DestroyTest() override {
    EXPECT_TRUE(got_message_);
    TestHandler::DestroyTest();
  }

 private:
  void SendProcessMessage(const CefRefPtr<CefBrowser>& browser,
                          const CefRefPtr<CefFrame>& frame) {
    EXPECT_TRUE(CefCurrentlyOn(send_thread_));

    auto message = CreateTestMessage(data_);
    frame->SendProcessMessage(PID_RENDERER, message);

    // The message is invalidated immediately
    EXPECT_FALSE(message->IsValid());
  }

  cef_thread_id_t send_thread_;
  TrackCallback got_message_;
  const TestData data_;

  IMPLEMENT_REFCOUNTING(SharedMessageTestHandler);
};

}  // namespace

TEST(SendSharedProcessMessageTest, CanSendAndReceiveFromUiThread) {
  CefRefPtr<SharedMessageTestHandler> handler =
      new SharedMessageTestHandler(TID_UI);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(SendSharedProcessMessageTest, CanSendAndReceiveFromIoThread) {
  CefRefPtr<SharedMessageTestHandler> handler =
      new SharedMessageTestHandler(TID_IO);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Entry point for creating shared process message renderer test objects.
// Called from client_app_delegates.cc.
void CreateSharedProcessMessageTests(
    ClientAppRenderer::DelegateSet& delegates) {
  delegates.insert(new SharedMessageRendererTest());
}
