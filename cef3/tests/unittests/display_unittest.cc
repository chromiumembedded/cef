// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <list>
#include "include/cef_runnable.h"
#include "tests/unittests/test_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// How it works:
// 1. Load kTitleUrl1 (title should be kTitleStr1)
// 2. Load kTitleUrl2 (title should be kTitleStr2)
// 3. History back to kTitleUrl1 (title should be kTitleStr1)
// 4. History forward to kTitleUrl2 (title should be kTitleStr2)
// 5. Set title via JavaScript (title should be kTitleStr3)

const char kTitleUrl1[] = "http://tests-title/nav1.html";
const char kTitleUrl2[] = "http://tests-title/nav2.html";
const char kTitleStr1[] = "Title 1";
const char kTitleStr2[] = "Title 2";
const char kTitleStr3[] = "Title 3";

// Browser side.
class TitleTestHandler : public TestHandler {
 public:
  TitleTestHandler()
      : step_(0) {}

  virtual void RunTest() OVERRIDE {
    // Add the resources that we will navigate to/from.
    AddResource(kTitleUrl1,
        "<html><head><title>" + std::string(kTitleStr1) +
        "</title></head>Nav1</html>", "text/html");
    AddResource(kTitleUrl2,
        "<html><head><title>" + std::string(kTitleStr2) +
        "</title></head>Nav2" +
        "<script>function setTitle() { window.document.title = '" +
        std::string(kTitleStr3) + "'; }</script>" +
        "</html>", "text/html");

    // Create the browser.
    CreateBrowser(kTitleUrl1);
  }

  virtual void OnTitleChange(CefRefPtr<CefBrowser> browser,
                             const CefString& title) OVERRIDE {
    std::string title_str = title;
    if (step_ == 0 || step_ == 2) {
      EXPECT_STREQ(kTitleStr1, title_str.c_str());
    } else if (step_ == 1 || step_ == 3) {
      EXPECT_STREQ(kTitleStr2, title_str.c_str());
    } else if (step_ == 4) {
      EXPECT_STREQ(kTitleStr3, title_str.c_str());
    }

    got_title_[step_].yes();

    if (step_ == 4)
      DestroyTest();
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    switch (step_++) {
      case 0:
        frame->LoadURL(kTitleUrl2);
        break;
      case 1:
        browser->GoBack();
        break;
      case 2:
        browser->GoForward();
        break;
      case 3:
        frame->ExecuteJavaScript("setTitle()", kTitleUrl2, 0);
        break;
      default:
        EXPECT_TRUE(false); // Not reached.
    }
  }

 private:
  virtual void DestroyTest() {
    for (int i = 0; i < 5; ++i)
      EXPECT_TRUE(got_title_[i]) << "step " << i;

    TestHandler::DestroyTest();
  }

  int step_;

  TrackCallback got_title_[5];
};

}  // namespace

// Test title notifications.
TEST(DisplayTest, Title) {
  CefRefPtr<TitleTestHandler> handler = new TitleTestHandler();
  handler->ExecuteTest();
}
