// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_callback.h"
#include "include/cef_scheme.h"
#include "tests/cefclient/client_app.h"
#include "tests/unittests/test_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kHNavDomain[] = "http://tests-hnav/";
const char kHNav1[] = "http://tests-hnav/nav1.html";
const char kHNav2[] = "http://tests-hnav/nav2.html";
const char kHNav3[] = "http://tests-hnav/nav3.html";
const char kHistoryNavMsg[] = "NavigationTest.HistoryNav";


enum NavAction {
  NA_LOAD = 1,
  NA_BACK,
  NA_FORWARD,
  NA_CLEAR
};

typedef struct {
  NavAction action;     // What to do
  const char* target;   // Where to be after navigation
  bool can_go_back;     // After navigation, can go back?
  bool can_go_forward;  // After navigation, can go forward?
} NavListItem;

// Array of navigation actions: X = current page, . = history exists
static NavListItem kHNavList[] = {
                                      // kHNav1 | kHNav2 | kHNav3
  {NA_LOAD, kHNav1, false, false},    //   X
  {NA_LOAD, kHNav2, true, false},     //   .        X
  {NA_BACK, kHNav1, false, true},     //   X        .
  {NA_FORWARD, kHNav2, true, false},  //   .        X
  {NA_LOAD, kHNav3, true, false},     //   .        .        X
  {NA_BACK, kHNav2, true, true},      //   .        X        .
  // TODO(cef): Enable once ClearHistory is implemented
  // {NA_CLEAR, kHNav2, false, false},   //            X
};

#define NAV_LIST_SIZE() (sizeof(kHNavList) / sizeof(NavListItem))

// Renderer side.
class HistoryNavRendererTest : public ClientApp::RenderDelegate {
 public:
  HistoryNavRendererTest()
      : nav_(0) {}

  virtual bool OnBeforeNavigation(CefRefPtr<ClientApp> app,
                                  CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefRequest> request,
                                  cef_navigation_type_t navigation_type,
                                  bool is_redirect) OVERRIDE {
    std::string url = request->GetURL();
    // Don't leak into other tests.
    if (url.find(kHNavDomain) != 0)
      return false;

    const NavListItem& item = kHNavList[nav_];

    EXPECT_STREQ(item.target, url.c_str());

    if (item.action == NA_LOAD)
      EXPECT_EQ(NAVIGATION_OTHER, navigation_type);
    else if (item.action == NA_BACK || item.action == NA_FORWARD)
      EXPECT_EQ(NAVIGATION_BACK_FORWARD, navigation_type);

    if (nav_ > 0) {
      const NavListItem& last_item = kHNavList[nav_ - 1];
      EXPECT_EQ(last_item.can_go_back, browser->CanGoBack());
      EXPECT_EQ(last_item.can_go_forward, browser->CanGoForward());
    } else {
      EXPECT_FALSE(browser->CanGoBack());
      EXPECT_FALSE(browser->CanGoForward());
    }

    SendTestResults(browser);
    nav_++;

    return false;
  }

 protected:
  // Send the test results.
  void SendTestResults(CefRefPtr<CefBrowser> browser) {
    // Check if the test has failed.
    bool result = !TestFailed();

    // Return the result to the browser process.
    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(kHistoryNavMsg);
    CefRefPtr<CefListValue> args = return_msg->GetArgumentList();
    EXPECT_TRUE(args.get());
    EXPECT_TRUE(args->SetInt(0, nav_));
    EXPECT_TRUE(args->SetBool(1, result));
    EXPECT_TRUE(browser->SendProcessMessage(PID_BROWSER, return_msg));
  }

  int nav_;

  IMPLEMENT_REFCOUNTING(HistoryNavRendererTest);
};

// Browser side.
class HistoryNavTestHandler : public TestHandler {
 public:
  HistoryNavTestHandler()
      : nav_(0),
        load_end_confirmation_(false),
        renderer_confirmation_(false) {}

