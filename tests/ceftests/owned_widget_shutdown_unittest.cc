// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/test/cef_test_helpers.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

#if CEF_API_ADDED(CEF_EXPERIMENTAL)

namespace {

constexpr char kTestUrl[] = "https://tests/OwnedWidgetShutdownTestHandler.html";
constexpr char kPasswordSetupUrl[] =
    "https://tests/OwnedWidgetShutdownPasswordSetup.html";
constexpr char kPasswordFormUrl[] =
    "https://password-test/OwnedWidgetShutdownTestHandler.html";
constexpr char kPasswordResultUrl[] =
    "https://password-test/OwnedWidgetShutdownResult.html";
constexpr int kOwnedWidgetDisplayDelayMs = 500;

class JSDialogShutdownTestHandler : public TestHandler {
 public:
  void RunTest() override {
    AddResource(kTestUrl, "<html><body>Test</body></html>", "text/html");
    CreateBrowser(kTestUrl);
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int http_status_code) override {
    if (!frame->IsMain() || dialog_requested_) {
      return;
    }

    initial_owned_widget_count_ =
        CefGetChromeBrowserOwnedWidgetCountForTests(browser);
    ASSERT_GE(initial_owned_widget_count_, 0);

    dialog_requested_.yes();
    CefExecuteJavaScriptWithUserGestureForTests(frame,
                                                "alert('Owned widget test')");
  }

  bool OnJSDialog(CefRefPtr<CefBrowser> browser,
                  const CefString& origin_url,
                  JSDialogType dialog_type,
                  const CefString& message_text,
                  const CefString& default_prompt_text,
                  CefRefPtr<CefJSDialogCallback> callback,
                  bool& suppress_message) override {
    EXPECT_EQ(JSDIALOGTYPE_ALERT, dialog_type);
    EXPECT_EQ("Owned widget test", message_text.ToString());
    EXPECT_FALSE(suppress_message);
    got_js_dialog_.yes();

    // Returning false uses the default Chromium dialog. Wait until its Widget
    // is owned by the browser before closing the browser window.
    GetUIThreadHelper()->PostTask(
        base::BindOnce(&JSDialogShutdownTestHandler::WaitForOwnedWidget,
                       base::Unretained(this)));
    return false;
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    EXPECT_TRUE(owned_widget_observed_);
    browser_teardown_.yes();
    TestHandler::OnBeforeClose(browser);
  }

  TrackCallback dialog_requested_;
  TrackCallback got_js_dialog_;
  TrackCallback owned_widget_observed_;
  TrackCallback browser_teardown_;

 private:
  void WaitForOwnedWidget() {
    auto browser = GetBrowser();
    if (!browser) {
      return;
    }

    const int count = CefGetChromeBrowserOwnedWidgetCountForTests(browser);
    if (count > initial_owned_widget_count_) {
      owned_widget_observed_.yes();
      GetUIThreadHelper()->PostDelayedTask(
          base::BindOnce(&JSDialogShutdownTestHandler::CloseBrowser,
                         base::Unretained(this)),
          kOwnedWidgetDisplayDelayMs);
      return;
    }

    GetUIThreadHelper()->PostDelayedTask(
        base::BindOnce(&JSDialogShutdownTestHandler::WaitForOwnedWidget,
                       base::Unretained(this)),
        10);
  }

  void CloseBrowser() {
    EXPECT_TRUE(owned_widget_observed_);
    DestroyTest();
  }

  int initial_owned_widget_count_ = -1;

