// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/unittests/test_handler.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "include/cef_cookie.h"
#include "include/cef_runnable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tests/cefclient/client_app.h"

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

// Browser side.
class NetNotifyTestHandler : public TestHandler {
 public:
  NetNotifyTestHandler(CompletionState* completion_state,
                       NetNotifyTestType test_type,
                       bool same_origin)
      : TestHandler(completion_state),
        test_type_(test_type),
        same_origin_(same_origin) {}

  virtual void SetupTest() OVERRIDE {
    url1_ = base::StringPrintf("%snav1.html?t=%d",
        kNetNotifyOrigin1, test_type_);
    url2_ = base::StringPrintf("%snav2.html?t=%d",
        same_origin_ ? kNetNotifyOrigin1 : kNetNotifyOrigin2, test_type_);

    cookie_manager_ = CefCookieManager::CreateManager(CefString(), true);

    AddResource(url1_,
        "<html>"
        "<head><script>document.cookie='name1=value1';</script></head>"
        "<body>Nav1</body>"
        "</html>", "text/html");
    AddResource(url2_,
        "<html>"
        "<head><script>document.cookie='name2=value2';</script></head>"
        "<body>Nav2</body>"
        "</html>", "text/html");

    // Create browser that loads the 1st URL.
    CreateBrowser(url1_);
  }

  virtual void RunTest() OVERRIDE {
    // Navigate to the 2nd URL.
    GetBrowser()->GetMainFrame()->LoadURL(url2_);
  }

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request) OVERRIDE {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    const std::string& url = request->GetURL();
    if (url.find(url1_) == 0)
      got_before_resource_load1_.yes();
    else if (url.find(url2_) == 0)
      got_before_resource_load2_.yes();
    else
      EXPECT_TRUE(false);  // Not reached

    return false;
  }

  virtual CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) OVERRIDE {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    const std::string& url = request->GetURL();
    if (url.find(url1_) == 0)
      got_get_resource_handler1_.yes();
    else if (url.find(url2_) == 0)
      got_get_resource_handler2_.yes();
    else
      EXPECT_TRUE(false);  // Not reached

    return TestHandler::GetResourceHandler(browser,  frame, request);
  }

  virtual CefRefPtr<CefCookieManager> GetCookieManager(
      CefRefPtr<CefBrowser> browser,
      const CefString& main_url) OVERRIDE {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    const std::string& url = main_url;
    if (url.find(url1_) == 0)
      got_get_cookie_manager1_.yes();
    else if (url.find(url2_) == 0)
      got_get_cookie_manager2_.yes();
    else
      EXPECT_TRUE(false);  // Not reached

    return cookie_manager_;
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    const std::string& url = frame->GetURL();
    if (url.find(url1_) == 0) {
      got_load_end1_.yes();
      SetupComplete();
    } else if (url.find(url2_) == 0) {
      got_load_end2_.yes();
      FinishTest();
    } else {
      EXPECT_TRUE(false);  // Not reached
    }
  }

  virtual bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) OVERRIDE {
    if (message->GetName().ToString() == kNetNotifyMsg) {
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      EXPECT_TRUE(args.get());

      NetNotifyTestType test_type =
          static_cast<NetNotifyTestType>(args->GetInt(0));
      EXPECT_EQ(test_type, test_type_);

      std::string url = args->GetString(1);
      if (url.find(url1_) == 0)
        got_process_message1_.yes();
      else if (url.find(url2_) == 0)
        got_process_message2_.yes();
      else
        EXPECT_TRUE(false);  // Not reached

      // Navigating cross-origin from the browser process will cause a new
      // render process to be created. We therefore need some information in
      // the request itself to tell us that the navigation has already been
      // delayed.
      url += "&delayed=true";

      if (test_type == NNTT_DELAYED_RENDERER) {
        // Load the URL from the render process.
        CefRefPtr<CefProcessMessage> message =
            CefProcessMessage::Create(kNetNotifyMsg);
        CefRefPtr<CefListValue> args = message->GetArgumentList();
        args->SetInt(0, test_type);
        args->SetString(1, url);
        EXPECT_TRUE(browser->SendProcessMessage(PID_RENDERER, message));
      } else {
        // Load the URL from the browser process.
        browser->GetMainFrame()->LoadURL(url);
      }
      return true;
    }

    // Message not handled.
    return false;
  }

 protected:
  void FinishTest() {
    // Verify that cookies were set correctly.
    class TestVisitor : public CefCookieVisitor {
     public:
      explicit TestVisitor(NetNotifyTestHandler* handler)
          : handler_(handler) {
      }
      virtual ~TestVisitor()   {
        // Destroy the test.
        CefPostTask(TID_UI,
            NewCefRunnableMethod(handler_, &NetNotifyTestHandler::DestroyTest));
      }

      virtual bool Visit(const CefCookie& cookie, int count, int total,
                         bool& deleteCookie)   {
        const std::string& name = CefString(&cookie.name);
        const std::string& value = CefString(&cookie.value);
        if (name == "name1" && value == "value1")
          handler_->got_cookie1_.yes();
        else if (name == "name2" && value == "value2")
          handler_->got_cookie2_.yes();
        return true;
      }

     private:
      NetNotifyTestHandler* handler_;
      IMPLEMENT_REFCOUNTING(TestVisitor);
    };

    cookie_manager_->VisitAllCookies(new TestVisitor(this));
  }

  virtual void DestroyTest() OVERRIDE {
    int browser_id = GetBrowser()->GetIdentifier();

    // Verify test expectations.
    EXPECT_TRUE(got_load_end1_) << " browser " << browser_id;
    EXPECT_TRUE(got_before_resource_load1_) << " browser " << browser_id;
    EXPECT_TRUE(got_get_resource_handler1_) << " browser " << browser_id;
    EXPECT_TRUE(got_get_cookie_manager1_) << " browser " << browser_id;
    EXPECT_TRUE(got_cookie1_) << " browser " << browser_id;
    EXPECT_TRUE(got_load_end2_) << " browser " << browser_id;
    EXPECT_TRUE(got_before_resource_load2_) << " browser " << browser_id;
    EXPECT_TRUE(got_get_resource_handler2_) << " browser " << browser_id;
    EXPECT_TRUE(got_get_cookie_manager2_) << " browser " << browser_id;
    EXPECT_TRUE(got_cookie2_) << " browser " << browser_id;

    if (test_type_ == NNTT_DELAYED_RENDERER ||
        test_type_ == NNTT_DELAYED_BROWSER) {
      EXPECT_TRUE(got_process_message1_) << " browser " << browser_id;
      EXPECT_TRUE(got_process_message2_) << " browser " << browser_id;
    } else {
      EXPECT_FALSE(got_process_message1_) << " browser " << browser_id;
      EXPECT_FALSE(got_process_message2_) << " browser " << browser_id;
    }

    cookie_manager_ = NULL;

    TestHandler::DestroyTest();
  }

  NetNotifyTestType test_type_;
  bool same_origin_;
  std::string url1_;
  std::string url2_;

  CefRefPtr<CefCookieManager> cookie_manager_;

  TrackCallback got_load_end1_;
  TrackCallback got_before_resource_load1_;
  TrackCallback got_get_resource_handler1_;
  TrackCallback got_get_cookie_manager1_;
  TrackCallback got_cookie1_;
  TrackCallback got_process_message1_;
  TrackCallback got_load_end2_;
  TrackCallback got_before_resource_load2_;
  TrackCallback got_get_resource_handler2_;
  TrackCallback got_get_cookie_manager2_;
  TrackCallback got_cookie2_;
  TrackCallback got_process_message2_;
};

