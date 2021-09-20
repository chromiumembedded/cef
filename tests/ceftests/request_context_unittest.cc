// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/cef_request_context.h"
#include "include/cef_request_context_handler.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_scoped_temp_dir.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

TEST(RequestContextTest, BasicGetGlobal) {
  CefRefPtr<CefRequestContext> context1 = CefRequestContext::GetGlobalContext();
  EXPECT_TRUE(context1.get());
  EXPECT_TRUE(context1->IsGlobal());
  EXPECT_TRUE(context1->IsSame(context1));
  EXPECT_TRUE(context1->IsSharingWith(context1));

  CefRefPtr<CefRequestContext> context2 = CefRequestContext::GetGlobalContext();
  EXPECT_TRUE(context2.get());
  EXPECT_TRUE(context2->IsGlobal());
  EXPECT_TRUE(context2->IsSame(context2));
  EXPECT_TRUE(context2->IsSharingWith(context2));

  EXPECT_TRUE(context1->IsSame(context2));
  EXPECT_TRUE(context2->IsSame(context1));
  EXPECT_TRUE(context1->IsSharingWith(context2));
  EXPECT_TRUE(context2->IsSharingWith(context1));
}

TEST(RequestContextTest, BasicCreate) {
  class Handler : public CefRequestContextHandler {
   public:
    Handler() {}

   private:
    IMPLEMENT_REFCOUNTING(Handler);
  };

  CefRefPtr<CefRequestContextHandler> handler = new Handler();

  CefRequestContextSettings settings;

  CefRefPtr<CefRequestContext> context1 =
      CefRequestContext::CreateContext(settings, handler.get());
  EXPECT_TRUE(context1.get());
  EXPECT_FALSE(context1->IsGlobal());
  EXPECT_TRUE(context1->IsSame(context1));
  EXPECT_TRUE(context1->IsSharingWith(context1));
  EXPECT_EQ(context1->GetHandler().get(), handler.get());

  CefRefPtr<CefRequestContext> context2 =
      CefRequestContext::CreateContext(settings, handler.get());
  EXPECT_TRUE(context2.get());
  EXPECT_FALSE(context2->IsGlobal());
  EXPECT_TRUE(context2->IsSame(context2));
  EXPECT_TRUE(context2->IsSharingWith(context2));
  EXPECT_EQ(context2->GetHandler().get(), handler.get());

  EXPECT_FALSE(context1->IsSame(context2));
  EXPECT_FALSE(context1->IsSharingWith(context2));
  EXPECT_FALSE(context2->IsSame(context1));
  EXPECT_FALSE(context2->IsSharingWith(context1));

  CefRefPtr<CefRequestContext> context3 = CefRequestContext::GetGlobalContext();
  EXPECT_TRUE(context3.get());
  EXPECT_FALSE(context3->IsSame(context1));
  EXPECT_FALSE(context3->IsSharingWith(context1));
  EXPECT_FALSE(context3->IsSame(context2));
  EXPECT_FALSE(context3->IsSharingWith(context2));
  EXPECT_FALSE(context1->IsSame(context3));
  EXPECT_FALSE(context1->IsSharingWith(context3));
  EXPECT_FALSE(context2->IsSame(context3));
  EXPECT_FALSE(context2->IsSharingWith(context3));
}

TEST(RequestContextTest, BasicCreateNoHandler) {
  CefRequestContextSettings settings;

  CefRefPtr<CefRequestContext> context1 =
      CefRequestContext::CreateContext(settings, nullptr);
  EXPECT_TRUE(context1.get());
  EXPECT_FALSE(context1->IsGlobal());
  EXPECT_TRUE(context1->IsSame(context1));
  EXPECT_TRUE(context1->IsSharingWith(context1));
  EXPECT_FALSE(context1->GetHandler().get());

  CefRefPtr<CefRequestContext> context2 =
      CefRequestContext::CreateContext(settings, nullptr);
  EXPECT_TRUE(context2.get());
  EXPECT_FALSE(context2->IsGlobal());
  EXPECT_TRUE(context2->IsSame(context2));
  EXPECT_TRUE(context2->IsSharingWith(context2));
  EXPECT_FALSE(context2->GetHandler().get());

  EXPECT_FALSE(context1->IsSame(context2));
  EXPECT_FALSE(context1->IsSharingWith(context2));
  EXPECT_FALSE(context2->IsSame(context1));
  EXPECT_FALSE(context2->IsSharingWith(context1));

  CefRefPtr<CefRequestContext> context3 = CefRequestContext::GetGlobalContext();
  EXPECT_TRUE(context3.get());
  EXPECT_FALSE(context3->IsSame(context1));
  EXPECT_FALSE(context3->IsSharingWith(context1));
  EXPECT_FALSE(context3->IsSame(context2));
  EXPECT_FALSE(context3->IsSharingWith(context2));
  EXPECT_FALSE(context1->IsSame(context3));
  EXPECT_FALSE(context1->IsSharingWith(context3));
  EXPECT_FALSE(context2->IsSame(context3));
  EXPECT_FALSE(context2->IsSharingWith(context3));
}