  IMPLEMENT_REFCOUNTING(JSDialogShutdownTestHandler);
};

class SavePasswordShutdownTestHandler : public TestHandler,
                                        public CefCompletionCallback {
 public:
  void RunTest() override {
    AddResource(kPasswordSetupUrl, "<html><body>Setup</body></html>",
                "text/html");
    AddResource(
        kPasswordFormUrl,
        "<html><body>"
        "<form method='post' action='" +
            std::string(kPasswordResultUrl) +
            "'>"
            "<input id='username' name='username' autocomplete='username'>"
            "<input id='password' name='password' type='password' "
            "autocomplete='current-password'>"
            "<button id='submit' type='submit'>Login</button>"
            "</form>"
            "</body></html>",
        "text/html");
    AddResource(kPasswordResultUrl, "<html><body>Logged in</body></html>",
                "text/html");
    CreateBrowser(kPasswordSetupUrl);
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int http_status_code) override {
    if (!frame->IsMain()) {
      return;
    }

    const std::string url = frame->GetURL();
    if (url == kPasswordSetupUrl && !stats_clear_requested_) {
      stats_clear_requested_.yes();
      CefClearChromeBrowserPasswordDismissalStatsForTests(
          browser, kPasswordFormUrl, this);
    } else if (url == kPasswordFormUrl && !form_submitted_) {
      initial_owned_widget_count_ =
          CefGetChromeBrowserOwnedWidgetCountForTests(browser);
      ASSERT_GE(initial_owned_widget_count_, 0);

      form_submitted_.yes();
      CefExecuteJavaScriptWithUserGestureForTests(
          frame,
          "document.getElementById('username').value='test-user';"
          "document.getElementById('password').value='test-password';"
          "document.getElementById('submit').click();");
    } else if (url == kPasswordResultUrl && !result_loaded_) {
      result_loaded_.yes();
      GetUIThreadHelper()->PostTask(
          base::BindOnce(&SavePasswordShutdownTestHandler::WaitForOwnedWidget,
                         base::Unretained(this)));
    }
  }

  void OnComplete() override {
    stats_cleared_.yes();

    auto browser = GetBrowser();
    ASSERT_TRUE(browser);
    auto frame = browser->GetMainFrame();
    ASSERT_TRUE(frame);
    frame->LoadURL(kPasswordFormUrl);
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    EXPECT_TRUE(password_widget_observed_);
    browser_teardown_.yes();
    TestHandler::OnBeforeClose(browser);
  }

  TrackCallback stats_clear_requested_;
  TrackCallback stats_cleared_;
  TrackCallback form_submitted_;
  TrackCallback result_loaded_;
  TrackCallback password_widget_observed_;
  TrackCallback browser_teardown_;

 private:
  void WaitForOwnedWidget() {
    auto browser = GetBrowser();
    if (!browser) {
      return;
    }

    const int count = CefGetChromeBrowserOwnedWidgetCountForTests(browser);
    if (count > initial_owned_widget_count_) {
      password_widget_observed_.yes();
      GetUIThreadHelper()->PostDelayedTask(
          base::BindOnce(&SavePasswordShutdownTestHandler::CloseBrowser,
                         base::Unretained(this)),
          kOwnedWidgetDisplayDelayMs);
      return;
    }

    GetUIThreadHelper()->PostDelayedTask(
        base::BindOnce(&SavePasswordShutdownTestHandler::WaitForOwnedWidget,
                       base::Unretained(this)),
        10);
  }

  void CloseBrowser() {
    EXPECT_TRUE(password_widget_observed_);
    DestroyTest();
  }

  int initial_owned_widget_count_ = -1;

  IMPLEMENT_REFCOUNTING(SavePasswordShutdownTestHandler);
};

class OwnedWindowObserver {
 public:
  virtual void OnOwnedWindowCreated(CefRefPtr<CefWindow> window) = 0;
  virtual void OnOwnedWindowDestroyed() = 0;

 protected:
  virtual ~OwnedWindowObserver() = default;
};

class OwnedWindowDelegate : public CefWindowDelegate {
 public:
  OwnedWindowDelegate(CefRefPtr<CefWindow> parent_window,
                      OwnedWindowObserver* observer)
      : parent_window_(parent_window), observer_(observer) {}

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window->Show();
    if (observer_) {
      observer_->OnOwnedWindowCreated(window);
    }
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    parent_window_ = nullptr;
    if (observer_) {
      observer_->OnOwnedWindowDestroyed();
    }
  }

  void DetachObserver() { observer_ = nullptr; }

  CefRefPtr<CefWindow> GetParentWindow(CefRefPtr<CefWindow> window,
                                       bool* is_menu,
                                       bool* can_activate_menu) override {
    *is_menu = true;
    *can_activate_menu = true;
    return parent_window_;
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(200, 100);
  }

 private:
  CefRefPtr<CefWindow> parent_window_;
  OwnedWindowObserver* observer_;

  IMPLEMENT_REFCOUNTING(OwnedWindowDelegate);
};

