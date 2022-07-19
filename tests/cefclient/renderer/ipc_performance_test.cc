// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/renderer/ipc_performance_test.h"

#include "include/cef_shared_process_message_builder.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/shared/common/binary_value_utils.h"

namespace {

// JS object member names.
constexpr char kMessageSize[] = "size";
constexpr char kTestId[] = "testId";
constexpr CefV8Value::PropertyAttribute kAttributes =
    static_cast<CefV8Value::PropertyAttribute>(
        V8_PROPERTY_ATTRIBUTE_READONLY | V8_PROPERTY_ATTRIBUTE_DONTENUM |
        V8_PROPERTY_ATTRIBUTE_DONTDELETE);

struct TestInfo {
  size_t message_size = 0;
  int id = 0;
  bool is_valid = false;
};

TestInfo GetTest(const CefV8ValueList& arguments, CefString& exception) {
  TestInfo info{};

  if (arguments.size() != 1 || !arguments[0]->IsObject()) {
    exception = "Invalid arguments; expecting a single object";
    return info;
  }

  CefRefPtr<CefV8Value> arg = arguments[0];
  CefRefPtr<CefV8Value> message_size = arg->GetValue(kMessageSize);
  if (!message_size.get() || !message_size->IsInt()) {
    exception =
        "Invalid arguments; object member 'size' is required and must have "
        "integer type";
    return info;
  }

  if (message_size->GetIntValue() < 1) {
    exception =
        "Invalid arguments; object member 'size' must be "
        "positive";
    return info;
  }

  CefRefPtr<CefV8Value> test_id = arg->GetValue(kTestId);
  if (!message_size.get() || !message_size->IsInt()) {
    exception =
        "Invalid arguments; object member 'testId' is required and must "
        "have integer type";
    return info;
  }

  info.message_size = static_cast<size_t>(message_size->GetIntValue());
  info.id = test_id->GetIntValue();
  info.is_valid = true;

  return info;
}

// Handle bindings in the render process.
class IpcDelegate final : public client::ClientAppRenderer::Delegate {
 public:
  class V8HandlerImpl final : public CefV8Handler {
   public:
    explicit V8HandlerImpl(const CefRefPtr<IpcDelegate>& delegate)
        : delegate_(delegate) {}
    V8HandlerImpl(const V8HandlerImpl&) = delete;
    V8HandlerImpl& operator=(const V8HandlerImpl&) = delete;

    bool Execute(const CefString& name,
                 CefRefPtr<CefV8Value> object,
                 const CefV8ValueList& arguments,
                 CefRefPtr<CefV8Value>& retval,
                 CefString& exception) override {
      if (name == bv_utils::kTestSendProcessMessage) {
        const auto test = GetTest(arguments, exception);
        if (test.is_valid) {
          SendTestProcessMessage(test.message_size, test.id);
        }
        return true;
      }

      if (name == bv_utils::kTestSendSMRProcessMessage) {
        const auto test = GetTest(arguments, exception);
        if (test.is_valid) {
          SendTestSMRProcessMessage(test.message_size, test.id);
        }
        return true;
      }

      return false;
    }

   private:
    void SendTestProcessMessage(size_t message_size, int test_id) {
      auto context = CefV8Context::GetCurrentContext();
      delegate_->SendTestProcessMessage(context->GetFrame(), message_size,
                                        test_id);
    }

    void SendTestSMRProcessMessage(size_t message_size, int test_id) {
      auto context = CefV8Context::GetCurrentContext();
      delegate_->SendTestSMRProcessMessage(context->GetFrame(), message_size,
                                           test_id);
    }

    CefRefPtr<IpcDelegate> delegate_;
    IMPLEMENT_REFCOUNTING(V8HandlerImpl);
  };

  IpcDelegate() = default;
  IpcDelegate(const IpcDelegate&) = delete;
  IpcDelegate& operator=(const IpcDelegate&) = delete;

