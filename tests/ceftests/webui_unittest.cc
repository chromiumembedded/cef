// Copyright 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_bind.h"
#include "include/cef_callback.h"
#include "include/cef_parser.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

typedef std::vector<std::string> UrlList;

class WebUITestHandler : public TestHandler {
 public:
  explicit WebUITestHandler(const UrlList& url_list)
    : url_list_(url_list),
      url_index_(0U),
      expected_error_code_(ERR_NONE) {
    CHECK(!url_list_.empty());
  }

  void set_expected_url(const std::string& expected_url) {
    expected_url_ = expected_url;
  }

  void set_expected_error_code(int error_code) {
    expected_error_code_ = error_code;
  }

  void RunTest() override {
    // Create the browser.
    CreateBrowser(url_list_[0]);

    // Time out the test after a reasonable period of time.
    SetTestTimeout((int(url_list_.size() / 5U) + 1) * 5000);
  }

  void NextNav() {
    base::Closure next_action;

    if (++url_index_ == url_list_.size()) {
      next_action = base::Bind(&WebUITestHandler::DestroyTest, this);
    } else {
      next_action = base::Bind(&WebUITestHandler::LoadURL, this,
                               url_list_[url_index_]);
    }

    // Wait a bit for the WebUI content to finish loading before performing the
    // next action.
    CefPostDelayedTask(TID_UI, next_action, 200);
  }

  void LoadURL(const std::string& url) {
    GetBrowser()->GetMainFrame()->LoadURL(url);
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (!isLoading) {
      // Verify that we navigated to the expected URL.
      std::string expected_url = expected_url_;
      if (expected_url.empty())
        expected_url = url_list_[url_index_];
      EXPECT_STREQ(expected_url.c_str(),
                   browser->GetMainFrame()->GetURL().ToString().c_str());

      NextNav();
    }
  }

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override {
    got_load_error_.yes();
    EXPECT_EQ(expected_error_code_, errorCode) <<
        "failedUrl = " << failedUrl.ToString();
  }

  void DestroyTest() override {
    if (expected_error_code_ == ERR_NONE)
      EXPECT_FALSE(got_load_error_);
    else
      EXPECT_TRUE(got_load_error_);

    TestHandler::DestroyTest();
  }

  UrlList url_list_;
  size_t url_index_;

  std::string expected_url_;
  int expected_error_code_;

  TrackCallback got_load_error_;

  IMPLEMENT_REFCOUNTING(WebUITestHandler);
};

}  // namespace


// Test hosts with special behaviors.

// Non-existing URLs should redirect to chrome://version/.
TEST(WebUITest, doesnotexist) {
  UrlList url_list;
  url_list.push_back("chrome://doesnotexist/");
  CefRefPtr<WebUITestHandler> handler = new WebUITestHandler(url_list);
  handler->set_expected_url("chrome://version/");
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// about:* URIs should redirect to chrome://*.
TEST(WebUITest, about) {
  UrlList url_list;
  url_list.push_back("about:license");
  CefRefPtr<WebUITestHandler> handler = new WebUITestHandler(url_list);
  handler->set_expected_url("chrome://license/");
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// chrome://network-error/X should generate network error X.
TEST(WebUITest, network_error) {
  UrlList url_list;
  // -310 is ERR_TOO_MANY_REDIRECTS
  url_list.push_back("chrome://network-error/-310");
  CefRefPtr<WebUITestHandler> handler = new WebUITestHandler(url_list);
  handler->set_expected_url("data:text/html,chromewebdata");
  handler->set_expected_error_code(ERR_TOO_MANY_REDIRECTS);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}


// Test hosts with a single URL.

namespace {

void RunWebUITest(const UrlList& url_list) {
  CefRefPtr<WebUITestHandler> handler = new WebUITestHandler(url_list);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

void RunWebUITest(const std::string& url) {
  UrlList url_list;
  url_list.push_back(url);
  RunWebUITest(url_list);
}

}  // namespace

#define WEBUI_TEST(name) \
  TEST(WebUITest, name) { \
    std::string name_str = #name; \
    std::replace(name_str.begin(), name_str.end(), '_', '-'); \
    RunWebUITest("chrome://" + name_str + "/"); \
  }

WEBUI_TEST(appcache_internals);
WEBUI_TEST(accessibility);
WEBUI_TEST(blob_internals);
WEBUI_TEST(credits);
WEBUI_TEST(gpu);
WEBUI_TEST(histograms);
WEBUI_TEST(indexeddb_internals);
WEBUI_TEST(license);
WEBUI_TEST(media_internals);
WEBUI_TEST(net_export);
WEBUI_TEST(network_errors);
WEBUI_TEST(serviceworker_internals);
WEBUI_TEST(system);
WEBUI_TEST(tracing);
WEBUI_TEST(version);
WEBUI_TEST(view_http_cache);
WEBUI_TEST(webrtc_internals);
WEBUI_TEST(webui_hosts);


// Test hosts with multiple URLs.

TEST(WebUITest, net_internals) {
  UrlList url_list;
  url_list.push_back("chrome://net-internals/#capture");
  url_list.push_back("chrome://net-internals/#export");
  url_list.push_back("chrome://net-internals/#import");
  url_list.push_back("chrome://net-internals/#proxy");
  url_list.push_back("chrome://net-internals/#events");
  url_list.push_back("chrome://net-internals/#timeline");
  url_list.push_back("chrome://net-internals/#dns");
  url_list.push_back("chrome://net-internals/#sockets");
  url_list.push_back("chrome://net-internals/#alt-svc");
  url_list.push_back("chrome://net-internals/#http2");
  url_list.push_back("chrome://net-internals/#quic");
  url_list.push_back("chrome://net-internals/#sdch");
  url_list.push_back("chrome://net-internals/#httpCache");
  url_list.push_back("chrome://net-internals/#modules");
  url_list.push_back("chrome://net-internals/#hsts");
  url_list.push_back("chrome://net-internals/#bandwidth");
  url_list.push_back("chrome://net-internals/#prerender");

  RunWebUITest(url_list);
}
