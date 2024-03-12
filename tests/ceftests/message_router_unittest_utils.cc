// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/message_router_unittest_utils.h"

extern const char kJSQueryFunc[] = "mrtQuery";
extern const char kJSQueryCancelFunc[] = "mrtQueryCancel";

namespace {

const char kTestDomainRoot[] = "https://tests-mr";
const char kDoneMessageName[] = "mrtNotifyMsg";
const char kJSNotifyFunc[] = "mrtNotify";
const char kJSAssertTotalCountFunc[] = "mrtAssertTotalCount";
const char kJSAssertBrowserCountFunc[] = "mrtAssertBrowserCount";
const char kJSAssertContextCountFunc[] = "mrtAssertContextCount";

void SetRouterConfig(CefMessageRouterConfig& config) {
  config.js_query_function = kJSQueryFunc;
  config.js_cancel_function = kJSQueryCancelFunc;
}

}  // namespace

// Entry point for creating the test delegate.
// Called from client_app_delegates.cc.
void CreateMessageRouterRendererTests(
    ClientAppRenderer::DelegateSet& delegates) {
  delegates.insert(new MRRenderDelegate);
}

bool MRRenderDelegate::V8HandlerImpl::Execute(const CefString& name,
                                              CefRefPtr<CefV8Value> object,
                                              const CefV8ValueList& arguments,
                                              CefRefPtr<CefV8Value>& retval,
                                              CefString& exception) {
  const std::string& message_name = name;
  if (message_name == kJSNotifyFunc) {
    EXPECT_EQ(1U, arguments.size());
    EXPECT_TRUE(arguments[0]->IsString());

    const CefString& msg = arguments[0]->GetStringValue();
    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    CefRefPtr<CefFrame> frame = context->GetFrame();

    CefRefPtr<CefProcessMessage> message =
        CefProcessMessage::Create(kDoneMessageName);
    CefRefPtr<CefListValue> args = message->GetArgumentList();
    args->SetString(0, msg);
    frame->SendProcessMessage(PID_BROWSER, message);
    return true;
  } else {
    EXPECT_EQ(2U, arguments.size());
    EXPECT_TRUE(arguments[0]->IsInt());
    EXPECT_TRUE(arguments[1]->IsInt());

    const int line_no = arguments[0]->GetIntValue();
    const int expected_count = arguments[1]->GetIntValue();
    int actual_count = -1;

    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    CefRefPtr<CefBrowser> browser = context->GetBrowser();

    if (name == kJSAssertTotalCountFunc) {
      actual_count =
          delegate_->message_router_->GetPendingCount(nullptr, nullptr);
    } else if (name == kJSAssertBrowserCountFunc) {
      actual_count =
          delegate_->message_router_->GetPendingCount(browser, nullptr);
    } else if (name == kJSAssertContextCountFunc) {
      actual_count =
          delegate_->message_router_->GetPendingCount(browser, context);
    }

    if (expected_count != actual_count) {
      std::stringstream ss;
      ss << message_name << " failed (line " << line_no << "); expected "
         << expected_count << ", got " << actual_count;
      exception = ss.str();
    }
  }

  return true;
}

void MRRenderDelegate::OnWebKitInitialized(CefRefPtr<ClientAppRenderer> app) {
  // Create the renderer-side router for query handling.
  CefMessageRouterConfig config;
  SetRouterConfig(config);
  message_router_ = CefMessageRouterRendererSide::Create(config);
}

void MRRenderDelegate::OnContextCreated(CefRefPtr<ClientAppRenderer> app,
                                        CefRefPtr<CefBrowser> browser,
                                        CefRefPtr<CefFrame> frame,
                                        CefRefPtr<CefV8Context> context) {
  const std::string& url = frame->GetURL();
  if (url.find(kTestDomainRoot) != 0) {
    return;
  }

  message_router_->OnContextCreated(browser, frame, context);

  // Register function handlers with the 'window' object.
  CefRefPtr<CefV8Value> window = context->GetGlobal();

  CefRefPtr<V8HandlerImpl> handler = new V8HandlerImpl(this);
  CefV8Value::PropertyAttribute attributes =
      static_cast<CefV8Value::PropertyAttribute>(
          V8_PROPERTY_ATTRIBUTE_READONLY | V8_PROPERTY_ATTRIBUTE_DONTENUM |
          V8_PROPERTY_ATTRIBUTE_DONTDELETE);

  CefRefPtr<CefV8Value> notify_func =
      CefV8Value::CreateFunction(kJSNotifyFunc, handler.get());
  window->SetValue(kJSNotifyFunc, notify_func, attributes);

  CefRefPtr<CefV8Value> total_count_func =
      CefV8Value::CreateFunction(kJSAssertTotalCountFunc, handler.get());
  window->SetValue(kJSAssertTotalCountFunc, total_count_func, attributes);

  CefRefPtr<CefV8Value> browser_count_func =
      CefV8Value::CreateFunction(kJSAssertBrowserCountFunc, handler.get());
  window->SetValue(kJSAssertBrowserCountFunc, browser_count_func, attributes);

  CefRefPtr<CefV8Value> context_count_func =
      CefV8Value::CreateFunction(kJSAssertContextCountFunc, handler.get());
  window->SetValue(kJSAssertContextCountFunc, context_count_func, attributes);
}