  void OnContextCreated(CefRefPtr<client::ClientAppRenderer> app,
                        CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override {
    CEF_REQUIRE_RENDERER_THREAD();

    CefRefPtr<CefV8Handler> handler = new V8HandlerImpl(this);

    // Register function handlers with the 'window' object.
    auto window = context->GetGlobal();
    window->SetValue(
        bv_utils::kTestSendProcessMessage,
        CefV8Value::CreateFunction(bv_utils::kTestSendProcessMessage, handler),
        kAttributes);

    window->SetValue(bv_utils::kTestSendSMRProcessMessage,
                     CefV8Value::CreateFunction(
                         bv_utils::kTestSendSMRProcessMessage, handler),
                     kAttributes);
  }

  bool OnProcessMessageReceived(CefRefPtr<client::ClientAppRenderer> app,
                                CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    CEF_REQUIRE_RENDERER_THREAD();
    const auto finish_time = bv_utils::Now();

    if (message->GetName() == bv_utils::kTestSendProcessMessage) {
      auto args = message->GetArgumentList();
      DCHECK_EQ(args->GetSize(), 1U);

      const auto browser_msg =
          bv_utils::GetBrowserMsgFromBinary(args->GetBinary(0));
      PassTestResultToJs(frame, finish_time, browser_msg);

      return true;
    }

    if (message->GetName() == bv_utils::kTestSendSMRProcessMessage) {
      const auto region = message->GetSharedMemoryRegion();
      DCHECK(region->IsValid());
      DCHECK_GE(region->Size(), sizeof(bv_utils::BrowserMessage));

      const auto browser_msg =
          static_cast<const bv_utils::BrowserMessage*>(region->Memory());

      PassTestResultToJs(frame, finish_time, *browser_msg);

      return true;
    }

    return false;
  }

 private:
  IMPLEMENT_REFCOUNTING(IpcDelegate);

  void SendTestProcessMessage(CefRefPtr<CefFrame> frame,
                              size_t message_size,
                              int test_id) {
    CEF_REQUIRE_RENDERER_THREAD();

    auto process_message =
        CefProcessMessage::Create(bv_utils::kTestSendProcessMessage);
    auto args = process_message->GetArgumentList();

    const auto buffer_size =
        std::max(message_size, sizeof(bv_utils::RendererMessage));
    std::vector<uint8_t> buffer(buffer_size);

    const auto renderer_msg =
        reinterpret_cast<bv_utils::RendererMessage*>(buffer.data());

    renderer_msg->test_id = test_id;
    renderer_msg->start_time = bv_utils::Now();

    args->SetBinary(0, bv_utils::CreateCefBinaryValue(buffer));
    frame->SendProcessMessage(PID_BROWSER, process_message);
  }

  void SendTestSMRProcessMessage(CefRefPtr<CefFrame> frame,
                                 size_t message_size,
                                 int test_id) {
    CEF_REQUIRE_RENDERER_THREAD();

    const auto buffer_size =
        std::max(message_size, sizeof(bv_utils::RendererMessage));
    const auto start_time = bv_utils::Now();

    auto builder = CefSharedProcessMessageBuilder::Create(
        bv_utils::kTestSendSMRProcessMessage, buffer_size);

    auto renderer_msg =
        static_cast<bv_utils::RendererMessage*>(builder->Memory());
    renderer_msg->test_id = test_id;
    renderer_msg->start_time = start_time;

    frame->SendProcessMessage(PID_BROWSER, builder->Build());
  }

  // Execute the onSuccess JavaScript callback.
  void PassTestResultToJs(CefRefPtr<CefFrame> frame,
                          const bv_utils::TimePoint& finish_time,
                          const bv_utils::BrowserMessage& msg) {
    const auto rendered_to_browser = msg.duration;
    const auto browser_to_rendered = finish_time - msg.start_time;

    CefString code = "testSendProcessMessageResult(" +
                     std::to_string(msg.test_id) + ", " +
                     bv_utils::ToMilliString(rendered_to_browser) + ", " +
                     bv_utils::ToMilliString(browser_to_rendered) + ");";

    frame->ExecuteJavaScript(code, frame->GetURL(), 0);
  }
};

}  // namespace

namespace client {
namespace ipc_performance_test {

void CreateDelegates(ClientAppRenderer::DelegateSet& delegates) {
  delegates.insert(new IpcDelegate());
}

}  // namespace ipc_performance_test
}  // namespace client
