// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

const char kTestHTMLWithRegions[] =
    "<html>"
    "  <body>"
    "    <div style=\"position: absolute; top: 50px; left: 50px; width: 200px; "
    "height: 200px; background-color: red; -webkit-app-region: drag;\">"
    "      <div style=\"position: absolute; top: 50%; left: 50%; "
    "transform: translate(-50%, -50%); width: 50px; height: 50px; "
    "background-color: blue; -webkit-app-region: no-drag;\">"
    "      </div>"
    "    </div>"
    "  </body>"
    "</html>";

const char kTestHTMLWithoutRegions[] = "<html><body>Hello World!</body></html>";

const char kTestHTMLWithChangingRegions[] =
    "<html>"
    "  <body>"
    "    <div id=\"layer\" style=\"position: absolute; top: 50px; left: 50px; "
    "width: 200px; height: 200px; background-color: red; "
    "-webkit-app-region: drag;\">"
    "      <div style=\"position: absolute; top: 50%; left: 50%; "
    "transform: translate(-50%, -50%); width: 50px; height: 50px; "
    "background-color: blue; -webkit-app-region: no-drag;\">"
    "      </div>"
    "    </div>"
    "    <script>"
    "      window.setTimeout(function() {"
    "        var layer = document.getElementById('layer');"
    "        layer.style.top = '0px';"
    "        layer.style.left = '0px';"
    "      }, 500);"
    "    </script>"
    "  </body>"
    "</html>";

