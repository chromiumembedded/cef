// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <list>

#include "include/base/cef_callback.h"
#include "include/cef_callback.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/renderer/client_app_renderer.h"

using client::ClientAppBrowser;
using client::ClientAppRenderer;

namespace {

const char kHNav1[] = "http://tests-hnav.com/nav1.html";
const char kHNav2[] = "http://tests-hnav.com/nav2.html";
const char kHNav3[] = "http://tests-hnav.com/nav3.html";
const char kHistoryNavMsg[] = "NavigationTest.HistoryNav";
const char kHistoryNavTestCmdKey[] = "nav-history-test";

const cef_transition_type_t kTransitionExplicitLoad =
    static_cast<cef_transition_type_t>(TT_EXPLICIT | TT_DIRECT_LOAD_FLAG);

// TT_FORWARD_BACK_FLAG is added to the original transition flags.
const cef_transition_type_t kTransitionExplicitForwardBack =
    static_cast<cef_transition_type_t>(kTransitionExplicitLoad |
                                       TT_FORWARD_BACK_FLAG);

enum NavAction { NA_LOAD = 1, NA_BACK, NA_FORWARD, NA_CLEAR };

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
class HistoryNavRendererTest : public ClientAppRenderer::Delegate,
                               public CefLoadHandler {
 public:
  HistoryNavRendererTest() : run_test_(false), nav_(0) {}

  void OnBrowserCreated(CefRefPtr<ClientAppRenderer> app,
                        CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefDictionaryValue> extra_info) override {
    run_test_ = extra_info && extra_info->HasKey(kHistoryNavTestCmdKey);
  }

  CefRefPtr<CefLoadHandler> GetLoadHandler(
      CefRefPtr<ClientAppRenderer> app) override {
    if (!run_test_)
      return nullptr;

    return this;
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    const NavListItem& item = kHNavList[nav_];

    const std::string& url = browser->GetMainFrame()->GetURL();
    EXPECT_STREQ(item.target, url.c_str());

    EXPECT_EQ(item.can_go_back, browser->CanGoBack())
        << "nav: " << nav_ << " isLoading: " << isLoading;
    EXPECT_EQ(item.can_go_back, canGoBack)
        << "nav: " << nav_ << " isLoading: " << isLoading;
    EXPECT_EQ(item.can_go_forward, browser->CanGoForward())
        << "nav: " << nav_ << " isLoading: " << isLoading;
    EXPECT_EQ(item.can_go_forward, canGoForward)
        << "nav: " << nav_ << " isLoading: " << isLoading;

    if (isLoading) {
      got_loading_state_start_.yes();
    } else {
      got_loading_state_end_.yes();
      SendTestResultsIfDone(browser, browser->GetMainFrame());
    }
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    const NavListItem& item = kHNavList[nav_];

    got_load_start_.yes();

    const std::string& url = frame->GetURL();
    EXPECT_STREQ(item.target, url.c_str());

    EXPECT_EQ(TT_EXPLICIT, transition_type);

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

    SendTestResultsIfDone(browser, frame);
  }

 protected:
  void SendTestResultsIfDone(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame) {
    if (got_load_end_ && got_loading_state_end_)
      SendTestResults(browser, frame);
  }

  // Send the test results.
  void SendTestResults(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame) {
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
    frame->SendProcessMessage(PID_BROWSER, return_msg);

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
      } else if (kHNavList[i].action == NA_FORWARD) {
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

    const auto transition_type = entry->GetTransitionType();
    if (expected_forwardback_[index])
      EXPECT_EQ(kTransitionExplicitForwardBack, transition_type);
    else
      EXPECT_EQ(kTransitionExplicitLoad, transition_type);

    EXPECT_FALSE(entry->HasPostData());
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
        load_state_change_loaded_confirmation_(false),
        renderer_confirmation_(false) {}

  void RunTest() override {
    // Add the resources that we will navigate to/from.
    AddResource(
        kHNav1,
        "<html><head><title>Nav1</title></head><body>Nav1</body></html>",
        "text/html");
    AddResource(kHNav2,
                "<html><head><title>Nav2</title><body>Nav2</body></html>",
                "text/html");
    AddResource(kHNav3,
                "<html><head><title>Nav3</title><body>Nav3</body></html>",
                "text/html");

    CefRefPtr<CefDictionaryValue> extra_info = CefDictionaryValue::Create();
    extra_info->SetBool(kHistoryNavTestCmdKey, true);

    // Create the browser.
    CreateBrowser(CefString(), nullptr, extra_info);

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
    if (load_end_confirmation_ && load_state_change_loaded_confirmation_ &&
        renderer_confirmation_) {
      load_end_confirmation_ = false;
      load_state_change_loaded_confirmation_ = false;
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
                      bool user_gesture,
                      bool is_redirect) override {
    const NavListItem& item = kHNavList[nav_];

    got_before_browse_[nav_].yes();

    std::string url = request->GetURL();
    EXPECT_STREQ(item.target, url.c_str());

    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());

    const auto transition_type = request->GetTransitionType();
    if (item.action == NA_LOAD) {
      EXPECT_EQ(kTransitionExplicitLoad, transition_type);
    } else if (item.action == NA_BACK || item.action == NA_FORWARD) {
      EXPECT_EQ(kTransitionExplicitForwardBack, transition_type);
    }

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

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return RV_CANCEL;
    }

    const NavListItem& item = kHNavList[nav_];
    const std::string& url = request->GetURL();

    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType())
        << "nav=" << nav_ << " url=" << url;

    const auto transition_type = request->GetTransitionType();
    if (item.action == NA_LOAD) {
      EXPECT_EQ(kTransitionExplicitLoad, transition_type)
          << "nav=" << nav_ << " url=" << url;
    } else if (item.action == NA_BACK || item.action == NA_FORWARD) {
      EXPECT_EQ(kTransitionExplicitForwardBack, transition_type)
          << "nav=" << nav_ << " url=" << url;
    }

    got_before_resource_load_[nav_].yes();

    if (url == item.target)
      got_correct_target_[nav_].yes();

    return RV_CONTINUE;
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (isLoading)
      return;

    const NavListItem& item = kHNavList[nav_];

    got_loading_state_change_[nav_].yes();

    if (item.can_go_back == canGoBack)
      got_correct_can_go_back_[nav_].yes();
    if (item.can_go_forward == canGoForward)
      got_correct_can_go_forward_[nav_].yes();

    load_state_change_loaded_confirmation_ = true;
    RunNextNavIfReady(browser);
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    if (browser->IsPopup() || !frame->IsMain())
      return;

    const NavListItem& item = kHNavList[nav_];

    got_load_start_[nav_].yes();

    if (item.action == NA_LOAD) {
      EXPECT_EQ(kTransitionExplicitLoad, transition_type);
    } else if (item.action == NA_BACK || item.action == NA_FORWARD) {
      EXPECT_EQ(kTransitionExplicitForwardBack, transition_type);
    }

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
    visitor = nullptr;

    std::string url1 = browser->GetMainFrame()->GetURL();
    std::string url2 = frame->GetURL();
    if (url1 == item.target && url2 == item.target)
      got_correct_load_end_url_[nav_].yes();

    load_end_confirmation_ = true;
    RunNextNavIfReady(browser);
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
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
  bool load_state_change_loaded_confirmation_;
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

  IMPLEMENT_REFCOUNTING(HistoryNavTestHandler);
};

}  // namespace

// Verify history navigation.
TEST(NavigationTest, History) {
  CefRefPtr<HistoryNavTestHandler> handler = new HistoryNavTestHandler();
  handler->ExecuteTest();

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
    }
  }

  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kDynIfrNav1[] = "http://tests-dynframe/nav1.html";
const char kDynIfrNav2[] = "http://tests-dynframe/nav2.html";

// Browser side.
class HistoryDynamicIFramesNavTestHandler : public TestHandler {
 public:
  HistoryDynamicIFramesNavTestHandler() : nav_(-1) {}

  void RunTest() override {
    // Add the resources that we will navigate to/from.
    AddResource(kDynIfrNav1,
                "<html>"
                " <head>"
                "  <title>Nav1</title>"
                "  <script language='javascript'>"
                "    function onload() {"
                "      fr = Math.floor(Math.random() * 10);"
                "      if(fr == 0) "
                "        fr = 1;"
                "      console.log('fr=' + fr);"
                "      for(i = 1; i <= fr; i++) {"
                "        try {"
                "          var n = 'DYN_' + Math.floor(Math.random() * 10000);"
                "  "
                "          d = document.createElement('div');"
                "          d.id = 'sf' + i; "
                "          d.innerText = n; "
                "          document.body.appendChild(d); "
                " "
                "          f = document.createElement('iframe'); "
                "          f.id = 'f_' + i; "
                "          f.name = n; "
                "          f.src = 'nav2.html'; "
                "          document.body.appendChild(f); "
                "        } catch(e) { "
                "          console.log('frame[' + i + ']: ' + e); "
                "        } "
                "      } "
                "    } "
                "  </script> "
                " </head> "
                " <body onload='onload();'> "
                "  Nav1 "
                " </body> "
                "</html>",
                "text/html");
    AddResource(
        kDynIfrNav2,
        "<html><head><title>Nav2</title></head><body>Nav2</body></html>",
        "text/html");

    // Create the browser.
    CreateBrowser(CefString());

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void RunNav(CefRefPtr<CefBrowser> browser) {
    EXPECT_LE(nav_, 3);
    EXPECT_FALSE(got_load_start_[nav_]);
    EXPECT_FALSE(got_load_end_[nav_]);

    if (nav_ == 0) {
      browser->GetMainFrame()->LoadURL(kDynIfrNav1);
    } else if (nav_ == 1) {
      browser->GetMainFrame()->LoadURL(kDynIfrNav2);
    } else if (nav_ == 2) {
      browser->GoBack();
    } else if (nav_ == 3) {
      browser->Reload();
    }
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);

    nav_ = 0;
    RunNav(browser);
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    if (!frame->IsMain())
      return;
    got_load_start_[nav_].yes();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (!frame->IsMain())
      return;
    CefString url = browser->GetMainFrame()->GetURL();
    got_load_end_[nav_].yes();

    if (nav_ == 3) {
      EXPECT_STREQ(url.ToString().c_str(), kDynIfrNav1);
      DestroyTest();
      return;
    }

    nav_++;
    RunNav(browser);
  }

