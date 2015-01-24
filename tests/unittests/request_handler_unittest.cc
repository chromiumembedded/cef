// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

// Include this first to avoid type conflicts with CEF headers.
#include "tests/unittests/chromium_includes.h"

#include "base/strings/stringprintf.h"

#include "include/base/cef_bind.h"
#include "include/cef_cookie.h"
#include "include/wrapper/cef_closure_task.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tests/cefclient/client_app.h"
#include "tests/unittests/test_handler.h"

using client::ClientApp;

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

bool g_net_notify_test = false;

// Browser side.
class NetNotifyBrowserTest : public ClientApp::BrowserDelegate {
 public:
  NetNotifyBrowserTest() {}

  void OnBeforeChildProcessLaunch(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefCommandLine> command_line) override {
    if (!g_net_notify_test)
      return;

    // Indicate to the render process that the test should be run.
    command_line->AppendSwitchWithValue("test", kNetNotifyMsg);
  }

 protected:
  IMPLEMENT_REFCOUNTING(NetNotifyBrowserTest);
};

// Browser side.
class NetNotifyTestHandler : public TestHandler {
 public:
  class RequestContextHandler : public CefRequestContextHandler {
   public:
    explicit RequestContextHandler(NetNotifyTestHandler* handler)
        : handler_(handler) {}

    CefRefPtr<CefCookieManager> GetCookieManager() override {
      EXPECT_TRUE(handler_);
      EXPECT_TRUE(CefCurrentlyOn(TID_IO));

      if (url_.find(handler_->url1_) == 0)
        handler_->got_get_cookie_manager1_.yes();
      else if (url_.find(handler_->url2_) == 0)
        handler_->got_get_cookie_manager2_.yes();
      else
        EXPECT_TRUE(false);  // Not reached

      return handler_->cookie_manager_;
    }

    void SetURL(const std::string& url) {
      url_ = url;
    }

    void Detach() {
      handler_ = NULL;
    }

   private:
    std::string url_;
    NetNotifyTestHandler* handler_;

    IMPLEMENT_REFCOUNTING(RequestContextHandler);
  };

  NetNotifyTestHandler(CompletionState* completion_state,
                       NetNotifyTestType test_type,
                       bool same_origin)
      : TestHandler(completion_state),
        test_type_(test_type),
        same_origin_(same_origin) {}

  void SetupTest() override {
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

    context_handler_ = new RequestContextHandler(this);
    context_handler_->SetURL(url1_);

    // Create browser that loads the 1st URL.
    CreateBrowser(url1_,
        CefRequestContext::CreateContext(context_handler_.get()));
  }