// Renderer side.
class NetNotifyRendererTest : public ClientApp::RenderDelegate {
 public:
  NetNotifyRendererTest() {}

  virtual bool OnBeforeNavigation(CefRefPtr<ClientApp> app,
                                  CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefRequest> request,
                                  cef_navigation_type_t navigation_type,
                                  bool is_redirect) OVERRIDE {
    const std::string& url = request->GetURL();

    // Don't execute this method for unrelated tests.
    if (url.find(kNetNotifyOrigin1) == std::string::npos &&
        url.find(kNetNotifyOrigin2) == std::string::npos) {
        return false;
    }

    NetNotifyTestType test_type = NNTT_NONE;

    // Extract the test type.
    size_t pos = url.find("t=");
    int intval = 0;
    if (pos > 0 && base::StringToInt(url.substr(pos + 2, 1), &intval))
      test_type = static_cast<NetNotifyTestType>(intval);
    EXPECT_GT(test_type, NNTT_NONE);

    // Check if the load has already been delayed.
    bool delay_loaded = (url.find("delayed=true") != std::string::npos);

    if (!delay_loaded && (test_type == NNTT_DELAYED_RENDERER ||
                          test_type == NNTT_DELAYED_BROWSER)) {
      // Delay load the URL.
      CefRefPtr<CefProcessMessage> message =
          CefProcessMessage::Create(kNetNotifyMsg);
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      args->SetInt(0, test_type);
      args->SetString(1, url);
      EXPECT_TRUE(browser->SendProcessMessage(PID_BROWSER, message));

      return true;
    }

    return false;
  }

  virtual bool OnProcessMessageReceived(
        CefRefPtr<ClientApp> app,
        CefRefPtr<CefBrowser> browser,
        CefProcessId source_process,
        CefRefPtr<CefProcessMessage> message) OVERRIDE {
    if (message->GetName().ToString() == kNetNotifyMsg) {
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      EXPECT_TRUE(args.get());

      NetNotifyTestType test_type =
          static_cast<NetNotifyTestType>(args->GetInt(0));
      EXPECT_EQ(test_type, NNTT_DELAYED_RENDERER);

      const std::string& url = args->GetString(1);

      // Load the URL from the render process.
      browser->GetMainFrame()->LoadURL(url);
      return true;
    }

    // Message not handled.
    return false;
  }

  IMPLEMENT_REFCOUNTING(NetNotifyRendererTest);
};

void RunNetNotifyTest(NetNotifyTestType test_type, bool same_origin) {
  TestHandler::CompletionState completion_state(3);

  CefRefPtr<NetNotifyTestHandler> handler1 =
      new NetNotifyTestHandler(&completion_state, test_type, same_origin);
  CefRefPtr<NetNotifyTestHandler> handler2 =
      new NetNotifyTestHandler(&completion_state, test_type, same_origin);
  CefRefPtr<NetNotifyTestHandler> handler3 =
      new NetNotifyTestHandler(&completion_state, test_type, same_origin);

  TestHandler::Collection collection(&completion_state);
  collection.AddTestHandler(handler1.get());
  collection.AddTestHandler(handler2.get());
  collection.AddTestHandler(handler3.get());

  collection.ExecuteTests();
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
    ClientApp::RenderDelegateSet& delegates) {
  delegates.insert(new NetNotifyRendererTest);
}
