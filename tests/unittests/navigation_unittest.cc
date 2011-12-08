// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "test_handler.h"

namespace {

static const char* kNav1 = "http://tests/nav1.html";
static const char* kNav2 = "http://tests/nav2.html";
static const char* kNav3 = "http://tests/nav3.html";

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
static NavListItem kNavList[] = {
                                     // kNav1 | kNav2 | kNav3
  {NA_LOAD, kNav1, false, false},    //   X
  {NA_LOAD, kNav2, true, false},     //   .       X
  {NA_BACK, kNav1, false, true},     //   X       .
  {NA_FORWARD, kNav2, true, false},  //   .       X
  {NA_LOAD, kNav3, true, false},     //   .       .       X
  {NA_BACK, kNav2, true, true},      //   .       X       .
  {NA_CLEAR, kNav2, false, false},   //           X
};

#define NAV_LIST_SIZE() (sizeof(kNavList) / sizeof(NavListItem))

class HistoryNavTestHandler : public TestHandler
{
public:
  HistoryNavTestHandler() : nav_(0) {}

  virtual void RunTest() OVERRIDE
  {
    // Add the resources that we will navigate to/from.
    AddResource(kNav1, "<html>Nav1</html>", "text/html");
    AddResource(kNav2, "<html>Nav2</html>", "text/html");
    AddResource(kNav3, "<html>Nav3</html>", "text/html");

    // Create the browser.
    CreateBrowser(CefString());
  }

  void RunNav(CefRefPtr<CefBrowser> browser)
  {
    if (nav_ == NAV_LIST_SIZE()) {
      // End of the nav list.
      DestroyTest();
      return;
    }

    const NavListItem& item = kNavList[nav_];

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
      browser->ClearHistory();
      // Not really a navigation action so go to the next one.
      nav_++;
      RunNav(browser);
      break;
    default:
      break;
    }
  }

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE
  {
    TestHandler::OnAfterCreated(browser);

    RunNav(browser);
  }

  virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              NavType navType,
                              bool isRedirect) OVERRIDE
  {
    const NavListItem& item = kNavList[nav_];

    got_before_browse_[nav_].yes();

    std::string url = request->GetURL();
    if (url == item.target)
      got_correct_target_[nav_].yes();

    if (((item.action == NA_BACK || item.action == NA_FORWARD) &&
         navType == NAVTYPE_BACKFORWARD) ||
        (item.action == NA_LOAD && navType == NAVTYPE_OTHER)) {
      got_correct_nav_type_[nav_].yes();
    }

    return false;
  }

  virtual void OnNavStateChange(CefRefPtr<CefBrowser> browser,
                                bool canGoBack,
                                bool canGoForward) OVERRIDE
  {
    const NavListItem& item = kNavList[nav_];

    got_nav_state_change_[nav_].yes();

    if (item.can_go_back == canGoBack)
      got_correct_can_go_back_[nav_].yes();
    if (item.can_go_forward == canGoForward)
      got_correct_can_go_forward_[nav_].yes();
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE
  {
    if(browser->IsPopup() || !frame->IsMain())
      return;

    const NavListItem& item = kNavList[nav_];

    got_load_end_[nav_].yes();

    if (item.can_go_back == browser->CanGoBack())
      got_correct_can_go_back2_[nav_].yes();
    if (item.can_go_forward == browser->CanGoForward())
      got_correct_can_go_forward2_[nav_].yes();

    nav_++;
    RunNav(browser);
  }

  int nav_;

  TrackCallback got_before_browse_[NAV_LIST_SIZE()];
  TrackCallback got_correct_target_[NAV_LIST_SIZE()];
  TrackCallback got_correct_nav_type_[NAV_LIST_SIZE()];
  TrackCallback got_nav_state_change_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_back_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_forward_[NAV_LIST_SIZE()];
  TrackCallback got_load_end_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_back2_[NAV_LIST_SIZE()];
  TrackCallback got_correct_can_go_forward2_[NAV_LIST_SIZE()];
};

} // namespace