  int nav_;
  TrackCallback got_load_start_[4];
  TrackCallback got_load_end_[4];

  IMPLEMENT_REFCOUNTING(HistoryDynamicIFramesNavTestHandler);
};

}  // namespace

// Verify history navigation of pages containing dynamically created iframes.
// See issue #2022 for background.
TEST(NavigationTest, HistoryDynamicIFrames) {
  CefRefPtr<HistoryDynamicIFramesNavTestHandler> handler =
      new HistoryDynamicIFramesNavTestHandler();
  handler->ExecuteTest();

  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(handler->got_load_start_[i]);
    EXPECT_TRUE(handler->got_load_end_[i]);
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

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

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

    handle_request = true;

    if (status_ != 0) {
      // Continue request.
      return true;
    }

    // Cancel request.
    g_got_invalid_request = true;
    return false;
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

  void Cancel() override { EXPECT_TRUE(CefCurrentlyOn(TID_IO)); }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    bytes_read = 0;
    bool has_data = false;

    size_t size = content_.size();
    if (offset_ < size) {
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(size - offset_));
      memcpy(data_out, content_.c_str() + offset_, transfer_size);
      offset_ += transfer_size;

      bytes_read = transfer_size;
      has_data = true;
    }

    return has_data;
  }

 protected:
  std::string content_;
  size_t offset_;
  int status_;
  std::string location_;

  IMPLEMENT_REFCOUNTING(RedirectSchemeHandler);
  DISALLOW_COPY_AND_ASSIGN(RedirectSchemeHandler);
};

class RedirectSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  RedirectSchemeHandlerFactory() {
    g_got_nav1_request = false;
    g_got_nav3_request = false;
    g_got_nav4_request = false;
    g_got_invalid_request = false;
  }

  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    return new RedirectSchemeHandler();
  }

  IMPLEMENT_REFCOUNTING(RedirectSchemeHandlerFactory);
  DISALLOW_COPY_AND_ASSIGN(RedirectSchemeHandlerFactory);
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

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return RV_CANCEL;
    }

    // Should be called for all but the second URL.
    std::string url = request->GetURL();

    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    EXPECT_EQ(kTransitionExplicitLoad, request->GetTransitionType());

    if (url == kRNav1) {
      got_nav1_before_resource_load_.yes();
    } else if (url == kRNav3) {
      got_nav3_before_resource_load_.yes();
    } else if (url == kRNav4) {
      got_nav4_before_resource_load_.yes();
    } else {
      got_invalid_before_resource_load_.yes();
    }

    return RV_CONTINUE;
  }

  void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefResponse> response,
                          CefString& new_url) override {
    // Should be called for each redirected URL.

    const std::string& old_url = request->GetURL();
    if (old_url == kRNav1 && new_url == kRNav2) {
      // Called due to the nav1 redirect response.
      got_nav1_redirect_.yes();

      EXPECT_EQ(302, response->GetStatus());
      EXPECT_STREQ("Found", response->GetStatusText().ToString().c_str());
      EXPECT_STREQ("", response->GetMimeType().ToString().c_str());
      EXPECT_STREQ(kRNav2,
                   response->GetHeaderByName("Location").ToString().c_str());

      // Change the redirect to the 3rd URL.
      new_url = kRNav3;
    } else if (old_url == kRNav1 && new_url == kRNav3) {
      // Called due to the redirect change above.
      got_nav2_redirect_.yes();

      EXPECT_EQ(307, response->GetStatus());
      EXPECT_STREQ("Internal Redirect",
                   response->GetStatusText().ToString().c_str());
      EXPECT_TRUE(response->GetMimeType().empty());
      EXPECT_STREQ(kRNav3,
                   response->GetHeaderByName("Location").ToString().c_str());
    } else if (old_url == kRNav3 && new_url == kRNav4) {
      // Called due to the nav3 redirect response.
      got_nav3_redirect_.yes();

      EXPECT_EQ(307, response->GetStatus());
      EXPECT_STREQ("Temporary Redirect",
                   response->GetStatusText().ToString().c_str());
      EXPECT_STREQ("", response->GetMimeType().ToString().c_str());
    } else {
      got_invalid_redirect_.yes();
    }
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    // Should only be called for the final loaded URL.
    std::string url = frame->GetURL();

    EXPECT_EQ(kTransitionExplicitLoad, transition_type);

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

  IMPLEMENT_REFCOUNTING(RedirectTestHandler);
};

// Like above but destroy the WebContents while the redirect is in-progress.
class RedirectDestroyTestHandler : public TestHandler {
 public:
  RedirectDestroyTestHandler() {}

  void RunTest() override {
    // Create the browser.
    CreateBrowser(kRNav1);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefResponse> response,
                          CefString& new_url) override {
    const std::string& old_url = request->GetURL();
    if (old_url == kRNav1 && new_url == kRNav2) {
      // Called due to the nav1 redirect response.
      got_nav1_redirect_.yes();

      new_url = "about:blank";

      // Destroy the test (and the underlying WebContents) while the redirect
      // is still pending.
      DestroyTest();
    }
  }

  TrackCallback got_nav1_redirect_;

  IMPLEMENT_REFCOUNTING(RedirectDestroyTestHandler);
};

}  // namespace

// Verify frame names and identifiers.
TEST(NavigationTest, Redirect) {
  CefRegisterSchemeHandlerFactory("http", "tests",
                                  new RedirectSchemeHandlerFactory());
  WaitForIOThread();

  CefRefPtr<RedirectTestHandler> handler = new RedirectTestHandler();
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
  ASSERT_FALSE(handler->got_nav2_redirect_);
  ASSERT_TRUE(handler->got_nav3_redirect_);
  ASSERT_FALSE(handler->got_invalid_redirect_);
  ASSERT_TRUE(g_got_nav1_request);
  ASSERT_TRUE(g_got_nav3_request);
  ASSERT_TRUE(g_got_nav4_request);
  ASSERT_FALSE(g_got_invalid_request);

  ReleaseAndWaitForDestructor(handler);
}

// Verify that destroying the WebContents while the redirect is in-progress does
// not result in a crash.
TEST(NavigationTest, RedirectDestroy) {
  CefRegisterSchemeHandlerFactory("http", "tests",
                                  new RedirectSchemeHandlerFactory());
  WaitForIOThread();

  CefRefPtr<RedirectDestroyTestHandler> handler =
      new RedirectDestroyTestHandler();
  handler->ExecuteTest();

  CefClearSchemeHandlerFactories();
  WaitForIOThread();

  ASSERT_TRUE(handler->got_nav1_redirect_);
  ASSERT_TRUE(g_got_nav1_request);
  ASSERT_FALSE(g_got_nav3_request);
  ASSERT_FALSE(g_got_nav4_request);
  ASSERT_FALSE(g_got_invalid_request);

  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char KONav1[] = "http://tests-onav.com/nav1.html";
const char KONav2[] = "http://tests-onav.com/nav2.html";
const char kOrderNavMsg[] = "NavigationTest.OrderNav";
const char kOrderNavClosedMsg[] = "NavigationTest.OrderNavClosed";
const char kOrderNavTestCmdKey[] = "nav-order-test";

class OrderNavLoadState {
 public:
  OrderNavLoadState(bool is_popup, bool browser_side)
      : is_popup_(is_popup), browser_side_(browser_side) {}

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) {
    if (isLoading) {
      EXPECT_TRUE(Verify(false, false, false, false));

      got_loading_state_start_.yes();
    } else {
      EXPECT_TRUE(Verify(true, false, true, true));

      got_loading_state_end_.yes();
    }
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) {
    EXPECT_TRUE(Verify(true, false, false, false));

    got_load_start_.yes();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) {
    EXPECT_TRUE(Verify(true, false, true, false));

    got_load_end_.yes();
  }

  bool IsStarted() const {
    return got_loading_state_start_ || got_loading_state_end_ ||
           got_load_start_ || got_load_end_;
  }

  bool IsDone() const {
    return got_loading_state_start_ && got_loading_state_end_ &&
           got_load_start_ && got_load_end_;
  }

  bool Verify(bool got_loading_state_start,
              bool got_loading_state_end,
              bool got_load_start,
              bool got_load_end) const {
    EXPECT_EQ(got_loading_state_start, got_loading_state_start_)
        << "Popup: " << is_popup_ << "; Browser Side: " << browser_side_;
    EXPECT_EQ(got_loading_state_end, got_loading_state_end_)
        << "Popup: " << is_popup_ << "; Browser Side: " << browser_side_;
    EXPECT_EQ(got_load_start, got_load_start_)
        << "Popup: " << is_popup_ << "; Browser Side: " << browser_side_;
    EXPECT_EQ(got_load_end, got_load_end_)
        << "Popup: " << is_popup_ << "; Browser Side: " << browser_side_;

    return got_loading_state_start == got_loading_state_start_ &&
           got_loading_state_end == got_loading_state_end_ &&
           got_load_start == got_load_start_ && got_load_end == got_load_end_;
  }

 private:
  bool is_popup_;
  bool browser_side_;

  TrackCallback got_loading_state_start_;
  TrackCallback got_loading_state_end_;
  TrackCallback got_load_start_;
  TrackCallback got_load_end_;
};