  virtual void RunTest() OVERRIDE {
    // Add the resources that we will navigate to/from.
    AddResource(kHNav1, "<html>Nav1</html>", "text/html");
    AddResource(kHNav2, "<html>Nav2</html>", "text/html");
    AddResource(kHNav3, "<html>Nav3</html>", "text/html");

    // Create the browser.
    CreateBrowser(CefString());
  }

  void RunNav(CefRefPtr<CefBrowser> browser) {
    if (nav_ == NAV_LIST_SIZE()) {
      // End of the nav list.
      DestroyTest();
      return;
    }

    const NavListItem& item = kHNavList[nav_];

    // Perform the action.
    switch (item.action) {
    case NA_LOAD:
      browser->GetMainFrame()->LoadURL(item.target);
      break;
    case NA_BACK:
      browser->GoBack();
      break;
    case NA_FORWARD:
      browser->GoForward();
      break;
    case NA_CLEAR:
      // TODO(cef): Enable once ClearHistory is implemented
      // browser->GetHost()->ClearHistory();
      // Not really a navigation action so go to the next one.
      nav_++;
      RunNav(browser);
      break;
    default:
      break;
    }
  }

  void RunNextNavIfReady(CefRefPtr<CefBrowser> browser) {
    if (load_end_confirmation_ && renderer_confirmation_) {
      load_end_confirmation_ = false;
      renderer_confirmation_ = false;
      nav_++;
      RunNav(browser);
    }
  }

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE {
    TestHandler::OnAfterCreated(browser);

    RunNav(browser);
  }

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request) OVERRIDE {
    const NavListItem& item = kHNavList[nav_];

    got_before_resource_load_[nav_].yes();

    std::string url = request->GetURL();
    if (url == item.target)
      got_correct_target_[nav_].yes();

    return false;
  }

  virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                    bool isLoading,
                                    bool canGoBack,
                                    bool canGoForward) OVERRIDE {
    const NavListItem& item = kHNavList[nav_];

    got_loading_state_change_[nav_].yes();

    if (item.can_go_back == canGoBack)
      got_correct_can_go_back_[nav_].yes();
    if (item.can_go_forward == canGoForward)
      got_correct_can_go_forward_[nav_].yes();
  }

  virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame) OVERRIDE {
    if(browser->IsPopup() || !frame->IsMain())
      return;

    const NavListItem& item = kHNavList[nav_];

    got_load_start_[nav_].yes();

    std::string url1 = browser->GetMainFrame()->GetURL();
    std::string url2 = frame->GetURL();
    if (url1 == item.target && url2 == item.target)
      got_correct_load_start_url_[nav_].yes();
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    if (browser->IsPopup() || !frame->IsMain())
      return;

    const NavListItem& item = kHNavList[nav_];

    got_load_end_[nav_].yes();

    std::string url1 = browser->GetMainFrame()->GetURL();
    std::string url2 = frame->GetURL();
    if (url1 == item.target && url2 == item.target)
      got_correct_load_end_url_[nav_].yes();

    if (item.can_go_back == browser->CanGoBack())
      got_correct_can_go_back2_[nav_].yes();
    if (item.can_go_forward == browser->CanGoForward())
      got_correct_can_go_forward2_[nav_].yes();

    load_end_confirmation_ = true;
    RunNextNavIfReady(browser);
  }

  virtual bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) OVERRIDE {
    if (message->GetName().ToString() == kHistoryNavMsg) {
      got_before_navigation_[nav_].yes();

      // Test that the renderer side succeeded.
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      EXPECT_TRUE(args.get());
      EXPECT_EQ(nav_, args->GetInt(0));
      EXPECT_TRUE(args->GetBool(1));

      renderer_confirmation_ = true;
      RunNextNavIfReady(browser);
      return true;
    }

    // Message not handled.
    return false;
  }

  int nav_;
  bool load_end_confirmation_;
  bool renderer_confirmation_;

  TrackCallback got_before_navigation_[NAV_LIST_SIZE()];
  TrackCallback got_before_resource_load_[NAV_LIST_SIZE()];
  TrackCallback got_correct_target_[NAV_LIST_SIZE()];
  TrackCallback got_loading_state_change_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_back_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_forward_[NAV_LIST_SIZE()];
  TrackCallback got_load_start_[NAV_LIST_SIZE()];
  TrackCallback got_correct_load_start_url_[NAV_LIST_SIZE()];
  TrackCallback got_load_end_[NAV_LIST_SIZE()];
  TrackCallback got_correct_load_end_url_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_back2_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_forward2_[NAV_LIST_SIZE()];
};

}  // namespace

