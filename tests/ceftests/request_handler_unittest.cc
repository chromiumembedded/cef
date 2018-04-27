// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

#include "include/base/cef_bind.h"
#include "include/base/cef_scoped_ptr.h"
#include "include/cef_cookie.h"
#include "include/cef_server.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/routing_test_handler.h"
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

bool g_net_notify_test = false;

// Browser side.
class NetNotifyBrowserTest : public ClientAppBrowser::Delegate {
 public:
  NetNotifyBrowserTest() {}

  void OnBeforeChildProcessLaunch(
      CefRefPtr<ClientAppBrowser> app,
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

    void SetURL(const std::string& url) { url_ = url; }

    void Detach() { handler_ = NULL; }

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
    std::stringstream ss;
    ss << kNetNotifyOrigin1 << "nav1.html?t=" << test_type_;
    url1_ = ss.str();
    ss.str("");
    ss << (same_origin_ ? kNetNotifyOrigin1 : kNetNotifyOrigin2)
       << "nav2.html?t=" << test_type_;
    url2_ = ss.str();

    cookie_manager_ = CefCookieManager::CreateManager(CefString(), true, NULL);

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

    context_handler_ = new RequestContextHandler(this);
    context_handler_->SetURL(url1_);

    // Create the request context that will use an in-memory cache.
    CefRequestContextSettings settings;
    CefRefPtr<CefRequestContext> request_context =
        CefRequestContext::CreateContext(settings, context_handler_.get());

    // Create browser that loads the 1st URL.
    CreateBrowser(url1_, request_context);
  }

  void RunTest() override {
    // Navigate to the 2nd URL.
    context_handler_->SetURL(url2_);
    GetBrowser()->GetMainFrame()->LoadURL(url2_);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefRequestCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    const std::string& url = request->GetURL();
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
    EXPECT_EQ(UR_SUCCESS, status);

    const std::string& url = request->GetURL();
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

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
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
      explicit TestVisitor(NetNotifyTestHandler* handler) : handler_(handler) {}
      ~TestVisitor() override {
        // Destroy the test.
        CefPostTask(TID_UI,
                    base::Bind(&NetNotifyTestHandler::DestroyTest, handler_));
      }

      bool Visit(const CefCookie& cookie,
                 int count,
                 int total,
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
    EXPECT_TRUE(got_resource_load_complete1_) << " browser " << browser_id;
    EXPECT_TRUE(got_get_cookie_manager1_) << " browser " << browser_id;
    EXPECT_TRUE(got_cookie1_) << " browser " << browser_id;
    EXPECT_TRUE(got_process_message1_) << " browser " << browser_id;
    EXPECT_TRUE(got_before_browse2_) << " browser " << browser_id;
    EXPECT_TRUE(got_load_end2_) << " browser " << browser_id;
    EXPECT_TRUE(got_before_resource_load2_) << " browser " << browser_id;
    EXPECT_TRUE(got_get_resource_handler2_) << " browser " << browser_id;
    EXPECT_TRUE(got_resource_load_complete2_) << " browser " << browser_id;
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
  TrackCallback got_resource_load_complete1_;
  TrackCallback got_get_cookie_manager1_;
  TrackCallback got_cookie1_;
  TrackCallback got_process_message1_;
  TrackCallback got_before_browse2_;
  TrackCallback got_load_end2_;
  TrackCallback got_before_resource_load2_;
  TrackCallback got_get_resource_handler2_;
  TrackCallback got_resource_load_complete2_;
  TrackCallback got_get_cookie_manager2_;
  TrackCallback got_cookie2_;
  TrackCallback got_process_message2_;
  TrackCallback got_before_browse2_will_delay_;
  TrackCallback got_before_browse2_delayed_;

  int64 response_length1_;
  int64 response_length2_;

  IMPLEMENT_REFCOUNTING(NetNotifyTestHandler);
};

// Renderer side.
class NetNotifyRendererTest : public ClientAppRenderer::Delegate,
                              public CefLoadHandler {
 public:
  NetNotifyRendererTest() : run_test_(false) {}

  void OnRenderThreadCreated(CefRefPtr<ClientAppRenderer> app,
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
      CefRefPtr<ClientAppRenderer> app) override {
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

  bool OnProcessMessageReceived(CefRefPtr<ClientAppRenderer> app,
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

namespace {

const char kResourceTestHtml[] = "http://test.com/resource.html";

class ResourceResponseTest : public TestHandler {
 public:
  enum TestMode {
    URL,
    HEADER,
    POST,
  };

  explicit ResourceResponseTest(TestMode mode)
      : browser_id_(0), main_request_id_(0U), sub_request_id_(0U) {
    if (mode == URL)
      resource_test_.reset(new UrlResourceTest);
    else if (mode == HEADER)
      resource_test_.reset(new HeaderResourceTest);
    else
      resource_test_.reset(new PostResourceTest);
  }

  void RunTest() override {
    AddResource(kResourceTestHtml, GetHtml(), "text/html");
    CreateBrowser(kResourceTestHtml);
    SetTestTimeout();
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override {
    EXPECT_UI_THREAD();
    EXPECT_EQ(0, browser_id_);
    browser_id_ = browser->GetIdentifier();
    EXPECT_GT(browser_id_, 0);

    // This method is only called for the main resource.
    EXPECT_STREQ(kResourceTestHtml, request->GetURL().ToString().c_str());

    // Browser-side navigation no longer exposes the actual request information.
    EXPECT_EQ(0U, request->GetIdentifier());

    return false;
  }

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefRequestCallback> callback) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    if (request->GetURL() == kResourceTestHtml) {
      // All loads of the main resource should keep the same request id.
      EXPECT_EQ(0U, main_request_id_);
      main_request_id_ = request->GetIdentifier();
      EXPECT_GT(main_request_id_, 0U);
      return RV_CONTINUE;
    }

    // All redirects of the sub-resource should keep the same request id.
    if (sub_request_id_ == 0U) {
      sub_request_id_ = request->GetIdentifier();
      EXPECT_GT(sub_request_id_, 0U);
    } else {
      EXPECT_EQ(sub_request_id_, request->GetIdentifier());
    }

    return resource_test_->OnBeforeResourceLoad(browser, frame, request)
               ? RV_CANCEL
               : RV_CONTINUE;
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    if (request->GetURL() == kResourceTestHtml) {
      EXPECT_EQ(main_request_id_, request->GetIdentifier());
      return TestHandler::GetResourceHandler(browser, frame, request);
    }

    EXPECT_EQ(sub_request_id_, request->GetIdentifier());
    return resource_test_->GetResourceHandler(browser, frame, request);
  }

  void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefResponse> response,
                          CefString& new_url) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());
    EXPECT_EQ(sub_request_id_, request->GetIdentifier());

    resource_test_->OnResourceRedirect(browser, frame, request, new_url);
  }

  bool OnResourceResponse(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefResponse> response) override {
    EXPECT_IO_THREAD();
    EXPECT_TRUE(browser.get());
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    EXPECT_TRUE(frame.get());
    EXPECT_TRUE(frame->IsMain());

    if (request->GetURL() == kResourceTestHtml) {
      EXPECT_EQ(main_request_id_, request->GetIdentifier());
      return false;
    }

    EXPECT_EQ(sub_request_id_, request->GetIdentifier());
    return resource_test_->OnResourceResponse(browser, frame, request,
                                              response);
  }

  void OnResourceLoadComplete(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              CefRefPtr<CefResponse> response,
                              URLRequestStatus status,
                              int64 received_content_length) override {
    EXPECT_IO_THREAD();
    EXPECT_TRUE(browser.get());
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    EXPECT_TRUE(frame.get());
    EXPECT_TRUE(frame->IsMain());

    if (request->GetURL() == kResourceTestHtml) {
      EXPECT_EQ(main_request_id_, request->GetIdentifier());
      return;
    }

    EXPECT_EQ(sub_request_id_, request->GetIdentifier());
    resource_test_->OnResourceLoadComplete(browser, frame, request, response,
                                           status, received_content_length);
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_UI_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    TestHandler::OnLoadEnd(browser, frame, httpStatusCode);
    DestroyTest();
  }

  void DestroyTest() override {
    resource_test_->CheckExpected();
    resource_test_.reset(NULL);

    TestHandler::DestroyTest();
  }

 private:
  std::string GetHtml() const {
    std::stringstream html;
    html << "<html><head>";

    const std::string& url = resource_test_->start_url();
    html << "<script type=\"text/javascript\" src=\"" << url << "\"></script>";

    html << "</head><body><p>Main</p></body></html>";
    return html.str();
  }

  class ResourceTest {
   public:
    ResourceTest(const std::string& start_url,
                 size_t expected_resource_response_ct = 2U,
                 size_t expected_before_resource_load_ct = 1U,
                 size_t expected_resource_redirect_ct = 0U,
                 size_t expected_resource_load_complete_ct = 1U)
        : start_url_(start_url),
          resource_response_ct_(0U),
          expected_resource_response_ct_(expected_resource_response_ct),
          before_resource_load_ct_(0),
          expected_before_resource_load_ct_(expected_before_resource_load_ct),
          get_resource_handler_ct_(0U),
          resource_redirect_ct_(0U),
          expected_resource_redirect_ct_(expected_resource_redirect_ct),
          resource_load_complete_ct_(0U),
          expected_resource_load_complete_ct_(
              expected_resource_load_complete_ct) {}
    virtual ~ResourceTest() {}

    const std::string& start_url() const { return start_url_; }

    virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefRefPtr<CefRequest> request) {
      before_resource_load_ct_++;
      return false;
    }

    virtual CefRefPtr<CefResourceHandler> GetResourceHandler(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request) {
      get_resource_handler_ct_++;

      const std::string& js_content = "<!-- -->";

      CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
          const_cast<char*>(js_content.c_str()), js_content.size());

      return new CefStreamResourceHandler(200, "OK", "text/javascript",
                                          CefResponse::HeaderMap(), stream);
    }

    virtual void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request,
                                    CefString& new_url) {
      resource_redirect_ct_++;
    }

