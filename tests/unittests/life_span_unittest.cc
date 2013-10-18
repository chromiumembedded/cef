// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_runnable.h"
#include "tests/unittests/test_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kLifeSpanUrl[] = "http://tests-life-span/test.html";
const char kUnloadDialogText[] = "Are you sure?";
const char kUnloadMsg[] = "LifeSpanTestHandler.Unload";

// Browser side.
class LifeSpanTestHandler : public TestHandler {
 public:
  struct Settings {
    Settings()
      : force_close(false),
        add_onunload_handler(false),
        allow_do_close(true),
        accept_before_unload_dialog(true) {}

    bool force_close;
    bool add_onunload_handler;
    bool allow_do_close;
    bool accept_before_unload_dialog;
  };

  explicit LifeSpanTestHandler(const Settings& settings)
      : settings_(settings),
        executing_delay_close_(false) {}

  virtual void RunTest() OVERRIDE {
    // Add the resources that we will navigate to/from.
    std::string page = "<html><script>";
    
    page += "window.onunload = function() { app.sendMessage('" +
            std::string(kUnloadMsg) + "'); };";

    if (settings_.add_onunload_handler) {
      page += "window.onbeforeunload = function() { return '" +
              std::string(kUnloadDialogText) + "'; };";
    }

    page += "</script><body>Page</body></html>";
    AddResource(kLifeSpanUrl, page, "text/html");

    // Create the browser.
    CreateBrowser(kLifeSpanUrl);
  }

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE {
    got_after_created_.yes();
    TestHandler::OnAfterCreated(browser);
  }

  virtual bool DoClose(CefRefPtr<CefBrowser> browser) OVERRIDE {
    if (executing_delay_close_)
      return false;

    EXPECT_TRUE(browser->IsSame(GetBrowser()));

    got_do_close_.yes();

    if (!settings_.allow_do_close) {
      // The close will be canceled.
      ScheduleDelayClose();
    }

    return !settings_.allow_do_close;
  }

  virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE {
    if (!executing_delay_close_) {
      got_before_close_.yes();
      EXPECT_TRUE(browser->IsSame(GetBrowser()));
    }

    TestHandler::OnBeforeClose(browser);
  }

  virtual bool OnBeforeUnloadDialog(
      CefRefPtr<CefBrowser> browser,
      const CefString& message_text,
      bool is_reload,
      CefRefPtr<CefJSDialogCallback> callback) OVERRIDE {
    if (executing_delay_close_) {
      callback->Continue(true, CefString());
      return true;
    }

    EXPECT_TRUE(browser->IsSame(GetBrowser()));
    EXPECT_STREQ(kUnloadDialogText, message_text.ToString().c_str());
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

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    got_load_end_.yes();
    EXPECT_TRUE(browser->IsSame(GetBrowser()));

    // Attempt to close the browser.
    browser->GetHost()->CloseBrowser(settings_.force_close);
  }

  virtual bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) OVERRIDE {
    const std::string& message_name = message->GetName();
    if (message_name == kUnloadMsg) {
      if (!executing_delay_close_)
        got_unload_message_.yes();
      return true;
    }

    return false;
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
    CefPostDelayedTask(TID_UI,
        NewCefRunnableMethod(this, &LifeSpanTestHandler::DelayClose), 100);
  }

  void DelayClose() {
    got_delay_close_.yes();
    executing_delay_close_ = true;
    DestroyTest();
  }

  Settings settings_;

  // Forces the window to close (bypasses test conditions).
  bool executing_delay_close_;
};

}  // namespace

TEST(LifeSpanTest, DoCloseAllow) {
  LifeSpanTestHandler::Settings settings;
  settings.allow_do_close = true;
  CefRefPtr<LifeSpanTestHandler> handler = new LifeSpanTestHandler(settings);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_after_created_);
  EXPECT_TRUE(handler->got_do_close_);
  EXPECT_TRUE(handler->got_before_close_);
  EXPECT_FALSE(handler->got_before_unload_dialog_);
  EXPECT_TRUE(handler->got_unload_message_);
  EXPECT_TRUE(handler->got_load_end_);
  EXPECT_FALSE(handler->got_delay_close_);
}

TEST(LifeSpanTest, DoCloseAllowForce) {
  LifeSpanTestHandler::Settings settings;
  settings.allow_do_close = true;
  settings.force_close = true;
  CefRefPtr<LifeSpanTestHandler> handler = new LifeSpanTestHandler(settings);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_after_created_);
  EXPECT_TRUE(handler->got_do_close_);
  EXPECT_TRUE(handler->got_before_close_);
  EXPECT_FALSE(handler->got_before_unload_dialog_);
  EXPECT_TRUE(handler->got_unload_message_);
  EXPECT_TRUE(handler->got_load_end_);
  EXPECT_FALSE(handler->got_delay_close_);
}

TEST(LifeSpanTest, DoCloseDisallow) {
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
}

TEST(LifeSpanTest, DoCloseDisallowForce) {
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
}

TEST(LifeSpanTest, DoCloseDisallowWithOnUnloadAllow) {
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
}

TEST(LifeSpanTest, DoCloseAllowWithOnUnloadForce) {
  LifeSpanTestHandler::Settings settings;
  settings.allow_do_close = true;
  settings.add_onunload_handler = true;
  settings.force_close = true;
  CefRefPtr<LifeSpanTestHandler> handler = new LifeSpanTestHandler(settings);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_after_created_);
  EXPECT_TRUE(handler->got_do_close_);
  EXPECT_TRUE(handler->got_before_close_);
  EXPECT_FALSE(handler->got_before_unload_dialog_);
  EXPECT_TRUE(handler->got_unload_message_);
  EXPECT_TRUE(handler->got_load_end_);
  EXPECT_FALSE(handler->got_delay_close_);
}

TEST(LifeSpanTest, DoCloseDisallowWithOnUnloadForce) {
  LifeSpanTestHandler::Settings settings;
  settings.allow_do_close = false;
  settings.add_onunload_handler = true;
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
}

TEST(LifeSpanTest, OnUnloadAllow) {
  LifeSpanTestHandler::Settings settings;
  settings.add_onunload_handler = true;
  settings.accept_before_unload_dialog = true;
  CefRefPtr<LifeSpanTestHandler> handler = new LifeSpanTestHandler(settings);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_after_created_);
  EXPECT_TRUE(handler->got_do_close_);
  EXPECT_TRUE(handler->got_before_close_);
  EXPECT_TRUE(handler->got_before_unload_dialog_);
  EXPECT_TRUE(handler->got_unload_message_);
  EXPECT_TRUE(handler->got_load_end_);
  EXPECT_FALSE(handler->got_delay_close_);
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
}