// Verify history navigation.
TEST(NavigationTest, History) {
  CefRefPtr<HistoryNavTestHandler> handler =
      new HistoryNavTestHandler();
  handler->ExecuteTest();

  for (size_t i = 0; i < NAV_LIST_SIZE(); ++i) {
    if (kHNavList[i].action != NA_CLEAR) {
      ASSERT_TRUE(handler->got_before_navigation_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_before_resource_load_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_target_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_load_start_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_load_start_url_[i]) << "i = " << i;
    }

    ASSERT_TRUE(handler->got_loading_state_change_[i]) << "i = " << i;
    ASSERT_TRUE(handler->got_correct_can_go_back_[i]) << "i = " << i;
    ASSERT_TRUE(handler->got_correct_can_go_forward_[i]) << "i = " << i;

    if (kHNavList[i].action != NA_CLEAR) {
      ASSERT_TRUE(handler->got_load_end_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_load_end_url_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_can_go_back2_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_can_go_forward2_[i]) << "i = " << i;
    }
  }
}


namespace {

const char kFNav1[] = "http://tests/nav1.html";
const char kFNav2[] = "http://tests/nav2.html";
const char kFNav3[] = "http://tests/nav3.html";

class FrameNameIdentNavTestHandler : public TestHandler {
 public:
  FrameNameIdentNavTestHandler() : browse_ct_(0) {}

