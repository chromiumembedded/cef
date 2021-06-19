// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <list>

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

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
      : step_(0), got_title_change_(false), got_loading_state_change_(false) {}

  void RunTest() override {
    // Add the resources that we will navigate to/from.
    AddResource(kTitleUrl1,
                "<html><head><title>" + std::string(kTitleStr1) +
                    "</title></head>Nav1</html>",
                "text/html");
    AddResource(kTitleUrl2,
                "<html><head><title>" + std::string(kTitleStr2) +
                    "</title></head>Nav2" +
                    "<script>function setTitle() { window.document.title = '" +
                    std::string(kTitleStr3) + "'; }</script>" + "</html>",
                "text/html");

    // Create the browser.
    CreateBrowser(kTitleUrl1);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override {
    // Ignore the 2nd OnTitleChange call which arrives after navigation
    // completion.
    if (got_title_change_)
      return;

    std::string title_str = title;
    if (step_ == 0 || step_ == 2) {
      EXPECT_STREQ(kTitleStr1, title_str.c_str());
    } else if (step_ == 1 || step_ == 3) {
      EXPECT_STREQ(kTitleStr2, title_str.c_str());
    } else if (step_ == 4) {
      EXPECT_STREQ(kTitleStr3, title_str.c_str());
    }

    got_title_[step_].yes();

    if (step_ == 4) {
      DestroyTest();
    } else {
      got_title_change_ = true;
      NextIfReady(browser);
    }
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (isLoading)
      return;

    // Call NextIfReady asynchronously because an additional call to
    // OnTitleChange will be triggered later in the current call stack due to
    // navigation completion and we want that call to arrive before execution of
    // NextIfReady.
    got_loading_state_change_ = true;
    CefPostTask(TID_UI,
                base::BindOnce(&TitleTestHandler::NextIfReady, this, browser));
  }

 private:
  void NextIfReady(CefRefPtr<CefBrowser> browser) {
    if (!got_title_change_ || !got_loading_state_change_)
      return;

    got_title_change_ = false;
    got_loading_state_change_ = false;

    switch (step_++) {
      case 0:
        browser->GetMainFrame()->LoadURL(kTitleUrl2);
        break;
      case 1:
        browser->GoBack();
        break;
      case 2:
        browser->GoForward();
        break;
      case 3:
        browser->GetMainFrame()->ExecuteJavaScript("setTitle()", kTitleUrl2, 0);
        break;
      default:
        EXPECT_TRUE(false);  // Not reached.
    }
  }

  void DestroyTest() override {
    for (int i = 0; i < 5; ++i)
      EXPECT_TRUE(got_title_[i]) << "step " << i;

    TestHandler::DestroyTest();
  }

  int step_;

  bool got_title_change_;
  bool got_loading_state_change_;

  TrackCallback got_title_[5];

  IMPLEMENT_REFCOUNTING(TitleTestHandler);
};

}  // namespace

// Test title notifications.
TEST(DisplayTest, Title) {
  CefRefPtr<TitleTestHandler> handler = new TitleTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kAutoResizeUrl[] = "http://tests-display/auto-resize.html";

class AutoResizeTestHandler : public RoutingTestHandler {
 public:
  AutoResizeTestHandler() {}

  void RunTest() override {
    // Add the resources that we will navigate to/from.
    AddResource(kAutoResizeUrl,
                "<html><head><style>"
                "body {overflow:hidden;margin:0px;padding:0px;}"
                "</style></head><body><div id=a>Content</div></body></html>",
                "text/html");

    // Create the browser.
    CreateBrowser(kAutoResizeUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    RoutingTestHandler::OnAfterCreated(browser);
    browser->GetHost()->SetAutoResizeEnabled(true, CefSize(10, 10),
                                             CefSize(500, 500));
  }

  bool OnAutoResize(CefRefPtr<CefBrowser> browser,
                    const CefSize& new_size) override {
    if (new_size.width == 1064 && new_size.height == 576) {
      // Ignore this initial resize that may or may not occur.
    } else if (!got_auto_resize1_) {
      got_auto_resize1_.yes();
      EXPECT_EQ(50, new_size.width);
      EXPECT_EQ(18, new_size.height);

      // Trigger a resize.
      browser->GetMainFrame()->ExecuteJavaScript(
          "document.getElementById('a').innerText='New Content';",
          kAutoResizeUrl, 0);
    } else if (!got_auto_resize2_) {
      got_auto_resize2_.yes();
      EXPECT_EQ(50, new_size.width);
      EXPECT_EQ(36, new_size.height);

      // Disable resize notifications.
      browser->GetHost()->SetAutoResizeEnabled(false, CefSize(), CefSize());

      // There should be no more resize notifications. End the test after a
      // short delay.
      browser->GetMainFrame()->ExecuteJavaScript(
          "document.getElementById('a').innerText='New Content Again';"
          "var interval = setInterval(function() {"
          "window.testQuery({request:'done'});clearInterval(interval);}, 50);",
          kAutoResizeUrl, 0);
    } else {
      EXPECT_TRUE(false);  // Not reached.
    }
    return true;
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    EXPECT_STREQ("done", request.ToString().c_str());
    EXPECT_FALSE(got_done_message_);
    got_done_message_.yes();
    DestroyTest();
    return true;
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_auto_resize1_);
    EXPECT_TRUE(got_auto_resize2_);
    EXPECT_TRUE(got_done_message_);
    TestHandler::DestroyTest();
  }

 private:
  TrackCallback got_auto_resize1_;
  TrackCallback got_auto_resize2_;
  TrackCallback got_done_message_;

  IMPLEMENT_REFCOUNTING(AutoResizeTestHandler);
};

}  // namespace

