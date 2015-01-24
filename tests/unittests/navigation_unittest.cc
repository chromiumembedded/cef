// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <list>

// Include this first to avoid type conflicts with CEF headers.
#include "tests/unittests/chromium_includes.h"

#include "include/base/cef_bind.h"
#include "include/cef_callback.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_closure_task.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tests/cefclient/client_app.h"
#include "tests/unittests/test_handler.h"
#include "tests/unittests/test_util.h"

using client::ClientApp;

namespace {

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

bool g_history_nav_test = false;

// Browser side.
class HistoryNavBrowserTest : public ClientApp::BrowserDelegate {
 public:
  HistoryNavBrowserTest() {}

  void OnBeforeChildProcessLaunch(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefCommandLine> command_line) override {
    if (!g_history_nav_test)
      return;

    // Indicate to the render process that the test should be run.
    command_line->AppendSwitchWithValue("test", kHistoryNavMsg);
  }

 protected:
  IMPLEMENT_REFCOUNTING(HistoryNavBrowserTest);
};

// Renderer side.
class HistoryNavRendererTest : public ClientApp::RenderDelegate,
                               public CefLoadHandler {
 public:
  HistoryNavRendererTest()
      : run_test_(false),
        nav_(0) {}

  void OnRenderThreadCreated(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefListValue> extra_info) override {
    if (!g_history_nav_test) {
      // Check that the test should be run.
      CefRefPtr<CefCommandLine> command_line =
          CefCommandLine::GetGlobalCommandLine();
      const std::string& test = command_line->GetSwitchValue("test");
      if (test != kHistoryNavMsg)
        return;
    }

    run_test_ = true;
  }

  CefRefPtr<CefLoadHandler> GetLoadHandler(
    CefRefPtr<ClientApp> app) override {
    if (!run_test_)
      return NULL;

    return this;
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    const NavListItem& item = kHNavList[nav_];

    const std::string& url = browser->GetMainFrame()->GetURL();
    if (isLoading) {
      got_loading_state_start_.yes();

      EXPECT_STRNE(item.target, url.c_str());

      if (nav_ > 0) {
        const NavListItem& last_item = kHNavList[nav_ - 1];
        EXPECT_EQ(last_item.can_go_back, browser->CanGoBack());
        EXPECT_EQ(last_item.can_go_back, canGoBack);
        EXPECT_EQ(last_item.can_go_forward, browser->CanGoForward());
        EXPECT_EQ(last_item.can_go_forward, canGoForward);
      } else {
        EXPECT_FALSE(browser->CanGoBack());
        EXPECT_FALSE(canGoBack);
        EXPECT_FALSE(browser->CanGoForward());
        EXPECT_FALSE(canGoForward);
      }
    } else {
      got_loading_state_end_.yes();

      EXPECT_STREQ(item.target, url.c_str());

      EXPECT_EQ(item.can_go_back, browser->CanGoBack());
      EXPECT_EQ(item.can_go_back, canGoBack);
      EXPECT_EQ(item.can_go_forward, browser->CanGoForward());
      EXPECT_EQ(item.can_go_forward, canGoForward);

      SendTestResultsIfDone(browser);
    }
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    const NavListItem& item = kHNavList[nav_];

    got_load_start_.yes();

    const std::string& url = frame->GetURL();
    EXPECT_STREQ(item.target, url.c_str());

    EXPECT_EQ(item.can_go_back, browser->CanGoBack());
    EXPECT_EQ(item.can_go_forward, browser->CanGoForward());
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    const NavListItem& item = kHNavList[nav_];

    got_load_end_.yes();

    const std::string& url = frame->GetURL();
    EXPECT_STREQ(item.target, url.c_str());

    EXPECT_EQ(item.can_go_back, browser->CanGoBack());
    EXPECT_EQ(item.can_go_forward, browser->CanGoForward());

    SendTestResultsIfDone(browser);
  }

  bool OnBeforeNavigation(CefRefPtr<ClientApp> app,
                          CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request,
                          cef_navigation_type_t navigation_type,
                          bool is_redirect) override {
    if (!run_test_)
      return false;

    const NavListItem& item = kHNavList[nav_];

    std::string url = request->GetURL();
    EXPECT_STREQ(item.target, url.c_str());

    EXPECT_EQ(RT_SUB_RESOURCE, request->GetResourceType());
    EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());

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

    return false;
  }

 protected:
  void SendTestResultsIfDone(CefRefPtr<CefBrowser> browser) {
    if (got_load_end_ && got_loading_state_end_)
      SendTestResults(browser);
  }

  // Send the test results.
  void SendTestResults(CefRefPtr<CefBrowser> browser) {
    EXPECT_TRUE(got_loading_state_start_);
    EXPECT_TRUE(got_loading_state_end_);
    EXPECT_TRUE(got_load_start_);
    EXPECT_TRUE(got_load_end_);

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

    // Reset the test results for the next navigation.
    got_loading_state_start_.reset();
    got_loading_state_end_.reset();
    got_load_start_.reset();
    got_load_end_.reset();

    nav_++;
  }

  bool run_test_;
  int nav_;

  TrackCallback got_loading_state_start_;
  TrackCallback got_loading_state_end_;
  TrackCallback got_load_start_;
  TrackCallback got_load_end_;

  IMPLEMENT_REFCOUNTING(HistoryNavRendererTest);
};

class NavigationEntryVisitor : public CefNavigationEntryVisitor {
 public:
  NavigationEntryVisitor(int nav, TrackCallback* callback)
    : nav_(nav),
      callback_(callback),
      expected_total_(0),
      expected_current_index_(-1),
      expected_forwardback_(),
      callback_count_(0) {
    // Determine the expected values.
    for (int i = 0; i <= nav_; ++i) {
      if (kHNavList[i].action == NA_LOAD) {
        expected_total_++;
        expected_current_index_++;
      } else if (kHNavList[i].action == NA_BACK) {
        expected_current_index_--;
      }  else if (kHNavList[i].action == NA_FORWARD) {
        expected_current_index_++;
      }
      expected_forwardback_[expected_current_index_] =
          (kHNavList[i].action != NA_LOAD);
    }
  }