void MRRenderDelegate::OnContextReleased(CefRefPtr<ClientAppRenderer> app,
                                         CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         CefRefPtr<CefV8Context> context) {
  const std::string& url = frame->GetURL();
  if (url.find(kTestDomainRoot) != 0) {
    return;
  }

  message_router_->OnContextReleased(browser, frame, context);
}

bool MRRenderDelegate::OnProcessMessageReceived(
    CefRefPtr<ClientAppRenderer> app,
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  const std::string& url = frame->GetURL();
  if (url.find(kTestDomainRoot) != 0) {
    return false;
  }

  return message_router_->OnProcessMessageReceived(browser, frame,
                                                   source_process, message);
}

void MRTestHandler::RunTest() {
  RunMRTest();

  // Time out the test after a reasonable period of time.
  SetTestTimeout(10000);
}

void MRTestHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  if (!message_router_.get()) {
    // Create the browser-side router for query handling.
    CefMessageRouterConfig config;

    SetRouterConfig(config);
    if (message_size_threshold_) {
      config.message_size_threshold = message_size_threshold_;
    }

    message_router_ = CefMessageRouterBrowserSide::Create(config);
    AddHandlers(message_router_);
  }
  TestHandler::OnAfterCreated(browser);
}

void MRTestHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  message_router_->OnBeforeClose(browser);
  TestHandler::OnBeforeClose(browser);
}

void MRTestHandler::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                              TerminationStatus status,
                                              int error_code,
                                              const CefString& error_string) {
  message_router_->OnRenderProcessTerminated(browser);
}

bool MRTestHandler::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefRequest> request,
                                   bool user_gesture,
                                   bool is_redirect) {
  message_router_->OnBeforeBrowse(browser, frame);
  return false;
}

// Returns true if the router handled the navigation.
bool MRTestHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  const std::string& message_name = message->GetName();
  if (message_name == kDoneMessageName) {
    CefRefPtr<CefListValue> args = message->GetArgumentList();
    EXPECT_EQ(1U, args->GetSize());
    EXPECT_EQ(VTYPE_STRING, args->GetType(0));
    OnNotify(browser, frame, args->GetString(0));
    return true;
  }

  return message_router_->OnProcessMessageReceived(browser, frame,
                                                   source_process, message);
}

CefRefPtr<CefMessageRouterBrowserSide> MRTestHandler::GetRouter() const {
  return message_router_;
}

void MRTestHandler::SetMessageSizeThreshold(size_t message_size_threshold) {
  message_size_threshold_ = message_size_threshold;
}

bool MRTestHandler::AssertQueryCount(
    CefRefPtr<CefBrowser> browser,
    CefMessageRouterBrowserSide::Handler* handler,
    int expected_count) {
  int actual_count = message_router_->GetPendingCount(browser, handler);
  EXPECT_EQ(expected_count, actual_count);
  return (expected_count == actual_count);
}

void MRTestHandler::AssertMainBrowser(CefRefPtr<CefBrowser> browser) {
  EXPECT_TRUE(browser.get());
  EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
}

SingleLoadTestHandler::SingleLoadTestHandler()
    : main_url_("https://tests-mr.com/main.html") {}

void SingleLoadTestHandler::RunMRTest() {
  AddOtherResources();
  AddResource(main_url_, GetMainHTML(), "text/html");

  CreateBrowser(main_url_, nullptr);
}

void SingleLoadTestHandler::AddHandlers(
    CefRefPtr<CefMessageRouterBrowserSide> message_router) {
  message_router->AddHandler(this, false);
}

void SingleLoadTestHandler::AssertMainFrame(CefRefPtr<CefFrame> frame) {
  EXPECT_TRUE(frame.get());
  EXPECT_TRUE(frame->IsMain());
  EXPECT_STREQ(main_url_.c_str(), frame->GetURL().ToString().c_str());
}