// Test OnAutoResize notification.
TEST(DisplayTest, AutoResize) {
  CefRefPtr<AutoResizeTestHandler> handler = new AutoResizeTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

// Browser side.
class ConsoleTestHandler : public TestHandler {
 public:
  struct TestConfig {
    // Use something other than 1 as |line| for testing.
    explicit TestConfig(cef_log_severity_t message_level)
        : level(message_level),
          message("'Test Message'"),
          expected_message("Test Message"),
          source("http://tests-console-message/level.html"),
          line(42) {}

    cef_log_severity_t level;
    std::string message;
    std::string expected_message;
    std::string source;
    int line;
    std::string function;
  };

  ConsoleTestHandler(const TestConfig& config) : config_(config) {}

  void RunTest() override {
    // Add the resources that will be used to print to console.
    AddResource(
        config_.source,
        CreateResourceContent(config_.message, config_.function, config_.line),
        "text/html");

    // Create the browser.
    CreateBrowser(config_.source);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (isLoading)
      return;

    // Print console message after loading.
    browser->GetMainFrame()->ExecuteJavaScript("printMessage()", config_.source,
                                               0);
  }

  bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                        cef_log_severity_t level,
                        const CefString& message,
                        const CefString& source,
                        int line) override {
    EXPECT_EQ(config_.level, level);
    EXPECT_EQ(config_.expected_message, message.ToString());
    EXPECT_EQ(config_.source, source.ToString());
    EXPECT_EQ(config_.line, line);

    TestHandler::DestroyTest();

    return false;
  }

 private:
  std::string CreateResourceContent(const CefString& message,
                                    const CefString& function,
                                    int line) {
    std::string content = "<html><script>function printMessage() { ";
    for (int i = 1; i < line; ++i) {
      // Add additional lines to test the |line| argument in |OnConsoleMessage|.
      content += ";\n";
    }
    content += "console." + function.ToString() + "(" + message.ToString() +
               "); }</script></html>";

    return content;
  }

  TestConfig config_;

  IMPLEMENT_REFCOUNTING(ConsoleTestHandler);
};

}  // namespace