class SyntheticOwnedWindowTestHandler : public TestHandler,
                                        public OwnedWindowObserver {
 public:
  SyntheticOwnedWindowTestHandler() {
    // The test closes the CefWindow directly to exercise native-window-driven
    // teardown instead of calling DestroyTest().
    SetDestroyTestExpected(false);
  }

  ~SyntheticOwnedWindowTestHandler() override {
    if (owned_window_delegate_) {
      // The native window may outlive the test after an assertion failure.
      owned_window_delegate_->DetachObserver();
    }
  }

  void RunTest() override {
    AddResource(kTestUrl, "<html><body>Test</body></html>", "text/html");
    CreateBrowser(kTestUrl);
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int http_status_code) override {
    if (!frame->IsMain() || owned_window_delegate_) {
      return;
    }

    auto browser_view = CefBrowserView::GetForBrowser(browser);
    ASSERT_TRUE(browser_view);
    main_window_ = browser_view->GetWindow();
    ASSERT_TRUE(main_window_);

    initial_owned_widget_count_ =
        CefGetChromeBrowserOwnedWidgetCountForTests(browser);
    ASSERT_GE(initial_owned_widget_count_, 0);

    owned_window_delegate_ = new OwnedWindowDelegate(main_window_, this);
    CefWindow::CreateTopLevelWindow(owned_window_delegate_);
  }

  void OnWindowDestroyed(int browser_id) override {
    EXPECT_TRUE(owned_window_created_);
    main_window_destroyed_.yes();
    main_window_ = nullptr;

    if (owned_window_destroyed_) {
      TestHandler::OnWindowDestroyed(browser_id);
      return;
    }

    // Some platforms don't expose top-level CefWindow ownership via
    // Widget::ForEachOwnedWidget. Close the remaining window explicitly and
    // keep the handler alive until its destruction callback completes.
    pending_browser_id_ = browser_id;
    EXPECT_TRUE(owned_window_);
    if (owned_window_) {
      owned_window_->Close();
    }
  }

  void OnOwnedWindowCreated(CefRefPtr<CefWindow> window) override {
    owned_window_created_.yes();
    owned_window_ = window;
    owned_widget_observed_ = CefGetChromeBrowserOwnedWidgetCountForTests(
                                 GetBrowser()) > initial_owned_widget_count_;

    // Close asynchronously so OnWindowCreated has completed and the native
    // ownership relationship is fully established.
    GetUIThreadHelper()->PostDelayedTask(
        base::BindOnce(&SyntheticOwnedWindowTestHandler::CloseMainWindow,
                       base::Unretained(this)),
        kOwnedWidgetDisplayDelayMs);
  }

  void OnOwnedWindowDestroyed() override {
    owned_window_destroyed_before_main_ = !main_window_destroyed_;
    owned_window_destroyed_.yes();
    owned_window_ = nullptr;

    if (pending_browser_id_) {
      const int browser_id = pending_browser_id_;
      pending_browser_id_ = 0;
      TestHandler::OnWindowDestroyed(browser_id);
    }
  }

  TrackCallback owned_window_created_;
  TrackCallback owned_window_destroyed_;
  TrackCallback main_window_destroyed_;
  bool owned_widget_observed_ = false;
  bool owned_window_destroyed_before_main_ = false;

 private:
  void CloseMainWindow() {
    ASSERT_TRUE(main_window_);
    ASSERT_TRUE(owned_window_);
    main_window_->Close();
  }

  CefRefPtr<CefWindow> main_window_;
  CefRefPtr<CefWindow> owned_window_;
  CefRefPtr<OwnedWindowDelegate> owned_window_delegate_;
  int initial_owned_widget_count_ = -1;
  int pending_browser_id_ = 0;

  IMPLEMENT_REFCOUNTING(SyntheticOwnedWindowTestHandler);
};

}  // namespace

TEST(OwnedWidgetShutdownTest, JSDialog) {
  if (UseAlloyStyleBrowserGlobal()) {
    GTEST_SKIP() << "Only supported with Chrome style";
  }

  CefRefPtr<JSDialogShutdownTestHandler> handler =
      new JSDialogShutdownTestHandler();
  handler->ExecuteTest();

  EXPECT_TRUE(handler->dialog_requested_);
  EXPECT_TRUE(handler->got_js_dialog_);
  EXPECT_TRUE(handler->owned_widget_observed_);
  EXPECT_TRUE(handler->browser_teardown_);

  ReleaseAndWaitForDestructor(handler);
}

TEST(OwnedWidgetShutdownTest, SavePassword) {
  if (UseAlloyStyleBrowserGlobal()) {
    GTEST_SKIP() << "Only supported with Chrome style";
  }
  if (IsRunningOnWayland()) {
    GTEST_SKIP() << "Password widgets are not enumerated as owned on Wayland";
  }

  CefRefPtr<SavePasswordShutdownTestHandler> handler =
      new SavePasswordShutdownTestHandler();
  handler->ExecuteTest();

  EXPECT_TRUE(handler->form_submitted_);
  EXPECT_TRUE(handler->stats_clear_requested_);
  EXPECT_TRUE(handler->stats_cleared_);
  EXPECT_TRUE(handler->result_loaded_);
  EXPECT_TRUE(handler->password_widget_observed_);
  EXPECT_TRUE(handler->browser_teardown_);

  ReleaseAndWaitForDestructor(handler);
}

TEST(OwnedWidgetShutdownTest, SyntheticOwnedWindow) {
  if (!UseViewsGlobal() || UseAlloyStyleBrowserGlobal() ||
      UseAlloyStyleWindowGlobal()) {
    GTEST_SKIP() << "Only supported with Views-hosted Chrome style";
  }
  CefRefPtr<SyntheticOwnedWindowTestHandler> handler =
      new SyntheticOwnedWindowTestHandler();
  handler->ExecuteTest();

  EXPECT_TRUE(handler->owned_window_created_);
  EXPECT_TRUE(handler->owned_window_destroyed_);
  EXPECT_TRUE(handler->main_window_destroyed_);
#if !defined(OS_LINUX)
  EXPECT_TRUE(handler->owned_widget_observed_);
  EXPECT_TRUE(handler->owned_window_destroyed_before_main_);
#endif

  ReleaseAndWaitForDestructor(handler);
}

#endif  // CEF_API_ADDED(CEF_EXPERIMENTAL)