  virtual void RunTest() OVERRIDE {
    // Add the frame resources.
    std::stringstream ss;

    // Page with named frame
    ss << "<html>Nav1<iframe src=\"" << kFNav2 << "\" name=\"nav2\"></html>";
    AddResource(kFNav1, ss.str(), "text/html");
    ss.str("");

    // Page with unnamed frame
    ss << "<html>Nav2<iframe src=\"" << kFNav3 << "\"></html>";
    AddResource(kFNav2, ss.str(), "text/html");
    ss.str("");

    AddResource(kFNav3, "<html>Nav3</html>", "text/html");

    // Create the browser.
    CreateBrowser(kFNav1);
  }

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request) OVERRIDE {
    std::string name = frame->GetName();
    CefRefPtr<CefFrame> parent = frame->GetParent();

    std::string url = request->GetURL();
    if (url == kFNav1) {
      frame1_ident_ = frame->GetIdentifier();
      if (name == "") {
        frame1_name_ = name;
        got_frame1_name_.yes();
      }
      if (!parent.get())
        got_frame1_ident_parent_before_.yes();
    } else if (url == kFNav2) {
      frame2_ident_ = frame->GetIdentifier();
      if (name == "nav2") {
        frame2_name_ = name;
        got_frame2_name_.yes();
      }
      if (parent.get() && frame1_ident_ == parent->GetIdentifier())
        got_frame2_ident_parent_before_.yes();
    } else if (url == kFNav3) {
      frame3_ident_ = frame->GetIdentifier();
      if (name == "<!--framePath //nav2/<!--frame0-->-->") {
        frame3_name_ = name;
        got_frame3_name_.yes();
      }
       if (parent.get() && frame2_ident_ == parent->GetIdentifier())
        got_frame3_ident_parent_before_.yes();
    }

    return false;
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    std::string url = frame->GetURL();
    CefRefPtr<CefFrame> parent = frame->GetParent();

    if (url == kFNav1) {
      if (frame1_ident_ == frame->GetIdentifier())
        got_frame1_ident_.yes();
      if (!parent.get())
        got_frame1_ident_parent_after_.yes();
    } else if (url == kFNav2) {
      if (frame2_ident_ == frame->GetIdentifier())
        got_frame2_ident_.yes();
      if (parent.get() && frame1_ident_ == parent->GetIdentifier())
        got_frame2_ident_parent_after_.yes();
    } else if (url == kFNav3) {
      if (frame3_ident_ == frame->GetIdentifier())
        got_frame3_ident_.yes();
      if (parent.get() && frame2_ident_ == parent->GetIdentifier())
        got_frame3_ident_parent_after_.yes();
    }

    if (++browse_ct_ == 3) {
      // Test GetFrameNames
      std::vector<CefString> names;
      browser->GetFrameNames(names);
      EXPECT_EQ((size_t)3, names.size());
      EXPECT_STREQ(frame1_name_.c_str(), names[0].ToString().c_str());
      EXPECT_STREQ(frame2_name_.c_str(), names[1].ToString().c_str());
      EXPECT_STREQ(frame3_name_.c_str(), names[2].ToString().c_str());

      // Test GetFrameIdentifiers
      std::vector<int64> idents;
      browser->GetFrameIdentifiers(idents);
      EXPECT_EQ((size_t)3, idents.size());
      EXPECT_EQ(frame1_ident_, idents[0]);
      EXPECT_EQ(frame2_ident_, idents[1]);
      EXPECT_EQ(frame3_ident_, idents[2]);

      DestroyTest();
    }
  }

  int browse_ct_;

  int64 frame1_ident_;
  std::string frame1_name_;
  int64 frame2_ident_;
  std::string frame2_name_;
  int64 frame3_ident_;
  std::string frame3_name_;

  TrackCallback got_frame1_name_;
  TrackCallback got_frame2_name_;
  TrackCallback got_frame3_name_;
  TrackCallback got_frame1_ident_;
  TrackCallback got_frame2_ident_;
  TrackCallback got_frame3_ident_;
  TrackCallback got_frame1_ident_parent_before_;
  TrackCallback got_frame2_ident_parent_before_;
  TrackCallback got_frame3_ident_parent_before_;
  TrackCallback got_frame1_ident_parent_after_;
  TrackCallback got_frame2_ident_parent_after_;
  TrackCallback got_frame3_ident_parent_after_;
};

}  // namespace

// Verify frame names and identifiers.
TEST(NavigationTest, FrameNameIdent) {
  CefRefPtr<FrameNameIdentNavTestHandler> handler =
      new FrameNameIdentNavTestHandler();
  handler->ExecuteTest();

  ASSERT_GT(handler->frame1_ident_, 0);
  ASSERT_GT(handler->frame2_ident_, 0);
  ASSERT_GT(handler->frame3_ident_, 0);
  ASSERT_TRUE(handler->got_frame1_name_);
  ASSERT_TRUE(handler->got_frame2_name_);
  ASSERT_TRUE(handler->got_frame3_name_);
  ASSERT_TRUE(handler->got_frame1_ident_);
  ASSERT_TRUE(handler->got_frame2_ident_);
  ASSERT_TRUE(handler->got_frame3_ident_);
  ASSERT_TRUE(handler->got_frame1_ident_parent_before_);
  ASSERT_TRUE(handler->got_frame2_ident_parent_before_);
  ASSERT_TRUE(handler->got_frame3_ident_parent_before_);
  ASSERT_TRUE(handler->got_frame1_ident_parent_after_);
  ASSERT_TRUE(handler->got_frame2_ident_parent_after_);
  ASSERT_TRUE(handler->got_frame3_ident_parent_after_);
}