  ~NavigationEntryVisitor() override {
    EXPECT_EQ(callback_count_, expected_total_);
    callback_->yes();
  }

  bool Visit(CefRefPtr<CefNavigationEntry> entry,
             bool current,
             int index,
             int total) override {
    // Only 3 loads total.
    EXPECT_LT(index, 3);
    EXPECT_LE(total, 3);

    EXPECT_EQ((expected_current_index_ == index), current);
    EXPECT_EQ(callback_count_, index);
    EXPECT_EQ(expected_total_, total);

    std::string expected_url;
    std::string expected_title;
    if (index == 0) {
      expected_url = kHNav1;
      expected_title = "Nav1";
    } else if (index == 1) {
      expected_url = kHNav2;
      expected_title = "Nav2";
    } else if (index == 2) {
      expected_url = kHNav3;
      expected_title = "Nav3";
    }

    EXPECT_TRUE(entry->IsValid());
    EXPECT_STREQ(expected_url.c_str(), entry->GetURL().ToString().c_str());
    EXPECT_STREQ(expected_url.c_str(),
                 entry->GetDisplayURL().ToString().c_str());
    EXPECT_STREQ(expected_url.c_str(),
                 entry->GetOriginalURL().ToString().c_str());
    EXPECT_STREQ(expected_title.c_str(), entry->GetTitle().ToString().c_str());

    if (expected_forwardback_[index])
      EXPECT_EQ(TT_EXPLICIT | TT_FORWARD_BACK_FLAG, entry->GetTransitionType());
    else
      EXPECT_EQ(TT_EXPLICIT, entry->GetTransitionType());

    EXPECT_FALSE(entry->HasPostData());
    EXPECT_TRUE(entry->GetFrameName().empty());
    EXPECT_GT(entry->GetCompletionTime().GetTimeT(), 0);
    EXPECT_EQ(200, entry->GetHttpStatusCode());

    callback_count_++;
    return true;
  }

 private:
  const int nav_;
  TrackCallback* callback_;
  int expected_total_;
  int expected_current_index_;
  bool expected_forwardback_[3];  // Only 3 loads total.
  int callback_count_;

  IMPLEMENT_REFCOUNTING(NavigationEntryVisitor);
};

// Browser side.
class HistoryNavTestHandler : public TestHandler {
 public:
  HistoryNavTestHandler()
      : nav_(0),
        load_end_confirmation_(false),
        renderer_confirmation_(false) {}

  void RunTest() override {
    // Add the resources that we will navigate to/from.
    AddResource(kHNav1,
        "<html><head><title>Nav1</title></head><body>Nav1</body></html>",
        "text/html");
    AddResource(kHNav2,
        "<html><head><title>Nav2</title><body>Nav2</body></html>",
        "text/html");
    AddResource(kHNav3,
        "<html><head><title>Nav3</title><body>Nav3</body></html>",
        "text/html");

    // Create the browser.
    CreateBrowser(CefString());

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
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

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);

    RunNav(browser);
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool is_redirect) override {
    const NavListItem& item = kHNavList[nav_];

    got_before_browse_[nav_].yes();

    std::string url = request->GetURL();
    EXPECT_STREQ(item.target, url.c_str());

    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    if (item.action == NA_LOAD)
      EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());
    else if (item.action == NA_BACK || item.action == NA_FORWARD)
      EXPECT_EQ(TT_EXPLICIT | TT_FORWARD_BACK_FLAG, request->GetTransitionType());

    if (nav_ > 0) {
      const NavListItem& last_item = kHNavList[nav_ - 1];
      EXPECT_EQ(last_item.can_go_back, browser->CanGoBack());
      EXPECT_EQ(last_item.can_go_forward, browser->CanGoForward());
    } else {
      EXPECT_FALSE(browser->CanGoBack());
      EXPECT_FALSE(browser->CanGoForward());
    }

    return false;
  }

  bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request) override {
    const NavListItem& item = kHNavList[nav_];

    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    if (item.action == NA_LOAD)
      EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());
    else if (item.action == NA_BACK || item.action == NA_FORWARD)
      EXPECT_EQ(TT_EXPLICIT | TT_FORWARD_BACK_FLAG, request->GetTransitionType());

    got_before_resource_load_[nav_].yes();

    std::string url = request->GetURL();
    if (url == item.target)
      got_correct_target_[nav_].yes();

    return false;
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    const NavListItem& item = kHNavList[nav_];

    got_loading_state_change_[nav_].yes();

    if (item.can_go_back == canGoBack)
      got_correct_can_go_back_[nav_].yes();
    if (item.can_go_forward == canGoForward)
      got_correct_can_go_forward_[nav_].yes();
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    if(browser->IsPopup() || !frame->IsMain())
      return;

    const NavListItem& item = kHNavList[nav_];

    got_load_start_[nav_].yes();

    std::string url1 = browser->GetMainFrame()->GetURL();
    std::string url2 = frame->GetURL();
    if (url1 == item.target && url2 == item.target)
      got_correct_load_start_url_[nav_].yes();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (browser->IsPopup() || !frame->IsMain())
      return;

    const NavListItem& item = kHNavList[nav_];

    got_load_end_[nav_].yes();

    // Test that navigation entries are correct.
    CefRefPtr<NavigationEntryVisitor> visitor =
        new NavigationEntryVisitor(nav_, &got_correct_history_[nav_]);
    browser->GetHost()->GetNavigationEntries(visitor.get(), false);
    visitor = NULL;

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

  bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) override {
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

  TrackCallback got_before_browse_[NAV_LIST_SIZE()];
  TrackCallback got_before_navigation_[NAV_LIST_SIZE()];
  TrackCallback got_before_resource_load_[NAV_LIST_SIZE()];
  TrackCallback got_correct_target_[NAV_LIST_SIZE()];
  TrackCallback got_loading_state_change_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_back_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_forward_[NAV_LIST_SIZE()];
  TrackCallback got_load_start_[NAV_LIST_SIZE()];
  TrackCallback got_correct_load_start_url_[NAV_LIST_SIZE()];
  TrackCallback got_load_end_[NAV_LIST_SIZE()];
  TrackCallback got_correct_history_[NAV_LIST_SIZE()];
  TrackCallback got_correct_load_end_url_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_back2_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_forward2_[NAV_LIST_SIZE()];
};

}  // namespace

