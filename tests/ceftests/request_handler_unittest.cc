// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "include/base/cef_callback.h"
#include "include/cef_cookie.h"
#include "include/cef_request_context_handler.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/renderer/client_app_renderer.h"

using client::ClientAppBrowser;
using client::ClientAppRenderer;

namespace {

enum NetNotifyTestType {
  NNTT_NONE = 0,
  NNTT_NORMAL,
  NNTT_DELAYED_RENDERER,
  NNTT_DELAYED_BROWSER,
};

const char kNetNotifyOrigin1[] = "http://tests-netnotify1/";
const char kNetNotifyOrigin2[] = "http://tests-netnotify2/";
const char kNetNotifyMsg[] = "RequestHandlerTest.NetNotify";
const char kNetNotifyTestCmdKey[] = "rh-net-notify-test";

// Browser side.
class NetNotifyTestHandler : public TestHandler {
 public:
  NetNotifyTestHandler(CompletionState* completion_state,
                       NetNotifyTestType test_type,
                       bool same_origin)
      : TestHandler(completion_state),
        test_type_(test_type),
        same_origin_(same_origin) {}

  void SetupTest() override {
    std::stringstream ss;
    ss << kNetNotifyOrigin1 << "nav1.html?t=" << test_type_;
    url1_ = ss.str();
    ss.str("");
    ss << (same_origin_ ? kNetNotifyOrigin1 : kNetNotifyOrigin2)
       << "nav2.html?t=" << test_type_;
    url2_ = ss.str();

    const std::string& resource1 =
        "<html>"
        "<head><script>document.cookie='name1=value1';</script></head>"
        "<body>Nav1</body>"
        "</html>";
    response_length1_ = static_cast<int64>(resource1.size());
    AddResource(url1_, resource1, "text/html");

    const std::string& resource2 =
        "<html>"
        "<head><script>document.cookie='name2=value2';</script></head>"
        "<body>Nav2</body>"
        "</html>";
    response_length2_ = static_cast<int64>(resource2.size());
    AddResource(url2_, resource2, "text/html");

    // Create the request context that will use an in-memory cache.
    CefRequestContextSettings settings;
    CefRefPtr<CefRequestContext> request_context =
        CefRequestContext::CreateContext(settings, nullptr);
    cookie_manager_ = request_context->GetCookieManager(nullptr);

    CefRefPtr<CefDictionaryValue> extra_info = CefDictionaryValue::Create();
    extra_info->SetBool(kNetNotifyTestCmdKey, true);

    // Create browser that loads the 1st URL.
    CreateBrowser(url1_, request_context, extra_info);
  }

  void RunTest() override {
    // Navigate to the 2nd URL.
    GetBrowser()->GetMainFrame()->LoadURL(url2_);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    const std::string& url = request->GetURL();
    if (IgnoreURL(url))
      return RV_CONTINUE;

    if (url.find(url1_) == 0)
      got_before_resource_load1_.yes();
    else if (url.find(url2_) == 0)
      got_before_resource_load2_.yes();
    else
      EXPECT_TRUE(false);  // Not reached

    return RV_CONTINUE;
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    const std::string& url = request->GetURL();
    if (IgnoreURL(url))
      return nullptr;

    if (url.find(url1_) == 0)
      got_get_resource_handler1_.yes();
    else if (url.find(url2_) == 0)
      got_get_resource_handler2_.yes();
    else
      EXPECT_TRUE(false);  // Not reached

    return TestHandler::GetResourceHandler(browser, frame, request);
  }

  void OnResourceLoadComplete(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              CefRefPtr<CefResponse> response,
                              URLRequestStatus status,
                              int64 received_content_length) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    const std::string& url = request->GetURL();
    if (IgnoreURL(url))
      return;