namespace {

const char kRNav1[] = "http://tests/nav1.html";
const char kRNav2[] = "http://tests/nav2.html";
const char kRNav3[] = "http://tests/nav3.html";
const char kRNav4[] = "http://tests/nav4.html";

bool g_got_nav1_request = false;
bool g_got_nav3_request = false;
bool g_got_nav4_request = false;
bool g_got_invalid_request = false;

class RedirectSchemeHandler : public CefResourceHandler {
 public:
  RedirectSchemeHandler() : offset_(0), status_(0) {}

  virtual bool ProcessRequest(CefRefPtr<CefRequest> request,
                              CefRefPtr<CefCallback> callback) OVERRIDE {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    std::string url = request->GetURL();
    if (url == kRNav1) {
      // Redirect using HTTP 302
      g_got_nav1_request = true;
      status_ = 302;
      location_ = kRNav2;
      content_ = "<html><body>Redirected Nav1</body></html>";
    } else if (url == kRNav3) {
      // Redirect using redirectUrl
      g_got_nav3_request = true;
      status_ = -1;
      location_ = kRNav4;
      content_ = "<html><body>Redirected Nav3</body></html>";
    } else if (url == kRNav4) {
      g_got_nav4_request = true;
      status_ = 200;
      content_ = "<html><body>Nav4</body></html>";
    }

    if (status_ != 0) {
      callback->Continue();
      return true;
    } else {
      g_got_invalid_request = true;
      return false;
    }
  }

  virtual void GetResponseHeaders(CefRefPtr<CefResponse> response,
                                  int64& response_length,
                                  CefString& redirectUrl) OVERRIDE {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    EXPECT_NE(status_, 0);

    response->SetStatus(status_);
    response->SetMimeType("text/html");
    response_length = content_.size();

    if (status_ == 302) {
      // Redirect using HTTP 302
      EXPECT_GT(location_.size(), static_cast<size_t>(0));
      response->SetStatusText("Found");
      CefResponse::HeaderMap headers;
      response->GetHeaderMap(headers);
      headers.insert(std::make_pair("Location", location_));
      response->SetHeaderMap(headers);
    } else if (status_ == -1) {
      // Rdirect using redirectUrl
      EXPECT_GT(location_.size(), static_cast<size_t>(0));
      redirectUrl = location_;
    }
  }

  virtual void Cancel() OVERRIDE {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
  }

  virtual bool ReadResponse(void* data_out,
                            int bytes_to_read,
                            int& bytes_read,
                            CefRefPtr<CefCallback> callback) OVERRIDE {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    size_t size = content_.size();
    if (offset_ < size) {
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(size - offset_));
      memcpy(data_out, content_.c_str() + offset_, transfer_size);
      offset_ += transfer_size;

      bytes_read = transfer_size;
      return true;
    }

    return false;
  }

 protected:
  std::string content_;
  size_t offset_;
  int status_;
  std::string location_;

  IMPLEMENT_REFCOUNTING(RedirectSchemeHandler);
};

class RedirectSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  RedirectSchemeHandlerFactory() {}

  virtual CefRefPtr<CefResourceHandler> Create(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      const CefString& scheme_name,
      CefRefPtr<CefRequest> request) OVERRIDE {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    return new RedirectSchemeHandler();
  }

  IMPLEMENT_REFCOUNTING(RedirectSchemeHandlerFactory);
};

class RedirectTestHandler : public TestHandler {
 public:
  RedirectTestHandler() {}