// Verify history navigation.
TEST(NavigationTest, History) {
  g_history_nav_test = true;
  CefRefPtr<HistoryNavTestHandler> handler =
      new HistoryNavTestHandler();
  handler->ExecuteTest();
  g_history_nav_test = false;

  for (size_t i = 0; i < NAV_LIST_SIZE(); ++i) {
    if (kHNavList[i].action != NA_CLEAR) {
      ASSERT_TRUE(handler->got_before_browse_[i]) << "i = " << i;
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
      ASSERT_TRUE(handler->got_correct_history_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_load_end_url_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_can_go_back2_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_can_go_forward2_[i]) << "i = " << i;
    }
  }

  ReleaseAndWaitForDestructor(handler);
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

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
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

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
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

  void Cancel() override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
  }

  bool ReadResponse(void* data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
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

  CefRefPtr<CefResourceHandler> Create(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      const CefString& scheme_name,
      CefRefPtr<CefRequest> request) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    return new RedirectSchemeHandler();
  }

  IMPLEMENT_REFCOUNTING(RedirectSchemeHandlerFactory);
};

class RedirectTestHandler : public TestHandler {
 public:
  RedirectTestHandler() {}

  void RunTest() override {
    // Create the browser.
    CreateBrowser(kRNav1);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request) override {
    // Should be called for all but the second URL.
    std::string url = request->GetURL();

    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());

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

  void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          const CefString& old_url,
                          CefString& new_url) override {
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

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    // Should only be called for the final loaded URL.
    std::string url = frame->GetURL();

    if (url == kRNav4) {
      got_nav4_load_start_.yes();
    } else {
      got_invalid_load_start_.yes();
    }
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
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

  ReleaseAndWaitForDestructor(handler);
}


namespace {

const char KONav1[] = "http://tests-onav/nav1.html";
const char KONav2[] = "http://tests-onav/nav2.html";
const char kOrderNavMsg[] = "NavigationTest.OrderNav";
const char kOrderNavClosedMsg[] = "NavigationTest.OrderNavClosed";

void SetOrderNavExtraInfo(CefRefPtr<CefListValue> extra_info) {
  // Arbitrary data for testing.
  extra_info->SetBool(0, true);
  CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
  dict->SetInt("key1", 5);
  dict->SetString("key2", "test string");
  extra_info->SetDictionary(1, dict);
  extra_info->SetDouble(2, 5.43322);
  extra_info->SetString(3, "some string");
}

bool g_order_nav_test = false;

// Browser side.
class OrderNavBrowserTest : public ClientApp::BrowserDelegate {
 public:
  OrderNavBrowserTest() {}

  void OnBeforeChildProcessLaunch(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefCommandLine> command_line) override {
    if (!g_order_nav_test)
      return;

    // Indicate to the render process that the test should be run.
    command_line->AppendSwitchWithValue("test", kOrderNavMsg);
  }

  void OnRenderProcessThreadCreated(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefListValue> extra_info) override {
    if (!g_order_nav_test)
      return;

    // Some data that we'll check for.
    SetOrderNavExtraInfo(extra_info);
  }

 protected:
  IMPLEMENT_REFCOUNTING(OrderNavBrowserTest);
};

class OrderNavLoadState {
 public:
  OrderNavLoadState(bool is_popup, bool browser_side)
      : is_popup_(is_popup),
        browser_side_(browser_side) {}

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) {
    if (isLoading) {
      EXPECT_TRUE(Verify(false, false, false, false));

      got_loading_state_start_.yes();
    } else {
      EXPECT_TRUE(Verify(true, false, true, false));

      got_loading_state_end_.yes();
    }
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) {
    EXPECT_TRUE(Verify(true, false, false, false));

    got_load_start_.yes();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) {
    EXPECT_TRUE(Verify(true, true, true, false));

    got_load_end_.yes();
  }

  bool IsStarted() {
    return got_loading_state_start_ ||
           got_loading_state_end_ ||
           got_load_start_ ||
           got_load_end_;
  }

  bool IsDone() {
    return got_loading_state_start_ &&
           got_loading_state_end_ &&
           got_load_start_ &&
           got_load_end_;
  }

 private:
  bool Verify(bool got_loading_state_start,
              bool got_loading_state_end,
              bool got_load_start,
              bool got_load_end) {
    EXPECT_EQ(got_loading_state_start, got_loading_state_start_)
      << "Popup: " << is_popup_
      << "; Browser Side: " << browser_side_;
    EXPECT_EQ(got_loading_state_end, got_loading_state_end_)
      << "Popup: " << is_popup_
      << "; Browser Side: " << browser_side_;
    EXPECT_EQ(got_load_start, got_load_start_)
      << "Popup: " << is_popup_
      << "; Browser Side: " << browser_side_;
    EXPECT_EQ(got_load_end, got_load_end_)
      << "Popup: " << is_popup_
      << "; Browser Side: " << browser_side_;

    return got_loading_state_start == got_loading_state_start_ &&
           got_loading_state_end == got_loading_state_end_ &&
           got_load_start == got_load_start_ &&
           got_load_end == got_load_end_;
  }

