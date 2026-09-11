// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <memory>

#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/osr_begin_frame_timer_mac.h"

namespace {

// Pump the actual Cocoa timer source, including the mode used by live resize.
void PumpRunLoop(NSString* mode, NSTimeInterval seconds) {
  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:seconds];
  while ([deadline timeIntervalSinceNow] > 0) {
    if (![[NSRunLoop mainRunLoop] runMode:mode beforeDate:deadline]) {
      break;
    }
  }
}

void TimerFiresDuringEventTrackingImpl() {
  client::OsrBeginFrameTimerMac timer;
  int count = 0;
  timer.Start(60, [&count]() { ++count; });
  PumpRunLoop(NSEventTrackingRunLoopMode, 0.1);
  EXPECT_GE(count, 2);
  timer.Stop();
}

void TimerStopReleasesCallbackImpl() {
  client::OsrBeginFrameTimerMac timer;
  auto state = std::make_shared<int>(0);
  std::weak_ptr<int> weak_state = state;
  int count = 0;
  timer.Start(60, [state, &count]() { ++count; });
  state.reset();
  PumpRunLoop(NSDefaultRunLoopMode, 0.1);
  EXPECT_GT(count, 0);
  timer.Stop();
  timer.Stop();
  EXPECT_TRUE(weak_state.expired());
  const int stopped_count = count;
  PumpRunLoop(NSDefaultRunLoopMode, 0.05);
  EXPECT_EQ(stopped_count, count);
}

void TimerRestartReplacesCallbackImpl() {
  client::OsrBeginFrameTimerMac timer;
  int old_count = 0;
  int new_count = 0;
  timer.Start(60, [&old_count]() { ++old_count; });
  timer.Start(30, [&new_count]() { ++new_count; });
  PumpRunLoop(NSDefaultRunLoopMode, 0.15);
  EXPECT_EQ(0, old_count);
  EXPECT_GE(new_count, 2);
}

void TimerStopsOnDestructionImpl() {
  int count = 0;
  {
    client::OsrBeginFrameTimerMac timer;
    timer.Start(60, [&count]() { ++count; });
  }
  PumpRunLoop(NSDefaultRunLoopMode, 0.05);
  EXPECT_EQ(0, count);
}

// Exercise the same timer as cefclient against real external-frame browsers,
// for both paint delivery paths. Animation and a subsequent resize must paint.
class ExternalBeginFrameTestHandler : public RoutingTestHandler,
                                      public CefRenderHandler {
 public:
  explicit ExternalBeginFrameTestHandler(bool shared_texture)
      : shared_texture_(shared_texture) {}

  void RunTest() override {
    const char kUrl[] = "https://tests/external_begin_frame";
    AddResource(kUrl, R"(
      <html><body><script>
      let count = 0;
      function animate() {
        document.body.style.background = (++count % 2) ? 'red' : 'blue';
        if (count === 3)
          window.testQuery({request: 'animated'});
        requestAnimationFrame(animate);
      }
      requestAnimationFrame(animate);
      </script></body></html>)",
                "text/html");
    CefWindowInfo window_info;
    window_info.SetAsWindowless(kNullWindowHandle);
    window_info.external_begin_frame_enabled = true;
    window_info.shared_texture_enabled = shared_texture_;
    CefBrowserSettings settings;
    settings.windowless_frame_rate = 60;
    CefBrowserHost::CreateBrowser(window_info, this, kUrl, settings, nullptr,
                                  nullptr);
    SetTestTimeout(10000);
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    RoutingTestHandler::OnAfterCreated(browser);
    timer_ = std::make_unique<client::OsrBeginFrameTimerMac>();
    timer_->Start(
        60, [browser]() { browser->GetHost()->SendExternalBeginFrame(); });
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    timer_.reset();
    RoutingTestHandler::OnBeforeClose(browser);
  }

  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
    rect = CefRect(0, 0, resized_ ? 240 : 160, 120);
  }

  bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                     CefScreenInfo& screen_info) override {
    screen_info.device_scale_factor = 1.0f;
    screen_info.rect = CefRect(0, 0, 800, 600);
    screen_info.available_rect = screen_info.rect;
    return true;
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    EXPECT_EQ("animated", request.ToString());
    animated_ = true;
    callback->Success("");
    return true;
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirty_rects,
               const void* buffer,
               int width,
               int height) override {
    EXPECT_FALSE(shared_texture_);
    EXPECT_NE(nullptr, buffer);
    OnFrame(browser, width, height);
  }

  void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                          PaintElementType type,
                          const RectList& dirty_rects,
                          const CefAcceleratedPaintInfo& info) override {
    EXPECT_TRUE(shared_texture_);
    EXPECT_NE(nullptr, info.shared_texture_io_surface);
    OnFrame(browser, info.extra.visible_rect.width,
            info.extra.visible_rect.height);
  }

  void DestroyTest() override {
    EXPECT_TRUE(animated_);
    EXPECT_TRUE(resized_);
    EXPECT_TRUE(finished_);
    RoutingTestHandler::DestroyTest();
  }

 private:
  void OnFrame(CefRefPtr<CefBrowser> browser, int width, int height) {
    if (!animated_ || finished_) {
      return;
    }
    if (!resized_ && width == 160 && height == 120) {
      resized_ = true;
      // Avoid changing view bounds reentrantly inside a paint callback.
      CefPostTask(TID_UI, base::BindOnce(
                              [](CefRefPtr<CefBrowser> browser) {
                                browser->GetHost()->WasResized();
                              },
                              browser));
    } else if (resized_ && width == 240 && height == 120) {
      finished_ = true;
      CefPostTask(
          TID_UI,
          base::BindOnce(&ExternalBeginFrameTestHandler::DestroyTest, this));
    }
  }

  const bool shared_texture_;
  bool animated_ = false;
  bool resized_ = false;
  bool finished_ = false;
  std::unique_ptr<client::OsrBeginFrameTimerMac> timer_;

  IMPLEMENT_REFCOUNTING(ExternalBeginFrameTestHandler);
};

}  // namespace

UI_THREAD_TEST(OsrBeginFrameTest, TimerFiresDuringEventTracking)
UI_THREAD_TEST(OsrBeginFrameTest, TimerStopReleasesCallback)
UI_THREAD_TEST(OsrBeginFrameTest, TimerRestartReplacesCallback)
UI_THREAD_TEST(OsrBeginFrameTest, TimerStopsOnDestruction)

TEST(OsrBeginFrameTest, BitmapAnimationAndResize) {
  CefRefPtr<ExternalBeginFrameTestHandler> handler =
      new ExternalBeginFrameTestHandler(false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(OsrBeginFrameTest, SharedTextureAnimationAndResize) {
  CefRefPtr<ExternalBeginFrameTestHandler> handler =
      new ExternalBeginFrameTestHandler(true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
