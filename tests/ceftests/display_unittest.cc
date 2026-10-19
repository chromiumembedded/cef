// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <list>

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

// How it works:
// 1. Load kTitleUrl1 (title should be kTitleStr1)
// 2. Load kTitleUrl2 (title should be kTitleStr2)
// 3. History back to kTitleUrl1 (title should be kTitleStr1)
// 4. History forward to kTitleUrl2 (title should be kTitleStr2)
// 5. Set title via JavaScript (title should be kTitleStr3)

const char kTitleUrl1[] = "https://tests-title/nav1.html";
const char kTitleUrl2[] = "https://tests-title/nav2.html";
const char kTitleStr1[] = "Title 1";
const char kTitleStr2[] = "Title 2";
const char kTitleStr3[] = "Title 3";

// Browser side.
class TitleTestHandler : public TestHandler {
 public:
  TitleTestHandler() = default;

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
    CreateReusableBrowser(kTitleUrl1);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override {
    // Title updates and load completion use different notification paths.
    // Ignore stale/duplicate updates, but require the expected title for each
    // document before advancing. A missing title still times out the test.
    if (got_title_[step_] ||
        browser->GetMainFrame()->GetURL() != ExpectedURL()) {
      return;
    }

    std::string title_str = title;
    const char* expected = step_ == 4       ? kTitleStr3
                           : step_ % 2 == 0 ? kTitleStr1
                                            : kTitleStr2;
    if (title_str != expected) {
      return;
    }

    got_title_[step_].yes();

    if (step_ == 4) {
      DestroyTest();
    } else {
      CefPostTask(TID_UI, base::BindOnce(&TitleTestHandler::NextIfReady, this,
                                         browser));
    }
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (isLoading || browser->GetMainFrame()->GetURL() != ExpectedURL()) {
      return;
    }

    // Call NextIfReady asynchronously because an additional call to
    // OnTitleChange will be triggered later in the current call stack due to
    // navigation completion and we want that call to arrive before execution of
    // NextIfReady.
    got_loading_state_change_ = true;
    CefPostTask(TID_UI,
                base::BindOnce(&TitleTestHandler::NextIfReady, this, browser));
  }

 private:
  const char* ExpectedURL() const {
    return step_ == 0 || step_ == 2 ? kTitleUrl1 : kTitleUrl2;
  }

  void NextIfReady(CefRefPtr<CefBrowser> browser) {
    if (step_ >= 4 || !got_title_[step_] || !got_loading_state_change_ ||
        browser->IsLoading() ||
        browser->GetMainFrame()->GetURL() != ExpectedURL()) {
      return;
    }

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
    for (int i = 0; i < 5; ++i) {
      EXPECT_TRUE(got_title_[i]) << "step " << i;
    }

    TestHandler::DestroyTest();
  }

  int step_ = 0;

  bool got_loading_state_change_ = false;

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

#if !defined(OS_LINUX)
namespace {

const char kAutoResizeUrl[] = "https://tests-display/auto-resize.html";

class AutoResizeTestHandler : public RoutingTestHandler {
 public:
  AutoResizeTestHandler() = default;

  void RunTest() override {
    // Add the resources that we will navigate to/from.
    AddResource(kAutoResizeUrl,
                "<html><head><style>"
                "html,body {overflow:hidden;margin:0;padding:0;}"
                "#a {width:50px;height:18px;background:red;}"
                "</style></head><body><div id=a></div></body></html>",
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
    if (!got_auto_resize1_) {
      // Initial empty-document layouts may be reported before page layout.
      if (new_size.width != 50 || new_size.height != 18) {
        return true;
      }
      got_auto_resize1_.yes();
      EXPECT_NEAR(50, new_size.width, 1);
      EXPECT_NEAR(18, new_size.height, 1);

      // Trigger a resize.
      browser->GetMainFrame()->ExecuteJavaScript(
          "requestAnimationFrame(() => requestAnimationFrame(() => "
          "document.getElementById('a').style.height='37px'));",
          kAutoResizeUrl, 0);
    } else if (!got_auto_resize2_) {
      if (new_size.width == 50 && new_size.height == 18) {
        return true;
      }
      got_auto_resize2_.yes();
      EXPECT_NEAR(50, new_size.width, 1);
      EXPECT_NEAR(37, new_size.height, 1);

      // Let the resized surface be submitted before disabling auto-resize.
      // Toggling it inside this callback can race the renderer's resize.
      browser->GetMainFrame()->ExecuteJavaScript(
          "requestAnimationFrame(() => requestAnimationFrame(() => "
          "window.testQuery({request:'disable'})));",
          kAutoResizeUrl, 0);
    } else if (new_size.width == 50 && new_size.height == 37) {
      // Duplicate notifications for the completed layout may already be queued.
    } else {
      EXPECT_TRUE(false);  // Not reached.
    }
    return true;
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    callback->Success("");
    if (request == "disable") {
      EXPECT_TRUE(got_auto_resize2_);
      browser->GetHost()->SetAutoResizeEnabled(false, CefSize(), CefSize());
      browser->GetMainFrame()->ExecuteJavaScript(
          "document.getElementById('a').style.height='60px';"
          "requestAnimationFrame(() => requestAnimationFrame(() => "
          "window.testQuery({request:'done'})));",
          kAutoResizeUrl, 0);
      return true;
    }
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
// Disabled on Linux because it causes DCHECK failures in the renderer process.
// See issue #4038.
TEST(DisplayTest, AutoResize) {
  CefRefPtr<AutoResizeTestHandler> handler = new AutoResizeTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
#endif  // !defined(OS_LINUX)

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
          source("https://tests-console-message/level.html") {}

    cef_log_severity_t level;
    std::string message;
    std::string expected_message;
    std::string source;
    int line = 42;
    std::string function;
  };

  explicit ConsoleTestHandler(const TestConfig& config) : config_(config) {}

  void RunTest() override {
    // Add the resources that will be used to print to console.
    AddResource(
        config_.source,
        CreateResourceContent(config_.message, config_.function, config_.line),
        "text/html");

    // Create the browser.
    CreateReusableBrowser(config_.source);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (isLoading) {
      return;
    }

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

const char kLoadinProgressUrl[] = "https://tests-display/loading-progress.html";

// Browser side.
class LoadingProgressTestHandler : public TestHandler {
 public:
  LoadingProgressTestHandler() = default;

  void RunTest() override {
    // Add the resources that we will navigate to/from.
    AddResource(kLoadinProgressUrl,
                "<html><head><style>"
                "body {overflow:hidden;margin:0px;padding:0px;}"
                "</style></head><body><div id=a>Content</div></body></html>",
                "text/html");

    // Create the browser.
    CreateReusableBrowser(kLoadinProgressUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (isLoading) {
      return;
    }

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