  bool is_popup_;
  bool browser_side_;

  TrackCallback got_loading_state_start_;
  TrackCallback got_loading_state_end_;
  TrackCallback got_load_start_;
  TrackCallback got_load_end_;
};

// Renderer side.
class OrderNavRendererTest : public ClientApp::RenderDelegate,
                             public CefLoadHandler {
 public:
  OrderNavRendererTest()
      : run_test_(false),
        browser_id_main_(0),
        browser_id_popup_(0),
        state_main_(false, false), 
        state_popup_(true, false) {}

  void OnRenderThreadCreated(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefListValue> extra_info) override {
    if (!g_order_nav_test) {
      // Check that the test should be run.
      CefRefPtr<CefCommandLine> command_line =
          CefCommandLine::GetGlobalCommandLine();
      const std::string& test = command_line->GetSwitchValue("test");
      if (test != kOrderNavMsg)
        return;
    }

    run_test_ = true;

    EXPECT_FALSE(got_webkit_initialized_);

    got_render_thread_created_.yes();

    // Verify that |extra_info| transferred successfully.
    CefRefPtr<CefListValue> expected = CefListValue::Create();
    SetOrderNavExtraInfo(expected);
    TestListEqual(expected, extra_info);
  }

  void OnWebKitInitialized(CefRefPtr<ClientApp> app) override {
    if (!run_test_)
      return;

    EXPECT_TRUE(got_render_thread_created_);

    got_webkit_initialized_.yes();
  }

  void OnBrowserCreated(CefRefPtr<ClientApp> app,
                        CefRefPtr<CefBrowser> browser) override {
    if (!run_test_)
      return;

    EXPECT_TRUE(got_render_thread_created_);
    EXPECT_TRUE(got_webkit_initialized_);

    if (browser->IsPopup()) {
      EXPECT_FALSE(got_browser_created_popup_);
      EXPECT_FALSE(got_browser_destroyed_popup_);
      EXPECT_FALSE(state_popup_.IsStarted());

      got_browser_created_popup_.yes();
      browser_id_popup_ = browser->GetIdentifier();
      EXPECT_GT(browser->GetIdentifier(), 0);
    } else {
      EXPECT_FALSE(got_browser_created_main_);
      EXPECT_FALSE(got_browser_destroyed_main_);
      EXPECT_FALSE(state_main_.IsStarted());

      got_browser_created_main_.yes();
      browser_id_main_ = browser->GetIdentifier();
      EXPECT_GT(browser->GetIdentifier(), 0);

      browser_main_ = browser;
    }
  }

  void OnBrowserDestroyed(CefRefPtr<ClientApp> app,
                          CefRefPtr<CefBrowser> browser) override {
    if (!run_test_)
      return;

    EXPECT_TRUE(got_render_thread_created_);
    EXPECT_TRUE(got_webkit_initialized_);

    if (browser->IsPopup()) {
      EXPECT_TRUE(got_browser_created_popup_);
      EXPECT_FALSE(got_browser_destroyed_popup_);
      EXPECT_TRUE(state_popup_.IsDone());

      got_browser_destroyed_popup_.yes();
      EXPECT_EQ(browser_id_popup_, browser->GetIdentifier());
      EXPECT_GT(browser->GetIdentifier(), 0);

      // Use |browser_main_| to send the message otherwise it will fail.
      SendTestResults(browser_main_, kOrderNavClosedMsg);
    } else {
      EXPECT_TRUE(got_browser_created_main_);
      EXPECT_FALSE(got_browser_destroyed_main_);
      EXPECT_TRUE(state_main_.IsDone());

      got_browser_destroyed_main_.yes();
      EXPECT_EQ(browser_id_main_, browser->GetIdentifier());
      EXPECT_GT(browser->GetIdentifier(), 0);

      browser_main_ = NULL;
    }
  }

  CefRefPtr<CefLoadHandler> GetLoadHandler(
      CefRefPtr<ClientApp> app) override {
    if (!run_test_)
      return NULL;

    return this;
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    EXPECT_TRUE(got_render_thread_created_);
    EXPECT_TRUE(got_webkit_initialized_);

    if (browser->IsPopup()) {
      EXPECT_TRUE(got_browser_created_popup_);
      EXPECT_FALSE(got_browser_destroyed_popup_);

      state_popup_.OnLoadingStateChange(browser, isLoading, canGoBack,
                                        canGoForward);
    } else {
      EXPECT_TRUE(got_browser_created_main_);
      EXPECT_FALSE(got_browser_destroyed_main_);

      state_main_.OnLoadingStateChange(browser, isLoading, canGoBack,
                                       canGoForward);
    }

    if (!isLoading)
      SendTestResultsIfDone(browser);
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    EXPECT_TRUE(got_render_thread_created_);
    EXPECT_TRUE(got_webkit_initialized_);

    if (browser->IsPopup()) {
      EXPECT_TRUE(got_browser_created_popup_);
      EXPECT_FALSE(got_browser_destroyed_popup_);

      state_popup_.OnLoadStart(browser, frame);
    } else {
      EXPECT_TRUE(got_browser_created_main_);
      EXPECT_FALSE(got_browser_destroyed_main_);

      state_main_.OnLoadStart(browser, frame);
    }
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_TRUE(got_render_thread_created_);
    EXPECT_TRUE(got_webkit_initialized_);

    if (browser->IsPopup()) {
      EXPECT_TRUE(got_browser_created_popup_);
      EXPECT_FALSE(got_browser_destroyed_popup_);

      state_popup_.OnLoadEnd(browser, frame, httpStatusCode);
    } else {
      EXPECT_TRUE(got_browser_created_main_);
      EXPECT_FALSE(got_browser_destroyed_main_);

      state_main_.OnLoadEnd(browser, frame, httpStatusCode);
    }

    SendTestResultsIfDone(browser);
  }

 protected:
  void SendTestResultsIfDone(CefRefPtr<CefBrowser> browser) {
    bool done = false;
    if (browser->IsPopup())
      done = state_popup_.IsDone();
    else
      done = state_main_.IsDone();

    if (done)
      SendTestResults(browser, kOrderNavMsg);
  }

  // Send the test results.
  void SendTestResults(CefRefPtr<CefBrowser> browser, const char* msg_name) {
    // Check if the test has failed.
    bool result = !TestFailed();

    // Return the result to the browser process.
    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(msg_name);
    CefRefPtr<CefListValue> args = return_msg->GetArgumentList();
    EXPECT_TRUE(args.get());
    EXPECT_TRUE(args->SetBool(0, result));
    if (browser->IsPopup())
      EXPECT_TRUE(args->SetInt(1, browser_id_popup_));
    else
      EXPECT_TRUE(args->SetInt(1, browser_id_main_));
    EXPECT_TRUE(browser->SendProcessMessage(PID_BROWSER, return_msg));
  }

  bool run_test_;

  int browser_id_main_;
  int browser_id_popup_;
  CefRefPtr<CefBrowser> browser_main_;
  TrackCallback got_render_thread_created_;
  TrackCallback got_webkit_initialized_;
  TrackCallback got_browser_created_main_;
  TrackCallback got_browser_destroyed_main_;
  TrackCallback got_browser_created_popup_;
  TrackCallback got_browser_destroyed_popup_;

  OrderNavLoadState state_main_;
  OrderNavLoadState state_popup_;

  IMPLEMENT_REFCOUNTING(OrderNavRendererTest);
};