    bool OnResourceResponse(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request,
                            CefRefPtr<CefResponse> response) {
      EXPECT_TRUE(CheckUrl(request->GetURL()));

      // Verify the response returned by GetResourceHandler.
      EXPECT_EQ(200, response->GetStatus());
      EXPECT_STREQ("OK", response->GetStatusText().ToString().c_str());
      EXPECT_STREQ("text/javascript",
                   response->GetMimeType().ToString().c_str());

      if (resource_response_ct_++ == 0U) {
        // Always redirect at least one time.
        OnResourceReceived(browser, frame, request, response);
        return true;
      }

      OnRetryReceived(browser, frame, request, response);
      return (resource_response_ct_ < expected_resource_response_ct_);
    }

    void OnResourceLoadComplete(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefRequest> request,
                                CefRefPtr<CefResponse> response,
                                URLRequestStatus status,
                                int64 received_content_length) {
      EXPECT_TRUE(CheckUrl(request->GetURL()));

      // Verify the response returned by GetResourceHandler.
      EXPECT_EQ(200, response->GetStatus());
      EXPECT_STREQ("OK", response->GetStatusText().ToString().c_str());
      EXPECT_STREQ("text/javascript",
                   response->GetMimeType().ToString().c_str());

      resource_load_complete_ct_++;
    }

    virtual bool CheckUrl(const std::string& url) const {
      return (url == start_url_);
    }

    virtual void CheckExpected() {
      EXPECT_TRUE(got_resource_);
      EXPECT_TRUE(got_resource_retry_);

      EXPECT_EQ(expected_resource_response_ct_, resource_response_ct_);
      EXPECT_EQ(expected_resource_response_ct_, get_resource_handler_ct_);
      EXPECT_EQ(expected_before_resource_load_ct_, before_resource_load_ct_);
      EXPECT_EQ(expected_resource_redirect_ct_, resource_redirect_ct_);
      EXPECT_EQ(expected_resource_load_complete_ct_,
                resource_load_complete_ct_);
    }

   protected:
    virtual void OnResourceReceived(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request,
                                    CefRefPtr<CefResponse> response) {
      got_resource_.yes();
    }

    virtual void OnRetryReceived(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefRequest> request,
                                 CefRefPtr<CefResponse> response) {
      got_resource_retry_.yes();
    }

   private:
    std::string start_url_;

    size_t resource_response_ct_;
    size_t expected_resource_response_ct_;
    size_t before_resource_load_ct_;
    size_t expected_before_resource_load_ct_;
    size_t get_resource_handler_ct_;
    size_t resource_redirect_ct_;
    size_t expected_resource_redirect_ct_;
    size_t resource_load_complete_ct_;
    size_t expected_resource_load_complete_ct_;

    TrackCallback got_resource_;
    TrackCallback got_resource_retry_;
  };

  class UrlResourceTest : public ResourceTest {
   public:
    UrlResourceTest()
        : ResourceTest("http://test.com/start_url.js", 3U, 2U, 1U) {
      redirect_url_ = "http://test.com/redirect_url.js";
    }

    bool CheckUrl(const std::string& url) const override {
      if (url == redirect_url_)
        return true;

      return ResourceTest::CheckUrl(url);
    }

    void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request,
                            CefString& new_url) override {
      ResourceTest::OnResourceRedirect(browser, frame, request, new_url);
      const std::string& old_url = request->GetURL();
      EXPECT_STREQ(start_url().c_str(), old_url.c_str());
      EXPECT_STREQ(redirect_url_.c_str(), new_url.ToString().c_str());
    }

   private:
    void OnResourceReceived(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request,
                            CefRefPtr<CefResponse> response) override {
      ResourceTest::OnResourceReceived(browser, frame, request, response);
      request->SetURL(redirect_url_);
    }

    void OnRetryReceived(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefRequest> request,
                         CefRefPtr<CefResponse> response) override {
      ResourceTest::OnRetryReceived(browser, frame, request, response);
      const std::string& new_url = request->GetURL();
      EXPECT_STREQ(redirect_url_.c_str(), new_url.c_str());
    }

    std::string redirect_url_;
  };

  class HeaderResourceTest : public ResourceTest {
   public:
    HeaderResourceTest() : ResourceTest("http://test.com/start_header.js") {
      expected_headers_.insert(std::make_pair("Test-Key1", "Value1"));
      expected_headers_.insert(std::make_pair("Test-Key2", "Value2"));
    }

   private:
    void OnResourceReceived(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request,
                            CefRefPtr<CefResponse> response) override {
      ResourceTest::OnResourceReceived(browser, frame, request, response);
      request->SetHeaderMap(expected_headers_);
    }

    void OnRetryReceived(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefRequest> request,
                         CefRefPtr<CefResponse> response) override {
      ResourceTest::OnRetryReceived(browser, frame, request, response);
      CefRequest::HeaderMap actual_headers;
      request->GetHeaderMap(actual_headers);
      TestMapEqual(expected_headers_, actual_headers, true);
    }

    CefRequest::HeaderMap expected_headers_;
  };

  class PostResourceTest : public ResourceTest {
   public:
    PostResourceTest() : ResourceTest("http://test.com/start_post.js") {
      CefRefPtr<CefPostDataElement> elem = CefPostDataElement::Create();
      const std::string data("Test Post Data");
      elem->SetToBytes(data.size(), data.c_str());

      expected_post_ = CefPostData::Create();
      expected_post_->AddElement(elem);
    }

   private:
    void OnResourceReceived(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request,
                            CefRefPtr<CefResponse> response) override {
      ResourceTest::OnResourceReceived(browser, frame, request, response);
      request->SetPostData(expected_post_);
    }

    void OnRetryReceived(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefRequest> request,
                         CefRefPtr<CefResponse> response) override {
      ResourceTest::OnRetryReceived(browser, frame, request, response);
      CefRefPtr<CefPostData> actual_post = request->GetPostData();
      TestPostDataEqual(expected_post_, actual_post);
    }

    CefRefPtr<CefPostData> expected_post_;
  };

  int browser_id_;
  uint64 main_request_id_;
  uint64 sub_request_id_;
  scoped_ptr<ResourceTest> resource_test_;

  IMPLEMENT_REFCOUNTING(ResourceResponseTest);
};

}  // namespace