TEST(RequestContextTest, BasicCreateSharedGlobal) {
  CefRequestContextSettings settings;

  CefRefPtr<CefRequestContext> context1 = CefRequestContext::GetGlobalContext();
  EXPECT_TRUE(context1.get());
  EXPECT_TRUE(context1->IsGlobal());
  EXPECT_TRUE(context1->IsSame(context1));
  EXPECT_TRUE(context1->IsSharingWith(context1));

  // Returns the same global context.
  CefRefPtr<CefRequestContext> context2 =
      CefRequestContext::CreateContext(context1, nullptr);
  EXPECT_TRUE(context2.get());
  EXPECT_TRUE(context2->IsGlobal());
  EXPECT_TRUE(context2->IsSame(context2));
  EXPECT_TRUE(context2->IsSame(context1));
  EXPECT_TRUE(context1->IsSame(context2));
  EXPECT_TRUE(context2->IsSharingWith(context2));
  EXPECT_TRUE(context2->IsSharingWith(context1));
  EXPECT_TRUE(context1->IsSharingWith(context2));
}

TEST(RequestContextTest, BasicCreateSharedOnDisk) {
  CefScopedTempDir tempdir;
  EXPECT_TRUE(tempdir.CreateUniqueTempDirUnderPath(
      CefTestSuite::GetInstance()->root_cache_path()));

  CefRequestContextSettings settings;
  CefString(&settings.cache_path) = tempdir.GetPath();

  CefRefPtr<CefRequestContext> context1 =
      CefRequestContext::CreateContext(settings, nullptr);
  EXPECT_TRUE(context1.get());
  EXPECT_FALSE(context1->IsGlobal());
  EXPECT_TRUE(context1->IsSame(context1));
  EXPECT_TRUE(context1->IsSharingWith(context1));

  CefRefPtr<CefRequestContext> context2 =
      CefRequestContext::CreateContext(context1, nullptr);
  EXPECT_TRUE(context2.get());
  EXPECT_FALSE(context2->IsGlobal());
  EXPECT_TRUE(context2->IsSame(context2));
  EXPECT_FALSE(context2->IsSame(context1));
  EXPECT_FALSE(context1->IsSame(context2));
  EXPECT_TRUE(context2->IsSharingWith(context2));
  EXPECT_TRUE(context2->IsSharingWith(context1));
  EXPECT_TRUE(context1->IsSharingWith(context2));

  CefRefPtr<CefRequestContext> context3 =
      CefRequestContext::CreateContext(context2, nullptr);
  EXPECT_TRUE(context3.get());
  EXPECT_FALSE(context3->IsGlobal());
  EXPECT_TRUE(context3->IsSame(context3));
  EXPECT_FALSE(context3->IsSame(context2));
  EXPECT_FALSE(context3->IsSame(context1));
  EXPECT_FALSE(context1->IsSame(context3));
  EXPECT_FALSE(context2->IsSame(context3));
  EXPECT_TRUE(context3->IsSharingWith(context3));
  EXPECT_TRUE(context3->IsSharingWith(context2));
  EXPECT_TRUE(context3->IsSharingWith(context1));
  EXPECT_TRUE(context1->IsSharingWith(context3));
  EXPECT_TRUE(context2->IsSharingWith(context3));

  CefRefPtr<CefRequestContext> context4 =
      CefRequestContext::CreateContext(context1, nullptr);
  EXPECT_TRUE(context4.get());
  EXPECT_FALSE(context4->IsGlobal());
  EXPECT_TRUE(context4->IsSame(context4));
  EXPECT_FALSE(context4->IsSame(context3));
  EXPECT_FALSE(context4->IsSame(context2));
  EXPECT_FALSE(context4->IsSame(context1));
  EXPECT_FALSE(context1->IsSame(context4));
  EXPECT_FALSE(context2->IsSame(context4));
  EXPECT_FALSE(context3->IsSame(context4));
  EXPECT_TRUE(context4->IsSharingWith(context4));
  EXPECT_TRUE(context4->IsSharingWith(context3));
  EXPECT_TRUE(context4->IsSharingWith(context2));
  EXPECT_TRUE(context4->IsSharingWith(context1));
  EXPECT_TRUE(context1->IsSharingWith(context4));
  EXPECT_TRUE(context2->IsSharingWith(context4));
  EXPECT_TRUE(context3->IsSharingWith(context4));
}

