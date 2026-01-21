// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/test/cef_test_helpers.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

const char kLifeSpanUrl[] = "https://tests-life-span/test.html";
const char kUnloadDialogText[] = "Are you sure?";
const char kUnloadMsg[] = "LifeSpanTestHandler.Unload";

// Browser side.
class LifeSpanTestHandler : public RoutingTestHandler {
 public:
  struct Settings {
    Settings() = default;

    bool force_close = false;
    bool add_onunload_handler = false;
    bool allow_do_close = true;
    bool accept_before_unload_dialog = true;
  };

  explicit LifeSpanTestHandler(const Settings& settings) : settings_(settings) {
    // By default no LifeSpan tests call DestroyTest().
    SetDestroyTestExpected(false);
  }

  void RunTest() override {
    // Add the resources that we will navigate to/from.
    std::string page = "<html><script>";

    page += "window.onunload = function() { window.testQuery({request:'" +
            std::string(kUnloadMsg) + "'}); };";

    if (settings_.add_onunload_handler) {
      page += "window.onbeforeunload = function() { return '" +
              std::string(kUnloadDialogText) + "'; };";
    }

    page += "</script><body>Page</body></html>";
    AddResource(kLifeSpanUrl, page, "text/html");

    // Create the browser.
    CreateBrowser(kLifeSpanUrl);

    // Intentionally don't call SetTestTimeout() for these tests.
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    got_after_created_.yes();
    RoutingTestHandler::OnAfterCreated(browser);
  }

  bool DoClose(CefRefPtr<CefBrowser> browser) override {
    EXPECT_TRUE(browser->GetHost()->IsReadyToBeClosed());

    if (executing_delay_close_) {
      return false;
    }

    EXPECT_TRUE(browser->IsSame(GetBrowser()));

    got_do_close_.yes();

    if (!settings_.allow_do_close) {
      // The close will be canceled.
      ScheduleDelayClose();
    }

    return !settings_.allow_do_close;
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    EXPECT_TRUE(browser->GetHost()->IsReadyToBeClosed());

    if (!executing_delay_close_) {
      got_before_close_.yes();
      EXPECT_TRUE(browser->IsSame(GetBrowser()));
    }

    RoutingTestHandler::OnBeforeClose(browser);
  }

  bool OnBeforeUnloadDialog(CefRefPtr<CefBrowser> browser,
                            const CefString& message_text,
                            bool is_reload,
                            CefRefPtr<CefJSDialogCallback> callback) override {
    EXPECT_FALSE(browser->GetHost()->IsReadyToBeClosed());

    if (executing_delay_close_) {
      callback->Continue(true, CefString());
      return true;
    }

    EXPECT_TRUE(browser->IsSame(GetBrowser()));

    // The message is no longer configurable via JavaScript.
    // See https://crbug.com/587940.
    EXPECT_STREQ("Is it OK to leave/reload this page?",
                 message_text.ToString().c_str());

    EXPECT_FALSE(is_reload);
    EXPECT_TRUE(callback.get());

    if (!settings_.accept_before_unload_dialog) {
      // The close will be canceled.
      ScheduleDelayClose();
    }

    got_before_unload_dialog_.yes();
    callback->Continue(settings_.accept_before_unload_dialog, CefString());
    return true;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    got_load_end_.yes();
    EXPECT_TRUE(browser->IsSame(GetBrowser()));

    if (settings_.add_onunload_handler) {
      // Send the page a user gesture to enable firing of the onbefureunload
      // handler. See https://crbug.com/707007.
      CefExecuteJavaScriptWithUserGestureForTests(frame, CefString());
    }

    EXPECT_FALSE(browser->GetHost()->IsReadyToBeClosed());

    // Attempt to close the browser.
    CloseBrowser(browser, settings_.force_close);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    if (request.ToString() == kUnloadMsg) {
      if (!executing_delay_close_) {
        got_unload_message_.yes();
      }
    }
    callback->Success("");
    return true;
  }

  TrackCallback got_after_created_;
  TrackCallback got_do_close_;
  TrackCallback got_before_close_;
  TrackCallback got_before_unload_dialog_;
  TrackCallback got_unload_message_;
  TrackCallback got_load_end_;
  TrackCallback got_delay_close_;

 private:
  // Wait a bit to make sure no additional events are received and then close
  // the window.
  void ScheduleDelayClose() {
    // This test will call DestroyTest().
    SetDestroyTestExpected(true);

    CefPostDelayedTask(
        TID_UI, base::BindOnce(&LifeSpanTestHandler::DelayClose, this), 100);
  }

