// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <vector>

#include "include/test/cef_test_helpers.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_display.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

#if CEF_API_ADDED(CEF_NEXT)
namespace {

constexpr char kUrl[] = "https://tests/window-capture.html";
enum class Target { kVisible, kHidden, kNull, kOffscreen };

void VerifyMutableSnapshot(CefRefPtr<CefImage> image) {
  float scale = 0;
  int width = 0, height = 0;
  ASSERT_TRUE(image->GetRepresentationInfo(1, scale, width, height));
  auto bitmap = image->GetAsBitmap(scale, CEF_COLOR_TYPE_RGBA_8888,
                                   CEF_ALPHA_TYPE_PREMULTIPLIED, width, height);
  ASSERT_TRUE(bitmap);
  std::vector<uint8_t> pixels(bitmap->GetSize());
  ASSERT_EQ(pixels.size(), bitmap->GetData(pixels.data(), pixels.size(), 0));
  // Add a second representation of the same image at twice the pixel scale.
  std::vector<uint8_t> doubled(pixels.size() * 4);
  for (int y = 0; y < height * 2; ++y) {
    for (int x = 0; x < width * 2; ++x) {
      std::copy_n(
          pixels.data() + (static_cast<size_t>(y / 2) * width + x / 2) * 4, 4,
          doubled.data() + (static_cast<size_t>(y) * width * 2 + x) * 4);
    }
  }
  ASSERT_TRUE(image->AddBitmap(
      scale * 2, width * 2, height * 2, CEF_COLOR_TYPE_RGBA_8888,
      CEF_ALPHA_TYPE_PREMULTIPLIED, doubled.data(), doubled.size()));
  EXPECT_TRUE(image->HasRepresentation(scale * 2));
  EXPECT_TRUE(image->RemoveRepresentation(scale * 2));
  EXPECT_FALSE(image->HasRepresentation(scale * 2));
  EXPECT_TRUE(image->HasRepresentation(scale));
  EXPECT_TRUE(image->GetAsPNG(scale, true, width, height));
}

class CaptureTestHandler : public RoutingTestHandler,
                           public CefCaptureBrowserWindowCallback,
                           public CefRenderHandler {
 public:
  CaptureTestHandler(bool alloy, bool native, Target target)
      : alloy_(alloy), native_(native), target_(target) {}

  void RunTest() override {
    SetTestTimeout(10000);
    if (target_ == Target::kNull) {
      SetSignalTestCompletionCount(1);
      CefPostTask(TID_UI, base::BindOnce(&CaptureTestHandler::Capture, this,
                                         CefRefPtr<CefBrowser>()));
      return;
    }
    AddResource(kUrl, R"HTML(<!doctype html>
      <style>html,body { margin:0; height:100%;
        background:linear-gradient(to right,black 50%,white 50%); }</style>
      <script>requestAnimationFrame(() => requestAnimationFrame(() =>
        window.testQuery({request:'ready'})));</script>)HTML",
                "text/html");
    if (target_ == Target::kOffscreen) {
      CefPostTask(TID_UI,
                  base::BindOnce(&CaptureTestHandler::CreateOffscreen, this));
    } else {
      SetUseViews(true);
      SetUseAlloyStyle(alloy_, alloy_);
      CreateBrowser(kUrl);
    }
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    RoutingTestHandler::OnAfterCreated(browser);
    if (target_ == Target::kOffscreen) {
      Capture(browser);
    }
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    if (started_ || target_ == Target::kOffscreen) {
      return false;
    }
    started_ = true;
    EXPECT_EQ("ready", request.ToString());
    callback->Success("");
    auto view = CefBrowserView::GetForBrowser(browser);
    EXPECT_TRUE(view);
    auto window = view->GetWindow();
    if (native_) {
      expected_size_ = window->GetSize();
    } else {
      const auto bounds = window->GetClientAreaBoundsInScreen();
      expected_size_ = CefSize(bounds.width, bounds.height);
    }
    expected_scale_ = window->GetDisplay()->GetDeviceScaleFactor();
    if (target_ == Target::kHidden) {
      window->Hide();
    }
    paint_deadline_ =
        std::chrono::steady_clock::now() + std::chrono::seconds(3);
    Capture(browser);
    return true;
  }