// Browser side.
class OrderNavTestHandler : public TestHandler {
 public:
  OrderNavTestHandler()
      : browser_id_main_(0),
        browser_id_popup_(0),
        state_main_(false, true),
        state_popup_(true, true),
        got_message_(false) {}

  void RunTest() override {
    // Add the resources that we will navigate to/from.
    AddResource(KONav1, "<html>Nav1</html>", "text/html");
    AddResource(KONav2, "<html>Nav2</html>", "text/html");

    // Create the browser.
    CreateBrowser(KONav1);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void ContinueIfReady(CefRefPtr<CefBrowser> browser) {
    if (!got_message_)
      return;

    bool done = false;
    if (browser->IsPopup())
      done = state_popup_.IsDone();
    else
      done = state_main_.IsDone();
    if (!done)
      return;

    got_message_ = false;

    if (!browser->IsPopup()) {
      // Create the popup window.
      browser->GetMainFrame()->ExecuteJavaScript(
          "window.open('" + std::string(KONav2) + "');", CefString(), 0);
    } else {
      // Close the popup window.
      browser_popup_->GetHost()->CloseBrowser(false);
    }
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);

    if (browser->IsPopup()) {
      browser_id_popup_ = browser->GetIdentifier();
      EXPECT_GT(browser_id_popup_, 0);
      browser_popup_ = browser;
    } else {
      browser_id_main_ = browser->GetIdentifier();
      EXPECT_GT(browser_id_main_, 0);
    }
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool is_redirect) override {
    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());

    if (browser->IsPopup()) {
      EXPECT_EQ(TT_LINK, request->GetTransitionType());
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_popup_, browser->GetIdentifier());
      got_before_browse_popup_.yes();
    } else {
      EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_main_, browser->GetIdentifier());
      got_before_browse_main_.yes();
    }

    std::string url = request->GetURL();
    if (url == KONav1)
      EXPECT_FALSE(browser->IsPopup());
    else if (url == KONav2)
      EXPECT_TRUE(browser->IsPopup());
    else
      EXPECT_TRUE(false);  // not reached

    return false;
  }

  bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request) override {
    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());

    if (browser->IsPopup()) {
      EXPECT_EQ(TT_LINK, request->GetTransitionType());
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_popup_, browser->GetIdentifier());
    } else {
      EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_main_, browser->GetIdentifier());
    }
    
    return false;
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (browser->IsPopup()) {
      state_popup_.OnLoadingStateChange(browser, isLoading, canGoBack,
                                        canGoForward);
    } else {
      state_main_.OnLoadingStateChange(browser, isLoading, canGoBack,
                                       canGoForward);
    }

    if (!isLoading)
      ContinueIfReady(browser);
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    if (browser->IsPopup()) {
      state_popup_.OnLoadStart(browser, frame);
    } else {
      state_main_.OnLoadStart(browser, frame);
    }
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (browser->IsPopup()) {
      state_popup_.OnLoadEnd(browser, frame, httpStatusCode);
    } else {
      state_main_.OnLoadEnd(browser, frame, httpStatusCode);
    }

    ContinueIfReady(browser);
  }

  bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) override {
    if (browser->IsPopup()) {
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_popup_, browser->GetIdentifier());
    } else {
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_main_, browser->GetIdentifier());
    }

    const std::string& msg_name = message->GetName();
    if (msg_name == kOrderNavMsg || msg_name == kOrderNavClosedMsg) {
      // Test that the renderer side succeeded.
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      EXPECT_TRUE(args.get());
      EXPECT_TRUE(args->GetBool(0));

      if (browser->IsPopup()) {
        EXPECT_EQ(browser_id_popup_, args->GetInt(1));
      } else {
        EXPECT_EQ(browser_id_main_, args->GetInt(1));
      }

      if (msg_name == kOrderNavMsg) {
        // Continue with the test.
        got_message_ = true;
        ContinueIfReady(browser);
      } else {
        // Popup was closed. End the test.
        browser_popup_ = NULL;
        DestroyTest();
      }

      return true;
    }

    // Message not handled.
    return false;
  }

 protected:
  void DestroyTest() override {
    // Verify test expectations.
    EXPECT_TRUE(got_before_browse_main_);
    EXPECT_TRUE(got_before_browse_popup_);

    TestHandler::DestroyTest();
  }

  int browser_id_main_;
  int browser_id_popup_;
  CefRefPtr<CefBrowser> browser_popup_;

  TrackCallback got_before_browse_main_;
  TrackCallback got_before_browse_popup_;

  OrderNavLoadState state_main_;
  OrderNavLoadState state_popup_;

  bool got_message_;
};

}  // namespace