namespace {

class PopupTestHandler : public TestHandler {
 public:
  enum Mode {
    MODE_WINDOW_OPEN,
    MODE_TARGETED_LINK,
    MODE_NOREFERRER_LINK,
  };

  PopupTestHandler(bool same_origin, Mode mode) : mode_(mode) {
    url_ = "http://tests-simple-rch1.com/nav1.html";
    if (same_origin)
      popup_url_ = "http://tests-simple-rch1.com/pop1.html";
    else
      popup_url_ = "http://tests-simple-rch2.com/pop1.html";
  }

  void RunTest() override {
    std::string link;
    if (mode_ == MODE_TARGETED_LINK) {
      link = "<a href=\"" + std::string(popup_url_) +
             "\" target=\"mytarget\"\">CLICK ME</a>";
    } else if (mode_ == MODE_NOREFERRER_LINK) {
      link = "<a href=\"" + std::string(popup_url_) +
             "\" rel=\"noreferrer\" target=\"_blank\"\">CLICK ME</a>";
    }

    AddResource(url_,
                "<html>"
                "<head><script>document.cookie='name1=value1';"
                "function doPopup() { window.open('" +
                    std::string(popup_url_) +
                    "'); }"
                    "</script></head>"
                    "<body><h1>" +
                    link +
                    "</h1></body>"
                    "</html>",
                "text/html");

    AddResource(popup_url_,
                "<html>"
                "<head><script>document.cookie='name2=value2';</script></head>"
                "<body>Nav1</body>"
                "</html>",
                "text/html");

    CefRequestContextSettings settings;

    context_ = CefRequestContext::CreateContext(settings, nullptr);
    cookie_manager_ = context_->GetCookieManager(nullptr);

    // Create browser that loads the 1st URL.
    CreateBrowser(url_, context_);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    CefRefPtr<CefRequestContext> context =
        browser->GetHost()->GetRequestContext();
    EXPECT_TRUE(context.get());
    EXPECT_TRUE(context->IsSame(context_));
    EXPECT_FALSE(context->IsGlobal());

    EXPECT_TRUE(frame->IsMain());

    const std::string& url = frame->GetURL();
    if (url == url_) {
      got_load_end1_.yes();
      LaunchPopup(browser);
    } else if (url == popup_url_) {
      got_load_end2_.yes();
      EXPECT_TRUE(browser->IsPopup());
      // Close the popup window.
      CloseBrowser(browser, true);
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
    got_on_before_popup_.yes();

    const std::string& url = target_url;
    EXPECT_STREQ(url.c_str(), popup_url_.c_str());

    EXPECT_EQ(WOD_NEW_FOREGROUND_TAB, target_disposition);

    if (mode_ == MODE_WINDOW_OPEN)
      EXPECT_FALSE(user_gesture);
    else
      EXPECT_TRUE(user_gesture);

    return false;
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnBeforeClose(browser);

    if (browser->IsPopup())
      FinishTest();
  }

 protected:
  void LaunchPopup(CefRefPtr<CefBrowser> browser) {
    if (mode_ == MODE_WINDOW_OPEN) {
      browser->GetMainFrame()->ExecuteJavaScript("doPopup()", url_, 0);
    } else if (mode_ == MODE_TARGETED_LINK || mode_ == MODE_NOREFERRER_LINK) {
      CefMouseEvent mouse_event;
      mouse_event.x = 20;
      mouse_event.y = 20;
      mouse_event.modifiers = 0;

      // Add some delay to avoid having events dropped or rate limited.
      CefPostDelayedTask(
          TID_UI,
          base::BindOnce(&CefBrowserHost::SendMouseClickEvent,
                         browser->GetHost(), mouse_event, MBT_LEFT, false, 1),
          50);
      CefPostDelayedTask(
          TID_UI,
          base::BindOnce(&CefBrowserHost::SendMouseClickEvent,
                         browser->GetHost(), mouse_event, MBT_LEFT, true, 1),
          100);
    } else {
      ADD_FAILURE();  // Not reached.
    }
  }

  void FinishTest() {
    // Verify that the cookies were set correctly.
    class TestVisitor : public CefCookieVisitor {
     public:
      explicit TestVisitor(PopupTestHandler* handler) : handler_(handler) {}
      ~TestVisitor() override {
        // Destroy the test.
        CefPostTask(TID_UI,
                    base::BindOnce(&PopupTestHandler::DestroyTest, handler_));
      }

      bool Visit(const CefCookie& cookie,
                 int count,
                 int total,
                 bool& deleteCookie) override {
        const std::string& name = CefString(&cookie.name);
        const std::string& value = CefString(&cookie.value);
        if (name == "name1" && value == "value1") {
          handler_->got_cookie1_.yes();
          deleteCookie = true;
        } else if (name == "name2" && value == "value2") {
          handler_->got_cookie2_.yes();
          deleteCookie = true;
        }
        return true;
      }

     private:
      PopupTestHandler* handler_;
      IMPLEMENT_REFCOUNTING(TestVisitor);
    };

    cookie_manager_->VisitAllCookies(new TestVisitor(this));
  }

  void DestroyTest() override {
    // Verify test expectations.
    EXPECT_TRUE(got_load_end1_);
    EXPECT_TRUE(got_on_before_popup_);
    EXPECT_TRUE(got_load_end2_);
    EXPECT_TRUE(got_cookie1_);
    EXPECT_TRUE(got_cookie2_);
    context_ = nullptr;

    TestHandler::DestroyTest();
  }

  std::string url_;
  std::string popup_url_;
  Mode mode_;

  CefRefPtr<CefRequestContext> context_;
  CefRefPtr<CefCookieManager> cookie_manager_;

  TrackCallback got_load_end1_;
  TrackCallback got_on_before_popup_;
  TrackCallback got_load_end2_;
  TrackCallback got_cookie1_;
  TrackCallback got_cookie2_;

  IMPLEMENT_REFCOUNTING(PopupTestHandler);
};

}  // namespace

// Test that a popup created using window.open() will get the same request
// context as the parent browser.
TEST(RequestContextTest, PopupBasicWindowOpenSameOrigin) {
  CefRefPtr<PopupTestHandler> handler =
      new PopupTestHandler(true, PopupTestHandler::MODE_WINDOW_OPEN);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(RequestContextTest, PopupBasicWindowOpenDifferentOrigin) {
  CefRefPtr<PopupTestHandler> handler =
      new PopupTestHandler(false, PopupTestHandler::MODE_WINDOW_OPEN);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that a popup created using a targeted link will get the same request
// context as the parent browser.
TEST(RequestContextTest, PopupBasicTargetedLinkSameOrigin) {
  CefRefPtr<PopupTestHandler> handler =
      new PopupTestHandler(true, PopupTestHandler::MODE_TARGETED_LINK);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(RequestContextTest, PopupBasicTargetedLinkDifferentOrigin) {
  CefRefPtr<PopupTestHandler> handler =
      new PopupTestHandler(false, PopupTestHandler::MODE_TARGETED_LINK);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that a popup created using a noreferrer link will get the same
// request context as the parent browser. A new render process will
// be created for the popup browser.
TEST(RequestContextTest, PopupBasicNoReferrerLinkSameOrigin) {
  CefRefPtr<PopupTestHandler> handler =
      new PopupTestHandler(true, PopupTestHandler::MODE_NOREFERRER_LINK);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(RequestContextTest, PopupBasicNoReferrerLinkDifferentOrigin) {
  CefRefPtr<PopupTestHandler> handler =
      new PopupTestHandler(false, PopupTestHandler::MODE_NOREFERRER_LINK);
  handler->ExecuteTest();
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
  enum TestMode {
    ALLOW_CLOSE_POPUP_FIRST,
    ALLOW_CLOSE_POPUP_LAST,
    DENY,
    NAVIGATE_AFTER_CREATION,
    DESTROY_PARENT_BEFORE_CREATION,
    DESTROY_PARENT_BEFORE_CREATION_FORCE,
    DESTROY_PARENT_DURING_CREATION,
    DESTROY_PARENT_DURING_CREATION_FORCE,
    DESTROY_PARENT_AFTER_CREATION,
    DESTROY_PARENT_AFTER_CREATION_FORCE,
  };

  PopupNavTestHandler(TestMode test_mode,
                      TestRequestContextMode rc_mode,
                      const std::string& rc_cache_path)
      : mode_(test_mode), rc_mode_(rc_mode), rc_cache_path_(rc_cache_path) {}

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

    CefRefPtr<CefRequestContext> request_context =
        CreateTestRequestContext(rc_mode_, rc_cache_path_);

    // Create the browser.
    CreateBrowser(kPopupNavPageUrl, request_context);

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
    EXPECT_FALSE(got_on_before_popup_);
    got_on_before_popup_.yes();

    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_EQ(GetBrowserId(), browser->GetIdentifier());
    EXPECT_STREQ(kPopupNavPageUrl, frame->GetURL().ToString().c_str());
    EXPECT_STREQ(kPopupNavPopupUrl, target_url.ToString().c_str());
    EXPECT_STREQ(kPopupNavPopupName, target_frame_name.ToString().c_str());
    EXPECT_EQ(WOD_NEW_FOREGROUND_TAB, target_disposition);
    EXPECT_FALSE(user_gesture);
    EXPECT_FALSE(*no_javascript_access);

    if (mode_ == DESTROY_PARENT_DURING_CREATION ||
        mode_ == DESTROY_PARENT_DURING_CREATION_FORCE) {
      // Destroy the main (parent) browser while popup creation is pending.
      CloseBrowser(browser, mode_ == DESTROY_PARENT_DURING_CREATION_FORCE);
    }

    return (mode_ == DENY);  // Return true to cancel the popup.
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);

    if (browser->IsPopup() && (mode_ == DESTROY_PARENT_AFTER_CREATION ||
                               mode_ == DESTROY_PARENT_AFTER_CREATION_FORCE)) {
      // Destroy the main (parent) browser immediately after the popup is
      // created.
      CloseBrowser(GetBrowser(), mode_ == DESTROY_PARENT_AFTER_CREATION_FORCE);
    }

    if (mode_ == NAVIGATE_AFTER_CREATION && browser->IsPopup()) {
      // Navigate to the 2nd popup URL instead of the 1st popup URL.
      browser->GetMainFrame()->LoadURL(kPopupNavPopupUrl2);
    }
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    const std::string& url = frame->GetURL();
    if (url == kPopupNavPageUrl) {
      EXPECT_FALSE(got_load_start_);
      got_load_start_.yes();
    } else if (url == kPopupNavPopupUrl) {
      EXPECT_FALSE(got_popup_load_start_);
      got_popup_load_start_.yes();
    } else if (url == kPopupNavPopupUrl2) {
      EXPECT_FALSE(got_popup_load_start2_);
      got_popup_load_start2_.yes();
    }
  }

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override {
    if (failedUrl == kPopupNavPageUrl) {
      EXPECT_FALSE(got_load_error_);
      got_load_error_.yes();
    } else if (failedUrl == kPopupNavPopupUrl) {
      EXPECT_FALSE(got_popup_load_error_);
      got_popup_load_error_.yes();
    } else if (failedUrl == kPopupNavPopupUrl2) {
      EXPECT_FALSE(got_popup_load_error2_);
      got_popup_load_error2_.yes();
    }
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    const std::string& url = frame->GetURL();
    if (url == kPopupNavPageUrl) {
      EXPECT_FALSE(got_load_end_);
      got_load_end_.yes();

      frame->ExecuteJavaScript("doPopup()", kPopupNavPageUrl, 0);

      if (mode_ == DESTROY_PARENT_BEFORE_CREATION ||
          mode_ == DESTROY_PARENT_BEFORE_CREATION_FORCE) {
        // Destroy the main (parent) browser immediately before the popup is
        // created.
        CloseBrowser(browser, mode_ == DESTROY_PARENT_BEFORE_CREATION_FORCE);
      }

      if (mode_ == DENY) {
        // Wait a bit to make sure the popup window isn't created.
        CefPostDelayedTask(
            TID_UI, base::BindOnce(&PopupNavTestHandler::DestroyTest, this),
            200);
      }
    } else if (url == kPopupNavPopupUrl) {
      EXPECT_FALSE(got_popup_load_end_);
      got_popup_load_end_.yes();

      if (mode_ == ALLOW_CLOSE_POPUP_FIRST) {
        // Close the popup browser first.
        CloseBrowser(browser, false);
      } else if (mode_ == ALLOW_CLOSE_POPUP_LAST) {
        // Close the main browser first.
        CloseBrowser(GetBrowser(), false);
      } else if (mode_ != NAVIGATE_AFTER_CREATION) {
        EXPECT_FALSE(true);  // Not reached.
      }
    } else if (url == kPopupNavPopupUrl2) {
      EXPECT_FALSE(got_popup_load_end2_);
      got_popup_load_end2_.yes();

      if (mode_ == NAVIGATE_AFTER_CREATION) {
        // Close the popup browser first.
        CloseBrowser(browser, false);
      } else {
        EXPECT_FALSE(true);  // Not reached.
      }
    } else {
      EXPECT_FALSE(true);  // Not reached.
    }
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnBeforeClose(browser);

    bool destroy_test = false;
    if (mode_ == ALLOW_CLOSE_POPUP_FIRST || mode_ == NAVIGATE_AFTER_CREATION) {
      // Destroy the test after the popup browser closes.
      if (browser->IsPopup())
        destroy_test = true;
    } else if (mode_ == ALLOW_CLOSE_POPUP_LAST ||
               mode_ == DESTROY_PARENT_BEFORE_CREATION ||
               mode_ == DESTROY_PARENT_BEFORE_CREATION_FORCE ||
               mode_ == DESTROY_PARENT_DURING_CREATION ||
               mode_ == DESTROY_PARENT_DURING_CREATION_FORCE ||
               mode_ == DESTROY_PARENT_AFTER_CREATION ||
               mode_ == DESTROY_PARENT_AFTER_CREATION_FORCE) {
      // Destroy the test after the main browser closes.
      if (!browser->IsPopup())
        destroy_test = true;
    }

    if (destroy_test) {
      CefPostTask(TID_UI,
                  base::BindOnce(&PopupNavTestHandler::DestroyTest, this));
    }
  }

 private:
  void DestroyTest() override {
    EXPECT_TRUE(got_load_start_);
    EXPECT_FALSE(got_load_error_);
    EXPECT_TRUE(got_load_end_);

    // OnBeforePopup may come before or after browser destruction with the
    // DESTROY_PARENT_BEFORE_CREATION* tests.
    if (mode_ != DESTROY_PARENT_BEFORE_CREATION &&
        mode_ != DESTROY_PARENT_BEFORE_CREATION_FORCE) {
      EXPECT_TRUE(got_on_before_popup_);
    }

    if (mode_ == ALLOW_CLOSE_POPUP_FIRST || mode_ == ALLOW_CLOSE_POPUP_LAST) {
      EXPECT_TRUE(got_popup_load_start_);
      EXPECT_FALSE(got_popup_load_error_);
      EXPECT_TRUE(got_popup_load_end_);
      EXPECT_FALSE(got_popup_load_start2_);
      EXPECT_FALSE(got_popup_load_error2_);
      EXPECT_FALSE(got_popup_load_end2_);
    } else if (mode_ == DENY || mode_ == DESTROY_PARENT_BEFORE_CREATION ||
               mode_ == DESTROY_PARENT_BEFORE_CREATION_FORCE ||
               mode_ == DESTROY_PARENT_DURING_CREATION ||
               mode_ == DESTROY_PARENT_DURING_CREATION_FORCE ||
               mode_ == DESTROY_PARENT_AFTER_CREATION ||
               mode_ == DESTROY_PARENT_AFTER_CREATION_FORCE) {
      EXPECT_FALSE(got_popup_load_start_);
      EXPECT_FALSE(got_popup_load_error_);
      EXPECT_FALSE(got_popup_load_end_);
      EXPECT_FALSE(got_popup_load_start2_);
      EXPECT_FALSE(got_popup_load_error2_);
      EXPECT_FALSE(got_popup_load_end2_);
    } else if (mode_ == NAVIGATE_AFTER_CREATION) {
      EXPECT_FALSE(got_popup_load_start_);

      // With browser-side navigation we will never actually begin the
      // navigation to the 1st popup URL, so there will be no load error.
      EXPECT_FALSE(got_popup_load_error_);

      EXPECT_FALSE(got_popup_load_end_);
      EXPECT_TRUE(got_popup_load_start2_);
      EXPECT_FALSE(got_popup_load_error2_);
      EXPECT_TRUE(got_popup_load_end2_);
    }

    // Will trigger destruction of all remaining browsers.
    TestHandler::DestroyTest();
  }

  const TestMode mode_;
  const TestRequestContextMode rc_mode_;
  const std::string rc_cache_path_;

  TrackCallback got_on_before_popup_;
  TrackCallback got_load_start_;
  TrackCallback got_load_error_;
  TrackCallback got_load_end_;
  TrackCallback got_popup_load_start_;
  TrackCallback got_popup_load_error_;
  TrackCallback got_popup_load_end_;
  TrackCallback got_popup_load_start2_;
  TrackCallback got_popup_load_error2_;
  TrackCallback got_popup_load_end2_;

  IMPLEMENT_REFCOUNTING(PopupNavTestHandler);
};

}  // namespace
#define POPUP_TEST_GROUP(test_name, test_mode)                     \
  RC_TEST_GROUP_IN_MEMORY(RequestContextTest, PopupNav##test_name, \
                          PopupNavTestHandler, test_mode)

// Test allowing popups and closing the popup browser first.
POPUP_TEST_GROUP(AllowClosePopupFirst, ALLOW_CLOSE_POPUP_FIRST)

// Test allowing popups and closing the main browser first to verify
// that internal objects are tracked correctly (see issue #2162).
POPUP_TEST_GROUP(AllowClosePopupLast, ALLOW_CLOSE_POPUP_LAST)

// Test denying popups.
POPUP_TEST_GROUP(Deny, DENY)

// Test navigation to a different origin after popup creation to
// verify that internal objects are tracked correctly (see issue
// #1392).
POPUP_TEST_GROUP(NavigateAfterCreation, NAVIGATE_AFTER_CREATION)

// Test destroying the parent browser during or immediately after
// popup creation to verify that internal objects are tracked
// correctly (see issue #2041).
POPUP_TEST_GROUP(DestroyParentBeforeCreation, DESTROY_PARENT_BEFORE_CREATION)
POPUP_TEST_GROUP(DestroyParentBeforeCreationForce,
                 DESTROY_PARENT_BEFORE_CREATION_FORCE)
POPUP_TEST_GROUP(DestroyParentDuringCreation, DESTROY_PARENT_DURING_CREATION)
POPUP_TEST_GROUP(DestroyParentDuringCreationForce,
                 DESTROY_PARENT_DURING_CREATION_FORCE)
POPUP_TEST_GROUP(DestroyParentAfterCreation, DESTROY_PARENT_AFTER_CREATION)
POPUP_TEST_GROUP(DestroyParentAfterCreationForce,
                 DESTROY_PARENT_AFTER_CREATION_FORCE)

namespace {

const char kResolveOrigin[] = "http://www.google.com";

class MethodTestHandler : public TestHandler {
 public:
  enum Method {
    METHOD_CLEAR_CERTIFICATE_EXCEPTIONS,
    METHOD_CLOSE_ALL_CONNECTIONS,
    METHOD_RESOLVE_HOST,
  };

  class CompletionCallback : public CefCompletionCallback,
                             public CefResolveCallback {
   public:
    CompletionCallback(MethodTestHandler* test_handler,
                       CefRefPtr<CefBrowser> browser)
        : test_handler_(test_handler), browser_(browser) {}

    ~CompletionCallback() override {
      // OnComplete should be executed.
      EXPECT_FALSE(test_handler_);
    }

    void OnComplete() override {
      EXPECT_UI_THREAD();

      // OnComplete should be executed only one time.
      EXPECT_TRUE(test_handler_);
      test_handler_->OnCompleteCallback(browser_);
      test_handler_ = nullptr;
      browser_ = nullptr;
    }

    void OnResolveCompleted(
        cef_errorcode_t result,
        const std::vector<CefString>& resolved_ips) override {
      EXPECT_EQ(ERR_NONE, result);
      EXPECT_TRUE(!resolved_ips.empty());
      OnComplete();
    }

   private:
    MethodTestHandler* test_handler_;
    CefRefPtr<CefBrowser> browser_;

    IMPLEMENT_REFCOUNTING(CompletionCallback);
  };

  MethodTestHandler(bool global_context, Method method)
      : global_context_(global_context), method_(method) {}

  void RunTest() override {
    const char kUrl[] = "http://tests/method.html";

    AddResource(kUrl, "<html><body>Method</body></html>", "text/html");

    CefRefPtr<CefRequestContext> request_context;
    if (!global_context_) {
      CefRequestContextSettings settings;
      request_context = CefRequestContext::CreateContext(settings, nullptr);
    }

    CreateBrowser(kUrl, request_context);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    CefRefPtr<CefRequestContext> context =
        browser->GetHost()->GetRequestContext();
    CefRefPtr<CompletionCallback> callback =
        new CompletionCallback(this, browser);
    if (method_ == METHOD_CLEAR_CERTIFICATE_EXCEPTIONS)
      context->ClearCertificateExceptions(callback);
    else if (method_ == METHOD_CLOSE_ALL_CONNECTIONS)
      context->CloseAllConnections(callback);
    else if (method_ == METHOD_RESOLVE_HOST)
      context->ResolveHost(kResolveOrigin, callback);
  }

  void OnCompleteCallback(CefRefPtr<CefBrowser> browser) {
    EXPECT_UI_THREAD();
    EXPECT_FALSE(got_completion_callback_);
    got_completion_callback_.yes();

    DestroyTest();
  }

 private:
  void DestroyTest() override {
    EXPECT_TRUE(got_completion_callback_);
    TestHandler::DestroyTest();
  }

  const bool global_context_;
  const Method method_;

  TrackCallback got_completion_callback_;

  IMPLEMENT_REFCOUNTING(MethodTestHandler);
};

}  // namespace

// Test CefRequestContext::ClearCertificateExceptions with the global
// context.
TEST(RequestContextTest, ClearCertificateExceptionsGlobal) {
  CefRefPtr<MethodTestHandler> handler = new MethodTestHandler(
      true, MethodTestHandler::METHOD_CLEAR_CERTIFICATE_EXCEPTIONS);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test CefRequestContext::ClearCertificateExceptions with a custom
// context.
TEST(RequestContextTest, ClearCertificateExceptionsCustom) {
  CefRefPtr<MethodTestHandler> handler = new MethodTestHandler(
      false, MethodTestHandler::METHOD_CLEAR_CERTIFICATE_EXCEPTIONS);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test CefRequestContext::CloseAllConnections with the global
// context.
TEST(RequestContextTest, CloseAllConnectionsGlobal) {
  CefRefPtr<MethodTestHandler> handler = new MethodTestHandler(
      true, MethodTestHandler::METHOD_CLOSE_ALL_CONNECTIONS);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test CefRequestContext::CloseAllConnections with a custom context.
TEST(RequestContextTest, CloseAllConnectionsCustom) {
  CefRefPtr<MethodTestHandler> handler = new MethodTestHandler(
      false, MethodTestHandler::METHOD_CLOSE_ALL_CONNECTIONS);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test CefRequestContext::ResolveHost with the global context.
TEST(RequestContextTest, ResolveHostGlobal) {
  CefRefPtr<MethodTestHandler> handler =
      new MethodTestHandler(true, MethodTestHandler::METHOD_RESOLVE_HOST);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test CefRequestContext::ResolveHost with a custom context.
TEST(RequestContextTest, ResolveHostCustom) {
  CefRefPtr<MethodTestHandler> handler =
      new MethodTestHandler(false, MethodTestHandler::METHOD_RESOLVE_HOST);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