TEST(RequestHandlerTest, ResourceResponseURL) {
  CefRefPtr<ResourceResponseTest> handler =
      new ResourceResponseTest(ResourceResponseTest::URL);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(RequestHandlerTest, ResourceResponseHeader) {
  CefRefPtr<ResourceResponseTest> handler =
      new ResourceResponseTest(ResourceResponseTest::HEADER);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(RequestHandlerTest, ResourceResponsePost) {
  CefRefPtr<ResourceResponseTest> handler =
      new ResourceResponseTest(ResourceResponseTest::POST);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kResourceTestHtml2[] = "http://test.com/resource2.html";

class BeforeResourceLoadTest : public TestHandler {
 public:
  enum TestMode {
    CANCEL,
    CANCEL_ASYNC,
    CANCEL_NAV,
    CONTINUE,
    CONTINUE_ASYNC,
  };

  explicit BeforeResourceLoadTest(TestMode mode) : test_mode_(mode) {}

  void RunTest() override {
    AddResource(kResourceTestHtml, "<html><body>Test</body></html>",
                "text/html");
    AddResource(kResourceTestHtml2, "<html><body>Test2</body></html>",
                "text/html");
    CreateBrowser(kResourceTestHtml);
    SetTestTimeout();
  }

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefRequestCallback> callback) override {
    EXPECT_IO_THREAD();

    // Allow the 2nd navigation to continue.
    const std::string& url = request->GetURL();
    if (url == kResourceTestHtml2) {
      got_before_resource_load2_.yes();
      EXPECT_EQ(CANCEL_NAV, test_mode_);
      return RV_CONTINUE;
    }

    EXPECT_FALSE(got_before_resource_load_);
    got_before_resource_load_.yes();

    if (test_mode_ == CANCEL) {
      // Cancel immediately.
      return RV_CANCEL;
    } else if (test_mode_ == CONTINUE) {
      // Continue immediately.
      return RV_CONTINUE;
    } else {
      if (test_mode_ == CANCEL_NAV) {
        // Cancel the request by navigating to a new URL.
        browser->GetMainFrame()->LoadURL(kResourceTestHtml2);
      } else {
        // Continue or cancel asynchronously.
        CefPostTask(TID_UI,
                    base::Bind(&CefRequestCallback::Continue, callback.get(),
                               test_mode_ == CONTINUE_ASYNC));
      }
      return RV_CONTINUE_ASYNC;
    }
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_UI_THREAD();

    EXPECT_FALSE(got_load_end_);
    got_load_end_.yes();

    const std::string& url = frame->GetURL();
    if (test_mode_ == CANCEL_NAV)
      EXPECT_STREQ(kResourceTestHtml2, url.data());
    else
      EXPECT_STREQ(kResourceTestHtml, url.data());

    TestHandler::OnLoadEnd(browser, frame, httpStatusCode);
    DestroyTest();
  }

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override {
    EXPECT_UI_THREAD();

    EXPECT_FALSE(got_load_error_);
    got_load_error_.yes();

    const std::string& url = failedUrl;
    EXPECT_STREQ(kResourceTestHtml, url.data());

    TestHandler::OnLoadError(browser, frame, errorCode, errorText, failedUrl);
    if (test_mode_ != CANCEL_NAV)
      DestroyTest();
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_before_resource_load_);

    if (test_mode_ == CANCEL_NAV)
      EXPECT_TRUE(got_before_resource_load2_);
    else
      EXPECT_FALSE(got_before_resource_load2_);

    if (test_mode_ == CONTINUE || test_mode_ == CONTINUE_ASYNC) {
      EXPECT_TRUE(got_load_end_);
      EXPECT_FALSE(got_load_error_);
    } else if (test_mode_ == CANCEL || test_mode_ == CANCEL_ASYNC) {
      EXPECT_FALSE(got_load_end_);
      EXPECT_TRUE(got_load_error_);
    }

    TestHandler::DestroyTest();
  }

 private:
  const TestMode test_mode_;

  TrackCallback got_before_resource_load_;
  TrackCallback got_before_resource_load2_;
  TrackCallback got_load_end_;
  TrackCallback got_load_error_;

  IMPLEMENT_REFCOUNTING(BeforeResourceLoadTest);
};

}  // namespace