TEST(DisplayTest, OnConsoleMessageDebug) {
  ConsoleTestHandler::TestConfig config(LOGSEVERITY_DEBUG);
  config.function = "debug";

  CefRefPtr<ConsoleTestHandler> handler = new ConsoleTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DisplayTest, OnConsoleMessageCount) {
  ConsoleTestHandler::TestConfig config(LOGSEVERITY_DEBUG);
  config.function = "count";
  config.expected_message = "Test Message: 1";

  CefRefPtr<ConsoleTestHandler> handler = new ConsoleTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DisplayTest, OnConsoleMessageTimeEnd) {
  ConsoleTestHandler::TestConfig config(LOGSEVERITY_WARNING);
  config.function = "timeEnd";
  config.expected_message = "Timer 'Test Message' does not exist";

  CefRefPtr<ConsoleTestHandler> handler = new ConsoleTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DisplayTest, OnConsoleMessageInfo) {
  ConsoleTestHandler::TestConfig config(LOGSEVERITY_INFO);
  config.function = "info";

  CefRefPtr<ConsoleTestHandler> handler = new ConsoleTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DisplayTest, OnConsoleMessageLog) {
  ConsoleTestHandler::TestConfig config(LOGSEVERITY_INFO);
  config.function = "log";

  CefRefPtr<ConsoleTestHandler> handler = new ConsoleTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DisplayTest, OnConsoleMessageGroup) {
  ConsoleTestHandler::TestConfig config(LOGSEVERITY_INFO);
  config.function = "group";

  CefRefPtr<ConsoleTestHandler> handler = new ConsoleTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DisplayTest, OnConsoleMessageGroupCollapsed) {
  ConsoleTestHandler::TestConfig config(LOGSEVERITY_INFO);
  config.function = "groupCollapsed";

  CefRefPtr<ConsoleTestHandler> handler = new ConsoleTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DisplayTest, OnConsoleMessageGroupEnd) {
  ConsoleTestHandler::TestConfig config(LOGSEVERITY_INFO);
  config.function = "groupEnd";

  CefRefPtr<ConsoleTestHandler> handler = new ConsoleTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DisplayTest, OnConsoleMessageTable) {
  ConsoleTestHandler::TestConfig config(LOGSEVERITY_INFO);
  config.function = "table";
  config.message = "[1, 2, 3]";
  config.expected_message = "1,2,3";

  CefRefPtr<ConsoleTestHandler> handler = new ConsoleTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DisplayTest, OnConsoleMessageTrace) {
  ConsoleTestHandler::TestConfig config(LOGSEVERITY_INFO);
  config.function = "trace";
  config.message = "";
  config.expected_message = "console.trace";

  CefRefPtr<ConsoleTestHandler> handler = new ConsoleTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DisplayTest, OnConsoleMessageWarn) {
  ConsoleTestHandler::TestConfig config(LOGSEVERITY_WARNING);
  config.function = "warn";

  CefRefPtr<ConsoleTestHandler> handler = new ConsoleTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DisplayTest, OnConsoleMessageError) {
  ConsoleTestHandler::TestConfig config(LOGSEVERITY_ERROR);
  config.function = "error";

  CefRefPtr<ConsoleTestHandler> handler = new ConsoleTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DisplayTest, OnConsoleMessageAssert) {
  ConsoleTestHandler::TestConfig config(LOGSEVERITY_ERROR);
  config.function = "assert";
  config.message = "false";
  config.expected_message = "console.assert";

  CefRefPtr<ConsoleTestHandler> handler = new ConsoleTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kLoadinProgressUrl[] = "http://tests-display/loading-progress.html";

// Browser side.
class LoadingProgressTestHandler : public TestHandler {
 public:
  LoadingProgressTestHandler() {}

  void RunTest() override {
    // Add the resources that we will navigate to/from.
    AddResource(kLoadinProgressUrl,
                "<html><head><style>"
                "body {overflow:hidden;margin:0px;padding:0px;}"
                "</style></head><body><div id=a>Content</div></body></html>",
                "text/html");

    // Create the browser.
    CreateBrowser(kLoadinProgressUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (isLoading)
      return;

    DestroyTest();
  }

  void OnLoadingProgressChange(CefRefPtr<CefBrowser> browser,
                               double progress) override {
    if (!got_loading_progress_change0_) {
      got_loading_progress_change0_.yes();
      EXPECT_GE(progress, 0.0);
    } else if (!got_loading_progress_change1_) {
      got_loading_progress_change1_.yes();
      EXPECT_LE(progress, 1.0);
    }
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_loading_progress_change0_);
    EXPECT_TRUE(got_loading_progress_change1_);
    TestHandler::DestroyTest();
  }

 private:
  TrackCallback got_loading_progress_change0_;
  TrackCallback got_loading_progress_change1_;

  IMPLEMENT_REFCOUNTING(LoadingProgressTestHandler);
};

}  // namespace

// Test OnLoadingProgressChange notification.
TEST(DisplayTest, LoadingProgress) {
  CefRefPtr<LoadingProgressTestHandler> handler =
      new LoadingProgressTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
