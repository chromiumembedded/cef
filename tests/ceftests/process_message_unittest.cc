// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/cef_process_message.h"
#include "include/cef_task.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/renderer/client_app_renderer.h"

using client::ClientAppRenderer;

namespace {

// Unique values for the SendRecv test.
const char kSendRecvUrl[] = "http://tests/ProcessMessageTest.SendRecv";
const char kSendRecvMsg[] = "ProcessMessageTest.SendRecv";

// Creates a test message.
CefRefPtr<CefProcessMessage> CreateTestMessage() {
  CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create(kSendRecvMsg);
  EXPECT_TRUE(msg.get());
  EXPECT_TRUE(msg->IsValid());
  EXPECT_FALSE(msg->IsReadOnly());

  CefRefPtr<CefListValue> args = msg->GetArgumentList();
  EXPECT_TRUE(args.get());
  EXPECT_TRUE(args->IsValid());
  EXPECT_FALSE(args->IsReadOnly());

  size_t index = 0;
  args->SetNull(index++);
  args->SetInt(index++, 5);
  args->SetDouble(index++, 10.543);
  args->SetBool(index++, true);
  args->SetString(index++, "test string");
  args->SetList(index++, args->Copy());

  EXPECT_EQ(index, args->GetSize());

  return msg;
}

// Renderer side.
class SendRecvRendererTest : public ClientAppRenderer::Delegate {
 public:
  SendRecvRendererTest() {}

  bool OnProcessMessageReceived(CefRefPtr<ClientAppRenderer> app,
                                CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    if (message->GetName() == kSendRecvMsg) {
      EXPECT_TRUE(browser.get());
      EXPECT_TRUE(frame.get());
      EXPECT_EQ(PID_BROWSER, source_process);
      EXPECT_TRUE(message.get());
      EXPECT_TRUE(message->IsValid());
      EXPECT_TRUE(message->IsReadOnly());

      const std::string& url = frame->GetURL();
      if (url == kSendRecvUrl) {
        // Echo the message back to the sender natively.
        frame->SendProcessMessage(PID_BROWSER, message);
        EXPECT_FALSE(message->IsValid());
        return true;
      }
    }

    // Message not handled.
    return false;
  }

  IMPLEMENT_REFCOUNTING(SendRecvRendererTest);
};

// Browser side.
class SendRecvTestHandler : public TestHandler {
 public:
  explicit SendRecvTestHandler(cef_thread_id_t send_thread)
      : send_thread_(send_thread) {}

  void RunTest() override {
    AddResource(kSendRecvUrl, "<html><body>TEST</body></html>", "text/html");
    CreateBrowser(kSendRecvUrl);

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
                  base::BindOnce(&SendRecvTestHandler::SendMessage, this,
                                 browser, frame));
    } else {
      SendMessage(browser, frame);
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

    // Verify that the recieved message is the same as the sent message.
    TestProcessMessageEqual(CreateTestMessage(), message);

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
  void SendMessage(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) {
    EXPECT_TRUE(CefCurrentlyOn(send_thread_));
    auto message = CreateTestMessage();
    frame->SendProcessMessage(PID_RENDERER, message);

    // The message will be invalidated immediately, no matter what thread we
    // send from.
    EXPECT_FALSE(message->IsValid());
  }

  cef_thread_id_t send_thread_;
  TrackCallback got_message_;

  IMPLEMENT_REFCOUNTING(SendRecvTestHandler);
};

}  // namespace

// Verify send from the UI thread and recieve.
TEST(ProcessMessageTest, SendRecvUI) {
  CefRefPtr<SendRecvTestHandler> handler = new SendRecvTestHandler(TID_UI);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify send from the IO thread and recieve.
TEST(ProcessMessageTest, SendRecvIO) {
  CefRefPtr<SendRecvTestHandler> handler = new SendRecvTestHandler(TID_IO);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify create.
TEST(ProcessMessageTest, Create) {
  CefRefPtr<CefProcessMessage> message =
      CefProcessMessage::Create(kSendRecvMsg);
  EXPECT_TRUE(message.get());
  EXPECT_TRUE(message->IsValid());
  EXPECT_FALSE(message->IsReadOnly());
  EXPECT_STREQ(kSendRecvMsg, message->GetName().ToString().c_str());

  CefRefPtr<CefListValue> args = message->GetArgumentList();
  EXPECT_TRUE(args.get());
  EXPECT_TRUE(args->IsValid());
  EXPECT_FALSE(args->IsOwned());
  EXPECT_FALSE(args->IsReadOnly());
}

// Verify copy.
TEST(ProcessMessageTest, Copy) {
  CefRefPtr<CefProcessMessage> message = CreateTestMessage();
  CefRefPtr<CefProcessMessage> message2 = message->Copy();
  TestProcessMessageEqual(message, message2);
}

// Entry point for creating process message renderer test objects.
// Called from client_app_delegates.cc.
void CreateProcessMessageRendererTests(
    ClientAppRenderer::DelegateSet& delegates) {
  // For ProcessMessageTest.SendRecv
  delegates.insert(new SendRecvRendererTest);
}