    EXPECT_EQ(UR_SUCCESS, status);
    if (url.find(url1_) == 0) {
      got_resource_load_complete1_.yes();
      EXPECT_EQ(response_length1_, received_content_length);
    } else if (url.find(url2_) == 0) {
      got_resource_load_complete2_.yes();
      EXPECT_EQ(response_length2_, received_content_length);
    } else {
      EXPECT_TRUE(false);  // Not reached
    }
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override {
    std::string url = request->GetURL();

    // Check if the load has already been delayed.
    bool delay_loaded = (url.find("delayed=true") != std::string::npos);

    if (url.find(url1_) == 0) {
      got_before_browse1_.yes();
      EXPECT_FALSE(delay_loaded);
    } else if (url.find(url2_) == 0) {
      got_before_browse2_.yes();
      if (delay_loaded) {
        got_before_browse2_delayed_.yes();
      } else if (test_type_ == NNTT_DELAYED_RENDERER ||
                 test_type_ == NNTT_DELAYED_BROWSER) {
        got_before_browse2_will_delay_.yes();

        // Navigating cross-origin from the browser process will cause a new
        // render process to be created. We therefore need some information in
        // the request itself to tell us that the navigation has already been
        // delayed.
        // Navigating cross-origin from the renderer process will cause the
        // process to be terminated with "bad IPC message" reason
        // INVALID_INITIATOR_ORIGIN (213).
        url += "&delayed=true";

        if (test_type_ == NNTT_DELAYED_RENDERER) {
          // Load the URL from the render process.
          CefRefPtr<CefProcessMessage> message =
              CefProcessMessage::Create(kNetNotifyMsg);
          CefRefPtr<CefListValue> args = message->GetArgumentList();
          args->SetInt(0, test_type_);
          args->SetString(1, url);
          frame->SendProcessMessage(PID_RENDERER, message);
        } else {
          // Load the URL from the browser process.
          frame->LoadURL(url);
        }

        // Cancel the load.
        return true;
      }
    } else {
      EXPECT_TRUE(false);  // Not reached
    }

    // Allow the load to continue.
    return false;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    const std::string& url = frame->GetURL();
    if (url.find(url1_) == 0) {
      got_load_end1_.yes();
      SetupCompleteIfDone();
    } else if (url.find(url2_) == 0) {
      got_load_end2_.yes();
      FinishTestIfDone();
    } else {
      EXPECT_TRUE(false);  // Not reached
    }
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    if (message->GetName().ToString() == kNetNotifyMsg) {
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      EXPECT_TRUE(args.get());

      std::string url = args->GetString(0);
      if (url.find(url1_) == 0) {
        got_process_message1_.yes();
        SetupCompleteIfDone();
      } else if (url.find(url2_) == 0) {
        got_process_message2_.yes();
        FinishTestIfDone();
      } else {
        EXPECT_TRUE(false);  // Not reached
      }

      return true;
    }

    // Message not handled.
    return false;
  }

  void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                 TerminationStatus status) override {
    got_process_terminated_ct_++;

    // Termination is expected for cross-origin requests initiated from the
    // renderer process.
    if (!(test_type_ == NNTT_DELAYED_RENDERER && !same_origin_)) {
      TestHandler::OnRenderProcessTerminated(browser, status);
    }

    FinishTest();
  }

 protected:
  void SetupCompleteIfDone() {
    if (got_load_end1_ && got_process_message1_)
      SetupComplete();
  }

  void FinishTestIfDone() {
    if (got_load_end2_ && got_process_message2_)
      FinishTest();
  }

  void FinishTest() {
    // Verify that cookies were set correctly.
    class TestVisitor : public CefCookieVisitor {
     public:
      explicit TestVisitor(NetNotifyTestHandler* handler) : handler_(handler) {}
      ~TestVisitor() override {
        // Destroy the test.
        CefPostTask(TID_UI, base::BindOnce(&NetNotifyTestHandler::DestroyTest,
                                           handler_));
      }

      bool Visit(const CefCookie& cookie,
                 int count,
                 int total,
                 bool& deleteCookie) override {
        const std::string& name = CefString(&cookie.name);
        const std::string& value = CefString(&cookie.value);
        if (name == "name1" && value == "value1") {
          handler_->got_cookie1_.yes();
          deleteCookie = true;
        } else if (name == "name2" && value == "value2") {
          handler_->got_cookie2_.yes();
          deleteCookie = true;
        }
        return true;
      }

     private:
      NetNotifyTestHandler* handler_;
      IMPLEMENT_REFCOUNTING(TestVisitor);
    };

    cookie_manager_->VisitAllCookies(new TestVisitor(this));
  }