  virtual void RunTest() OVERRIDE {
    // Create the browser.
    CreateBrowser(kRNav1);
  }

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request) OVERRIDE {
    // Should be called for all but the second URL.
    std::string url = request->GetURL();

    if (url == kRNav1) {
      got_nav1_before_resource_load_.yes();
    } else if (url == kRNav3) {
      got_nav3_before_resource_load_.yes();
    } else if (url == kRNav4) {
      got_nav4_before_resource_load_.yes();
    } else {
      got_invalid_before_resource_load_.yes();
    }

    return false;
  }

  virtual void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  const CefString& old_url,
                                  CefString& new_url) OVERRIDE {
    // Should be called for each redirected URL.

    if (old_url == kRNav1 && new_url == kRNav2) {
      // Called due to the nav1 redirect response.
      got_nav1_redirect_.yes();

      // Change the redirect to the 3rd URL.
      new_url = kRNav3;
    } else if (old_url == kRNav1 && new_url == kRNav3) {
      // Called due to the redirect change above.
      got_nav2_redirect_.yes();
    } else if (old_url == kRNav3 && new_url == kRNav4) {
      // Called due to the nav3 redirect response.
      got_nav3_redirect_.yes();
    } else {
      got_invalid_redirect_.yes();
    }
  }

  virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame) OVERRIDE {
    // Should only be called for the final loaded URL.
    std::string url = frame->GetURL();

    if (url == kRNav4) {
      got_nav4_load_start_.yes();
    } else {
      got_invalid_load_start_.yes();
    }
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    // Should only be called for the final loaded URL.
    std::string url = frame->GetURL();

    if (url == kRNav4) {
      got_nav4_load_end_.yes();
      DestroyTest();
    } else {
      got_invalid_load_end_.yes();
    }
  }

  TrackCallback got_nav1_before_resource_load_;
  TrackCallback got_nav3_before_resource_load_;
  TrackCallback got_nav4_before_resource_load_;
  TrackCallback got_invalid_before_resource_load_;
  TrackCallback got_nav4_load_start_;
  TrackCallback got_invalid_load_start_;
  TrackCallback got_nav4_load_end_;
  TrackCallback got_invalid_load_end_;
  TrackCallback got_nav1_redirect_;
  TrackCallback got_nav2_redirect_;
  TrackCallback got_nav3_redirect_;
  TrackCallback got_invalid_redirect_;
};

}  // namespace

// Verify frame names and identifiers.
TEST(NavigationTest, Redirect) {
  CefRegisterSchemeHandlerFactory("http", "tests",
      new RedirectSchemeHandlerFactory());
  WaitForIOThread();

  CefRefPtr<RedirectTestHandler> handler =
      new RedirectTestHandler();
  handler->ExecuteTest();

  CefClearSchemeHandlerFactories();
  WaitForIOThread();

  ASSERT_TRUE(handler->got_nav1_before_resource_load_);
  ASSERT_TRUE(handler->got_nav3_before_resource_load_);
  ASSERT_TRUE(handler->got_nav4_before_resource_load_);
  ASSERT_FALSE(handler->got_invalid_before_resource_load_);
  ASSERT_TRUE(handler->got_nav4_load_start_);
  ASSERT_FALSE(handler->got_invalid_load_start_);
  ASSERT_TRUE(handler->got_nav4_load_end_);
  ASSERT_FALSE(handler->got_invalid_load_end_);
  ASSERT_TRUE(handler->got_nav1_redirect_);
  ASSERT_TRUE(handler->got_nav2_redirect_);
  ASSERT_TRUE(handler->got_nav3_redirect_);
  ASSERT_FALSE(handler->got_invalid_redirect_);
  ASSERT_TRUE(g_got_nav1_request);
  ASSERT_TRUE(g_got_nav3_request);
  ASSERT_TRUE(g_got_nav4_request);
  ASSERT_FALSE(g_got_invalid_request);
}


// Entry point for creating navigation renderer test objects.
// Called from client_app_delegates.cc.
void CreateNavigationRendererTests(
    ClientApp::RenderDelegateSet& delegates) {
  delegates.insert(new HistoryNavRendererTest);
}