  void DelayClose() {
    got_delay_close_.yes();
    executing_delay_close_ = true;
    DestroyTest();
  }

  Settings settings_;

  // Forces the window to close (bypasses test conditions).
  bool executing_delay_close_ = false;

  IMPLEMENT_REFCOUNTING(LifeSpanTestHandler);
};

}  // namespace

TEST(LifeSpanTest, DoCloseAllow) {
  LifeSpanTestHandler::Settings settings;
  CefRefPtr<LifeSpanTestHandler> handler = new LifeSpanTestHandler(settings);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_after_created_);
  if (handler->use_alloy_style_browser()) {
    EXPECT_TRUE(handler->got_do_close_);
    // Delivery of the testQuery message from the onunload event races browser
    // destruction with Chrome style browsers, see issue #4037.
    EXPECT_TRUE(handler->got_unload_message_);
  } else {
    EXPECT_FALSE(handler->got_do_close_);
  }
  EXPECT_TRUE(handler->got_before_close_);
  EXPECT_FALSE(handler->got_before_unload_dialog_);
  EXPECT_TRUE(handler->got_load_end_);
  EXPECT_FALSE(handler->got_delay_close_);

  ReleaseAndWaitForDestructor(handler);
}

TEST(LifeSpanTest, DoCloseAllowForce) {
  LifeSpanTestHandler::Settings settings;
  settings.force_close = true;
  CefRefPtr<LifeSpanTestHandler> handler = new LifeSpanTestHandler(settings);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_after_created_);
  if (handler->use_alloy_style_browser()) {
    EXPECT_TRUE(handler->got_do_close_);
    // Delivery of the testQuery message from the onunload event races browser
    // destruction with Chrome style browsers, see issue #4037.
    EXPECT_TRUE(handler->got_unload_message_);
  } else {
    EXPECT_FALSE(handler->got_do_close_);
  }
  EXPECT_TRUE(handler->got_before_close_);
  EXPECT_FALSE(handler->got_before_unload_dialog_);
  EXPECT_TRUE(handler->got_load_end_);
  EXPECT_FALSE(handler->got_delay_close_);

  ReleaseAndWaitForDestructor(handler);
}

TEST(LifeSpanTest, DoCloseDisallow) {
  // Test not supported with Chrome style browser.
  if (!UseAlloyStyleBrowserGlobal()) {
    return;
  }

  LifeSpanTestHandler::Settings settings;
  settings.allow_do_close = false;
  CefRefPtr<LifeSpanTestHandler> handler = new LifeSpanTestHandler(settings);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_after_created_);
  EXPECT_TRUE(handler->got_do_close_);
  EXPECT_FALSE(handler->got_before_close_);
  EXPECT_FALSE(handler->got_before_unload_dialog_);
  EXPECT_TRUE(handler->got_unload_message_);
  EXPECT_TRUE(handler->got_load_end_);
  EXPECT_TRUE(handler->got_delay_close_);

  ReleaseAndWaitForDestructor(handler);
}

TEST(LifeSpanTest, DoCloseDisallowForce) {
  // Test not supported with Chrome style browser.
  if (!UseAlloyStyleBrowserGlobal()) {
    return;
  }

  LifeSpanTestHandler::Settings settings;
  settings.allow_do_close = false;
  settings.force_close = true;
  CefRefPtr<LifeSpanTestHandler> handler = new LifeSpanTestHandler(settings);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_after_created_);
  EXPECT_TRUE(handler->got_do_close_);
  EXPECT_FALSE(handler->got_before_close_);
  EXPECT_FALSE(handler->got_before_unload_dialog_);
  EXPECT_TRUE(handler->got_unload_message_);
  EXPECT_TRUE(handler->got_load_end_);
  EXPECT_TRUE(handler->got_delay_close_);

  ReleaseAndWaitForDestructor(handler);
}

TEST(LifeSpanTest, DoCloseDisallowWithOnUnloadAllow) {
  // Test not supported with Chrome style browser.
  if (!UseAlloyStyleBrowserGlobal()) {
    return;
  }

  LifeSpanTestHandler::Settings settings;
  settings.allow_do_close = false;
  settings.add_onunload_handler = true;
  settings.accept_before_unload_dialog = true;
  CefRefPtr<LifeSpanTestHandler> handler = new LifeSpanTestHandler(settings);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_after_created_);
  EXPECT_TRUE(handler->got_do_close_);
  EXPECT_FALSE(handler->got_before_close_);
  EXPECT_TRUE(handler->got_before_unload_dialog_);
  EXPECT_TRUE(handler->got_unload_message_);
  EXPECT_TRUE(handler->got_load_end_);
  EXPECT_TRUE(handler->got_delay_close_);

  ReleaseAndWaitForDestructor(handler);
}