class DraggableRegionsTestHandler : public TestHandler,
                                    public CefDragHandler,
                                    public CefFrameHandler {
 public:
  // Test steps executed in order.
  enum Step {
    // Nav 1: Two regions (get notification).
    kStepWithRegions = 1,
    // Nav 2: Starts with the same region as Nav 1 (no notification),
    // then a changed region (get notification).
    kStepWithChangingRegions,
    // Nav 3: No regions (get notification).
    kStepWithoutRegions,
    // GoBack: Two regions (get notification), then a changed region (get
    // notification). Note the first notification is not sent if
    // BackForwardCache is enabled.
    kStepWithChangingRegions2,
    kStepWithChangingRegions3,
    // GoForward: No regions (get notification).
    kStepWithoutRegions2,

    kStepMax = kStepWithoutRegions2,
  };

  explicit DraggableRegionsTestHandler(bool same_origin)
      : same_origin_(same_origin) {}

  void RunTest() override {
    // Add HTML documents with and without draggable regions.
    AddResource(GetURL(kStepWithRegions), kTestHTMLWithRegions, "text/html");
    AddResource(GetURL(kStepWithChangingRegions), kTestHTMLWithChangingRegions,
                "text/html");
    AddResource(GetURL(kStepWithoutRegions), kTestHTMLWithoutRegions,
                "text/html");

    // Create the browser
    CreateBrowser(GetURL(kStepWithRegions));

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  CefRefPtr<CefDragHandler> GetDragHandler() override { return this; }
  CefRefPtr<CefFrameHandler> GetFrameHandler() override { return this; }

  void OnDraggableRegionsChanged(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      const std::vector<CefDraggableRegion>& regions) override {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(browser->IsSame(GetBrowser()));
    EXPECT_TRUE(frame->IsMain());

    draggable_regions_changed_ct_++;

    switch (step_) {
      case kStepWithRegions:
      case kStepWithChangingRegions2:
        EXPECT_EQ(2U, regions.size());
        EXPECT_NEAR(50, regions[0].bounds.x, 1);
        EXPECT_NEAR(50, regions[0].bounds.y, 1);
        EXPECT_NEAR(200, regions[0].bounds.width, 1);
        EXPECT_NEAR(200, regions[0].bounds.height, 1);
        EXPECT_EQ(1, regions[0].draggable);
        EXPECT_NEAR(125, regions[1].bounds.x, 1);
        EXPECT_NEAR(125, regions[1].bounds.y, 1);
        EXPECT_NEAR(50, regions[1].bounds.width, 1);
        EXPECT_NEAR(50, regions[1].bounds.height, 1);
        EXPECT_EQ(0, regions[1].draggable);
        break;
      case kStepWithChangingRegions:
      case kStepWithChangingRegions3:
        EXPECT_EQ(2U, regions.size());
        EXPECT_EQ(0, regions[0].bounds.x);
        EXPECT_EQ(0, regions[0].bounds.y);
        EXPECT_NEAR(200, regions[0].bounds.width, 1);
        EXPECT_NEAR(200, regions[0].bounds.height, 1);
        EXPECT_EQ(1, regions[0].draggable);
        EXPECT_NEAR(75, regions[1].bounds.x, 1);
        EXPECT_NEAR(75, regions[1].bounds.y, 1);
        EXPECT_NEAR(50, regions[1].bounds.width, 2);
        EXPECT_NEAR(50, regions[1].bounds.height, 2);
        EXPECT_EQ(0, regions[1].draggable);
        break;
      case kStepWithoutRegions:
      case kStepWithoutRegions2:
        EXPECT_TRUE(regions.empty());
        break;
    }

    NextTest(browser);
  }

  void OnFrameAttached(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       bool reattached) override {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(browser->IsSame(GetBrowser()));
    EXPECT_TRUE(frame->IsMain());

    if (reattached) {
      // When BackForwardCache is enabled and we go back to
      // kTestHTMLWithChangingRegions, draggable regions will already be in the
      // final position because the page content is not reloaded.
      if (step_ == kStepWithChangingRegions2) {
        step_ = kStepWithChangingRegions3;
        expected_draggable_regions_changed_ct_--;
      }
    }
  }

  void DestroyTest() override {
    EXPECT_EQ(expected_draggable_regions_changed_ct_,
              draggable_regions_changed_ct_);

    TestHandler::DestroyTest();
  }

 private:
  void NextTest(CefRefPtr<CefBrowser> browser) {
    CefRefPtr<CefFrame> frame(browser->GetMainFrame());

    switch (step_) {
      case kStepWithRegions:
        step_ = kStepWithChangingRegions;
        frame->LoadURL(GetURL(kStepWithChangingRegions));
        break;
      case kStepWithChangingRegions:
        step_ = kStepWithoutRegions;
        frame->LoadURL(GetURL(kStepWithoutRegions));
        break;
      case kStepWithoutRegions: {
        step_ = kStepWithChangingRegions2;
        browser->GoBack();
        break;
      }
      case kStepWithChangingRegions2: {
        step_ = kStepWithChangingRegions3;
        break;
      }
      case kStepWithChangingRegions3: {
        step_ = kStepWithoutRegions2;
        browser->GoForward();
        break;
      }
      case kStepWithoutRegions2: {
        DestroyTest();
        break;
      }
    }
  }

  std::string GetURL(Step step) const {
    // When |same_origin_| is true every other URL gets a different origin.
    switch (step) {
      case kStepWithRegions:
        return same_origin_ ? "http://test.com/regions"
                            : "http://test2.com/regions";
      case kStepWithChangingRegions:
      case kStepWithChangingRegions2:
      case kStepWithChangingRegions3:
        return "http://test.com/changing-regions";
      case kStepWithoutRegions:
      case kStepWithoutRegions2:
        return same_origin_ ? "http://test.com/no-regions"
                            : "http://test2.com/no-regions";
    }

    NOTREACHED();
    return "";
  }

  const bool same_origin_;

  Step step_ = kStepWithRegions;
  int draggable_regions_changed_ct_ = 0;
  int expected_draggable_regions_changed_ct_ = kStepMax;

  IMPLEMENT_REFCOUNTING(DraggableRegionsTestHandler);
};

}  // namespace

// Verify that draggable regions work in the same origin.
TEST(DraggableRegionsTest, DraggableRegionsSameOrigin) {
  CefRefPtr<DraggableRegionsTestHandler> handler =
      new DraggableRegionsTestHandler(/*same_origin=*/true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify that draggable regions work cross-origin.
TEST(DraggableRegionsTest, DraggableRegionsCrossOrigin) {
  CefRefPtr<DraggableRegionsTestHandler> handler =
      new DraggableRegionsTestHandler(/*same_origin=*/false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