  void OnCaptureComplete(CefRefPtr<CefImage> image) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_FALSE(in_call_);
    EXPECT_EQ(requests_, ++callbacks_);
    EXPECT_EQ(target_ == Target::kVisible, image != nullptr);
    image_ = image;
    if (image) {
      float scale = 0;
      int width = 0, height = 0;
      EXPECT_FALSE(image->IsEmpty());
      EXPECT_TRUE(image->GetRepresentationInfo(1, scale, width, height));
      EXPECT_FLOAT_EQ(expected_scale_, scale);
      EXPECT_NEAR(expected_size_.width, image->GetWidth(), 1);
      EXPECT_NEAR(expected_size_.height, image->GetHeight(), 1);
      EXPECT_NEAR(std::round(expected_size_.width * scale), width, 1);
      EXPECT_NEAR(std::round(expected_size_.height * scale), height, 1);
      auto pixels =
          image->GetAsBitmap(scale, CEF_COLOR_TYPE_RGBA_8888,
                             CEF_ALPHA_TYPE_PREMULTIPLIED, width, height);
      EXPECT_TRUE(pixels);
      if (pixels && width > 0 && height > 0) {
        std::array<std::array<uint8_t, 4>, 2> markers{};
        for (const bool white : {false, true}) {
          const int x = white ? 3 * width / 4 : width / 4;
          const int y = 3 * height / 4;
          auto& pixel = markers[white ? 1 : 0];
          EXPECT_EQ(pixel.size(),
                    pixels->GetData(pixel.data(), pixel.size(),
                                    (static_cast<size_t>(y) * width + x) * 4));
        }
        const std::array<uint8_t, 4> black{0, 0, 0, 255};
        const std::array<uint8_t, 4> white{255, 255, 255, 255};
        // rAF acknowledges renderer updates, not compositor presentation. The
        // capture API does not promise to flush page paints. Wait for the page
        // markers, with a deadline, without retrying failed/invalid captures.
        if ((markers[0] != black || markers[1] != white) &&
            std::chrono::steady_clock::now() < paint_deadline_) {
          GetUIThreadHelper()->PostDelayedTask(
              base::BindOnce(&CaptureTestHandler::Capture, this, GetBrowser()),
              16);
          return;
        }
        EXPECT_EQ(black, markers[0]);
        EXPECT_EQ(white, markers[1]);
      }
    }
    DestroyTest();
    if (target_ == Target::kNull) {
      SignalTestCompletion();
    }
  }

  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
    rect = CefRect(0, 0, 200, 200);
  }
  void OnPaint(CefRefPtr<CefBrowser>,
               PaintElementType,
               const RectList&,
               const void*,
               int,
               int) override {}

  int callbacks() const { return callbacks_; }
  int requests() const { return requests_; }
  CefRefPtr<CefImage> image() const { return image_; }

 private:
  void Capture(CefRefPtr<CefBrowser> browser) {
    ++requests_;
    in_call_ = true;
    CefCaptureBrowserWindowForTests(browser, native_, this);
    in_call_ = false;
  }

  void CreateOffscreen() {
    CefWindowInfo info;
    info.SetAsWindowless(kNullWindowHandle);
    info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
    EXPECT_TRUE(CefBrowserHost::CreateBrowser(
        info, this, kUrl, CefBrowserSettings(), nullptr, nullptr));
  }

  const bool alloy_;
  const bool native_;
  const Target target_;
  CefSize expected_size_;
  float expected_scale_ = 0;
  CefRefPtr<CefImage> image_;
  int callbacks_ = 0;
  int requests_ = 0;
  std::chrono::steady_clock::time_point paint_deadline_;
  bool in_call_ = false;
  bool started_ = false;
  IMPLEMENT_REFCOUNTING(CaptureTestHandler);
};

class BrowserWindowCaptureTest : public testing::TestWithParam<bool> {
 protected:
  void Run(bool alloy, Target target = Target::kVisible) {
    CefRefPtr<CaptureTestHandler> handler =
        new CaptureTestHandler(alloy, GetParam(), target);
    handler->ExecuteTest();
    EXPECT_EQ(handler->requests(), handler->callbacks());
    if (target != Target::kVisible) {
      EXPECT_EQ(1, handler->requests());
    }
    // The image remains usable after the source browser/window have closed.
    auto image = handler->image();
    if (target == Target::kVisible && image) {
      int width = 0, height = 0;
      EXPECT_TRUE(image->GetAsPNG(1, true, width, height));
      EXPECT_GT(width, 0);
      EXPECT_GT(height, 0);
      VerifyMutableSnapshot(image);
    }
    ReleaseAndWaitForDestructor(handler);
  }
};

TEST_P(BrowserWindowCaptureTest, Chrome) {
#if !defined(OS_MAC)
  if (GetParam()) {
    GTEST_SKIP()
        << "Native frame geometry and alpha assertions require macOS; "
           "Windows uses PrintWindow client capture and Linux uses Aura";
  }
#endif
  Run(false);
}
TEST_P(BrowserWindowCaptureTest, Alloy) {
#if !defined(OS_MAC)
  if (GetParam()) {
    GTEST_SKIP()
        << "Native frame geometry and alpha assertions require macOS; "
           "Windows uses PrintWindow client capture and Linux uses Aura";
  }
#endif
  Run(true);
}
TEST_P(BrowserWindowCaptureTest, ChromeHidden) {
  Run(false, Target::kHidden);
}
TEST_P(BrowserWindowCaptureTest, AlloyHidden) {
  Run(true, Target::kHidden);
}
TEST_P(BrowserWindowCaptureTest, NullBrowser) {
  Run(false, Target::kNull);
}
TEST_P(BrowserWindowCaptureTest, Offscreen) {
  Run(true, Target::kOffscreen);
}

INSTANTIATE_TEST_SUITE_P(,
                         BrowserWindowCaptureTest,
                         testing::Values(false, true),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Native" : "Compositor";
                         });

}  // namespace
#endif  // CEF_API_ADDED(CEF_NEXT)