// Renderer side.
class OrderNavRendererTest : public ClientAppRenderer::Delegate,
                             public CefLoadHandler {
 public:
  OrderNavRendererTest()
      : run_test_(false),
        browser_id_main_(0),
        browser_id_popup_(0),
        state_main_(false, false),
        state_popup_(true, false) {}

  void OnWebKitInitialized(CefRefPtr<ClientAppRenderer> app) override {
    EXPECT_FALSE(got_webkit_initialized_);

    got_webkit_initialized_.yes();
  }

  void OnBrowserCreated(CefRefPtr<ClientAppRenderer> app,
                        CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefDictionaryValue> extra_info) override {
    run_test_ = extra_info && extra_info->HasKey(kOrderNavTestCmdKey);
    if (!run_test_)
      return;

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

  void OnBrowserDestroyed(CefRefPtr<ClientAppRenderer> app,
                          CefRefPtr<CefBrowser> browser) override {
    if (!run_test_)
      return;

    EXPECT_TRUE(got_webkit_initialized_);

    if (browser->IsPopup()) {
      EXPECT_TRUE(got_browser_created_popup_);
      EXPECT_FALSE(got_browser_destroyed_popup_);
      EXPECT_TRUE(state_popup_.IsDone());

      got_browser_destroyed_popup_.yes();
      EXPECT_EQ(browser_id_popup_, browser->GetIdentifier());
      EXPECT_GT(browser->GetIdentifier(), 0);

      // Use |browser_main_| to send the message otherwise it will fail.
      SendTestResults(browser_main_, browser_main_->GetMainFrame(),
                      kOrderNavClosedMsg);
    } else {
      EXPECT_TRUE(got_browser_created_main_);
      EXPECT_FALSE(got_browser_destroyed_main_);
      EXPECT_TRUE(state_main_.IsDone());

      got_browser_destroyed_main_.yes();
      EXPECT_EQ(browser_id_main_, browser->GetIdentifier());
      EXPECT_GT(browser->GetIdentifier(), 0);

      browser_main_ = nullptr;
    }
  }

  CefRefPtr<CefLoadHandler> GetLoadHandler(
      CefRefPtr<ClientAppRenderer> app) override {
    if (!run_test_)
      return nullptr;

    return this;
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
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
      SendTestResultsIfDone(browser, browser->GetMainFrame());
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
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

    SendTestResultsIfDone(browser, frame);
  }

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override {
    ADD_FAILURE() << "renderer OnLoadError url: " << failedUrl.ToString()
                  << " error: " << errorCode;
  }

 protected:
  void SendTestResultsIfDone(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame) {
    bool done = false;
    if (browser->IsPopup())
      done = state_popup_.IsDone();
    else
      done = state_main_.IsDone();

    if (done)
      SendTestResults(browser, frame, kOrderNavMsg);
  }

  // Send the test results.
  void SendTestResults(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       const char* msg_name) {
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
    frame->SendProcessMessage(PID_BROWSER, return_msg);
  }

  bool run_test_;

  int browser_id_main_;
  int browser_id_popup_;
  CefRefPtr<CefBrowser> browser_main_;
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

  // Returns state that will be checked in the renderer process via
  // OrderNavRendererTest::OnBrowserCreated.
  CefRefPtr<CefDictionaryValue> GetExtraInfo() {
    CefRefPtr<CefDictionaryValue> extra_info = CefDictionaryValue::Create();
    extra_info->SetBool(kOrderNavTestCmdKey, true);
    return extra_info;
  }

  void RunTest() override {
    // Add the resources that we will navigate to/from.
    AddResource(KONav1, "<html>Nav1</html>", "text/html");
    AddResource(KONav2, "<html>Nav2</html>", "text/html");

    // Create the browser.
    CreateBrowser(KONav1, nullptr, GetExtraInfo());

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
      CloseBrowser(browser_popup_, false);
    }
  }

  bool OnBeforePopup(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      const CefString& target_url,
      const CefString& target_frame_name,
      CefLifeSpanHandler::WindowOpenDisposition target_disposition,
      bool user_gesture,
      const CefPopupFeatures& popupFeatures,
      CefWindowInfo& windowInfo,
      CefRefPtr<CefClient>& client,
      CefBrowserSettings& settings,
      CefRefPtr<CefDictionaryValue>& extra_info,
      bool* no_javascript_access) override {
    extra_info = GetExtraInfo();
    return false;
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
                      bool user_gesture,
                      bool is_redirect) override {
    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());

    if (browser->IsPopup()) {
      EXPECT_EQ(TT_LINK, request->GetTransitionType());
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_popup_, browser->GetIdentifier());
      got_before_browse_popup_.yes();
    } else {
      EXPECT_EQ(kTransitionExplicitLoad, request->GetTransitionType());
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

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return RV_CANCEL;
    }

    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());

    if (browser->IsPopup()) {
      EXPECT_EQ(TT_LINK, request->GetTransitionType());
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_popup_, browser->GetIdentifier());
    } else {
      EXPECT_EQ(kTransitionExplicitLoad, request->GetTransitionType());
      EXPECT_GT(browser->GetIdentifier(), 0);
      EXPECT_EQ(browser_id_main_, browser->GetIdentifier());
    }

    return RV_CONTINUE;
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
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    if (browser->IsPopup()) {
      EXPECT_EQ(TT_LINK, transition_type);
      state_popup_.OnLoadStart(browser, frame);
    } else {
      EXPECT_EQ(kTransitionExplicitLoad, transition_type);
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

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override {
    ADD_FAILURE() << "browser OnLoadError url: " << failedUrl.ToString()
                  << " error: " << errorCode;
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
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
        browser_popup_ = nullptr;
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

    EXPECT_TRUE(state_main_.Verify(true, true, true, true));
    EXPECT_TRUE(state_popup_.Verify(true, true, true, true));

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

  IMPLEMENT_REFCOUNTING(OrderNavTestHandler);
};

}  // namespace