TEST(LifeSpanTest, DoCloseAllowWithOnUnloadForce) {
  LifeSpanTestHandler::Settings settings;
  settings.add_onunload_handler = true;
  settings.force_close = true;
  CefRefPtr<LifeSpanTestHandler> handler = new LifeSpanTestHandler(settings);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_after_created_);
  if (handler->use_alloy_style_browser()) {
    EXPECT_TRUE(handler->got_do_close_);
    // Delivery of the testQuery message from the onunload event races browser
    // destruction with Chrome style browsers, see issue #4037.
    EXPECT_TRUE(handler->got_unload_message_);
  } else {
    EXPECT_FALSE(handler->got_do_close_);
  }
  EXPECT_TRUE(handler->got_before_close_);
  EXPECT_TRUE(handler->got_before_unload_dialog_);
  EXPECT_TRUE(handler->got_load_end_);
  EXPECT_FALSE(handler->got_delay_close_);

  ReleaseAndWaitForDestructor(handler);
}

TEST(LifeSpanTest, DoCloseDisallowWithOnUnloadForce) {
  // Test not supported with Chrome style browser.
  if (!UseAlloyStyleBrowserGlobal()) {
    return;
  }

  LifeSpanTestHandler::Settings settings;
  settings.allow_do_close = false;
  settings.add_onunload_handler = true;
  settings.force_close = true;
  CefRefPtr<LifeSpanTestHandler> handler = new LifeSpanTestHandler(settings);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_after_created_);
  EXPECT_TRUE(handler->got_do_close_);
  EXPECT_FALSE(handler->got_before_close_);
  EXPECT_TRUE(handler->got_before_unload_dialog_);
  EXPECT_TRUE(handler->got_unload_message_);
  EXPECT_TRUE(handler->got_load_end_);
  EXPECT_TRUE(handler->got_delay_close_);

  ReleaseAndWaitForDestructor(handler);
}

TEST(LifeSpanTest, OnUnloadAllow) {
  LifeSpanTestHandler::Settings settings;
  settings.add_onunload_handler = true;
  settings.accept_before_unload_dialog = true;
  CefRefPtr<LifeSpanTestHandler> handler = new LifeSpanTestHandler(settings);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_after_created_);
  if (handler->use_alloy_style_browser()) {
    EXPECT_TRUE(handler->got_do_close_);
    // Delivery of the testQuery message from the onunload event races browser
    // destruction with Chrome style browsers, see issue #4037.
    EXPECT_TRUE(handler->got_unload_message_);
  } else {
    EXPECT_FALSE(handler->got_do_close_);
  }
  EXPECT_TRUE(handler->got_before_close_);
  EXPECT_TRUE(handler->got_before_unload_dialog_);
  EXPECT_TRUE(handler->got_load_end_);
  EXPECT_FALSE(handler->got_delay_close_);

  ReleaseAndWaitForDestructor(handler);
}

TEST(LifeSpanTest, OnUnloadDisallow) {
  LifeSpanTestHandler::Settings settings;
  settings.add_onunload_handler = true;
  settings.accept_before_unload_dialog = false;
  CefRefPtr<LifeSpanTestHandler> handler = new LifeSpanTestHandler(settings);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_after_created_);
  EXPECT_FALSE(handler->got_do_close_);
  EXPECT_FALSE(handler->got_before_close_);
  EXPECT_TRUE(handler->got_before_unload_dialog_);
  EXPECT_FALSE(handler->got_unload_message_);
  EXPECT_TRUE(handler->got_load_end_);
  EXPECT_TRUE(handler->got_delay_close_);

  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kDevToolsTestUrl[] = "https://tests-devtools/test.html";
const cef_color_t kDevToolsBackgroundColor = CefColorSetARGB(255, 128, 64, 32);

// Test OnBeforeDevToolsPopup callback
class DevToolsPopupTestHandler : public TestHandler {
 public:
  DevToolsPopupTestHandler() = default;

  DevToolsPopupTestHandler(const DevToolsPopupTestHandler&) = delete;
  DevToolsPopupTestHandler& operator=(const DevToolsPopupTestHandler&) = delete;