  void RunTest() override {
    // Navigate to the 2nd URL.
    context_handler_->SetURL(url2_);
    GetBrowser()->GetMainFrame()->LoadURL(url2_);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request) override {
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

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
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

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
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
        url += "&delayed=true";

        if (test_type_ == NNTT_DELAYED_RENDERER) {
          // Load the URL from the render process.
          CefRefPtr<CefProcessMessage> message =
              CefProcessMessage::Create(kNetNotifyMsg);
          CefRefPtr<CefListValue> args = message->GetArgumentList();
          args->SetInt(0, test_type_);
          args->SetString(1, url);
          EXPECT_TRUE(browser->SendProcessMessage(PID_RENDERER, message));
        } else {
          // Load the URL from the browser process.
          browser->GetMainFrame()->LoadURL(url);
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

  bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
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
      explicit TestVisitor(NetNotifyTestHandler* handler)
          : handler_(handler) {
      }
      ~TestVisitor() override {
        // Destroy the test.
        CefPostTask(TID_UI,
            base::Bind(&NetNotifyTestHandler::DestroyTest, handler_));
      }

      bool Visit(const CefCookie& cookie, int count, int total,
                 bool& deleteCookie) override {
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

  void DestroyTest() override {
    int browser_id = GetBrowser()->GetIdentifier();

    // Verify test expectations.
    EXPECT_TRUE(got_before_browse1_) << " browser " << browser_id;
    EXPECT_TRUE(got_load_end1_) << " browser " << browser_id;
    EXPECT_TRUE(got_before_resource_load1_) << " browser " << browser_id;
    EXPECT_TRUE(got_get_resource_handler1_) << " browser " << browser_id;
    EXPECT_TRUE(got_get_cookie_manager1_) << " browser " << browser_id;
    EXPECT_TRUE(got_cookie1_) << " browser " << browser_id;
    EXPECT_TRUE(got_process_message1_) << " browser " << browser_id;
    EXPECT_TRUE(got_before_browse2_) << " browser " << browser_id;
    EXPECT_TRUE(got_load_end2_) << " browser " << browser_id;
    EXPECT_TRUE(got_before_resource_load2_) << " browser " << browser_id;
    EXPECT_TRUE(got_get_resource_handler2_) << " browser " << browser_id;
    EXPECT_TRUE(got_get_cookie_manager2_) << " browser " << browser_id;
    EXPECT_TRUE(got_cookie2_) << " browser " << browser_id;
    EXPECT_TRUE(got_process_message2_) << " browser " << browser_id;

    if (test_type_ == NNTT_DELAYED_RENDERER ||
        test_type_ == NNTT_DELAYED_BROWSER) {
      EXPECT_TRUE(got_before_browse2_will_delay_) << " browser " << browser_id;
      EXPECT_TRUE(got_before_browse2_delayed_) << " browser " << browser_id;
    } else {
      EXPECT_FALSE(got_before_browse2_will_delay_) << " browser " << browser_id;
      EXPECT_FALSE(got_before_browse2_delayed_) << " browser " << browser_id;
    }

    context_handler_->Detach();
    context_handler_ = NULL;
    cookie_manager_ = NULL;
    
    TestHandler::DestroyTest();
  }

  NetNotifyTestType test_type_;
  bool same_origin_;
  std::string url1_;
  std::string url2_;

  CefRefPtr<RequestContextHandler> context_handler_;

  CefRefPtr<CefCookieManager> cookie_manager_;

  TrackCallback got_before_browse1_;
  TrackCallback got_load_end1_;
  TrackCallback got_before_resource_load1_;
  TrackCallback got_get_resource_handler1_;
  TrackCallback got_get_cookie_manager1_;
  TrackCallback got_cookie1_;
  TrackCallback got_process_message1_;
  TrackCallback got_before_browse2_;
  TrackCallback got_load_end2_;
  TrackCallback got_before_resource_load2_;
  TrackCallback got_get_resource_handler2_;
  TrackCallback got_get_cookie_manager2_;
  TrackCallback got_cookie2_;
  TrackCallback got_process_message2_;
  TrackCallback got_before_browse2_will_delay_;
  TrackCallback got_before_browse2_delayed_;
};

// Renderer side.
class NetNotifyRendererTest : public ClientApp::RenderDelegate,
                              public CefLoadHandler {
 public:
  NetNotifyRendererTest()
      : run_test_(false) {}

  void OnRenderThreadCreated(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefListValue> extra_info) override {
    if (!g_net_notify_test) {
      // Check that the test should be run.
      CefRefPtr<CefCommandLine> command_line =
          CefCommandLine::GetGlobalCommandLine();
      const std::string& test = command_line->GetSwitchValue("test");
      if (test != kNetNotifyMsg)
        return;
    }

    run_test_ = true;
  }

  CefRefPtr<CefLoadHandler> GetLoadHandler(
      CefRefPtr<ClientApp> app) override {
    if (run_test_)
      return this;
    return NULL;
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
    EXPECT_TRUE(browser->SendProcessMessage(PID_BROWSER, message));
  }

  bool OnProcessMessageReceived(
        CefRefPtr<ClientApp> app,
        CefRefPtr<CefBrowser> browser,
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
      browser->GetMainFrame()->LoadURL(url);
      return true;
    }

    // Message not handled.
    return false;
  }

 private:
  bool run_test_;

  IMPLEMENT_REFCOUNTING(NetNotifyRendererTest);
};

void RunNetNotifyTest(NetNotifyTestType test_type, bool same_origin) {
  g_net_notify_test = true;

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

  ReleaseAndWaitForDestructor(handler1);
  ReleaseAndWaitForDestructor(handler2);
  ReleaseAndWaitForDestructor(handler3);

  g_net_notify_test = false;
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


// Entry point for creating request handler browser test objects.
// Called from client_app_delegates.cc.
void CreateRequestHandlerBrowserTests(
    ClientApp::BrowserDelegateSet& delegates) {
  delegates.insert(new NetNotifyBrowserTest);
}

// Entry point for creating request handler renderer test objects.
// Called from client_app_delegates.cc.
void CreateRequestHandlerRendererTests(
    ClientApp::RenderDelegateSet& delegates) {
  delegates.insert(new NetNotifyRendererTest);
}