// Verify the order of navigation-related callbacks.
TEST(NavigationTest, Order) {
  CefRefPtr<OrderNavTestHandler> handler = new OrderNavTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kLoadNav1[] = "http://tests-conav1.com/nav1.html";
const char kLoadNavSameOrigin2[] = "http://tests-conav1.com/nav2.html";
const char kLoadNavCrossOrigin2[] = "http://tests-conav2.com/nav2.html";
const char kLoadNavMsg[] = "NavigationTest.LoadNav";
const char kLoadNavTestCmdKey[] = "nav-load-test";

// Renderer side.
class LoadNavRendererTest : public ClientAppRenderer::Delegate,
                            public CefLoadHandler {
 public:
  LoadNavRendererTest() : run_test_(false), browser_id_(0), load_ct_(0) {}
  ~LoadNavRendererTest() override { EXPECT_EQ(0, browser_id_); }

  void OnBrowserCreated(CefRefPtr<ClientAppRenderer> app,
                        CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefDictionaryValue> extra_info) override {
    run_test_ = extra_info && extra_info->HasKey(kLoadNavTestCmdKey);
    if (!run_test_)
      return;

    EXPECT_EQ(0, browser_id_);
    browser_id_ = browser->GetIdentifier();
    EXPECT_GT(browser_id_, 0);
    got_browser_created_.yes();
  }

  void OnBrowserDestroyed(CefRefPtr<ClientAppRenderer> app,
                          CefRefPtr<CefBrowser> browser) override {
    if (!run_test_)
      return;

    EXPECT_TRUE(got_browser_created_);
    EXPECT_TRUE(got_loading_state_end_);

    EXPECT_EQ(browser_id_, browser->GetIdentifier());
    browser_id_ = 0;
  }

  CefRefPtr<CefLoadHandler> GetLoadHandler(
      CefRefPtr<ClientAppRenderer> app) override {
    if (!run_test_)
      return nullptr;

    return this;
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (!isLoading) {
      EXPECT_TRUE(got_browser_created_);

      got_loading_state_end_.yes();

      EXPECT_EQ(browser_id_, browser->GetIdentifier());

      load_ct_++;
      SendTestResults(browser, browser->GetMainFrame());
    }
  }

 protected:
  // Send the test results.
  void SendTestResults(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame) {
    // Check if the test has failed.
    bool result = !TestFailed();

    // Return the result to the browser process.
    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(kLoadNavMsg);
    CefRefPtr<CefListValue> args = return_msg->GetArgumentList();
    EXPECT_TRUE(args.get());
    EXPECT_TRUE(args->SetBool(0, result));
    EXPECT_TRUE(args->SetInt(1, browser->GetIdentifier()));
    EXPECT_TRUE(args->SetInt(2, load_ct_));
    frame->SendProcessMessage(PID_BROWSER, return_msg);
  }

  bool run_test_;

  int browser_id_;
  int load_ct_;
  TrackCallback got_browser_created_;
  TrackCallback got_loading_state_end_;

  IMPLEMENT_REFCOUNTING(LoadNavRendererTest);
};

// Browser side.
class LoadNavTestHandler : public TestHandler {
 public:
  enum TestMode {
    LOAD,
    LEFT_CLICK,
    MIDDLE_CLICK,
    CTRL_LEFT_CLICK,
  };

  LoadNavTestHandler(TestMode mode,
                     bool same_origin,
                     bool cancel_in_open_url = false)
      : mode_(mode),
        same_origin_(same_origin),
        cancel_in_open_url_(cancel_in_open_url),
        browser_id_current_(0),
        renderer_load_ct_(0) {}

  std::string GetURL2() const {
    return same_origin_ ? kLoadNavSameOrigin2 : kLoadNavCrossOrigin2;
  }

  bool ExpectOpenURL() const {
    return mode_ == MIDDLE_CLICK || mode_ == CTRL_LEFT_CLICK;
  }

  void RunTest() override {
    const std::string& url2 = GetURL2();
    std::string link;
    if (mode_ != LOAD)
      link = "<a href=\"" + url2 + "\">CLICK ME</a>";

    // Add the resources that we will navigate to/from.
    AddResource(kLoadNav1,
                "<html><body><h1>" + link + "Nav1</h1></body></html>",
                "text/html");
    AddResource(url2, "<html>Nav2</html>", "text/html");

    CefRefPtr<CefDictionaryValue> extra_info = CefDictionaryValue::Create();
    extra_info->SetBool(kLoadNavTestCmdKey, true);

    // Create the browser.
    CreateBrowser(kLoadNav1, nullptr, extra_info);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void ContinueIfReady(CefRefPtr<CefBrowser> browser) {
    if (!got_message_ || !got_load_end_)
      return;

    std::string url = browser->GetMainFrame()->GetURL();
    if (url == kLoadNav1) {
      // Verify the behavior of the previous load.
      EXPECT_TRUE(got_before_browse_);
      EXPECT_TRUE(got_before_resource_load_);
      EXPECT_TRUE(got_load_start_);
      EXPECT_TRUE(got_load_end_);
      EXPECT_FALSE(got_open_url_from_tab_);

      got_before_browse_.reset();
      got_before_resource_load_.reset();
      got_load_start_.reset();
      got_load_end_.reset();
      got_message_.reset();

      EXPECT_EQ(1, renderer_load_ct_);

      // Load the next url.
      if (mode_ == LOAD) {
        browser->GetMainFrame()->LoadURL(GetURL2());
      } else {
        // Navigate to the URL by clicking a link.
        CefMouseEvent mouse_event;
        mouse_event.x = 20;
        mouse_event.y = 20;
#if defined(OS_MAC)
        // Use cmd instead of ctrl on OS X.
        mouse_event.modifiers =
            (mode_ == CTRL_LEFT_CLICK ? EVENTFLAG_COMMAND_DOWN : 0);
#else
        mouse_event.modifiers =
            (mode_ == CTRL_LEFT_CLICK ? EVENTFLAG_CONTROL_DOWN : 0);
#endif

        cef_mouse_button_type_t button_type =
            (mode_ == MIDDLE_CLICK ? MBT_MIDDLE : MBT_LEFT);
        browser->GetHost()->SendMouseClickEvent(mouse_event, button_type, false,
                                                1);
        browser->GetHost()->SendMouseClickEvent(mouse_event, button_type, true,
                                                1);
      }

      if (cancel_in_open_url_) {
        // The next navigation should not occur. Therefore call DestroyTest()
        // after a reasonable timeout.
        CefPostDelayedTask(
            TID_UI, base::BindOnce(&LoadNavTestHandler::DestroyTest, this),
            500);
      }
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

  cef_transition_type_t ExpectedOpenURLTransitionType() const {
    if (mode_ != LEFT_CLICK && IsChromeRuntimeEnabled()) {
      // Because we triggered the navigation with LoadURL in OnOpenURLFromTab.
      return kTransitionExplicitLoad;
    }
    return TT_LINK;
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override {
    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    if (mode_ == LOAD || request->GetURL() == kLoadNav1) {
      EXPECT_EQ(kTransitionExplicitLoad, request->GetTransitionType());
      if (IsChromeRuntimeEnabled()) {
        // With the Chrome runtime this is true on initial navigation via
        // chrome::AddTabAt() and also true for clicked links.
        EXPECT_TRUE(user_gesture);
      } else {
        EXPECT_FALSE(user_gesture);
      }
    } else {
      EXPECT_EQ(ExpectedOpenURLTransitionType(), request->GetTransitionType());

      if (mode_ == LEFT_CLICK || IsChromeRuntimeEnabled()) {
        EXPECT_TRUE(user_gesture);
      } else {
        EXPECT_FALSE(user_gesture);
      }
    }

    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());

    if (ExpectOpenURL() && request->GetURL() == GetURL2()) {
      // OnOpenURLFromTab should be called first for the file URL navigation.
      EXPECT_TRUE(got_open_url_from_tab_);
    } else {
      EXPECT_FALSE(got_open_url_from_tab_);
    }

    got_before_browse_.yes();

    return false;
  }

  bool OnOpenURLFromTab(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        const CefString& target_url,
                        cef_window_open_disposition_t target_disposition,
                        bool user_gesture) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));

    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());

    // OnOpenURLFromTab should only be called for the file URL.
    EXPECT_STREQ(GetURL2().c_str(), target_url.ToString().c_str());

    if (mode_ == LOAD)
      EXPECT_FALSE(user_gesture);
    else
      EXPECT_TRUE(user_gesture);

    EXPECT_EQ(WOD_NEW_BACKGROUND_TAB, target_disposition);

    // OnOpenURLFromTab should be called before OnBeforeBrowse for the file URL.
    EXPECT_FALSE(got_before_browse_);

    got_open_url_from_tab_.yes();

    if (!cancel_in_open_url_ && IsChromeRuntimeEnabled()) {
      // The chrome runtime may create a new popup window, which is not the
      // behavior that this test expects. Instead, match the alloy runtime
      // behavior by navigating in the current window.
      browser->GetMainFrame()->LoadURL(target_url);
      return true;
    }

    return cancel_in_open_url_;
  }

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return RV_CANCEL;
    }

    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());

    const auto transition_type = request->GetTransitionType();
    if (mode_ == LOAD || request->GetURL() == kLoadNav1) {
      EXPECT_EQ(kTransitionExplicitLoad, transition_type);
    } else {
      EXPECT_EQ(ExpectedOpenURLTransitionType(), transition_type);
    }

    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());

    got_before_resource_load_.yes();

    return RV_CONTINUE;
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());

    if (mode_ == LOAD || frame->GetURL() == kLoadNav1) {
      EXPECT_EQ(kTransitionExplicitLoad, transition_type);
    } else {
      EXPECT_EQ(ExpectedOpenURLTransitionType(), transition_type);
    }

    got_load_start_.yes();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());

    got_load_end_.yes();
    ContinueIfReady(browser);
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    EXPECT_GT(browser_id_current_, 0);
    EXPECT_EQ(browser_id_current_, browser->GetIdentifier());

    const std::string& msg_name = message->GetName();
    if (msg_name == kLoadNavMsg) {
      // Test that the renderer side succeeded.
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      EXPECT_TRUE(args.get());
      EXPECT_TRUE(args->GetBool(0));

      EXPECT_EQ(browser_id_current_, args->GetInt(1));

      renderer_load_ct_ = args->GetInt(2);
      EXPECT_GE(renderer_load_ct_, 1);

      // Continue with the test.
      got_message_.yes();
      ContinueIfReady(browser);

      return true;
    }

    // Message not handled.
    return false;
  }

  void DestroyTest() override {
    if (cancel_in_open_url_) {
      EXPECT_FALSE(got_before_browse_);
      EXPECT_FALSE(got_before_resource_load_);
      EXPECT_FALSE(got_load_start_);
      EXPECT_FALSE(got_load_end_);
      EXPECT_FALSE(got_message_);

      // We should only navigate a single time if the 2nd load is canceled.
      EXPECT_EQ(1, renderer_load_ct_);
    } else {
      EXPECT_TRUE(got_before_browse_);
      EXPECT_TRUE(got_before_resource_load_);
      EXPECT_TRUE(got_load_start_);
      EXPECT_TRUE(got_load_end_);
      EXPECT_TRUE(got_message_);

      if (same_origin_) {
        // The renderer process should always be reused.
        EXPECT_EQ(2, renderer_load_ct_);
      } else {
        // Each renderer process is only used for a single navigation.
        EXPECT_EQ(1, renderer_load_ct_);
      }
    }

    if (ExpectOpenURL())
      EXPECT_TRUE(got_open_url_from_tab_);
    else
      EXPECT_FALSE(got_open_url_from_tab_);

    TestHandler::DestroyTest();
  }

 protected:
  const TestMode mode_;
  const bool same_origin_;
  const bool cancel_in_open_url_;

  int browser_id_current_;
  int renderer_load_ct_;

  TrackCallback got_before_browse_;
  TrackCallback got_open_url_from_tab_;
  TrackCallback got_before_resource_load_;
  TrackCallback got_load_start_;
  TrackCallback got_load_end_;
  TrackCallback got_message_;

  IMPLEMENT_REFCOUNTING(LoadNavTestHandler);
};

}  // namespace