TEST(RequestHandlerTest, BeforeResourceLoadCancel) {
  CefRefPtr<BeforeResourceLoadTest> handler =
      new BeforeResourceLoadTest(BeforeResourceLoadTest::CANCEL);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(RequestHandlerTest, BeforeResourceLoadCancelAsync) {
  CefRefPtr<BeforeResourceLoadTest> handler =
      new BeforeResourceLoadTest(BeforeResourceLoadTest::CANCEL_ASYNC);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(RequestHandlerTest, BeforeResourceLoadCancelNav) {
  CefRefPtr<BeforeResourceLoadTest> handler =
      new BeforeResourceLoadTest(BeforeResourceLoadTest::CANCEL_NAV);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(RequestHandlerTest, BeforeResourceLoadContinue) {
  CefRefPtr<BeforeResourceLoadTest> handler =
      new BeforeResourceLoadTest(BeforeResourceLoadTest::CONTINUE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(RequestHandlerTest, BeforeResourceLoadContinueAsync) {
  CefRefPtr<BeforeResourceLoadTest> handler =
      new BeforeResourceLoadTest(BeforeResourceLoadTest::CONTINUE_ASYNC);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

// For response filtering we need to test:
// - Passing through content unchanged.
// - Not reading all of the input buffer.
// - Needing more input and getting it.
// - Needing more input and not getting it.
// - Filter error.

const char kResponseFilterTestUrl[] = "http://tests.com/response_filter.html";
const size_t kResponseBufferSize = 1024 * 32;  // 32kb

const char kInputHeader[] = "<html><head></head><body>";
const char kInputFooter[] = "</body></html>";

// Repeat |content| the minimum number of times necessary to satisfy
// |desired_min_size|. If |calculated_repeat_ct| is non-NULL it will be set to
// the number of times that |content| was repeated.
std::string CreateInput(const std::string& content,
                        size_t desired_min_size,
                        size_t* calculated_repeat_ct = nullptr) {
  const size_t header_footer_size =
      sizeof(kInputHeader) + sizeof(kInputFooter) - 2;
  EXPECT_GE(desired_min_size, header_footer_size + content.size());
  desired_min_size -= header_footer_size;

  size_t repeat_ct =
      static_cast<size_t>(std::ceil(static_cast<double>(desired_min_size) /
                                    static_cast<double>(content.size())));
  if (calculated_repeat_ct)
    *calculated_repeat_ct = repeat_ct;

  std::string result;
  result.reserve(header_footer_size + (content.size() * repeat_ct));

  result = kInputHeader;
  while (repeat_ct--)
    result += content;
  result += kInputFooter;

  return result;
}

std::string CreateOutput(const std::string& content, size_t repeat_ct) {
  const size_t header_footer_size =
      sizeof(kInputHeader) + sizeof(kInputFooter) - 2;

  std::string result;
  result.reserve(header_footer_size + (content.size() * repeat_ct));

  result = kInputHeader;
  while (repeat_ct--)
    result += content;
  result += kInputFooter;

  return result;
}

// Base class for test filters.
class ResponseFilterTestBase : public CefResponseFilter {
 public:
  ResponseFilterTestBase() : filter_count_(0U) {}

  bool InitFilter() override {
    EXPECT_FALSE(got_init_filter_);
    got_init_filter_.yes();
    return true;
  }

  FilterStatus Filter(void* data_in,
                      size_t data_in_size,
                      size_t& data_in_read,
                      void* data_out,
                      size_t data_out_size,
                      size_t& data_out_written) override {
    if (data_in_size == 0U)
      EXPECT_FALSE(data_in);
    else
      EXPECT_TRUE(data_in);
    EXPECT_EQ(data_in_read, 0U);
    EXPECT_TRUE(data_out);
    EXPECT_GT(data_out_size, 0U);
    EXPECT_EQ(data_out_written, 0U);
    filter_count_++;
    return RESPONSE_FILTER_ERROR;
  }

  // Returns the input that will be fed into the filter.
  virtual std::string GetInput() = 0;

  // Verify the output from the filter.
  virtual void VerifyOutput(cef_urlrequest_status_t status,
                            int64 received_content_length,
                            const std::string& received_content) {
    EXPECT_TRUE(got_init_filter_);
    EXPECT_GT(filter_count_, 0U);
  }

 protected:
  TrackCallback got_init_filter_;
  size_t filter_count_;

  IMPLEMENT_REFCOUNTING(ResponseFilterTestBase);
};

// Pass through the contents unchanged.
class ResponseFilterPassThru : public ResponseFilterTestBase {
 public:
  explicit ResponseFilterPassThru(bool limit_read) : limit_read_(limit_read) {}

  FilterStatus Filter(void* data_in,
                      size_t data_in_size,
                      size_t& data_in_read,
                      void* data_out,
                      size_t data_out_size,
                      size_t& data_out_written) override {
    ResponseFilterTestBase::Filter(data_in, data_in_size, data_in_read,
                                   data_out, data_out_size, data_out_written);

    if (limit_read_) {
      // Read at most 1k bytes.
      data_in_read = std::min(data_in_size, static_cast<size_t>(1024U));
    } else {
      // Read all available bytes.
      data_in_read = data_in_size;
    }

    data_out_written = std::min(data_in_read, data_out_size);
    memcpy(data_out, data_in, data_out_written);

    return RESPONSE_FILTER_DONE;
  }

  std::string GetInput() override {
    input_ = CreateInput("FOOBAR ", kResponseBufferSize * 2U);
    return input_;
  }

  void VerifyOutput(cef_urlrequest_status_t status,
                    int64 received_content_length,
                    const std::string& received_content) override {
    ResponseFilterTestBase::VerifyOutput(status, received_content_length,
                                         received_content);

    if (limit_read_)
      // Expected to read 2 full buffers of kResponseBufferSize at 1kb
      // increments (2 * 32) and one partial buffer.
      EXPECT_EQ(2U * 32U + 1U, filter_count_);
    else {
      // Expected to read 2 full buffers of kResponseBufferSize and one partial
      // buffer.
      EXPECT_EQ(3U, filter_count_);
    }
    EXPECT_STREQ(input_.c_str(), received_content.c_str());

    // Input size and content size should match.
    EXPECT_EQ(input_.size(), static_cast<size_t>(received_content_length));
    EXPECT_EQ(input_.size(), received_content.size());
  }

 private:
  std::string input_;
  bool limit_read_;
};

const char kFindString[] = "REPLACE_THIS_STRING";
const char kReplaceString[] = "This is the replaced string!";

// Helper for passing params to Write().
#define WRITE_PARAMS data_out_ptr, data_out_size, data_out_written

// Replace all instances of |kFindString| with |kReplaceString|.
// This implementation is similar to the example in
// tests/shared/response_filter_test.cc.
class ResponseFilterNeedMore : public ResponseFilterTestBase {
 public:
  ResponseFilterNeedMore()
      : find_match_offset_(0U),
        replace_overflow_size_(0U),
        input_size_(0U),
        repeat_ct_(0U) {}

  FilterStatus Filter(void* data_in,
                      size_t data_in_size,
                      size_t& data_in_read,
                      void* data_out,
                      size_t data_out_size,
                      size_t& data_out_written) override {
    ResponseFilterTestBase::Filter(data_in, data_in_size, data_in_read,
                                   data_out, data_out_size, data_out_written);

    // All data will be read.
    data_in_read = data_in_size;

    const size_t find_size = sizeof(kFindString) - 1;

    const char* data_in_ptr = static_cast<char*>(data_in);
    char* data_out_ptr = static_cast<char*>(data_out);

    // Reset the overflow.
    std::string old_overflow;
    if (!overflow_.empty()) {
      old_overflow = overflow_;
      overflow_.clear();
    }

    const size_t likely_out_size =
        data_in_size + replace_overflow_size_ + old_overflow.size();
    if (data_out_size < likely_out_size) {
      // We'll likely need to use the overflow buffer. Size it appropriately.
      overflow_.reserve(likely_out_size - data_out_size);
    }

    if (!old_overflow.empty()) {
      // Write the overflow from last time.
      Write(old_overflow.c_str(), old_overflow.size(), WRITE_PARAMS);
    }

    // Evaluate each character in the input buffer. Track how many characters in
    // a row match kFindString. If kFindString is completely matched then write
    // kReplaceString. Otherwise, write the input characters as-is.
    for (size_t i = 0U; i < data_in_size; ++i) {
      if (data_in_ptr[i] == kFindString[find_match_offset_]) {
        // Matched the next character in the find string.
        if (++find_match_offset_ == find_size) {
          // Complete match of the find string. Write the replace string.
          Write(kReplaceString, sizeof(kReplaceString) - 1, WRITE_PARAMS);

          // Start over looking for a match.
          find_match_offset_ = 0;
        }
        continue;
      }

      // Character did not match the find string.
      if (find_match_offset_ > 0) {
        // Write the portion of the find string that has matched so far.
        Write(kFindString, find_match_offset_, WRITE_PARAMS);

        // Start over looking for a match.
        find_match_offset_ = 0;
      }

      // Write the current character.
      Write(&data_in_ptr[i], 1, WRITE_PARAMS);
    }

    // If a match is currently in-progress and input was provided then we need
    // more data. Otherwise, we're done.
    return find_match_offset_ > 0 && data_in_size > 0
               ? RESPONSE_FILTER_NEED_MORE_DATA
               : RESPONSE_FILTER_DONE;
  }

  std::string GetInput() override {
    const std::string& input = CreateInput(
        std::string(kFindString) + " ", kResponseBufferSize * 2U, &repeat_ct_);
    input_size_ = input.size();

    const size_t find_size = sizeof(kFindString) - 1;
    const size_t replace_size = sizeof(kReplaceString) - 1;

    // Determine a reasonable amount of space for find/replace overflow.
    if (replace_size > find_size)
      replace_overflow_size_ = (replace_size - find_size) * repeat_ct_;

    return input;
  }

  void VerifyOutput(cef_urlrequest_status_t status,
                    int64 received_content_length,
                    const std::string& received_content) override {
    ResponseFilterTestBase::VerifyOutput(status, received_content_length,
                                         received_content);

    const std::string& output =
        CreateOutput(std::string(kReplaceString) + " ", repeat_ct_);
    EXPECT_STREQ(output.c_str(), received_content.c_str());

    // Pre-filter content length should be the original input size.
    EXPECT_EQ(input_size_, static_cast<size_t>(received_content_length));

    // Filtered content length should be the output size.
    EXPECT_EQ(output.size(), received_content.size());

    // Expected to read 2 full buffers of kResponseBufferSize and one partial
    // buffer, and then one additional call to drain the overflow.
    EXPECT_EQ(4U, filter_count_);
  }

 private:
  inline void Write(const char* str,
                    size_t str_size,
                    char*& data_out_ptr,
                    size_t data_out_size,
                    size_t& data_out_written) {
    // Number of bytes remaining in the output buffer.
    const size_t remaining_space = data_out_size - data_out_written;
    // Maximum number of bytes we can write into the output buffer.
    const size_t max_write = std::min(str_size, remaining_space);

    // Write the maximum portion that fits in the output buffer.
    if (max_write == 1) {
      // Small optimization for single character writes.
      *data_out_ptr = str[0];
      data_out_ptr += 1;
      data_out_written += 1;
    } else if (max_write > 1) {
      memcpy(data_out_ptr, str, max_write);
      data_out_ptr += max_write;
      data_out_written += max_write;
    }

    if (max_write < str_size) {
      // Need to write more bytes than will fit in the output buffer. Store the
      // remainder in the overflow buffer.
      overflow_ += std::string(str + max_write, str_size - max_write);
    }
  }

  // The portion of the find string that is currently matching.
  size_t find_match_offset_;

  // The likely amount of overflow.
  size_t replace_overflow_size_;

  // Overflow from the output buffer.
  std::string overflow_;

  // The original input size.
  size_t input_size_;

  // The number of times the find string was repeated.
  size_t repeat_ct_;
};

// Return a filter error.
class ResponseFilterError : public ResponseFilterTestBase {
 public:
  ResponseFilterError() {}

  FilterStatus Filter(void* data_in,
                      size_t data_in_size,
                      size_t& data_in_read,
                      void* data_out,
                      size_t data_out_size,
                      size_t& data_out_written) override {
    ResponseFilterTestBase::Filter(data_in, data_in_size, data_in_read,
                                   data_out, data_out_size, data_out_written);

    return RESPONSE_FILTER_ERROR;
  }

  std::string GetInput() override {
    return kInputHeader + std::string("ERROR") + kInputFooter;
  }

  void VerifyOutput(cef_urlrequest_status_t status,
                    int64 received_content_length,
                    const std::string& received_content) override {
    ResponseFilterTestBase::VerifyOutput(status, received_content_length,
                                         received_content);

    EXPECT_EQ(UR_FAILED, status);

    // Expect empty content.
    const std::string& output = std::string(kInputHeader) + kInputFooter;
    EXPECT_STREQ(output.c_str(), received_content.c_str());
    EXPECT_EQ(0U, received_content_length);

    // Expect to only be called one time.
    EXPECT_EQ(filter_count_, 1U);
  }
};

// Browser side.
class ResponseFilterTestHandler : public TestHandler {
 public:
  explicit ResponseFilterTestHandler(
      CefRefPtr<ResponseFilterTestBase> response_filter)
      : response_filter_(response_filter) {}

  void RunTest() override {
    const std::string& resource = response_filter_->GetInput();
    AddResource(kResponseFilterTestUrl, resource, "text/html");

    // Create the browser.
    CreateBrowser(kResponseFilterTestUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  CefRefPtr<CefResponseFilter> GetResourceResponseFilter(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefResponse> response) override {
    EXPECT_IO_THREAD();

    DCHECK(!got_resource_response_filter_);
    got_resource_response_filter_.yes();
    return response_filter_;
  }

  void OnResourceLoadComplete(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              CefRefPtr<CefResponse> response,
                              URLRequestStatus status,
                              int64 received_content_length) override {
    EXPECT_IO_THREAD();

    DCHECK(!got_resource_load_complete_);
    got_resource_load_complete_.yes();

    status_ = status;
    received_content_length_ = received_content_length;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    DCHECK(!got_load_end_);
    got_load_end_.yes();

    EXPECT_TRUE(httpStatusCode == 0 || httpStatusCode == 200);

    GetOutputContent(frame);
  }

 private:
  // Retrieve the output content using a StringVisitor. This effectively
  // serializes the DOM from the renderer process so any comparison to the
  // filter output is somewhat error-prone.
  void GetOutputContent(CefRefPtr<CefFrame> frame) {
    class StringVisitor : public CefStringVisitor {
     public:
      typedef base::Callback<void(const std::string& /*received_content*/)>
          VisitorCallback;

      explicit StringVisitor(const VisitorCallback& callback)
          : callback_(callback) {}

      void Visit(const CefString& string) override {
        callback_.Run(string);
        callback_.Reset();
      }

     private:
      VisitorCallback callback_;

      IMPLEMENT_REFCOUNTING(StringVisitor);
    };

    frame->GetSource(new StringVisitor(
        base::Bind(&ResponseFilterTestHandler::VerifyOutput, this)));
  }

  void VerifyOutput(const std::string& received_content) {
    response_filter_->VerifyOutput(status_, received_content_length_,
                                   received_content);
    response_filter_ = nullptr;

    DestroyTest();
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_resource_response_filter_);
    EXPECT_TRUE(got_resource_load_complete_);
    EXPECT_TRUE(got_load_end_);

    TestHandler::DestroyTest();
  }

  CefRefPtr<ResponseFilterTestBase> response_filter_;

  TrackCallback got_resource_response_filter_;
  TrackCallback got_resource_load_complete_;
  TrackCallback got_load_end_;

  URLRequestStatus status_;
  int64 received_content_length_;

  IMPLEMENT_REFCOUNTING(ResponseFilterTestHandler);
};

}  // namespace

// Pass through contents unchanged. Read all available input.
TEST(RequestHandlerTest, ResponseFilterPassThruReadAll) {
  CefRefPtr<ResponseFilterTestHandler> handler =
      new ResponseFilterTestHandler(new ResponseFilterPassThru(false));
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Pass through contents unchanged. Read limited input.
TEST(RequestHandlerTest, ResponseFilterPassThruReadLimited) {
  CefRefPtr<ResponseFilterTestHandler> handler =
      new ResponseFilterTestHandler(new ResponseFilterPassThru(true));
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Find/replace contents such that we occasionally need more data.
TEST(RequestHandlerTest, ResponseFilterNeedMore) {
  CefRefPtr<ResponseFilterTestHandler> handler =
      new ResponseFilterTestHandler(new ResponseFilterNeedMore());
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Error during filtering.
TEST(RequestHandlerTest, ResponseFilterError) {
  CefRefPtr<ResponseFilterTestHandler> handler =
      new ResponseFilterTestHandler(new ResponseFilterError());
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kCookieAccessScheme[] = "http";
const char kCookieAccessDomain[] = "test-cookies.com";
const char kCookieAccessServerIP[] = "127.0.0.1";
const uint16 kCookieAccessServerPort = 8099;

std::string GetCookieAccessOrigin(bool server_backend) {
  std::stringstream ss;
  if (server_backend) {
    ss << kCookieAccessScheme << "://" << kCookieAccessServerIP << ":"
       << kCookieAccessServerPort;
  } else {
    ss << kCookieAccessScheme << "://" << kCookieAccessDomain;
  }
  ss << "/";
  return ss.str();
}

std::string GetCookieAccessUrl1(bool server_backend) {
  return GetCookieAccessOrigin(server_backend) + "cookie1.html";
}

std::string GetCookieAccessUrl2(bool server_backend) {
  return GetCookieAccessOrigin(server_backend) + "cookie2.html";
}

void TestCookieString(const std::string& cookie_str,
                      TrackCallback& got_cookie_js,
                      TrackCallback& got_cookie_net) {
  if (cookie_str.find("name_js=value_js") != std::string::npos) {
    got_cookie_js.yes();
  }
  if (cookie_str.find("name_net=value_net") != std::string::npos) {
    got_cookie_net.yes();
  }
}

struct CookieAccessData {
  CefRefPtr<CefResponse> response;
  std::string response_data;

  TrackCallback got_request_;
  TrackCallback got_cookie_js_;
  TrackCallback got_cookie_net_;

  // Only used with scheme handler backend.
  TrackCallback got_can_set_cookie_js_;
  TrackCallback got_can_set_cookie_net_;
  TrackCallback got_can_get_cookie_js_;
  TrackCallback got_can_get_cookie_net_;
};

class CookieAccessResponseHandler {
 public:
  CookieAccessResponseHandler() {}
  virtual void AddResponse(const std::string& url, CookieAccessData* data) = 0;

 protected:
  virtual ~CookieAccessResponseHandler() {}
};

std::string GetHeaderValue(const CefServer::HeaderMap& header_map,
                           const std::string& header_name) {
  CefServer::HeaderMap::const_iterator it = header_map.find(header_name);
  if (it != header_map.end())
    return it->second;
  return std::string();
}

// Serves request responses.
class CookieAccessSchemeHandler : public CefResourceHandler {
 public:
  explicit CookieAccessSchemeHandler(CookieAccessData* data)
      : data_(data), offset_(0) {}

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    CefRequest::HeaderMap headerMap;
    request->GetHeaderMap(headerMap);
    const std::string& cookie_str = GetHeaderValue(headerMap, "Cookie");
    TestCookieString(cookie_str, data_->got_cookie_js_, data_->got_cookie_net_);

    // Continue immediately.
    callback->Continue();
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    response->SetStatus(data_->response->GetStatus());
    response->SetStatusText(data_->response->GetStatusText());
    response->SetMimeType(data_->response->GetMimeType());

    CefResponse::HeaderMap headerMap;
    data_->response->GetHeaderMap(headerMap);
    response->SetHeaderMap(headerMap);

    response_length = data_->response_data.length();
  }

  bool ReadResponse(void* response_data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    bool has_data = false;
    bytes_read = 0;

    size_t size = data_->response_data.length();
    if (offset_ < size) {
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(size - offset_));
      memcpy(response_data_out, data_->response_data.c_str() + offset_,
             transfer_size);
      offset_ += transfer_size;

      bytes_read = transfer_size;
      has_data = true;
    }

    return has_data;
  }

  bool CanGetCookie(const CefCookie& cookie) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    TestCookie(cookie, data_->got_can_get_cookie_js_,
               data_->got_can_get_cookie_net_);
    return true;
  }

  bool CanSetCookie(const CefCookie& cookie) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    TestCookie(cookie, data_->got_can_set_cookie_js_,
               data_->got_can_set_cookie_net_);
    return true;
  }

  void Cancel() override { EXPECT_TRUE(CefCurrentlyOn(TID_IO)); }

 private:
  static void TestCookie(const CefCookie& cookie,
                         TrackCallback& got_cookie_js,
                         TrackCallback& got_cookie_net) {
    const std::string& cookie_name = CefString(&cookie.name);
    const std::string& cookie_val = CefString(&cookie.value);
    if (cookie_name == "name_js") {
      EXPECT_STREQ("value_js", cookie_val.c_str());
      got_cookie_js.yes();
    } else if (cookie_name == "name_net") {
      EXPECT_STREQ("value_net", cookie_val.c_str());
      got_cookie_net.yes();
    } else {
      ADD_FAILURE() << "Unexpected cookie: " << cookie_name;
    }
  }

  // |data_| is not owned by this object.
  CookieAccessData* data_;

  size_t offset_;

  IMPLEMENT_REFCOUNTING(CookieAccessSchemeHandler);
};

class CookieAccessSchemeHandlerFactory : public CefSchemeHandlerFactory,
                                         public CookieAccessResponseHandler {
 public:
  CookieAccessSchemeHandlerFactory() {}

  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    const std::string& url = request->GetURL();
    ResponseDataMap::const_iterator it = data_map_.find(url);
    if (it != data_map_.end()) {
      it->second->got_request_.yes();

      // There should be no cookie data in this callback.
      CefRequest::HeaderMap headerMap;
      request->GetHeaderMap(headerMap);
      EXPECT_TRUE(headerMap.find("Cookie") == headerMap.end());

      return new CookieAccessSchemeHandler(it->second);
    }

    // Unknown test.
    ADD_FAILURE() << "Unexpected url: " << url;
    return nullptr;
  }

  void AddResponse(const std::string& url, CookieAccessData* data) override {
    data_map_.insert(std::make_pair(url, data));
  }

  void Shutdown(const base::Closure& complete_callback) {
    if (!CefCurrentlyOn(TID_IO)) {
      CefPostTask(TID_IO,
                  base::Bind(&CookieAccessSchemeHandlerFactory::Shutdown, this,
                             complete_callback));
      return;
    }

    complete_callback.Run();
  }

 private:
  // Map of URL to Data.
  typedef std::map<std::string, CookieAccessData*> ResponseDataMap;
  ResponseDataMap data_map_;

  IMPLEMENT_REFCOUNTING(CookieAccessSchemeHandlerFactory);
};

// HTTP server handler.
class CookieAccessServerHandler : public CefServerHandler,
                                  public CookieAccessResponseHandler {
 public:
  CookieAccessServerHandler()
      : initialized_(false),
        expected_connection_ct_(-1),
        actual_connection_ct_(0),
        expected_http_request_ct_(-1),
        actual_http_request_ct_(0) {}

  virtual ~CookieAccessServerHandler() { RunCompleteCallback(); }

  // Must be called before CreateServer().
  void AddResponse(const std::string& url, CookieAccessData* data) override {
    EXPECT_FALSE(initialized_);
    data_map_.insert(std::make_pair(url, data));
  }

  // Must be called before CreateServer().
  void SetExpectedRequestCount(int count) {
    EXPECT_FALSE(initialized_);
    expected_connection_ct_ = expected_http_request_ct_ = count;
  }

  // |complete_callback| will be executed on the UI thread after the server is
  // started.
  void CreateServer(const base::Closure& complete_callback) {
    EXPECT_UI_THREAD();

    if (expected_connection_ct_ < 0) {
      // Default to the assumption of one request per registered URL.
      SetExpectedRequestCount(static_cast<int>(data_map_.size()));
    }

    EXPECT_FALSE(initialized_);
    initialized_ = true;

    EXPECT_TRUE(complete_callback_.is_null());
    complete_callback_ = complete_callback;

    CefServer::CreateServer(kCookieAccessServerIP, kCookieAccessServerPort, 10,
                            this);
  }

  // Results in a call to VerifyResults() and eventual execution of the
  // |complete_callback| on the UI thread via CookieAccessServerHandler
  // destruction.
  void ShutdownServer(const base::Closure& complete_callback) {
    EXPECT_UI_THREAD();

    EXPECT_TRUE(complete_callback_.is_null());
    complete_callback_ = complete_callback;

    EXPECT_TRUE(server_);
    if (server_)
      server_->Shutdown();
  }

  void OnServerCreated(CefRefPtr<CefServer> server) override {
    EXPECT_TRUE(server);
    EXPECT_TRUE(server->IsRunning());
    EXPECT_FALSE(server->HasConnection());

    EXPECT_FALSE(got_server_created_);
    got_server_created_.yes();

    EXPECT_FALSE(server_);
    server_ = server;

    EXPECT_FALSE(server_runner_);
    server_runner_ = server_->GetTaskRunner();
    EXPECT_TRUE(server_runner_);
    EXPECT_TRUE(server_runner_->BelongsToCurrentThread());

    CefPostTask(
        TID_UI,
        base::Bind(&CookieAccessServerHandler::RunCompleteCallback, this));
  }

  void OnServerDestroyed(CefRefPtr<CefServer> server) override {
    EXPECT_TRUE(VerifyServer(server));
    EXPECT_FALSE(server->IsRunning());
    EXPECT_FALSE(server->HasConnection());

    EXPECT_FALSE(got_server_destroyed_);
    got_server_destroyed_.yes();

    server_ = nullptr;

    VerifyResults();
  }

  void OnClientConnected(CefRefPtr<CefServer> server,
                         int connection_id) override {
    EXPECT_TRUE(VerifyServer(server));
    EXPECT_TRUE(server->HasConnection());
    EXPECT_TRUE(server->IsValidConnection(connection_id));

    EXPECT_TRUE(connection_id_set_.find(connection_id) ==
                connection_id_set_.end());
    connection_id_set_.insert(connection_id);

    actual_connection_ct_++;
  }

  void OnClientDisconnected(CefRefPtr<CefServer> server,
                            int connection_id) override {
    EXPECT_TRUE(VerifyServer(server));
    EXPECT_FALSE(server->IsValidConnection(connection_id));

    ConnectionIdSet::iterator it = connection_id_set_.find(connection_id);
    EXPECT_TRUE(it != connection_id_set_.end());
    connection_id_set_.erase(it);
  }

  void OnHttpRequest(CefRefPtr<CefServer> server,
                     int connection_id,
                     const CefString& client_address,
                     CefRefPtr<CefRequest> request) override {
    EXPECT_TRUE(VerifyServer(server));
    EXPECT_TRUE(VerifyConnection(connection_id));
    EXPECT_FALSE(client_address.empty());

    // Log the requests for better error reporting.
    request_log_ += request->GetMethod().ToString() + " " +
                    request->GetURL().ToString() + "\n";

    HandleRequest(server, connection_id, request);

    actual_http_request_ct_++;
  }

  void OnWebSocketRequest(CefRefPtr<CefServer> server,
                          int connection_id,
                          const CefString& client_address,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefCallback> callback) override {
    NOTREACHED();
  }

  void OnWebSocketConnected(CefRefPtr<CefServer> server,
                            int connection_id) override {
    NOTREACHED();
  }

  void OnWebSocketMessage(CefRefPtr<CefServer> server,
                          int connection_id,
                          const void* data,
                          size_t data_size) override {
    NOTREACHED();
  }

 private:
  bool RunningOnServerThread() {
    return server_runner_ && server_runner_->BelongsToCurrentThread();
  }

  bool VerifyServer(CefRefPtr<CefServer> server) {
    V_DECLARE();
    V_EXPECT_TRUE(RunningOnServerThread());
    V_EXPECT_TRUE(server);
    V_EXPECT_TRUE(server_);
    V_EXPECT_TRUE(server->GetAddress().ToString() ==
                  server_->GetAddress().ToString());
    V_RETURN();
  }

  bool VerifyConnection(int connection_id) {
    return connection_id_set_.find(connection_id) != connection_id_set_.end();
  }

  void VerifyResults() {
    EXPECT_TRUE(RunningOnServerThread());

    EXPECT_TRUE(got_server_created_);
    EXPECT_TRUE(got_server_destroyed_);
    EXPECT_TRUE(connection_id_set_.empty());
    EXPECT_EQ(expected_connection_ct_, actual_connection_ct_) << request_log_;
    EXPECT_EQ(expected_http_request_ct_, actual_http_request_ct_)
        << request_log_;
  }

  void HandleRequest(CefRefPtr<CefServer> server,
                     int connection_id,
                     CefRefPtr<CefRequest> request) {
    const std::string& url = request->GetURL();
    ResponseDataMap::const_iterator it = data_map_.find(url);
    if (it != data_map_.end()) {
      it->second->got_request_.yes();

      CefRequest::HeaderMap headerMap;
      request->GetHeaderMap(headerMap);
      const std::string& cookie_str = GetHeaderValue(headerMap, "cookie");
      TestCookieString(cookie_str, it->second->got_cookie_js_,
                       it->second->got_cookie_net_);

      SendResponse(server, connection_id, it->second->response,
                   it->second->response_data);
    } else {
      // Unknown test.
      ADD_FAILURE() << "Unexpected url: " << url;
      server->SendHttp500Response(connection_id, "Unknown test");
    }
  }

  void SendResponse(CefRefPtr<CefServer> server,
                    int connection_id,
                    CefRefPtr<CefResponse> response,
                    const std::string& response_data) {
    int response_code = response->GetStatus();
    const CefString& content_type = response->GetMimeType();
    int64 content_length = static_cast<int64>(response_data.size());

    CefResponse::HeaderMap extra_headers;
    response->GetHeaderMap(extra_headers);

    server->SendHttpResponse(connection_id, response_code, content_type,
                             content_length, extra_headers);

    if (content_length != 0) {
      server->SendRawData(connection_id, response_data.data(),
                          response_data.size());
      server->CloseConnection(connection_id);
    }

    // The connection should be closed.
    EXPECT_FALSE(server->IsValidConnection(connection_id));
  }

  void RunCompleteCallback() {
    EXPECT_UI_THREAD();

    EXPECT_FALSE(complete_callback_.is_null());
    complete_callback_.Run();
    complete_callback_.Reset();
  }

  // Map of URL to Data.
  typedef std::map<std::string, CookieAccessData*> ResponseDataMap;
  ResponseDataMap data_map_;

  CefRefPtr<CefServer> server_;
  CefRefPtr<CefTaskRunner> server_runner_;
  bool initialized_;

  // Only accessed on the UI thread.
  base::Closure complete_callback_;

  // After initialization the below members are only accessed on the server
  // thread.

  TrackCallback got_server_created_;
  TrackCallback got_server_destroyed_;

  typedef std::set<int> ConnectionIdSet;
  ConnectionIdSet connection_id_set_;

  int expected_connection_ct_;
  int actual_connection_ct_;
  int expected_http_request_ct_;
  int actual_http_request_ct_;

  std::string request_log_;

  IMPLEMENT_REFCOUNTING(CookieAccessServerHandler);
  DISALLOW_COPY_AND_ASSIGN(CookieAccessServerHandler);
};

class CookieAccessTestHandler : public RoutingTestHandler {
 public:
  enum TestMode {
    ALLOW = 0,
    BLOCK_READ = 1 << 0,
    BLOCK_WRITE = 1 << 1,
    BLOCK_READ_WRITE = BLOCK_READ | BLOCK_WRITE,
    BLOCK_ALL = 1 << 2,
  };

  class RequestContextHandler : public CefRequestContextHandler {
   public:
    explicit RequestContextHandler(CookieAccessTestHandler* handler)
        : handler_(handler) {}

    CefRefPtr<CefCookieManager> GetCookieManager() override {
      EXPECT_TRUE(handler_);
      EXPECT_TRUE(CefCurrentlyOn(TID_IO));

      handler_->got_cookie_manager_.yes();
      return handler_->cookie_manager_;
    }

    void Detach() { handler_ = NULL; }

   private:
    CookieAccessTestHandler* handler_;

    IMPLEMENT_REFCOUNTING(RequestContextHandler);
  };

  CookieAccessTestHandler(TestMode test_mode, bool server_backend)
      : test_mode_(test_mode), server_backend_(server_backend) {}

  void RunTest() override {
    if (test_mode_ == BLOCK_ALL) {
      cookie_manager_ = CefCookieManager::GetBlockingManager();
      context_handler_ = new RequestContextHandler(this);

      // Create a request context that uses |context_handler_|.
      CefRequestContextSettings settings;
      context_ =
          CefRequestContext::CreateContext(settings, context_handler_.get());
    } else {
      cookie_manager_ = CefCookieManager::GetGlobalManager(nullptr);
    }
    SetTestTimeout();

    CefPostTask(TID_UI,
                base::Bind(&CookieAccessTestHandler::StartBackend, this,
                           base::Bind(&CookieAccessTestHandler::RunTestContinue,
                                      this)));
  }

  void DestroyTest() override {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::Bind(&CookieAccessTestHandler::DestroyTest, this));
      return;
    }

    cookie_manager_ = NULL;
    if (context_)
      context_ = NULL;
    if (context_handler_) {
      context_handler_->Detach();
      context_handler_ = NULL;
    }

    // Got both network requests.
    EXPECT_TRUE(data1_.got_request_);
    EXPECT_TRUE(data2_.got_request_);

    if (test_mode_ == BLOCK_ALL) {
      EXPECT_TRUE(got_cookie_manager_);

      // The callback to set the cookie comes before the actual storage fails.
      EXPECT_TRUE(got_can_set_cookie1_);

      if (!server_backend_) {
        // The callback to set the cookie comes before the actual storage fails.
        EXPECT_TRUE(data1_.got_can_set_cookie_net_);
      } else {
        EXPECT_FALSE(data1_.got_can_set_cookie_net_);
      }

      // No cookies stored anywhere.
      EXPECT_FALSE(got_can_get_cookies2_);
      EXPECT_FALSE(got_cookie_js1_);
      EXPECT_FALSE(got_cookie_js2_);
      EXPECT_FALSE(got_cookie_js3_);
      EXPECT_FALSE(got_cookie_net1_);
      EXPECT_FALSE(got_cookie_net2_);
      EXPECT_FALSE(got_cookie_net3_);
      EXPECT_FALSE(data1_.got_cookie_js_);
      EXPECT_FALSE(data1_.got_cookie_net_);
      EXPECT_FALSE(data1_.got_can_get_cookie_js_);
      EXPECT_FALSE(data1_.got_can_get_cookie_net_);
      EXPECT_FALSE(data1_.got_can_set_cookie_js_);
      EXPECT_FALSE(data2_.got_cookie_js_);
      EXPECT_FALSE(data2_.got_cookie_net_);
      EXPECT_FALSE(data2_.got_can_get_cookie_js_);
      EXPECT_FALSE(data2_.got_can_get_cookie_net_);
      EXPECT_FALSE(data2_.got_can_set_cookie_js_);
      EXPECT_FALSE(data2_.got_can_set_cookie_net_);
    } else {
      EXPECT_FALSE(got_cookie_manager_);

      // Always get a call to CanSetCookie for the 1st network request due to
      // the network cookie.
      EXPECT_TRUE(got_can_set_cookie1_);
      // Always get a call to CanGetCookies for the 2nd network request due to
      // the JS cookie.
      EXPECT_TRUE(got_can_get_cookies2_);

      // Always get the JS cookie via JS.
      EXPECT_TRUE(got_cookie_js1_);
      EXPECT_TRUE(got_cookie_js2_);
      EXPECT_TRUE(got_cookie_js3_);

      // Only get the net cookie via JS if cookie write was allowed.
      if (test_mode_ & BLOCK_WRITE) {
        EXPECT_FALSE(got_cookie_net1_);
        EXPECT_FALSE(got_cookie_net2_);
        EXPECT_FALSE(got_cookie_net3_);
      } else {
        EXPECT_TRUE(got_cookie_net1_);
        EXPECT_TRUE(got_cookie_net2_);
        EXPECT_TRUE(got_cookie_net3_);
      }

      // No cookies sent for the 1st network request.
      EXPECT_FALSE(data1_.got_cookie_js_);
      EXPECT_FALSE(data1_.got_cookie_net_);

      // 2nd network request...
      if (test_mode_ & BLOCK_READ) {
        // No cookies sent if reading was blocked.
        EXPECT_FALSE(data2_.got_cookie_js_);
        EXPECT_FALSE(data2_.got_cookie_net_);
      } else if (test_mode_ & BLOCK_WRITE) {
        // Only JS cookie sent if writing was blocked.
        EXPECT_TRUE(data2_.got_cookie_js_);
        EXPECT_FALSE(data2_.got_cookie_net_);
      } else {
        // All cookies sent.
        EXPECT_TRUE(data2_.got_cookie_js_);
        EXPECT_TRUE(data2_.got_cookie_net_);
      }

      if (!server_backend_) {
        // No query to get cookies with the 1st network request because none
        // have been set yet.
        EXPECT_FALSE(data1_.got_can_get_cookie_js_);
        EXPECT_FALSE(data1_.got_can_get_cookie_net_);

        // JS cookie is not set via a network request.
        EXPECT_FALSE(data1_.got_can_set_cookie_js_);
        EXPECT_FALSE(data2_.got_can_set_cookie_js_);

        // No query to set the net cookie for the 1st network request if write
        // was blocked.
        if (test_mode_ & BLOCK_WRITE) {
          EXPECT_FALSE(data1_.got_can_set_cookie_net_);
        } else {
          EXPECT_TRUE(data1_.got_can_set_cookie_net_);
        }

        // Net cookie is not set via the 2nd network request.
        EXPECT_FALSE(data2_.got_can_set_cookie_net_);

        // No query to get the JS cookie for the 2nd network request if read was
        // blocked.
        if (test_mode_ & BLOCK_READ) {
          EXPECT_FALSE(data2_.got_can_get_cookie_js_);
        } else {
          EXPECT_TRUE(data2_.got_can_get_cookie_js_);
        }

        // No query to get the net cookie for the 2nd network request if read or
        // write (of the net cookie) was blocked.
        if (test_mode_ & (BLOCK_READ | BLOCK_WRITE)) {
          EXPECT_FALSE(data2_.got_can_get_cookie_net_);
        } else {
          EXPECT_TRUE(data2_.got_can_get_cookie_net_);
        }
      }
    }

    TestHandler::DestroyTest();
  }

  bool CanGetCookies(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     CefRefPtr<CefRequest> request) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    const std::string& url = request->GetURL();
    if (url == GetCookieAccessUrl2(server_backend_)) {
      EXPECT_FALSE(got_can_get_cookies2_);
      got_can_get_cookies2_.yes();
    } else {
      ADD_FAILURE() << "Unexpected url: " << url;
    }

    return !(test_mode_ & BLOCK_READ);
  }

  bool CanSetCookie(CefRefPtr<CefBrowser> browser,
                    CefRefPtr<CefFrame> frame,
                    CefRefPtr<CefRequest> request,
                    const CefCookie& cookie) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    // Expecting the network cookie only.
    EXPECT_STREQ("name_net", CefString(&cookie.name).ToString().c_str());
    EXPECT_STREQ("value_net", CefString(&cookie.value).ToString().c_str());

    const std::string& url = request->GetURL();
    if (url == GetCookieAccessUrl1(server_backend_)) {
      EXPECT_FALSE(got_can_set_cookie1_);
      got_can_set_cookie1_.yes();
    } else {
      ADD_FAILURE() << "Unexpected url: " << url;
    }

    return !(test_mode_ & BLOCK_WRITE);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    const std::string& url = frame->GetURL();
    const std::string& cookie_str = request.ToString();
    if (url == GetCookieAccessUrl1(server_backend_)) {
      TestCookieString(cookie_str, got_cookie_js1_, got_cookie_net1_);
      browser->GetMainFrame()->LoadURL(GetCookieAccessUrl2(server_backend_));
    } else if (url == GetCookieAccessUrl2(server_backend_)) {
      TestCookieString(cookie_str, got_cookie_js2_, got_cookie_net2_);
      FinishTest();
    } else {
      ADD_FAILURE() << "Unexpected url: " << url;
    }
    return true;
  }

 private:
  void AddResponses(CookieAccessResponseHandler* handler) {
    // 1st request sets a cookie via net response headers and JS, then retrieves
    // the cookies via JS.
    {
      data1_.response = CefResponse::Create();
      data1_.response->SetMimeType("text/html");
      data1_.response->SetStatus(200);
      data1_.response->SetStatusText("OK");

      CefResponse::HeaderMap headerMap;
      data1_.response->GetHeaderMap(headerMap);
      headerMap.insert(std::make_pair("Set-Cookie", "name_net=value_net"));
      data1_.response->SetHeaderMap(headerMap);

      data1_.response_data =
          "<html><head>"
          "<script>"
          "document.cookie='name_js=value_js';"
          "window.testQuery({request:document.cookie});"
          "</script>"
          "</head><body>COOKIE ACCESS TEST 1</body></html>";

      handler->AddResponse(GetCookieAccessUrl1(server_backend_), &data1_);
    }

    // 2nd request retrieves the cookies via JS.
    {
      data2_.response = CefResponse::Create();
      data2_.response->SetMimeType("text/html");
      data2_.response->SetStatus(200);
      data2_.response->SetStatusText("OK");

      data2_.response_data =
          "<html><head>"
          "<script>"
          "window.testQuery({request:document.cookie});"
          "</script>"
          "</head><body>COOKIE ACCESS TEST 2</body></html>";

      handler->AddResponse(GetCookieAccessUrl2(server_backend_), &data2_);
    }
  }

  void StartBackend(const base::Closure& complete_callback) {
    if (server_backend_) {
      StartServer(complete_callback);
    } else {
      StartSchemeHandler(complete_callback);
    }
  }

  void StartServer(const base::Closure& complete_callback) {
    EXPECT_FALSE(server_handler_);

    server_handler_ = new CookieAccessServerHandler();
    AddResponses(server_handler_.get());
    server_handler_->CreateServer(complete_callback);
  }

  void StartSchemeHandler(const base::Closure& complete_callback) {
    // Add the factory registration.
    scheme_factory_ = new CookieAccessSchemeHandlerFactory();
    AddResponses(scheme_factory_.get());
    if (context_) {
      context_->RegisterSchemeHandlerFactory(
          kCookieAccessScheme, kCookieAccessDomain, scheme_factory_.get());
    } else {
      CefRegisterSchemeHandlerFactory(kCookieAccessScheme, kCookieAccessDomain,
                                      scheme_factory_.get());
    }

    complete_callback.Run();
  }

  void RunTestContinue() {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::Bind(&CookieAccessTestHandler::RunTestContinue, this));
      return;
    }

    CreateBrowser(GetCookieAccessUrl1(server_backend_), context_);
  }

  void FinishTest() {
    // Verify that cookies were set correctly.
    class TestVisitor : public CefCookieVisitor {
     public:
      explicit TestVisitor(CookieAccessTestHandler* handler)
          : handler_(handler) {}
      ~TestVisitor() override {
        // Destroy the test.
        CefPostTask(
            TID_UI,
            base::Bind(
                &CookieAccessTestHandler::ShutdownBackend, handler_,
                base::Bind(&CookieAccessTestHandler::DestroyTest, handler_)));
      }

      bool Visit(const CefCookie& cookie,
                 int count,
                 int total,
                 bool& deleteCookie) override {
        const std::string& name = CefString(&cookie.name);
        const std::string& value = CefString(&cookie.value);
        if (name == "name_js" && value == "value_js")
          handler_->got_cookie_js3_.yes();
        else if (name == "name_net" && value == "value_net")
          handler_->got_cookie_net3_.yes();

        // Clean up the cookies.
        deleteCookie = true;

        return true;
      }

     private:
      CookieAccessTestHandler* handler_;
      IMPLEMENT_REFCOUNTING(TestVisitor);
    };

    cookie_manager_->VisitAllCookies(new TestVisitor(this));
  }

  void ShutdownBackend(const base::Closure& complete_callback) {
    if (server_backend_) {
      ShutdownServer(complete_callback);
    } else {
      ShutdownSchemeHandler(complete_callback);
    }
  }

  void ShutdownServer(const base::Closure& complete_callback) {
    EXPECT_TRUE(server_handler_);

    server_handler_->ShutdownServer(complete_callback);
    server_handler_ = nullptr;
  }

  void ShutdownSchemeHandler(const base::Closure& complete_callback) {
    EXPECT_TRUE(scheme_factory_);

    if (context_) {
      context_->RegisterSchemeHandlerFactory(kCookieAccessScheme,
                                             kCookieAccessDomain, nullptr);
    } else {
      CefRegisterSchemeHandlerFactory(kCookieAccessScheme, kCookieAccessDomain,
                                      nullptr);
    }
    scheme_factory_->Shutdown(complete_callback);
    scheme_factory_ = nullptr;
  }

  TestMode test_mode_;
  bool server_backend_;
  CefRefPtr<RequestContextHandler> context_handler_;
  CefRefPtr<CefRequestContext> context_;
  CefRefPtr<CefCookieManager> cookie_manager_;

  CefRefPtr<CookieAccessServerHandler> server_handler_;
  CefRefPtr<CookieAccessSchemeHandlerFactory> scheme_factory_;

  CookieAccessData data1_;
  CookieAccessData data2_;

  TrackCallback got_cookie_manager_;

  // 1st request.
  TrackCallback got_can_set_cookie1_;
  TrackCallback got_cookie_js1_;
  TrackCallback got_cookie_net1_;

  // 2nd request.
  TrackCallback got_can_get_cookies2_;
  TrackCallback got_cookie_js2_;
  TrackCallback got_cookie_net2_;

  // From cookie manager.
  TrackCallback got_cookie_js3_;
  TrackCallback got_cookie_net3_;

  DISALLOW_COPY_AND_ASSIGN(CookieAccessTestHandler);
  IMPLEMENT_REFCOUNTING(CookieAccessTestHandler);
};

}  // namespace