// Verify the order of navigation-related callbacks.
TEST(NavigationTest, Order) {
  g_order_nav_test = true;
  CefRefPtr<OrderNavTestHandler> handler =
      new OrderNavTestHandler();
  handler->ExecuteTest();
  g_order_nav_test = false;
  ReleaseAndWaitForDestructor(handler);
}


namespace {

const char kCrossOriginNav1[] = "http://tests-conav1/nav1.html";
const char kCrossOriginNav2[] = "http://tests-conav2/nav2.html";
const char kCrossOriginNavMsg[] = "NavigationTest.CrossOriginNav";

bool g_cross_origin_nav_test = false;

// Browser side.
class CrossOriginNavBrowserTest : public ClientApp::BrowserDelegate {
 public:
  CrossOriginNavBrowserTest() {}

  void OnBeforeChildProcessLaunch(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefCommandLine> command_line) override {
    if (!g_cross_origin_nav_test)
      return;

    // Indicate to the render process that the test should be run.
    command_line->AppendSwitchWithValue("test", kCrossOriginNavMsg);
  }

 protected:
  IMPLEMENT_REFCOUNTING(CrossOriginNavBrowserTest);
};

// Renderer side.
class CrossOriginNavRendererTest : public ClientApp::RenderDelegate,
                                   public CefLoadHandler {
 public:
  CrossOriginNavRendererTest()
      : run_test_(false) {}
  ~CrossOriginNavRendererTest() override {
    EXPECT_TRUE(status_list_.empty());
  }

  void OnRenderThreadCreated(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefListValue> extra_info) override {
    if (!g_cross_origin_nav_test) {
      // Check that the test should be run.
      CefRefPtr<CefCommandLine> command_line =
          CefCommandLine::GetGlobalCommandLine();
      const std::string& test = command_line->GetSwitchValue("test");
      if (test != kCrossOriginNavMsg)
        return;
    }

    run_test_ = true;

    EXPECT_FALSE(got_webkit_initialized_);

    got_render_thread_created_.yes();
  }

  void OnWebKitInitialized(CefRefPtr<ClientApp> app) override {
    if (!run_test_)
      return;

    EXPECT_TRUE(got_render_thread_created_);

    got_webkit_initialized_.yes();
  }

  void OnBrowserCreated(CefRefPtr<ClientApp> app,
                        CefRefPtr<CefBrowser> browser) override {
    if (!run_test_)
      return;

    EXPECT_TRUE(got_render_thread_created_);
    EXPECT_TRUE(got_webkit_initialized_);

    EXPECT_FALSE(GetStatus(browser));
    Status* status = AddStatus(browser);
    status->got_browser_created.yes();
  }

  void OnBrowserDestroyed(CefRefPtr<ClientApp> app,
                          CefRefPtr<CefBrowser> browser) override {
    if (!run_test_)
      return;

    EXPECT_TRUE(got_render_thread_created_);
    EXPECT_TRUE(got_webkit_initialized_);

    Status* status = GetStatus(browser);
    EXPECT_TRUE(status);

    EXPECT_TRUE(status->got_browser_created);
    EXPECT_TRUE(status->got_loading_state_end);

    EXPECT_EQ(status->browser_id, browser->GetIdentifier());

    EXPECT_TRUE(RemoveStatus(browser));
  }

  CefRefPtr<CefLoadHandler> GetLoadHandler(
    CefRefPtr<ClientApp> app) override {
    if (!run_test_)
      return NULL;

    return this;
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (!isLoading) {
      EXPECT_TRUE(got_render_thread_created_);
      EXPECT_TRUE(got_webkit_initialized_);

      Status* status = GetStatus(browser);
      EXPECT_TRUE(status);

      EXPECT_TRUE(status->got_browser_created);
      EXPECT_FALSE(status->got_loading_state_end);

      status->got_loading_state_end.yes();

      EXPECT_EQ(status->browser_id, browser->GetIdentifier());

      SendTestResults(browser);
    }
  }

 protected:
  // Send the test results.
  void SendTestResults(CefRefPtr<CefBrowser> browser) {
    // Check if the test has failed.
    bool result = !TestFailed();

    // Return the result to the browser process.
    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(kCrossOriginNavMsg);
    CefRefPtr<CefListValue> args = return_msg->GetArgumentList();
    EXPECT_TRUE(args.get());
    EXPECT_TRUE(args->SetBool(0, result));
    EXPECT_TRUE(args->SetInt(1, browser->GetIdentifier()));
    EXPECT_TRUE(browser->SendProcessMessage(PID_BROWSER, return_msg));
  }

  bool run_test_;

  TrackCallback got_render_thread_created_;
  TrackCallback got_webkit_initialized_;

  struct Status {
    Status() : browser_id(0) {}

    CefRefPtr<CefBrowser> browser;
    int browser_id;
    TrackCallback got_browser_created;
    TrackCallback got_loading_state_end;
  };
  typedef std::list<Status> StatusList;
  StatusList status_list_;

  Status* GetStatus(CefRefPtr<CefBrowser> browser) {
    StatusList::iterator it = status_list_.begin();
    for (; it != status_list_.end(); ++it) {
      Status& status = (*it);
      if (status.browser->IsSame(browser))
        return &status;
    }

    return NULL;
  }

  Status* AddStatus(CefRefPtr<CefBrowser> browser) {
    Status status;
    status.browser = browser;
    status.browser_id = browser->GetIdentifier();
    EXPECT_GT(status.browser_id, 0);
    status_list_.push_back(status);
    return &status_list_.back();
  }

  bool RemoveStatus(CefRefPtr<CefBrowser> browser) {
    StatusList::iterator it = status_list_.begin();
    for (; it != status_list_.end(); ++it) {
      Status& status = (*it);
      if (status.browser->IsSame(browser)) {
        status_list_.erase(it);
        return true;
      }
    }

    return false;
  }

  IMPLEMENT_REFCOUNTING(CrossOriginNavRendererTest);
};

// Browser side.
class CrossOriginNavTestHandler : public TestHandler {
 public:
  CrossOriginNavTestHandler()
      : browser_id_current_(0),
        got_message_(false),
        got_load_end_(false) {}