// Verify history navigation.
TEST(NavigationTest, History)
{
  CefRefPtr<HistoryNavTestHandler> handler =
      new HistoryNavTestHandler();
  handler->ExecuteTest();

  for (size_t i = 0; i < NAV_LIST_SIZE(); ++i) {
    if (kNavList[i].action != NA_CLEAR) {
      ASSERT_TRUE(handler->got_before_browse_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_target_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_nav_type_[i]) << "i = " << i;
    }

    if (i == 0 || kNavList[i].can_go_back != kNavList[i-1].can_go_back ||
        kNavList[i].can_go_forward != kNavList[i-1].can_go_forward) {
      // Back/forward state has changed from one navigation to the next.
      ASSERT_TRUE(handler->got_nav_state_change_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_can_go_back_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_can_go_forward_[i]) << "i = " << i;
    } else {
      ASSERT_FALSE(handler->got_nav_state_change_[i]) << "i = " << i;
      ASSERT_FALSE(handler->got_correct_can_go_back_[i]) << "i = " << i;
      ASSERT_FALSE(handler->got_correct_can_go_forward_[i]) << "i = " << i;
    }

    if (kNavList[i].action != NA_CLEAR) {
      ASSERT_TRUE(handler->got_load_end_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_can_go_back2_[i]) << "i = " << i;
      ASSERT_TRUE(handler->got_correct_can_go_forward2_[i]) << "i = " << i;
    }
  }
}


namespace {

class FrameNameIdentNavTestHandler : public TestHandler
{
public:
  FrameNameIdentNavTestHandler() : browse_ct_(0) {}

  virtual void RunTest() OVERRIDE
  {
    // Add the frame resources.
    std::stringstream ss;

    // Page with named frame
    ss << "<html>Nav1<iframe src=\"" << kNav2 << "\" name=\"nav2\"></html>";
    AddResource(kNav1, ss.str(), "text/html");
    ss.str("");

    // Page with unnamed frame
    ss << "<html>Nav2<iframe src=\"" << kNav3 << "\"></html>";
    AddResource(kNav2, ss.str(), "text/html");
    ss.str("");

    AddResource(kNav3, "<html>Nav3</html>", "text/html");

    // Create the browser.
    CreateBrowser(kNav1);
  }

  virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              NavType navType,
                              bool isRedirect) OVERRIDE
  {
    std::string url = request->GetURL();
    std::string name = frame->GetName();
    CefRefPtr<CefFrame> parent = frame->GetParent();

    if (url == kNav1) {
      frame1_ident_ = frame->GetIdentifier();
      if (name == "")
        got_frame1_name_.yes();
      if (!parent.get())
        got_frame1_ident_parent_before_.yes();
    } else if (url == kNav2) {
      frame2_ident_ = frame->GetIdentifier();
      if (name == "nav2")
        got_frame2_name_.yes();
      if (parent.get() && frame1_ident_ == parent->GetIdentifier())
        got_frame2_ident_parent_before_.yes();
    } else if (url == kNav3) {
      frame3_ident_ = frame->GetIdentifier();
      if (name == "<!--framePath //nav2/<!--frame0-->-->")
        got_frame3_name_.yes();
       if (parent.get() && frame2_ident_ == parent->GetIdentifier())
        got_frame3_ident_parent_before_.yes();
    }

    return false;
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE
  {
    std::string url = frame->GetURL();
    CefRefPtr<CefFrame> parent = frame->GetParent();

    if (url == kNav1) {
      if (frame1_ident_ == frame->GetIdentifier())
        got_frame1_ident_.yes();
      if (!parent.get())
        got_frame1_ident_parent_after_.yes();
    } else if (url == kNav2) {
      if (frame2_ident_ == frame->GetIdentifier())
        got_frame2_ident_.yes();
      if (parent.get() && frame1_ident_ == parent->GetIdentifier())
        got_frame2_ident_parent_after_.yes();
    } else if (url == kNav3) {
      if (frame3_ident_ == frame->GetIdentifier())
        got_frame3_ident_.yes();
      if (parent.get() && frame2_ident_ == parent->GetIdentifier())
        got_frame3_ident_parent_after_.yes();
    }

    if (++browse_ct_ == 3)
      DestroyTest();
  }

  int browse_ct_;

  long long frame1_ident_;
  long long frame2_ident_;
  long long frame3_ident_;

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

} // namespace

// Verify frame names and identifiers.
TEST(NavigationTest, FrameNameIdent)
{
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