  void DestroyTest() override {
    int browser_id = GetBrowser()->GetIdentifier();

    // Verify test expectations.
    EXPECT_TRUE(got_before_browse1_) << " browser " << browser_id;
    EXPECT_TRUE(got_load_end1_) << " browser " << browser_id;
    EXPECT_TRUE(got_before_resource_load1_) << " browser " << browser_id;
    EXPECT_TRUE(got_get_resource_handler1_) << " browser " << browser_id;
    EXPECT_TRUE(got_resource_load_complete1_) << " browser " << browser_id;
    EXPECT_TRUE(got_cookie1_) << " browser " << browser_id;
    EXPECT_TRUE(got_process_message1_) << " browser " << browser_id;
    EXPECT_TRUE(got_before_browse2_) << " browser " << browser_id;

    if (test_type_ == NNTT_DELAYED_RENDERER && !same_origin_) {
      EXPECT_EQ(1, got_process_terminated_ct_) << " browser " << browser_id;
      EXPECT_FALSE(got_load_end2_) << " browser " << browser_id;
      EXPECT_FALSE(got_before_resource_load2_) << " browser " << browser_id;
      EXPECT_FALSE(got_get_resource_handler2_) << " browser " << browser_id;
      EXPECT_FALSE(got_resource_load_complete2_) << " browser " << browser_id;
      EXPECT_FALSE(got_cookie2_) << " browser " << browser_id;
      EXPECT_FALSE(got_process_message2_) << " browser " << browser_id;
    } else {
      EXPECT_EQ(0, got_process_terminated_ct_) << " browser " << browser_id;
      EXPECT_TRUE(got_load_end2_) << " browser " << browser_id;
      EXPECT_TRUE(got_before_resource_load2_) << " browser " << browser_id;
      EXPECT_TRUE(got_get_resource_handler2_) << " browser " << browser_id;
      EXPECT_TRUE(got_resource_load_complete2_) << " browser " << browser_id;
      EXPECT_TRUE(got_cookie2_) << " browser " << browser_id;
      EXPECT_TRUE(got_process_message2_) << " browser " << browser_id;
    }

    if (test_type_ == NNTT_DELAYED_RENDERER ||
        test_type_ == NNTT_DELAYED_BROWSER) {
      EXPECT_TRUE(got_before_browse2_will_delay_) << " browser " << browser_id;
      if (test_type_ == NNTT_DELAYED_RENDERER && !same_origin_) {
        EXPECT_FALSE(got_before_browse2_delayed_) << " browser " << browser_id;
      } else {
        EXPECT_TRUE(got_before_browse2_delayed_) << " browser " << browser_id;
      }
    } else {
      EXPECT_FALSE(got_before_browse2_will_delay_) << " browser " << browser_id;
      EXPECT_FALSE(got_before_browse2_delayed_) << " browser " << browser_id;
    }

    cookie_manager_ = nullptr;

    TestHandler::DestroyTest();
  }

  NetNotifyTestType test_type_;
  bool same_origin_;
  std::string url1_;
  std::string url2_;

  CefRefPtr<CefCookieManager> cookie_manager_;

  TrackCallback got_before_browse1_;
  TrackCallback got_load_end1_;
  TrackCallback got_before_resource_load1_;
  TrackCallback got_get_resource_handler1_;
  TrackCallback got_resource_load_complete1_;
  TrackCallback got_cookie1_;
  TrackCallback got_process_message1_;
  TrackCallback got_before_browse2_;
  TrackCallback got_load_end2_;
  TrackCallback got_before_resource_load2_;
  TrackCallback got_get_resource_handler2_;
  TrackCallback got_resource_load_complete2_;
  TrackCallback got_cookie2_;
  TrackCallback got_process_message2_;
  TrackCallback got_before_browse2_will_delay_;
  TrackCallback got_before_browse2_delayed_;
  int got_process_terminated_ct_ = 0;

  int64 response_length1_;
  int64 response_length2_;

  IMPLEMENT_REFCOUNTING(NetNotifyTestHandler);
};

