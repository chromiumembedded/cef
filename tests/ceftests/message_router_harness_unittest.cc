// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/message_router_unittest_utils.h"

namespace {

// Used to verify that the test harness (bound functions) behave correctly.
class HarnessTestHandler : public SingleLoadTestHandler {
 public:
  explicit HarnessTestHandler(bool test_success)
      : test_success_(test_success) {}

  std::string GetMainHTML() override {
    std::string html;
    if (test_success_) {
      // All assertions should pass.
      html =
          "<html><body><script>\n"
          "var fail_ct = 0;\n"
          "try { window.mrtAssertTotalCount(" LINESTR
          ",0); } catch (e) { fail_ct++; }\n"
          "try { window.mrtAssertBrowserCount(" LINESTR
          ",0); } catch (e) { fail_ct++; }\n"
          "try { window.mrtAssertContextCount(" LINESTR
          ",0); } catch (e) { fail_ct++; }\n"
          "window.mrtNotify('' + (fail_ct == 0));"
          "</script></body></html>";
    } else {
      // All assertions should fail.
      html =
          "<html><body><script>\n"
          "var fail_ct = 0;\n"
          "try { window.mrtAssertTotalCount(" LINESTR
          ",1); } catch (e) { fail_ct++; }\n"
          "try { window.mrtAssertBrowserCount(" LINESTR
          ",1); } catch (e) { fail_ct++; }\n"
          "try { window.mrtAssertContextCount(" LINESTR
          ",1); } catch (e) { fail_ct++; }\n"
          "window.mrtNotify('' + (fail_ct == 3));"
          "</script></body></html>";
    }
    return html;
  }

  void OnNotify(CefRefPtr<CefBrowser> browser,
                CefRefPtr<CefFrame> frame,
                const std::string& message) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);

    got_done_.yes();
    EXPECT_STREQ("true", message.c_str());
    DestroyTest();
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_done_);
    TestHandler::DestroyTest();
  }

 private:
  const bool test_success_;
  TrackCallback got_done_;
};

}  // namespace

// Verify that the test harness works with successful assertions.
TEST(MessageRouterTest, HarnessSuccess) {
  CefRefPtr<HarnessTestHandler> handler = new HarnessTestHandler(true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify that the test harness works with failed assertions.
TEST(MessageRouterTest, HarnessFailure) {
  CefRefPtr<HarnessTestHandler> handler = new HarnessTestHandler(false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