// Verify navigation-related callbacks when browsing same-origin via LoadURL().
TEST(NavigationTest, LoadSameOriginLoadURL) {
  CefRefPtr<LoadNavTestHandler> handler =
      new LoadNavTestHandler(LoadNavTestHandler::LOAD, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify navigation-related callbacks when browsing same-origin via left-click.
TEST(NavigationTest, LoadSameOriginLeftClick) {
  CefRefPtr<LoadNavTestHandler> handler =
      new LoadNavTestHandler(LoadNavTestHandler::LEFT_CLICK, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify navigation-related callbacks when browsing same-origin via middle-
// click.
TEST(NavigationTest, LoadSameOriginMiddleClick) {
  CefRefPtr<LoadNavTestHandler> handler =
      new LoadNavTestHandler(LoadNavTestHandler::MIDDLE_CLICK, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Same as above but cancel the 2nd navigation in OnOpenURLFromTab.
TEST(NavigationTest, LoadSameOriginMiddleClickCancel) {
  CefRefPtr<LoadNavTestHandler> handler =
      new LoadNavTestHandler(LoadNavTestHandler::MIDDLE_CLICK, true, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify navigation-related callbacks when browsing same-origin via ctrl+left-
// click.
TEST(NavigationTest, LoadSameOriginCtrlLeftClick) {
  CefRefPtr<LoadNavTestHandler> handler =
      new LoadNavTestHandler(LoadNavTestHandler::CTRL_LEFT_CLICK, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Same as above but cancel the 2nd navigation in OnOpenURLFromTab.
TEST(NavigationTest, LoadSameOriginCtrlLeftClickCancel) {
  CefRefPtr<LoadNavTestHandler> handler =
      new LoadNavTestHandler(LoadNavTestHandler::CTRL_LEFT_CLICK, true, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify navigation-related callbacks when browsing cross-origin via LoadURL().
TEST(NavigationTest, LoadCrossOriginLoadURL) {
  CefRefPtr<LoadNavTestHandler> handler =
      new LoadNavTestHandler(LoadNavTestHandler::LOAD, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify navigation-related callbacks when browsing cross-origin via left-
// click.
TEST(NavigationTest, LoadCrossOriginLeftClick) {
  CefRefPtr<LoadNavTestHandler> handler =
      new LoadNavTestHandler(LoadNavTestHandler::LEFT_CLICK, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify navigation-related callbacks when browsing cross-origin via middle-
// click.
TEST(NavigationTest, LoadCrossOriginMiddleClick) {
  CefRefPtr<LoadNavTestHandler> handler =
      new LoadNavTestHandler(LoadNavTestHandler::MIDDLE_CLICK, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Same as above but cancel the 2nd navigation in OnOpenURLFromTab.
TEST(NavigationTest, LoadCrossOriginMiddleClickCancel) {
  CefRefPtr<LoadNavTestHandler> handler =
      new LoadNavTestHandler(LoadNavTestHandler::MIDDLE_CLICK, false, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify navigation-related callbacks when browsing cross-origin via ctrl+left-
// click.
TEST(NavigationTest, LoadCrossOriginCtrlLeftClick) {
  CefRefPtr<LoadNavTestHandler> handler =
      new LoadNavTestHandler(LoadNavTestHandler::CTRL_LEFT_CLICK, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Same as above but cancel the 2nd navigation in OnOpenURLFromTab.
TEST(NavigationTest, LoadCrossOriginCtrlLeftClickCancel) {
  CefRefPtr<LoadNavTestHandler> handler =
      new LoadNavTestHandler(LoadNavTestHandler::CTRL_LEFT_CLICK, false, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kSimultPopupMainUrl[] = "http://www.tests-sp.com/main.html";
const char kSimultPopupPopupUrl[] = "http://www.tests-sp.com/popup";
const size_t kSimultPopupCount = 5U;

// Test multiple popups simultaniously.
class PopupSimultaneousTestHandler : public TestHandler {
 public:
  explicit PopupSimultaneousTestHandler(bool same_url)
      : same_url_(same_url),
        before_popup_ct_(0U),
        after_created_ct_(0U),
        before_close_ct_(0U) {}

  void RunTest() override {
    std::string main_html = "<html><script>\n";
    for (size_t i = 0; i < kSimultPopupCount; ++i) {
      if (same_url_) {
        popup_url_[i] = std::string(kSimultPopupPopupUrl) + ".html";
      } else {
        std::stringstream ss;
        ss << kSimultPopupPopupUrl << i << ".html";
        popup_url_[i] = ss.str();
      }
      main_html += "window.open('" + popup_url_[i] + "');\n";
      AddResource(popup_url_[i], "<html>Popup " + popup_url_[i] + "</html>",
                  "text/html");
    }
    main_html += "</script></html>";

    AddResource(kSimultPopupMainUrl, main_html, "text/html");

    // Create the browser.
    CreateBrowser(kSimultPopupMainUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     const CefString& target_url,
                     const CefString& target_frame_name,
                     cef_window_open_disposition_t target_disposition,
                     bool user_gesture,
                     const CefPopupFeatures& popupFeatures,
                     CefWindowInfo& windowInfo,
                     CefRefPtr<CefClient>& client,
                     CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue>& extra_info,
                     bool* no_javascript_access) override {
    const std::string& url = target_url;
    EXPECT_LT(before_popup_ct_, kSimultPopupCount);
    EXPECT_STREQ(popup_url_[before_popup_ct_].c_str(), url.c_str())
        << before_popup_ct_;
    before_popup_ct_++;
    return false;
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);

    if (browser->IsPopup()) {
      EXPECT_LT(after_created_ct_, kSimultPopupCount);
      browser_id_[after_created_ct_] = browser->GetIdentifier();
      after_created_ct_++;
    }
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (isLoading)
      return;

    if (browser->IsPopup()) {
      const std::string& url = browser->GetMainFrame()->GetURL();
      for (size_t i = 0; i < kSimultPopupCount; ++i) {
        if (browser->GetIdentifier() == browser_id_[i]) {
          EXPECT_STREQ(popup_url_[i].c_str(), url.c_str()) << i;

          got_loading_state_change_[i].yes();
          CloseBrowser(browser, true);
          return;
        }
      }
      EXPECT_FALSE(true);  // Not reached.
    }
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnBeforeClose(browser);

    if (browser->IsPopup()) {
      const std::string& url = browser->GetMainFrame()->GetURL();
      for (size_t i = 0; i < kSimultPopupCount; ++i) {
        if (browser->GetIdentifier() == browser_id_[i]) {
          EXPECT_TRUE(got_loading_state_change_[i]);
          EXPECT_STREQ(popup_url_[i].c_str(), url.c_str()) << i;

          got_before_close_[i].yes();

          if (++before_close_ct_ == kSimultPopupCount)
            DestroyTest();
          return;
        }
      }
      EXPECT_FALSE(true);  // Not reached.
    }
  }

 private:
  void DestroyTest() override {
    EXPECT_EQ(kSimultPopupCount, before_popup_ct_);
    EXPECT_EQ(kSimultPopupCount, after_created_ct_);
    EXPECT_EQ(kSimultPopupCount, before_close_ct_);

    for (size_t i = 0; i < kSimultPopupCount; ++i) {
      EXPECT_GT(browser_id_[i], 0) << i;
      EXPECT_TRUE(got_loading_state_change_[i]) << i;
      EXPECT_TRUE(got_before_close_[i]) << i;
    }

    TestHandler::DestroyTest();
  }

  const bool same_url_;
  std::string popup_url_[kSimultPopupCount];
  size_t before_popup_ct_;
  int browser_id_[kSimultPopupCount];
  size_t after_created_ct_;
  TrackCallback got_loading_state_change_[kSimultPopupCount];
  TrackCallback got_before_close_[kSimultPopupCount];
  size_t before_close_ct_;

  IMPLEMENT_REFCOUNTING(PopupSimultaneousTestHandler);
};

}  // namespace

// Test simultaneous popups with different URLs.
TEST(NavigationTest, PopupSimultaneousDifferentUrl) {
  CefRefPtr<PopupSimultaneousTestHandler> handler =
      new PopupSimultaneousTestHandler(false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test simultaneous popups with the same URL.
TEST(NavigationTest, PopupSimultaneousSameUrl) {
  CefRefPtr<PopupSimultaneousTestHandler> handler =
      new PopupSimultaneousTestHandler(true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kPopupJSOpenMainUrl[] = "http://www.tests-pjso.com/main.html";
const char kPopupJSOpenPopupUrl[] = "http://www.tests-pjso.com/popup.html";

// Test a popup where the URL is a JavaScript URI that opens another popup.
class PopupJSWindowOpenTestHandler : public TestHandler {
 public:
  PopupJSWindowOpenTestHandler()
      : before_popup_ct_(0U),
        after_created_ct_(0U),
        load_end_ct_(0U),
        before_close_ct_(0U) {}

  void RunTest() override {
    AddResource(kPopupJSOpenMainUrl, "<html>Main</html>", "text/html");
    AddResource(kPopupJSOpenPopupUrl, "<html>Popup</html>", "text/html");

    // Create the browser.
    CreateBrowser(kPopupJSOpenMainUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     const CefString& target_url,
                     const CefString& target_frame_name,
                     cef_window_open_disposition_t target_disposition,
                     bool user_gesture,
                     const CefPopupFeatures& popupFeatures,
                     CefWindowInfo& windowInfo,
                     CefRefPtr<CefClient>& client,
                     CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue>& extra_info,
                     bool* no_javascript_access) override {
    before_popup_ct_++;
    return false;
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);

    if (browser->IsPopup()) {
      after_created_ct_++;
      if (!popup1_)
        popup1_ = browser;
      else if (!popup2_)
        popup2_ = browser;
      else
        ADD_FAILURE();
    }
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (isLoading)
      return;

    if (browser->IsPopup()) {
      const std::string& url = browser->GetMainFrame()->GetURL();
      if (url == kPopupJSOpenPopupUrl) {
        EXPECT_TRUE(browser->IsSame(popup2_));
        popup2_ = nullptr;

        // OnLoadingStateChange is not currently called for browser-side
        // navigations of empty popups. See https://crbug.com/789252.
        // Explicitly close the empty popup here as a workaround.
        CloseBrowser(popup1_, true);
        popup1_ = nullptr;
      } else {
        // Empty popup.
        EXPECT_TRUE(url.empty());
        EXPECT_TRUE(browser->IsSame(popup1_));
        popup1_ = nullptr;
      }

      load_end_ct_++;
      CloseBrowser(browser, true);
    } else if (browser->GetMainFrame()->GetURL() == kPopupJSOpenMainUrl) {
      // Load the problematic JS URI.
      // This will result in 2 popups being created:
      // - An empty popup
      // - A popup that loads kPopupJSOpenPopupUrl
      browser->GetMainFrame()->LoadURL(
          "javascript:window.open(\"javascript:window.open('" +
          std::string(kPopupJSOpenPopupUrl) + "')\")");
    }
  }

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override {
    ADD_FAILURE() << "OnLoadError url: " << failedUrl.ToString()
                  << " error: " << errorCode;
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnBeforeClose(browser);

    before_close_ct_++;
    if (before_close_ct_ == 2U)
      DestroyTest();
  }

 private:
  void DestroyTest() override {
    EXPECT_EQ(2U, before_popup_ct_);
    EXPECT_EQ(2U, after_created_ct_);
    EXPECT_EQ(2U, before_close_ct_);

    // OnLoadingStateChange is not currently called for browser-side
    // navigations of empty popups. See https://crbug.com/789252.
    EXPECT_EQ(1U, load_end_ct_);

    TestHandler::DestroyTest();
  }

  CefRefPtr<CefBrowser> popup1_;
  CefRefPtr<CefBrowser> popup2_;

  size_t before_popup_ct_;
  size_t after_created_ct_;
  size_t load_end_ct_;
  size_t before_close_ct_;

  IMPLEMENT_REFCOUNTING(PopupJSWindowOpenTestHandler);
};

}  // namespace

// Test a popup where the URL is a JavaScript URI that opens another popup.
TEST(NavigationTest, PopupJSWindowOpen) {
  CefRefPtr<PopupJSWindowOpenTestHandler> handler =
      new PopupJSWindowOpenTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kPopupJSEmptyMainUrl[] = "http://www.tests-pjse.com/main.html";

// Test creation of a popup where the URL is empty.
class PopupJSWindowEmptyTestHandler : public TestHandler {
 public:
  PopupJSWindowEmptyTestHandler() {}

  void RunTest() override {
    AddResource(kPopupJSEmptyMainUrl, "<html>Main</html>", "text/html");

    // Create the browser.
    CreateBrowser(kPopupJSEmptyMainUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     const CefString& target_url,
                     const CefString& target_frame_name,
                     cef_window_open_disposition_t target_disposition,
                     bool user_gesture,
                     const CefPopupFeatures& popupFeatures,
                     CefWindowInfo& windowInfo,
                     CefRefPtr<CefClient>& client,
                     CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue>& extra_info,
                     bool* no_javascript_access) override {
    got_before_popup_.yes();
    return false;
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);

    if (browser->IsPopup()) {
      got_after_created_popup_.yes();
    }
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (isLoading)
      return;

    if (browser->IsPopup()) {
      got_load_end_popup_.yes();
      CloseBrowser(browser, true);
    } else {
      browser->GetMainFrame()->LoadURL("javascript:window.open('')");
    }
  }

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override {
    ADD_FAILURE() << "OnLoadError url: " << failedUrl.ToString()
                  << " error: " << errorCode;
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnBeforeClose(browser);

    if (browser->IsPopup()) {
      got_before_close_popup_.yes();
      DestroyTest();
    }
  }

 private:
  void DestroyTest() override {
    EXPECT_TRUE(got_before_popup_);
    EXPECT_TRUE(got_after_created_popup_);
    EXPECT_TRUE(got_load_end_popup_);
    EXPECT_TRUE(got_before_close_popup_);

    TestHandler::DestroyTest();
  }

  TrackCallback got_before_popup_;
  TrackCallback got_after_created_popup_;
  TrackCallback got_load_end_popup_;
  TrackCallback got_before_close_popup_;

  IMPLEMENT_REFCOUNTING(PopupJSWindowEmptyTestHandler);
};

}  // namespace

// Test creation of a popup where the URL is empty.
TEST(NavigationTest, PopupJSWindowEmpty) {
  CefRefPtr<PopupJSWindowEmptyTestHandler> handler =
      new PopupJSWindowEmptyTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kBrowseNavPageUrl[] = "http://tests-browsenav/nav.html";

// Browser side.
class BrowseNavTestHandler : public TestHandler {
 public:
  BrowseNavTestHandler(bool allow) : allow_(allow), destroyed_(false) {}

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
                      bool user_gesture,
                      bool is_redirect) override {
    const std::string& url = request->GetURL();
    EXPECT_STREQ(kBrowseNavPageUrl, url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    got_before_browse_.yes();

    return !allow_;
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
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

  IMPLEMENT_REFCOUNTING(BrowseNavTestHandler);
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

namespace {

const char kSameNavPageUrl[] = "http://tests-samenav/nav.html";

// Browser side.
class SameNavTestHandler : public TestHandler {
 public:
  SameNavTestHandler() : destroyed_(false), step_(0) {}

  void RunTest() override {
    AddResource(kSameNavPageUrl, "<html>Test</html>", "text/html");

    // Create the browser.
    expected_url_ = kSameNavPageUrl;
    CreateBrowser(kSameNavPageUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override {
    const std::string& url = request->GetURL();
    EXPECT_STREQ(expected_url_.c_str(), url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    got_before_browse_.yes();

    return false;
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    const std::string& url = frame->GetURL();
    EXPECT_STREQ(expected_url_.c_str(), url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    got_load_start_.yes();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    const std::string& url = frame->GetURL();
    EXPECT_STREQ(expected_url_.c_str(), url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    got_load_end_.yes();
    ContinueTestIfDone();
  }

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override {
    got_load_error_.yes();
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    const std::string& url = browser->GetMainFrame()->GetURL();
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());

    if (isLoading) {
      // Verify the previous URL.
      if (step_ == 0)
        EXPECT_TRUE(url.empty());
      else
        EXPECT_STREQ(kSameNavPageUrl, url.c_str());

      got_loading_state_changed_start_.yes();
    } else {
      EXPECT_STREQ(expected_url_.c_str(), url.c_str());

      got_loading_state_changed_end_.yes();
      ContinueTestIfDone();
    }
  }

 private:
  void ContinueTestIfDone() {
    if (step_ == 0) {
      // First navigation should trigger all callbacks except OnLoadError.
      if (got_loading_state_changed_end_ && got_load_end_) {
        EXPECT_TRUE(got_before_browse_);
        EXPECT_TRUE(got_loading_state_changed_start_);
        EXPECT_TRUE(got_load_start_);
        EXPECT_FALSE(got_load_error_);

        got_before_browse_.reset();
        got_loading_state_changed_start_.reset();
        got_loading_state_changed_end_.reset();
        got_load_start_.reset();
        got_load_end_.reset();

        step_++;
        expected_url_ = kSameNavPageUrl + std::string("#fragment");
        GetBrowser()->GetMainFrame()->LoadURL(expected_url_);
      }
    } else if (step_ == 1) {
      step_++;
      DestroyTest();
    } else {
      EXPECT_TRUE(false);  // Not reached.
    }
  }

  void DestroyTest() override {
    if (destroyed_)
      return;
    destroyed_ = true;

    EXPECT_EQ(2, step_);

    // Second (fragment) navigation should only trigger OnLoadingStateChange.
    EXPECT_FALSE(got_before_browse_);
    EXPECT_TRUE(got_loading_state_changed_start_);
    EXPECT_TRUE(got_loading_state_changed_end_);
    EXPECT_FALSE(got_load_start_);
    EXPECT_FALSE(got_load_end_);
    EXPECT_FALSE(got_load_error_);

    TestHandler::DestroyTest();
  }

  bool destroyed_;
  int step_;
  std::string expected_url_;

  TrackCallback got_before_browse_;
  TrackCallback got_load_start_;
  TrackCallback got_load_end_;
  TrackCallback got_load_error_;
  TrackCallback got_loading_state_changed_start_;
  TrackCallback got_loading_state_changed_end_;

  IMPLEMENT_REFCOUNTING(SameNavTestHandler);
};

}  // namespace

// Test that same page navigation does not call OnLoadStart/OnLoadEnd.
TEST(NavigationTest, SamePage) {
  CefRefPtr<SameNavTestHandler> handler = new SameNavTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kCancelPageUrl[] = "http://tests-cancelnav/nav.html";

// A scheme handler that never starts sending data.
class UnstartedSchemeHandler : public CefResourceHandler {
 public:
  UnstartedSchemeHandler() {}

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    // Continue immediately.
    handle_request = true;
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
    response->SetStatus(200);
    response->SetMimeType("text/html");
    response_length = 100;
  }

  void Cancel() override { callback_ = nullptr; }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    callback_ = callback;

    // Pretend that we'll provide the data later.
    bytes_read = 0;
    return true;
  }

 protected:
  CefRefPtr<CefResourceReadCallback> callback_;

  IMPLEMENT_REFCOUNTING(UnstartedSchemeHandler);
  DISALLOW_COPY_AND_ASSIGN(UnstartedSchemeHandler);
};

// Browser side.
class CancelBeforeNavTestHandler : public TestHandler {
 public:
  CancelBeforeNavTestHandler() : destroyed_(false) {}

  void RunTest() override {
    // Create the browser.
    CreateBrowser(kCancelPageUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override {
    EXPECT_TRUE(got_loading_state_changed_start_);
    EXPECT_FALSE(got_before_browse_);
    EXPECT_FALSE(got_get_resource_handler_);
    EXPECT_FALSE(got_load_start_);
    EXPECT_FALSE(got_cancel_load_);
    EXPECT_FALSE(got_load_error_);
    EXPECT_FALSE(got_load_end_);
    EXPECT_FALSE(got_loading_state_changed_end_);

    const std::string& url = request->GetURL();
    EXPECT_STREQ(kCancelPageUrl, url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    got_before_browse_.yes();

    return false;
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    EXPECT_TRUE(got_loading_state_changed_start_);
    EXPECT_TRUE(got_before_browse_);
    EXPECT_FALSE(got_get_resource_handler_);
    EXPECT_FALSE(got_load_start_);
    EXPECT_FALSE(got_cancel_load_);
    EXPECT_FALSE(got_load_error_);
    EXPECT_FALSE(got_load_end_);
    EXPECT_FALSE(got_loading_state_changed_end_);

    const std::string& url = request->GetURL();
    EXPECT_STREQ(kCancelPageUrl, url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    got_get_resource_handler_.yes();

    CefPostTask(TID_UI,
                base::BindOnce(&CancelBeforeNavTestHandler::CancelLoad, this));

    return new UnstartedSchemeHandler();
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    EXPECT_TRUE(false);  // Not reached.
    got_load_start_.yes();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_TRUE(false);  // Not reached.
    got_load_end_.yes();
  }

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override {
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_STREQ("", frame->GetURL().ToString().c_str());
    EXPECT_EQ(ERR_ABORTED, errorCode);
    EXPECT_STREQ(kCancelPageUrl, failedUrl.ToString().c_str());
    got_load_error_.yes();
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    const std::string& url = browser->GetMainFrame()->GetURL();
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(url.empty());

    if (isLoading) {
      EXPECT_FALSE(got_loading_state_changed_start_);
      EXPECT_FALSE(got_before_browse_);
      EXPECT_FALSE(got_get_resource_handler_);
      EXPECT_FALSE(got_load_start_);
      EXPECT_FALSE(got_cancel_load_);
      EXPECT_FALSE(got_load_error_);
      EXPECT_FALSE(got_load_end_);
      EXPECT_FALSE(got_loading_state_changed_end_);

      got_loading_state_changed_start_.yes();
    } else {
      EXPECT_TRUE(got_loading_state_changed_start_);
      EXPECT_TRUE(got_before_browse_);
      EXPECT_TRUE(got_get_resource_handler_);
      EXPECT_FALSE(got_load_start_);
      EXPECT_TRUE(got_cancel_load_);
      EXPECT_TRUE(got_load_error_);
      EXPECT_FALSE(got_load_end_);
      EXPECT_FALSE(got_loading_state_changed_end_);

      got_loading_state_changed_end_.yes();

      DestroyTest();
    }
  }

 private:
  void CancelLoad() {
    got_cancel_load_.yes();
    GetBrowser()->StopLoad();
  }

  void DestroyTest() override {
    if (destroyed_)
      return;
    destroyed_ = true;

    EXPECT_TRUE(got_loading_state_changed_start_);
    EXPECT_TRUE(got_before_browse_);
    EXPECT_TRUE(got_get_resource_handler_);
    EXPECT_FALSE(got_load_start_);
    EXPECT_TRUE(got_cancel_load_);
    EXPECT_TRUE(got_load_error_);
    EXPECT_FALSE(got_load_end_);
    EXPECT_TRUE(got_loading_state_changed_end_);

    TestHandler::DestroyTest();
  }

  bool destroyed_;

  TrackCallback got_loading_state_changed_start_;
  TrackCallback got_before_browse_;
  TrackCallback got_get_resource_handler_;
  TrackCallback got_load_start_;
  TrackCallback got_cancel_load_;
  TrackCallback got_load_error_;
  TrackCallback got_load_end_;
  TrackCallback got_loading_state_changed_end_;

  IMPLEMENT_REFCOUNTING(CancelBeforeNavTestHandler);
};

}  // namespace

// Test that navigation canceled before commit does not call
// OnLoadStart/OnLoadEnd.
TEST(NavigationTest, CancelBeforeCommit) {
  CefRefPtr<CancelBeforeNavTestHandler> handler =
      new CancelBeforeNavTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

// A scheme handler that stalls after writing some data.
class StalledSchemeHandler : public CefResourceHandler {
 public:
  StalledSchemeHandler() : offset_(0), write_size_(0) {}

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    // Continue immediately.
    handle_request = true;
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
    response->SetStatus(200);
    response->SetMimeType("text/html");
    content_ = "<html><body>Test</body></html>";
    // Write this number of bytes and then stall.
    write_size_ = content_.size() / 2U;
    response_length = content_.size();
  }

  void Cancel() override { callback_ = nullptr; }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    bytes_read = 0;

    size_t size = content_.size();
    if (offset_ >= write_size_) {
      // Now stall.
      callback_ = callback;
      return true;
    }

    bool has_data = false;

    if (offset_ < size) {
      // Write up to |write_size_| bytes.
      int transfer_size =
          std::min(bytes_to_read, std::min(static_cast<int>(write_size_),
                                           static_cast<int>(size - offset_)));
      memcpy(data_out, content_.c_str() + offset_, transfer_size);
      offset_ += transfer_size;

      bytes_read = transfer_size;
      has_data = true;
    }

    return has_data;
  }

 protected:
  std::string content_;
  size_t offset_;
  size_t write_size_;
  CefRefPtr<CefResourceReadCallback> callback_;

  IMPLEMENT_REFCOUNTING(StalledSchemeHandler);
  DISALLOW_COPY_AND_ASSIGN(StalledSchemeHandler);
};

// Browser side.
class CancelAfterNavTestHandler : public TestHandler {
 public:
  CancelAfterNavTestHandler() : destroyed_(false) {}

  void RunTest() override {
    // Create the browser.
    CreateBrowser(kCancelPageUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override {
    EXPECT_TRUE(got_loading_state_changed_start_);
    EXPECT_FALSE(got_before_browse_);
    EXPECT_FALSE(got_get_resource_handler_);
    EXPECT_FALSE(got_load_start_);
    EXPECT_FALSE(got_cancel_load_);
    EXPECT_FALSE(got_load_error_);
    EXPECT_FALSE(got_load_end_);
    EXPECT_FALSE(got_loading_state_changed_end_);

    const std::string& url = request->GetURL();
    EXPECT_STREQ(kCancelPageUrl, url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    got_before_browse_.yes();

    return false;
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    EXPECT_TRUE(got_loading_state_changed_start_);
    EXPECT_TRUE(got_before_browse_);
    EXPECT_FALSE(got_get_resource_handler_);
    EXPECT_FALSE(got_load_start_);
    EXPECT_FALSE(got_cancel_load_);
    EXPECT_FALSE(got_load_error_);
    EXPECT_FALSE(got_load_end_);
    EXPECT_FALSE(got_loading_state_changed_end_);

    const std::string& url = request->GetURL();
    EXPECT_STREQ(kCancelPageUrl, url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    got_get_resource_handler_.yes();

    // The required delay is longer when browser-side navigation is enabled.
    CefPostDelayedTask(
        TID_UI, base::BindOnce(&CancelAfterNavTestHandler::CancelLoad, this),
        1000);

    return new StalledSchemeHandler();
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    EXPECT_TRUE(got_loading_state_changed_start_);
    EXPECT_TRUE(got_before_browse_);
    EXPECT_TRUE(got_get_resource_handler_);
    EXPECT_FALSE(got_load_start_);
    EXPECT_FALSE(got_cancel_load_);
    EXPECT_FALSE(got_load_error_);
    EXPECT_FALSE(got_load_end_);
    EXPECT_FALSE(got_loading_state_changed_end_);

    const std::string& url = frame->GetURL();
    EXPECT_STREQ(kCancelPageUrl, url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    got_load_start_.yes();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_TRUE(got_loading_state_changed_start_);
    EXPECT_TRUE(got_before_browse_);
    EXPECT_TRUE(got_get_resource_handler_);
    EXPECT_TRUE(got_load_start_);
    EXPECT_TRUE(got_cancel_load_);
    EXPECT_TRUE(got_load_error_);
    EXPECT_FALSE(got_load_end_);
    EXPECT_FALSE(got_loading_state_changed_end_);

    const std::string& url = frame->GetURL();
    EXPECT_STREQ(kCancelPageUrl, url.c_str());
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
    EXPECT_TRUE(got_loading_state_changed_start_);
    EXPECT_TRUE(got_before_browse_);
    EXPECT_TRUE(got_get_resource_handler_);
    EXPECT_TRUE(got_load_start_);
    EXPECT_TRUE(got_cancel_load_);
    EXPECT_FALSE(got_load_error_);
    EXPECT_FALSE(got_load_end_);
    EXPECT_FALSE(got_loading_state_changed_end_);

    const std::string& url = failedUrl;
    EXPECT_STREQ(kCancelPageUrl, url.c_str());
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    got_load_error_.yes();
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    const std::string& url = browser->GetMainFrame()->GetURL();
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());

    if (isLoading) {
      EXPECT_FALSE(got_loading_state_changed_start_);
      EXPECT_FALSE(got_before_browse_);
      EXPECT_FALSE(got_get_resource_handler_);
      EXPECT_FALSE(got_load_start_);
      EXPECT_FALSE(got_cancel_load_);
      EXPECT_FALSE(got_load_error_);
      EXPECT_FALSE(got_load_end_);
      EXPECT_FALSE(got_loading_state_changed_end_);

      EXPECT_TRUE(url.empty());

      got_loading_state_changed_start_.yes();
    } else {
      EXPECT_TRUE(got_loading_state_changed_start_);
      EXPECT_TRUE(got_before_browse_);
      EXPECT_TRUE(got_get_resource_handler_);
      EXPECT_TRUE(got_load_start_);
      EXPECT_TRUE(got_cancel_load_);
      EXPECT_TRUE(got_load_error_);
      EXPECT_TRUE(got_load_end_);
      EXPECT_FALSE(got_loading_state_changed_end_);

      EXPECT_STREQ(kCancelPageUrl, url.c_str());

      got_loading_state_changed_end_.yes();
      DestroyTestIfDone();
    }
  }

 private:
  void CancelLoad() {
    got_cancel_load_.yes();
    GetBrowser()->StopLoad();
  }

  void DestroyTestIfDone() {
    if (got_loading_state_changed_end_ && got_load_end_)
      DestroyTest();
  }

  void DestroyTest() override {
    if (destroyed_)
      return;
    destroyed_ = true;

    EXPECT_TRUE(got_loading_state_changed_start_);
    EXPECT_TRUE(got_before_browse_);
    EXPECT_TRUE(got_get_resource_handler_);
    EXPECT_TRUE(got_load_start_);
    EXPECT_TRUE(got_cancel_load_);
    EXPECT_TRUE(got_load_error_);
    EXPECT_TRUE(got_load_end_);
    EXPECT_TRUE(got_loading_state_changed_end_);

    TestHandler::DestroyTest();
  }

  bool destroyed_;

  TrackCallback got_loading_state_changed_start_;
  TrackCallback got_before_browse_;
  TrackCallback got_get_resource_handler_;
  TrackCallback got_load_start_;
  TrackCallback got_cancel_load_;
  TrackCallback got_load_error_;
  TrackCallback got_load_end_;
  TrackCallback got_loading_state_changed_end_;

  IMPLEMENT_REFCOUNTING(CancelAfterNavTestHandler);
};

}  // namespace

// Test that navigation canceled after commit calls everything.
TEST(NavigationTest, CancelAfterCommit) {
  CefRefPtr<CancelAfterNavTestHandler> handler =
      new CancelAfterNavTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kExtraInfoUrl[] = "http://tests-extrainfonav.com/extra.html";
const char kExtraInfoPopupUrl[] =
    "http://tests-extrainfonav.com/extra_popup.html";
const char kExtraInfoNavMsg[] = "NavigationTest.ExtraInfoNav";
const char kExtraInfoTestCmdKey[] = "nav-extra-info-test";

void SetBrowserExtraInfo(CefRefPtr<CefDictionaryValue> extra_info) {
  // Necessary for identifying the test case.
  extra_info->SetBool(kExtraInfoTestCmdKey, true);

  // Arbitrary data for testing.
  extra_info->SetBool("bool", true);
  CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
  dict->SetInt("key1", 5);
  dict->SetString("key2", "test string");
  extra_info->SetDictionary("dictionary", dict);
  extra_info->SetDouble("double", 5.43322);
  extra_info->SetString("string", "some string");
}

// Renderer side
class ExtraInfoNavRendererTest : public ClientAppRenderer::Delegate {
 public:
  ExtraInfoNavRendererTest() : run_test_(false) {}

  void OnBrowserCreated(CefRefPtr<ClientAppRenderer> app,
                        CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefDictionaryValue> extra_info) override {
    run_test_ = extra_info && extra_info->HasKey(kExtraInfoTestCmdKey);
    if (!run_test_)
      return;

    CefRefPtr<CefDictionaryValue> expected = CefDictionaryValue::Create();
    SetBrowserExtraInfo(expected);
    TestDictionaryEqual(expected, extra_info);

    SendTestResults(browser, browser->GetMainFrame());
  }

 protected:
  // Send the test results.
  void SendTestResults(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame) {
    // Check if the test has failed.
    bool result = !TestFailed();

    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(kExtraInfoNavMsg);
    CefRefPtr<CefListValue> args = return_msg->GetArgumentList();
    EXPECT_TRUE(args.get());
    EXPECT_TRUE(args->SetBool(0, result));
    EXPECT_TRUE(args->SetBool(1, browser->IsPopup()));
    frame->SendProcessMessage(PID_BROWSER, return_msg);
  }

  bool run_test_;

  IMPLEMENT_REFCOUNTING(ExtraInfoNavRendererTest);
};

class ExtraInfoNavTestHandler : public TestHandler {
 public:
  ExtraInfoNavTestHandler() : popup_opened_(false) {}

  void RunTest() override {
    AddResource(kExtraInfoUrl,
                "<html><head></head><body>ExtraInfo</body></html>",
                "text/html");
    AddResource(kExtraInfoPopupUrl, "<html>ExtraInfoPopup</html>", "text/html");

    CefRefPtr<CefDictionaryValue> extra_info = CefDictionaryValue::Create();
    SetBrowserExtraInfo(extra_info);

    // Create the browser.
    CreateBrowser(kExtraInfoUrl, nullptr, extra_info);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (popup_opened_) {
      DestroyTest();
    } else {
      browser->GetMainFrame()->ExecuteJavaScript(
          "window.open('" + std::string(kExtraInfoPopupUrl) + "');",
          CefString(), 0);
    }
  }

  bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     const CefString& target_url,
                     const CefString& target_frame_name,
                     cef_window_open_disposition_t target_disposition,
                     bool user_gesture,
                     const CefPopupFeatures& popupFeatures,
                     CefWindowInfo& windowInfo,
                     CefRefPtr<CefClient>& client,
                     CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue>& extra_info,
                     bool* no_javascript_access) override {
    const std::string& url = target_url;
    EXPECT_FALSE(popup_opened_);
    EXPECT_STREQ(kExtraInfoPopupUrl, url.c_str());

    CefRefPtr<CefDictionaryValue> extra = CefDictionaryValue::Create();
    SetBrowserExtraInfo(extra);

    extra_info = extra;

    popup_opened_ = true;
    return false;
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    if (message->GetName().ToString() == kExtraInfoNavMsg) {
      // Test that the renderer side succeeded.
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      EXPECT_TRUE(args.get());
      EXPECT_TRUE(args->GetBool(0));
      if (popup_opened_) {
        EXPECT_TRUE(args->GetBool(1));
        got_process_message_popup_.yes();
      } else {
        EXPECT_FALSE(args->GetBool(1));
        got_process_message_main_.yes();
      }
      return true;
    }

    // Message not handled.
    return false;
  }

 protected:
  bool popup_opened_;
  TrackCallback got_process_message_main_;
  TrackCallback got_process_message_popup_;

  void DestroyTest() override {
    // Verify test expectations.
    EXPECT_TRUE(got_process_message_main_);
    EXPECT_TRUE(got_process_message_popup_);

    TestHandler::DestroyTest();
  }

  IMPLEMENT_REFCOUNTING(ExtraInfoNavTestHandler);
};

}  // namespace

TEST(NavigationTest, ExtraInfo) {
  CefRefPtr<ExtraInfoNavTestHandler> handler = new ExtraInfoNavTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Entry point for creating navigation renderer test objects.
// Called from client_app_delegates.cc.
void CreateNavigationRendererTests(ClientAppRenderer::DelegateSet& delegates) {
  delegates.insert(new HistoryNavRendererTest);
  delegates.insert(new OrderNavRendererTest);
  delegates.insert(new LoadNavRendererTest);
  delegates.insert(new ExtraInfoNavRendererTest);
}