// Allow reading and writing of cookies with server backend.
TEST(RequestHandlerTest, CookieAccessServerAllow) {
  CefRefPtr<CookieAccessTestHandler> handler =
      new CookieAccessTestHandler(CookieAccessTestHandler::ALLOW, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Block reading of cookies with server backend.
TEST(RequestHandlerTest, CookieAccessServerBlockRead) {
  CefRefPtr<CookieAccessTestHandler> handler =
      new CookieAccessTestHandler(CookieAccessTestHandler::BLOCK_READ, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Block writing of cookies with server backend.
TEST(RequestHandlerTest, CookieAccessServerBlockWrite) {
  CefRefPtr<CookieAccessTestHandler> handler =
      new CookieAccessTestHandler(CookieAccessTestHandler::BLOCK_WRITE, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Block reading and writing of cookies with server backend.
TEST(RequestHandlerTest, CookieAccessServerBlockReadWrite) {
  CefRefPtr<CookieAccessTestHandler> handler = new CookieAccessTestHandler(
      CookieAccessTestHandler::BLOCK_READ_WRITE, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Block all cookies with server backend.
TEST(RequestHandlerTest, CookieAccessServerBlockAll) {
  CefRefPtr<CookieAccessTestHandler> handler =
      new CookieAccessTestHandler(CookieAccessTestHandler::BLOCK_ALL, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Allow reading and writing of cookies with scheme handler backend.
TEST(RequestHandlerTest, CookieAccessSchemeAllow) {
  CefRefPtr<CookieAccessTestHandler> handler =
      new CookieAccessTestHandler(CookieAccessTestHandler::ALLOW, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Block reading of cookies with scheme handler backend.
TEST(RequestHandlerTest, CookieAccessSchemeBlockRead) {
  CefRefPtr<CookieAccessTestHandler> handler =
      new CookieAccessTestHandler(CookieAccessTestHandler::BLOCK_READ, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Block writing of cookies with scheme handler backend.
TEST(RequestHandlerTest, CookieAccessSchemeBlockWrite) {
  CefRefPtr<CookieAccessTestHandler> handler =
      new CookieAccessTestHandler(CookieAccessTestHandler::BLOCK_WRITE, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Block reading and writing of cookies with scheme handler backend.
TEST(RequestHandlerTest, CookieAccessSchemeBlockReadWrite) {
  CefRefPtr<CookieAccessTestHandler> handler = new CookieAccessTestHandler(
      CookieAccessTestHandler::BLOCK_READ_WRITE, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Block all cookies with scheme handler backend.
TEST(RequestHandlerTest, CookieAccessSchemeBlockAll) {
  CefRefPtr<CookieAccessTestHandler> handler =
      new CookieAccessTestHandler(CookieAccessTestHandler::BLOCK_ALL, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Entry point for creating request handler browser test objects.
// Called from client_app_delegates.cc.
void CreateRequestHandlerBrowserTests(
    ClientAppBrowser::DelegateSet& delegates) {
  delegates.insert(new NetNotifyBrowserTest);
}

// Entry point for creating request handler renderer test objects.
// Called from client_app_delegates.cc.
void CreateRequestHandlerRendererTests(
    ClientAppRenderer::DelegateSet& delegates) {
  delegates.insert(new NetNotifyRendererTest);
}