  void RunTest() override {
    const std::string html =
        "<html>"
        "<head><title>DevTools Test</title></head>"
        "<body><h1>DevTools Popup Test</h1></body>"
        "</html>";

    AddResource(kDevToolsTestUrl, html, "text/html");
    CreateBrowser(kDevToolsTestUrl);
    SetTestTimeout();
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);

    if (!main_browser_) {
      // First browser created is the main browser
      got_main_after_created_.yes();
      main_browser_ = browser;
    } else {
      // Second browser is the DevTools browser
      EXPECT_FALSE(got_devtools_after_created_);
      got_devtools_after_created_.yes();
      EXPECT_TRUE(browser->IsPopup());
      devtools_browser_ = browser;
    }
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (!frame->IsMain()) {
      return;
    }

    if (browser->IsSame(main_browser_)) {
      EXPECT_FALSE(got_main_load_end_);
      got_main_load_end_.yes();

      // Open DevTools after main browser loads
      CefWindowInfo windowInfo;
      CefBrowserSettings settings;

#if defined(OS_WIN)
      windowInfo.SetAsPopup(nullptr, "DevTools");
#endif

      // Set a custom background color to verify it's passed to the callback
      settings.background_color = kDevToolsBackgroundColor;

      main_browser_->GetHost()->ShowDevTools(windowInfo, this, settings,
                                             CefPoint());
    } else if (devtools_browser_ && browser->IsSame(devtools_browser_)) {
      EXPECT_FALSE(got_devtools_load_end_);
      got_devtools_load_end_.yes();

      // Close DevTools browser after it loads
      CefPostTask(TID_UI, base::BindOnce(
                              &DevToolsPopupTestHandler::CloseDevTools, this));
    }
  }

  void OnBeforeDevToolsPopup(CefRefPtr<CefBrowser> browser,
                             CefWindowInfo& windowInfo,
                             CefRefPtr<CefClient>& client,
                             CefBrowserSettings& settings,
                             CefRefPtr<CefDictionaryValue>& extra_info,
                             bool* use_default_window) override {
    EXPECT_FALSE(got_before_devtools_popup_);
    got_before_devtools_popup_.yes();

    EXPECT_TRUE(browser.get());
    EXPECT_TRUE(browser->IsSame(main_browser_));
    EXPECT_EQ(client.get(), this);
    EXPECT_TRUE(use_default_window);

    // Verify settings passed to ShowDevTools are received in the callback
    EXPECT_EQ(settings.background_color, kDevToolsBackgroundColor);
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    if (devtools_browser_ && browser->IsSame(devtools_browser_)) {
      got_devtools_before_close_.yes();
      devtools_browser_ = nullptr;

      // Close the main browser after DevTools is closed
      CefPostTask(
          TID_UI,
          base::BindOnce(&DevToolsPopupTestHandler::CloseMainBrowser, this));
    } else if (browser->IsSame(main_browser_)) {
      got_main_before_close_.yes();
      main_browser_ = nullptr;

      // Test is complete after main browser closes
      DestroyTest();
    }
    TestHandler::OnBeforeClose(browser);
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_main_after_created_);
    EXPECT_TRUE(got_main_load_end_);
    EXPECT_TRUE(got_before_devtools_popup_);
    EXPECT_TRUE(got_devtools_after_created_);
    EXPECT_TRUE(got_devtools_load_end_);
    EXPECT_TRUE(got_devtools_before_close_);
    EXPECT_TRUE(got_main_before_close_);

    TestHandler::DestroyTest();
  }

 private:
  void CloseDevTools() {
    if (devtools_browser_) {
      devtools_browser_->GetHost()->CloseBrowser(false);
    }
  }

  void CloseMainBrowser() {
    if (main_browser_) {
      main_browser_->GetHost()->CloseBrowser(false);
    }
  }

  CefRefPtr<CefBrowser> main_browser_;
  CefRefPtr<CefBrowser> devtools_browser_;

  TrackCallback got_main_after_created_;
  TrackCallback got_main_load_end_;
  TrackCallback got_before_devtools_popup_;
  TrackCallback got_devtools_after_created_;
  TrackCallback got_devtools_load_end_;
  TrackCallback got_devtools_before_close_;
  TrackCallback got_main_before_close_;

  IMPLEMENT_REFCOUNTING(DevToolsPopupTestHandler);
};

}  // namespace

// This works with both Chrome and Alloy style main browsers because the
// DevTools popup is always Chrome style.
TEST(LifeSpanTest, OnBeforeDevToolsPopup) {
  CefRefPtr<DevToolsPopupTestHandler> handler = new DevToolsPopupTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