  void RunTest() override {
    // Add the resources that we will navigate to/from.
    AddResource(kCrossOriginNav1, "<html>Nav1</html>", "text/html");
    AddResource(kCrossOriginNav2, "<html>Nav2</html>", "text/html");

    // Create the browser.
    CreateBrowser(kCrossOriginNav1);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void ContinueIfReady(CefRefPtr<CefBrowser> browser) {
    if (!got_message_ || !got_load_end_)
      return;

    got_message_ = false;
    got_load_end_ = false;

    std::string url = browser->GetMainFrame()->GetURL();
    if (url == kCrossOriginNav1) {
      // Load the next url.
      browser->GetMainFrame()->LoadURL(kCrossOriginNav2);
    } else {
      // Done with the test.
      DestroyTest();
    }
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);

    EXPECT_EQ(browser_id_current_, 0);
    browser_id_current_ = browser->GetIdentifier();
    EXPECT_GT(browser_id_current_, 0);
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool is_redirect) override {
    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());

    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());

    return false;
  }

  bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request) override {
    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    EXPECT_EQ(TT_EXPLICIT, request->GetTransitionType());

    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());
    
    return false;
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());

    got_load_end_ = true;
    ContinueIfReady(browser);
  }

  bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) override {
    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());

    const std::string& msg_name = message->GetName();
    if (msg_name == kCrossOriginNavMsg) {
      // Test that the renderer side succeeded.
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      EXPECT_TRUE(args.get());
      EXPECT_TRUE(args->GetBool(0));

      EXPECT_EQ(browser_id_current_, args->GetInt(1));

      // Continue with the test.
      got_message_ = true;
      ContinueIfReady(browser);

      return true;
    }

    // Message not handled.
    return false;
  }

 protected:
  int browser_id_current_;

  bool got_message_;
  bool got_load_end_;
};

}  // namespace

// Verify navigation-related callbacks when browsing cross-origin.
TEST(NavigationTest, CrossOrigin) {
  g_cross_origin_nav_test = true;
  CefRefPtr<CrossOriginNavTestHandler> handler =
      new CrossOriginNavTestHandler();
  handler->ExecuteTest();
  g_cross_origin_nav_test = false;
  ReleaseAndWaitForDestructor(handler);
}


namespace {

const char kPopupNavPageUrl[] = "http://tests-popup.com/page.html";
const char kPopupNavPopupUrl[] = "http://tests-popup.com/popup.html";
const char kPopupNavPopupUrl2[] = "http://tests-popup2.com/popup.html";
const char kPopupNavPopupName[] = "my_popup";

// Browser side.
class PopupNavTestHandler : public TestHandler {
 public:
  enum Mode {
    ALLOW,
    DENY,
    NAVIGATE_AFTER_CREATION,
  };

  PopupNavTestHandler(Mode mode)
      : mode_(mode) {}

  void RunTest() override {
    // Add the resources that we will navigate to/from.
    std::string page = "<html><script>function doPopup() { window.open('" +
                       std::string(kPopupNavPopupUrl) + "', '" +
                       std::string(kPopupNavPopupName) +
                       "'); }</script>Page</html>";
    AddResource(kPopupNavPageUrl, page, "text/html");
    AddResource(kPopupNavPopupUrl, "<html>Popup</html>", "text/html");
    if (mode_ == NAVIGATE_AFTER_CREATION)
      AddResource(kPopupNavPopupUrl2, "<html>Popup2</html>", "text/html");

    // Create the browser.
    CreateBrowser(kPopupNavPageUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     const CefString& target_url,
                     const CefString& target_frame_name,
                     const CefPopupFeatures& popupFeatures,
                     CefWindowInfo& windowInfo,
                     CefRefPtr<CefClient>& client,
                     CefBrowserSettings& settings,
                     bool* no_javascript_access) override {
    got_on_before_popup_.yes();

    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_STREQ(kPopupNavPageUrl, frame->GetURL().ToString().c_str());
    EXPECT_STREQ(kPopupNavPopupUrl, target_url.ToString().c_str());
    EXPECT_STREQ(kPopupNavPopupName, target_frame_name.ToString().c_str());
    EXPECT_FALSE(*no_javascript_access);

    return (mode_ == DENY);  // Return true to cancel the popup.
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);

    if (mode_ == NAVIGATE_AFTER_CREATION && browser->IsPopup())
      browser->GetMainFrame()->LoadURL(kPopupNavPopupUrl2);
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    std::string url = frame->GetURL();
    if (url == kPopupNavPageUrl) {
      frame->ExecuteJavaScript("doPopup()", kPopupNavPageUrl, 0);

      if (mode_ == DENY) {
        // Wait a bit to make sure the popup window isn't created.
        CefPostDelayedTask(TID_UI,
            base::Bind(&PopupNavTestHandler::DestroyTest, this), 200);
      }
    } else if (url == kPopupNavPopupUrl) {
      if (mode_ != NAVIGATE_AFTER_CREATION) {
        if (mode_ != DENY) {
          got_popup_load_end_.yes();
          browser->GetHost()->CloseBrowser(false);
          DestroyTest();
        } else {
          EXPECT_FALSE(true); // Not reached.
        }
      }
    } else if (url == kPopupNavPopupUrl2) {
      if (mode_ == NAVIGATE_AFTER_CREATION) {
        got_popup_load_end_.yes();
        browser->GetHost()->CloseBrowser(false);
        DestroyTest();
      } else {
        EXPECT_FALSE(true); // Not reached.
      }
    } else {
      EXPECT_FALSE(true); // Not reached.
    }
  }