// Renderer side.
class NetNotifyRendererTest : public ClientAppRenderer::Delegate,
                              public CefLoadHandler {
 public:
  NetNotifyRendererTest() : run_test_(false) {}

  void OnBrowserCreated(CefRefPtr<ClientAppRenderer> app,
                        CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefDictionaryValue> extra_info) override {
    run_test_ = extra_info && extra_info->HasKey(kNetNotifyTestCmdKey);
  }

  CefRefPtr<CefLoadHandler> GetLoadHandler(
      CefRefPtr<ClientAppRenderer> app) override {
    if (run_test_)
      return this;
    return nullptr;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (!run_test_)
      return;

    const std::string& url = frame->GetURL();

    // Continue in the browser process.
    CefRefPtr<CefProcessMessage> message =
        CefProcessMessage::Create(kNetNotifyMsg);
    CefRefPtr<CefListValue> args = message->GetArgumentList();
    args->SetString(0, url);
    frame->SendProcessMessage(PID_BROWSER, message);
  }

  bool OnProcessMessageReceived(CefRefPtr<ClientAppRenderer> app,
                                CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    if (message->GetName().ToString() == kNetNotifyMsg) {
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      EXPECT_TRUE(args.get());

      NetNotifyTestType test_type =
          static_cast<NetNotifyTestType>(args->GetInt(0));
      EXPECT_EQ(test_type, NNTT_DELAYED_RENDERER);

      const std::string& url = args->GetString(1);

      // Load the URL from the render process.
      frame->LoadURL(url);
      return true;
    }

    // Message not handled.
    return false;
  }

 private:
  bool run_test_;

  IMPLEMENT_REFCOUNTING(NetNotifyRendererTest);
};

void RunNetNotifyTest(NetNotifyTestType test_type,
                      bool same_origin,
                      size_t count = 3U) {
  TestHandler::CompletionState completion_state(static_cast<int>(count));
  TestHandler::Collection collection(&completion_state);

  std::vector<CefRefPtr<NetNotifyTestHandler>> handlers;
  for (size_t i = 0U; i < count; ++i) {
    CefRefPtr<NetNotifyTestHandler> handler =
        new NetNotifyTestHandler(&completion_state, test_type, same_origin);
    collection.AddTestHandler(handler.get());
    handlers.push_back(handler);
  }

  collection.ExecuteTests();

  while (!handlers.empty()) {
    auto handler = handlers.front();
    handlers.erase(handlers.begin());
    ReleaseAndWaitForDestructor(handler);
  }
}

}  // namespace

// Verify network notifications for multiple browsers existing simultaniously.
// URL loading is from the same origin and is not delayed.
TEST(RequestHandlerTest, NotificationsSameOriginDirect) {
  RunNetNotifyTest(NNTT_NORMAL, true);
}

// Verify network notifications for multiple browsers existing simultaniously.
// URL loading is from the same origin and is continued asynchronously from the
// render process.
TEST(RequestHandlerTest, NotificationsSameOriginDelayedRenderer) {
  RunNetNotifyTest(NNTT_DELAYED_RENDERER, true);
}

// Verify network notifications for multiple browsers existing simultaniously.
// URL loading is from the same origin and is continued asynchronously from the
// browser process.
TEST(RequestHandlerTest, NotificationsSameOriginDelayedBrowser) {
  RunNetNotifyTest(NNTT_DELAYED_BROWSER, true);
}

// Verify network notifications for multiple browsers existing simultaniously.
// URL loading is from a different origin and is not delayed.
TEST(RequestHandlerTest, NotificationsCrossOriginDirect) {
  RunNetNotifyTest(NNTT_NORMAL, false);
}

// Verify network notifications for multiple browsers existing simultaniously.
// URL loading is from a different origin and is continued asynchronously from
// the render process.
TEST(RequestHandlerTest, NotificationsCrossOriginDelayedRenderer) {
  RunNetNotifyTest(NNTT_DELAYED_RENDERER, false);
}

// Verify network notifications for multiple browsers existing simultaniously.
// URL loading is from a different origin and is continued asynchronously from
// the browser process.
TEST(RequestHandlerTest, NotificationsCrossOriginDelayedBrowser) {
  RunNetNotifyTest(NNTT_DELAYED_BROWSER, false);
}

// Entry point for creating request handler renderer test objects.
// Called from client_app_delegates.cc.
void CreateRequestHandlerRendererTests(
    ClientAppRenderer::DelegateSet& delegates) {
  delegates.insert(new NetNotifyRendererTest);
}