 private:
  void DestroyTest() override {
    EXPECT_TRUE(got_on_before_popup_);
    if (mode_ != DENY)
      EXPECT_TRUE(got_popup_load_end_);
    else
      EXPECT_FALSE(got_popup_load_end_);

    TestHandler::DestroyTest();
  }

  const Mode mode_;

  TrackCallback got_on_before_popup_;
  TrackCallback got_popup_load_end_;
};

}  // namespace

// Test allowing popups.
TEST(NavigationTest, PopupAllow) {
  CefRefPtr<PopupNavTestHandler> handler =
      new PopupNavTestHandler(PopupNavTestHandler::ALLOW);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test denying popups.
TEST(NavigationTest, PopupDeny) {
  CefRefPtr<PopupNavTestHandler> handler =
      new PopupNavTestHandler(PopupNavTestHandler::DENY);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test navigation to a different origin after popup creation to verify that
// internal objects are tracked correctly (see issue #1392).
TEST(NavigationTest, PopupNavigateAfterCreation) {
  CefRefPtr<PopupNavTestHandler> handler =
      new PopupNavTestHandler(PopupNavTestHandler::NAVIGATE_AFTER_CREATION);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}


namespace {

const char kBrowseNavPageUrl[] = "http://tests-browsenav/nav.html";

// Browser side.
class BrowseNavTestHandler : public TestHandler {
 public:
  BrowseNavTestHandler(bool allow)
      : allow_(allow),
        destroyed_(false) {}

  void RunTest() override {
    AddResource(kBrowseNavPageUrl, "<html>Test</html>", "text/html");

    // Create the browser.
    CreateBrowser(kBrowseNavPageUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool is_redirect) override {
    const std::string& url = request->GetURL();
    EXPECT_STREQ(kBrowseNavPageUrl, url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());
    
    got_before_browse_.yes();
    
    return !allow_;
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    const std::string& url = frame->GetURL();
    EXPECT_STREQ(kBrowseNavPageUrl, url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    got_load_start_.yes();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    const std::string& url = frame->GetURL();
    EXPECT_STREQ(kBrowseNavPageUrl, url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    got_load_end_.yes();
    DestroyTestIfDone();
  }

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override {
    const std::string& url = frame->GetURL();
    EXPECT_STREQ("", url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    EXPECT_EQ(ERR_ABORTED, errorCode);
    EXPECT_STREQ(kBrowseNavPageUrl, failedUrl.ToString().c_str());

    got_load_error_.yes();
    DestroyTestIfDone();
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    const std::string& url = browser->GetMainFrame()->GetURL();
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());

    if (isLoading) {
      EXPECT_STREQ("", url.c_str());

      got_loading_state_changed_start_.yes();
    } else {
      if (allow_)
        EXPECT_STREQ(kBrowseNavPageUrl, url.c_str());
      else
        EXPECT_STREQ("", url.c_str());

      got_loading_state_changed_end_.yes();
      DestroyTestIfDone();
    }
  }

 private:
  void DestroyTestIfDone() {
    if (destroyed_)
      return;

    if (got_loading_state_changed_end_) {
      if (allow_) {
        if (got_load_end_)
          DestroyTest();
      } else if (got_load_error_) {
        DestroyTest();
      }
    }
  }

  void DestroyTest() override {
    if (destroyed_)
      return;
    destroyed_ = true;

    EXPECT_TRUE(got_before_browse_);
    EXPECT_TRUE(got_loading_state_changed_start_);
    EXPECT_TRUE(got_loading_state_changed_end_);

    if (allow_) {
      EXPECT_TRUE(got_load_start_);
      EXPECT_TRUE(got_load_end_);
      EXPECT_FALSE(got_load_error_);
    } else {
      EXPECT_FALSE(got_load_start_);
      EXPECT_FALSE(got_load_end_);
      EXPECT_TRUE(got_load_error_);
    }

    TestHandler::DestroyTest();
  }

  bool allow_;
  bool destroyed_;

  TrackCallback got_before_browse_;
  TrackCallback got_load_start_;
  TrackCallback got_load_end_;
  TrackCallback got_load_error_;
  TrackCallback got_loading_state_changed_start_;
  TrackCallback got_loading_state_changed_end_;
};

}  // namespace

// Test allowing navigation.
TEST(NavigationTest, BrowseAllow) {
  CefRefPtr<BrowseNavTestHandler> handler = new BrowseNavTestHandler(true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test denying navigation.
TEST(NavigationTest, BrowseDeny) {
  CefRefPtr<BrowseNavTestHandler> handler = new BrowseNavTestHandler(false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}


// Entry point for creating navigation browser test objects.
// Called from client_app_delegates.cc.
void CreateNavigationBrowserTests(ClientApp::BrowserDelegateSet& delegates) {
  delegates.insert(new HistoryNavBrowserTest);
  delegates.insert(new OrderNavBrowserTest);
  delegates.insert(new CrossOriginNavBrowserTest);
}

// Entry point for creating navigation renderer test objects.
// Called from client_app_delegates.cc.
void CreateNavigationRendererTests(ClientApp::RenderDelegateSet& delegates) {
  delegates.insert(new HistoryNavRendererTest);
  delegates.insert(new OrderNavRendererTest);
  delegates.insert(new CrossOriginNavRendererTest);
}
