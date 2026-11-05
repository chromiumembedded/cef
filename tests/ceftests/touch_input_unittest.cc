// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include "include/cef_parser.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/geometry_util.h"
#include "tests/shared/browser/osr_input_util.h"
#include "tests/shared/browser/osr_touchpad.h"

namespace {

constexpr char kUrl[] = "https://tests/touch-input";

// Preventing the DOM default action suppresses generated gestures. The existing
// MouseGestureInputTest cases cover unhandled touches producing tap/scroll
// gestures, including CefInputHandler consumption and iframe routing.
class TouchInputTestHandler : public RoutingTestHandler,
                              public CefRenderHandler {
 public:
  TouchInputTestHandler(bool osr, bool iframe, bool touchpad)
      : osr_(osr), iframe_(iframe), touchpad_(touchpad) {}

  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser>,
      CefRefPtr<CefFrame>,
      CefRefPtr<CefRequest>,
      bool,
      bool,
      const CefString&,
      bool& disable_default_handling) override {
    disable_default_handling = true;
    return this;
  }

  void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override {
    rect = CefRect(0, 0, 400, 300);
  }

  void OnPaint(CefRefPtr<CefBrowser>,
               PaintElementType,
               const RectList&,
               const void*,
               int,
               int) override {}

  bool GetScreenInfo(CefRefPtr<CefBrowser>, CefScreenInfo& info) override {
    info.device_scale_factor = 1.25f;
    return true;
  }

  void RunTest() override {
    SetTestTimeout(10000);
    const std::string html = R"(
      <style>body{margin:0;height:3000px;touch-action:none}
      button{position:absolute;left:0;top:0;width:30px;height:30px}</style>
      <button id="ready">Ready</button><script>
      ready.onclick=()=>window.testQuery({request:'ready'});
      for(const type of ['touchstart','touchmove','touchend','touchcancel']) {
        document.addEventListener(type,e=> {
          if(e.cancelable) e.preventDefault();
          const changed=e.changedTouches[0];
          const points=Array.from(e.touches,t=>({id:t.identifier,
            x:t.clientX,y:t.clientY,force:t.force}));
          window.testQuery({request:JSON.stringify({type:e.type,
            changed:e.changedTouches.length,id:changed.identifier,
            shift:e.shiftKey,points,scroll:scrollY})});
        },{passive:false});
      }
      window.onload=()=>requestAnimationFrame(()=>requestAnimationFrame(()=>
        window.testQuery({request:'loaded'})));
      </script>)";
    target_url_ = iframe_ ? "https://touch-other.test/touch-child" : kUrl;
    AddResource(target_url_, html, "text/html");
    if (iframe_) {
      AddResource(
          kUrl,
          "<style>body{margin:0}iframe{position:absolute;left:80px;top:80px;"
          "width:300px;height:200px;border:0}</style><iframe src='" +
              target_url_ +
              "'></iframe><script>"
              "document.ontouchstart=()=>window.testQuery({request:'parent-"
              "touch'});"
              "</script>",
          "text/html");
    }
    if (osr_) {
      SetUseAlloyStyle(true, true);
      CefWindowInfo info;
#if defined(OS_WIN)
      info.SetAsWindowless(GetDesktopWindow());
#else
      info.SetAsWindowless(kNullWindowHandle);
#endif
      info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
      CreateReusableBrowser(info, kUrl, CefBrowserSettings());
    } else {
      SetUseViews(true);
      CreateReusableBrowser(kUrl);
    }
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t,
               const CefString& request,
               bool,
               CefRefPtr<Callback> callback) override {
    callback->Success("");
    EXPECT_EQ(target_url_, frame->GetURL().ToString());
    EXPECT_EQ(!iframe_, frame->IsMain());
    if (request == "parent-touch") {
      ADD_FAILURE() << "Touch reached the parent instead of the iframe";
      DestroyTest();
    } else if (request == "loaded") {
      browser->GetHost()->SetFocus(true);
      Arm(browser);
    } else if (request == "ready") {
      if (!started_) {
        started_ = true;
        Send(browser);
      }
    } else {
      auto value = CefParseJSON(request, JSON_PARSER_RFC);
      EXPECT_TRUE(value && value->GetType() == VTYPE_DICTIONARY);
      if (!value || value->GetType() != VTYPE_DICTIONARY) {
        DestroyTest();
        return true;
      }
      const auto report = value->GetDictionary();
      constexpr const char* types[] = {
          "touchstart",  "touchstart", "touchmove", "touchend",
          "touchcancel", "touchstart", "touchend"};
      constexpr size_t counts[] = {1, 2, 2, 1, 0, 1, 0};
      EXPECT_LT(step_, std::size(types));
      if (step_ >= std::size(types)) {
        DestroyTest();
        return true;
      }
      EXPECT_EQ(types[step_], report->GetString("type").ToString());
      EXPECT_EQ(1, report->GetInt("changed"));
      EXPECT_TRUE(report->GetBool("shift"));
      EXPECT_EQ(0, report->GetInt("scroll"));
      const int id = report->GetInt("id");
      if (step_ == 0 || step_ == 5) {
        primary_ = id;
      } else if (step_ == 1) {
        secondary_ = id;
        EXPECT_NE(primary_, secondary_);
      } else {
        EXPECT_EQ(step_ == 3 ? secondary_ : primary_, id);
      }
      const auto points = report->GetList("points");
      EXPECT_TRUE(points);
      if (points) {
        EXPECT_EQ(counts[step_], points->GetSize());
        const double tolerance = osr_ ? 0.801 : 0.001;
        for (size_t i = 0; i < points->GetSize(); ++i) {
          const auto point = points->GetDictionary(i);
          const bool primary = point->GetInt("id") == primary_;
          const bool moved = step_ >= 2 && step_ <= 4;
          auto number = [&](const char* key) {
            return point->GetType(key) == VTYPE_INT ? point->GetInt(key)
                                                    : point->GetDouble(key);
          };
          EXPECT_NEAR(primary ? (moved ? 44.75 : 40.5) : 100.25, number("x"),
                      tolerance);
          EXPECT_NEAR(primary ? (moved ? 62.5 : 60.25) : 100.5, number("y"),
                      tolerance);
          EXPECT_DOUBLE_EQ(touchpad_ ? 0 : 0.5, number("force"));
        }
      }
      if (++step_ == std::size(types)) {
        finished_ = true;
        DestroyTest();
      } else {
        Send(browser);
      }
    }
    return true;
  }

 private:
  void Arm(CefRefPtr<CefBrowser> browser) {
    if (started_ || finished_) {
      return;
    }
    CefMouseEvent mouse;
    mouse.x = mouse.y = iframe_ ? 90 : 10;
    browser->GetHost()->SendMouseClickEvent(mouse, MBT_LEFT, false, 1);
    browser->GetHost()->SendMouseClickEvent(mouse, MBT_LEFT, true, 1);
    GetUIThreadHelper()->PostDelayedTask(
        base::BindOnce(&TouchInputTestHandler::Arm, base::Unretained(this),
                       browser),
        25);
  }

  void Send(CefRefPtr<CefBrowser> browser) {
    CefPostTask(TID_IO, base::BindOnce(&TouchInputTestHandler::SendOnIO, this,
                                       browser, step_));
  }

  void SendOnIO(CefRefPtr<CefBrowser> browser, size_t step) {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    constexpr std::array<int, 7> ids = {100000, 200000, 100000, 200000,
                                        100000, 300000, 300000};
    constexpr cef_touch_event_type_t types[] = {
        CEF_TET_PRESSED,   CEF_TET_PRESSED, CEF_TET_MOVED,   CEF_TET_RELEASED,
        CEF_TET_CANCELLED, CEF_TET_PRESSED, CEF_TET_RELEASED};
    CefTouchEvent touch;
    touch.id = ids[step];
    const bool secondary = step == 1 || step == 3;
    const bool moved = step == 2 || step == 4;
    touch.x = secondary ? 100.25f : moved ? 44.75f : 40.5f;
    touch.y = secondary ? 100.5f : moved ? 62.5f : 60.25f;
    touch.x += iframe_ ? 80 : 0;
    touch.y += iframe_ ? 80 : 0;
    touch.radius_x = 6;
    touch.radius_y = 7;
    touch.pressure = 0.5f;
    touch.pointer_type = CEF_POINTER_TYPE_TOUCH;
    touch.modifiers = EVENTFLAG_SHIFT_DOWN;
    touch.type = types[step];
    if (touchpad_) {
      if (step == 0) {
        touchpad_emulator_.SetEnabled(true);
      }
      const auto id = static_cast<uint64_t>(ids[step]);
      std::erase_if(touchpad_frame_,
                    [id](const auto& point) { return point.id == id; });
      std::vector<CefTouchEvent> events;
      if (touch.type == CEF_TET_CANCELLED) {
        touchpad_frame_.clear();
        events = touchpad_emulator_.ProcessFrame(std::nullopt);
        touchpad_emulator_.ProcessFrame(touchpad_frame_);
      } else {
        if (touch.type != CEF_TET_RELEASED) {
          touchpad_frame_.push_back({id, touch.x / 400, touch.y / 300, true});
        }
        events = touchpad_emulator_.ProcessFrame(touchpad_frame_);
      }
      ASSERT_EQ(1U, events.size());
      touch = events[0];
      client::MapTouchpadToView(touch, 500, 375, 1.25f);
      client::DeviceToLogical(touch, 1.25f);
      touch.modifiers = EVENTFLAG_SHIFT_DOWN;
    }
    if (step == 0 && !iframe_) {
      // Rejected presses must not reserve the contact ID or reach browser UI.
      auto invalid = touch;
      if (!osr_) {
        invalid.x = -1;
        browser->GetHost()->SendTouchEvent(invalid);
      }
      invalid = touch;
      invalid.id = -1;
      browser->GetHost()->SendTouchEvent(invalid);
      invalid = touch;
      invalid.x = std::numeric_limits<float>::infinity();
      browser->GetHost()->SendTouchEvent(invalid);
      invalid = touch;
      invalid.rotation_angle = 180;
      browser->GetHost()->SendTouchEvent(invalid);
      invalid = touch;
      invalid.pressure = 2;
      browser->GetHost()->SendTouchEvent(invalid);
      invalid = touch;
      invalid.type = static_cast<cef_touch_event_type_t>(999);
      browser->GetHost()->SendTouchEvent(invalid);
      invalid = touch;
      invalid.pointer_type = static_cast<cef_pointer_type_t>(999);
      browser->GetHost()->SendTouchEvent(invalid);
    }
    if (step == 0) {
      auto orphan = touch;
      orphan.type = CEF_TET_MOVED;
      browser->GetHost()->SendTouchEvent(orphan);
    }
    browser->GetHost()->SendTouchEvent(touch);
    if (step == 0) {
      browser->GetHost()->SendTouchEvent(touch);  // Duplicate press is ignored.
    }
    touch.x = -1000;  // Asynchronous dispatch must own its snapshot.
  }

  const bool osr_;
  const bool iframe_;
  const bool touchpad_;
  client::OsrTouchpadEmulator touchpad_emulator_;
  client::OsrTouchpadFrame touchpad_frame_;
  std::string target_url_;
  size_t step_ = 0;
  int primary_ = -1;
  int secondary_ = -1;
  bool started_ = false;
  bool finished_ = false;
  IMPLEMENT_REFCOUNTING(TouchInputTestHandler);
};

void RunTouchTest(bool osr, bool iframe, bool touchpad = false) {
  CefRefPtr<TouchInputTestHandler> handler =
      new TouchInputTestHandler(osr, iframe, touchpad);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(TouchInputTest, MultiTouchWindowed) {
  RunTouchTest(false, false);
}

TEST(TouchInputTest, MultiTouchCrossOriginWindowed) {
  RunTouchTest(false, true);
}

TEST(TouchInputTest, MultiTouchOsr125) {
  RunTouchTest(true, false);
}

TEST(TouchInputTest, TouchpadCrossOriginOsr125) {
  RunTouchTest(true, true, true);
}

TEST(OsrInputUtilTest, PointerTouchLifecycle) {
  client::OsrPointerTouchEmulator emulator;
  EXPECT_TRUE(emulator.Pointer(CEF_TET_PRESSED, 0.2f, 0.3f).empty());
  EXPECT_TRUE(emulator.SetEnabled(true).empty());
  EXPECT_TRUE(emulator.Pointer(CEF_TET_MOVED, 0.2f, 0.3f).empty());
  EXPECT_TRUE(emulator.Pointer(CEF_TET_RELEASED, 0.2f, 0.3f).empty());
  auto events = emulator.Pointer(CEF_TET_PRESSED, 0.2f, 0.3f);
  ASSERT_EQ(1U, events.size());
  const int id = events[0].id;
  EXPECT_EQ(CEF_TET_PRESSED, events[0].type);
  EXPECT_EQ(CEF_POINTER_TYPE_TOUCH, events[0].pointer_type);
  EXPECT_TRUE(emulator.Pointer(CEF_TET_PRESSED, 0.4f, 0.5f).empty());
  EXPECT_TRUE(emulator.Pointer(CEF_TET_MOVED, 0.2f, 0.3f).empty());
  events = emulator.Pointer(CEF_TET_MOVED, 0.4f, 0.5f);
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(id, events[0].id);
  EXPECT_EQ(CEF_TET_MOVED, events[0].type);
  events = emulator.Pointer(CEF_TET_RELEASED, 0.4f, 0.5f);
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(id, events[0].id);
  EXPECT_EQ(CEF_TET_RELEASED, events[0].type);
  EXPECT_TRUE(emulator.Cancel().empty());
  EXPECT_TRUE(emulator.Pointer(CEF_TET_MOVED, 0.6f, 0.7f).empty());
}

TEST(OsrInputUtilTest, PointerTouchCancellation) {
  client::OsrPointerTouchEmulator emulator;
  emulator.SetEnabled(true);
  emulator.Pointer(CEF_TET_PRESSED, 0.125f, 0.625f);
  auto events = emulator.SetEnabled(false);
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(CEF_TET_CANCELLED, events[0].type);
  EXPECT_FLOAT_EQ(0.125f, events[0].x);
  EXPECT_FLOAT_EQ(0.625f, events[0].y);
  EXPECT_TRUE(emulator.SetEnabled(false).empty());
  emulator.SetEnabled(true);
  EXPECT_TRUE(emulator.Pointer(CEF_TET_MOVED, 0.2f, 0.3f).empty());
  emulator.Pointer(CEF_TET_PRESSED, 0.2f, 0.3f);
  ASSERT_EQ(1U, emulator.Cancel().size());
  EXPECT_TRUE(emulator.Cancel().empty());
  // A lost capture does not disable the mode; the next press starts cleanly.
  EXPECT_EQ(1U, emulator.Pointer(CEF_TET_PRESSED, 0.2f, 0.3f).size());
}

TEST(OsrInputUtilTest, PointerTouchScroll) {
  client::OsrPointerTouchEmulator emulator;
  EXPECT_TRUE(emulator.Scroll(0.5f, 0.5f, 0, -0.1f).empty());
  emulator.SetEnabled(true);
  EXPECT_TRUE(emulator.Scroll(0.5f, 0.5f, 0, 0).empty());
  for (const auto& delta : {std::pair(0.f, -0.125f), std::pair(0.125f, 0.f),
                            std::pair(-1.f, 1.f)}) {
    auto events = emulator.Scroll(1.f, 0.f, delta.first, delta.second);
    ASSERT_EQ(2U, events.size());
    EXPECT_EQ(CEF_TET_PRESSED, events[0].type);
    EXPECT_EQ(CEF_TET_MOVED, events[1].type);
    EXPECT_FLOAT_EQ(std::clamp(delta.first, -0.25f, 0.25f),
                    events[1].x - events[0].x);
    EXPECT_FLOAT_EQ(std::clamp(delta.second, -0.25f, 0.25f),
                    events[1].y - events[0].y);
    for (const auto& event : events) {
      EXPECT_EQ(events[0].id, event.id);
      EXPECT_GE(event.x, 0);
      EXPECT_LE(event.x, 1);
      EXPECT_GE(event.y, 0);
      EXPECT_LE(event.y, 1);
    }
    EXPECT_TRUE(emulator.scrolling());
    events = emulator.EndScroll();
    ASSERT_EQ(1U, events.size());
    EXPECT_EQ(CEF_TET_CANCELLED, events[0].type);
    EXPECT_FALSE(emulator.scrolling());
    EXPECT_TRUE(emulator.EndScroll().empty());
  }
  emulator.Pointer(CEF_TET_PRESSED, 0.2f, 0.3f);
  EXPECT_TRUE(emulator.Scroll(0.5f, 0.5f, 0, -0.125f).empty());
  auto events = emulator.Pointer(CEF_TET_RELEASED, 0.2f, 0.3f);
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(CEF_TET_RELEASED, events[0].type);
  EXPECT_EQ(2U, emulator.Scroll(0.5f, 0.5f, 0, -0.125f).size());
}

TEST(OsrInputUtilTest, PointerTouchWindowsWheelScale) {
  // Partial detents must move the touch contact by the same DIPs as native
  // scrolling, independent of display scale or the selected axis.
  for (float scale : {1.f, 1.25f, 2.4f}) {
    const int pixel_width = static_cast<int>(400 * scale);
    const int pixel_height = static_cast<int>(300 * scale);
    const int width = client::DeviceToLogical(pixel_width, scale);
    const int height = client::DeviceToLogical(pixel_height, scale);
    for (bool horizontal : {false, true}) {
      client::OsrPointerTouchEmulator emulator;
      emulator.SetEnabled(true);
      const float delta = client::GetOsrWindowsWheelDelta(0.125f, 3);
      for (int step = 1; step <= 2; ++step) {
        auto events =
            emulator.Scroll(0.5f, 0.5f, horizontal ? delta / width : 0,
                            horizontal ? 0 : delta / height);
        ASSERT_FALSE(events.empty());
        auto contact = events.back();
        client::MapTouchpadToView(contact, pixel_width, pixel_height, scale);
        client::DeviceToLogical(contact, scale);
        EXPECT_NEAR(width / 2.f + (horizontal ? 12.5f * step : 0), contact.x,
                    0.001f);
        EXPECT_NEAR(height / 2.f + (horizontal ? 0 : 12.5f * step), contact.y,
                    0.001f);
      }
    }
  }
}

TEST(OsrInputUtilTest, PointerTouchScrollBurst) {
  client::OsrPointerTouchEmulator emulator;
  emulator.SetEnabled(true);
  auto events = emulator.Scroll(0.5f, 0.5f, 0, -0.001f);
  ASSERT_EQ(2U, events.size());
  const int id = events[0].id;
  for (int i = 1; i < 40; ++i) {
    // The pointer can hover elsewhere without moving or releasing the swipe.
    EXPECT_TRUE(emulator.Pointer(CEF_TET_MOVED, 0.8f, 0.8f).empty());
    EXPECT_TRUE(emulator.Pointer(CEF_TET_RELEASED, 0.8f, 0.8f).empty());
    events = emulator.Scroll(0.8f, 0.8f, 0, -0.001f);
    ASSERT_EQ(1U, events.size());
    EXPECT_EQ(CEF_TET_MOVED, events[0].type);
    EXPECT_EQ(id, events[0].id);
    EXPECT_FLOAT_EQ(0.5f, events[0].x);
  }
  EXPECT_NEAR(0.46f, events[0].y, 0.00001f);
  events = emulator.EndScroll();
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(CEF_TET_CANCELLED, events[0].type);
  EXPECT_EQ(id, events[0].id);
  // A later burst starts at the current pointer, not the previous endpoint.
  events = emulator.Scroll(0.8f, 0.8f, 0.01f, 0);
  ASSERT_EQ(2U, events.size());
  EXPECT_EQ(CEF_TET_PRESSED, events[0].type);
  EXPECT_FLOAT_EQ(0.8f, events[0].x);
}

TEST(OsrInputUtilTest, PointerTouchScrollInterruption) {
  client::OsrPointerTouchEmulator emulator;
  emulator.SetEnabled(true);
  emulator.Scroll(0.5f, 0.5f, 0, -0.01f);
  auto events = emulator.Pointer(CEF_TET_PRESSED, 0.2f, 0.3f);
  ASSERT_EQ(2U, events.size());
  EXPECT_EQ(CEF_TET_CANCELLED, events[0].type);
  EXPECT_EQ(CEF_TET_PRESSED, events[1].type);
  EXPECT_FLOAT_EQ(0.2f, events[1].x);
  EXPECT_FALSE(emulator.scrolling());
  // A stale idle callback must not cancel a replacement drag contact.
  EXPECT_TRUE(emulator.EndScroll().empty());
  EXPECT_TRUE(emulator.Scroll(0.5f, 0.5f, 0, -0.01f).empty());
  EXPECT_EQ(1U, emulator.Pointer(CEF_TET_RELEASED, 0.2f, 0.3f).size());
  emulator.Scroll(0.5f, 0.5f, 0, -0.01f);
  events = emulator.SetEnabled(false);
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(CEF_TET_CANCELLED, events[0].type);
  EXPECT_FALSE(emulator.scrolling());
  EXPECT_TRUE(emulator.EndScroll().empty());
}

TEST(OsrInputUtilTest, PointerTouchScrollBounds) {
  client::OsrPointerTouchEmulator emulator;
  emulator.SetEnabled(true);
  emulator.Scroll(0.5f, 0.5f, 0, -0.25f);
  auto events = emulator.Scroll(0.5f, 0.5f, 0, -0.25f);
  ASSERT_EQ(1U, events.size());
  EXPECT_FLOAT_EQ(0, events[0].y);
  EXPECT_TRUE(emulator.Scroll(0.5f, 0.5f, 0, -0.25f).empty());
  EXPECT_TRUE(emulator.scrolling());
  events = emulator.Scroll(0.5f, 0.5f, 0, 0.01f);
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(CEF_TET_MOVED, events[0].type);
  EXPECT_FLOAT_EQ(0.01f, events[0].y);
}

TEST(OsrInputUtilTest, PointerTouchInvalidCoordinates) {
  client::OsrPointerTouchEmulator emulator;
  emulator.SetEnabled(true);
  const float infinity = std::numeric_limits<float>::infinity();
  EXPECT_TRUE(emulator.Pointer(CEF_TET_PRESSED, infinity, 0).empty());
  EXPECT_TRUE(emulator.Scroll(0, 0, infinity, 0).empty());
  auto events = emulator.Pointer(CEF_TET_PRESSED, -1, 2);
  ASSERT_EQ(1U, events.size());
  EXPECT_FLOAT_EQ(0, events[0].x);
  EXPECT_FLOAT_EQ(1, events[0].y);
}

TEST(OsrInputUtilTest, TouchpadFrameAssembly) {
  client::OsrTouchpadFrameAssembler assembler;
  const client::OsrTouchpadContact first = {1000, 0.25f, 0.5f, true};
  const client::OsrTouchpadContact second = {2000, 0.75f, 0.5f, true};
  EXPECT_FALSE(assembler.AddReport(2, {first}));
  auto frame = assembler.AddReport(0, {second});
  ASSERT_TRUE(frame);
  ASSERT_EQ(2U, frame->size());
  EXPECT_EQ(first.id, (*frame)[0].id);
  EXPECT_EQ(second.id, (*frame)[1].id);
  // Parallel reports may reserve unused contact collections after the count.
  frame = assembler.AddReport(1, {first, second});
  ASSERT_TRUE(frame);
  ASSERT_EQ(1U, frame->size());
  EXPECT_EQ(first.id, (*frame)[0].id);
  EXPECT_FALSE(assembler.AddReport(2, {first}));
  // A new frame replaces an incomplete old frame.
  frame = assembler.AddReport(1, {second});
  ASSERT_TRUE(frame);
  ASSERT_EQ(1U, frame->size());
  EXPECT_EQ(second.id, (*frame)[0].id);
  frame = assembler.AddReport(0, {});
  ASSERT_TRUE(frame);
  EXPECT_TRUE(frame->empty());
  EXPECT_FALSE(assembler.AddReport(257, {first}));
  EXPECT_TRUE(assembler.AddReport(1, {first}));
}

TEST(OsrInputUtilTest, TouchpadContactLifecycle) {
  client::OsrTouchpadEmulator emulator;
  EXPECT_TRUE(emulator.SetEnabled(true).empty());
  EXPECT_TRUE(emulator.enabled());
  client::OsrTouchpadFrame frame = {{1000, 0.25f, 0.5f, true}};
  auto events = emulator.ProcessFrame(frame);
  ASSERT_EQ(1U, events.size());
  const int first_id = events[0].id;
  EXPECT_EQ(static_cast<int>(CEF_TET_PRESSED), events[0].type);
  frame.push_back({2000, 0.75f, 0.5f, true});
  events = emulator.ProcessFrame(frame);
  ASSERT_EQ(1U, events.size());
  const int second_id = events[0].id;
  EXPECT_NE(first_id, second_id);
  EXPECT_TRUE(emulator.ProcessFrame(frame).empty());
  // Reordering a native frame must not release or change either contact.
  std::reverse(frame.begin(), frame.end());
  EXPECT_TRUE(emulator.ProcessFrame(frame).empty());
  frame[1].x = 0.3f;
  events = emulator.ProcessFrame(frame);
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(first_id, events[0].id);
  EXPECT_EQ(static_cast<int>(CEF_TET_MOVED), events[0].type);
  // Explicit HID up carries a final position; evdev can omit the contact.
  frame[0].down = false;
  frame[0].x = 0.8f;
  events = emulator.ProcessFrame(frame);
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(second_id, events[0].id);
  EXPECT_EQ(static_cast<int>(CEF_TET_RELEASED), events[0].type);
  EXPECT_FLOAT_EQ(0.8f, events[0].x);
  events = emulator.ProcessFrame(client::OsrTouchpadFrame());
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(first_id, events[0].id);
  EXPECT_EQ(static_cast<int>(CEF_TET_RELEASED), events[0].type);
  EXPECT_TRUE(emulator.ProcessFrame(client::OsrTouchpadFrame()).empty());
}

TEST(OsrInputUtilTest, TouchpadActivationAndCancellation) {
  client::OsrTouchpadEmulator emulator;
  const client::OsrTouchpadFrame held = {{1, 0.25f, 0.5f, true},
                                         {2, 0.75f, 0.5f, true}};
  EXPECT_TRUE(emulator.ProcessFrame(held).empty());
  EXPECT_TRUE(emulator.SetEnabled(true).empty());
  EXPECT_TRUE(emulator.ProcessFrame(held).empty());
  EXPECT_TRUE(
      emulator.ProcessFrame(client::OsrTouchpadFrame{{1, 0.3f, 0.5f, true}})
          .empty());
  EXPECT_TRUE(emulator.ProcessFrame(client::OsrTouchpadFrame()).empty());
  auto events = emulator.ProcessFrame(held);
  ASSERT_EQ(2U, events.size());
  events = emulator.SetEnabled(false);
  ASSERT_EQ(2U, events.size());
  EXPECT_FALSE(emulator.enabled());
  for (const auto& event : events) {
    EXPECT_EQ(static_cast<int>(CEF_TET_CANCELLED), event.type);
  }
  EXPECT_TRUE(emulator.SetEnabled(false).empty());
  EXPECT_TRUE(emulator.ProcessFrame(held).empty());
  EXPECT_TRUE(emulator.SetEnabled(true).empty());
  EXPECT_TRUE(emulator.ProcessFrame(held).empty());
  emulator.ProcessFrame(client::OsrTouchpadFrame());
  EXPECT_EQ(2U, emulator.ProcessFrame(held).size());
}

TEST(OsrInputUtilTest, TouchpadLostSynchronization) {
  client::OsrTouchpadEmulator emulator;
  emulator.SetEnabled(true);
  client::OsrTouchpadFrame frame = {{1, 0.25f, 0.5f, true}};
  ASSERT_EQ(1U, emulator.ProcessFrame(frame).size());
  auto events = emulator.ProcessFrame(std::nullopt);
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(static_cast<int>(CEF_TET_CANCELLED), events[0].type);
  EXPECT_TRUE(emulator.ProcessFrame(frame).empty());
  EXPECT_TRUE(emulator.ProcessFrame(client::OsrTouchpadFrame()).empty());
  ASSERT_EQ(1U, emulator.ProcessFrame(frame).size());
  frame.push_back(frame[0]);
  events = emulator.ProcessFrame(frame);
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(static_cast<int>(CEF_TET_CANCELLED), events[0].type);
  emulator.ProcessFrame(client::OsrTouchpadFrame());
  frame.resize(1);
  frame[0].x = std::numeric_limits<float>::infinity();
  EXPECT_TRUE(emulator.ProcessFrame(frame).empty());
  frame[0].x = 0.25f;
  EXPECT_TRUE(emulator.ProcessFrame(frame).empty());
  emulator.ProcessFrame(client::OsrTouchpadFrame());
  EXPECT_EQ(1U, emulator.ProcessFrame(frame).size());
}

TEST(OsrInputUtilTest, TouchpadContactLimit) {
  client::OsrTouchpadEmulator emulator;
  emulator.SetEnabled(true);
  client::OsrTouchpadFrame frame;
  for (uint64_t id = 0; id < 17; ++id) {
    frame.push_back({id, 0.5f, 0.5f, true});
  }
  EXPECT_EQ(16U, emulator.ProcessFrame(frame).size());
  frame.erase(frame.begin());
  auto events = emulator.ProcessFrame(frame);
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(static_cast<int>(CEF_TET_RELEASED), events[0].type);
  // Free capacity must not turn an already-held, ignored finger into a press.
  EXPECT_TRUE(emulator.ProcessFrame(frame).empty());
  frame.pop_back();
  EXPECT_TRUE(emulator.ProcessFrame(frame).empty());
  frame.push_back({16, 0.5f, 0.5f, true});
  events = emulator.ProcessFrame(frame);
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(static_cast<int>(CEF_TET_PRESSED), events[0].type);
}

TEST(OsrInputUtilTest, TouchpadFractionalCoordinates) {
  EXPECT_FLOAT_EQ(0.375f, client::NormalizeTouchpadAxis(-25, -100, 100));
  EXPECT_FLOAT_EQ(0, client::NormalizeTouchpadAxis(-101, -100, 100));
  EXPECT_FLOAT_EQ(1, client::NormalizeTouchpadAxis(101, -100, 100));
  EXPECT_FLOAT_EQ(0, client::NormalizeTouchpadAxis(1, 1, 1));
  CefTouchEvent event;
  event.x = client::NormalizeTouchpadAxis(-25, -100, 100);
  event.y = client::NormalizeTouchpadAxis(25, 0, 100);
  client::MapTouchpadToView(event, 403, 303, 1.25f);
  client::DeviceToLogical(event, 1.25f);
  EXPECT_FLOAT_EQ(120.75f, event.x);
  EXPECT_FLOAT_EQ(60.5f, event.y);
  for (float scale : {1.f, 1.1f, 1.25f, 1.5f, 1.75f, 2.f}) {
    event.x = event.y = 1;
    client::MapTouchpadToView(event, 403, 303, scale);
    client::DeviceToLogical(event, scale);
    const int width = client::DeviceToLogical(403, scale);
    const int height = client::DeviceToLogical(303, scale);
    EXPECT_LT(event.x, width);
    EXPECT_LT(event.y, height);
    EXPECT_NEAR(width, event.x, 0.001);
    EXPECT_NEAR(height, event.y, 0.001);
  }
  event.x = event.y = 0;
  client::MapTouchpadToView(event, 403, 303, 1.25f);
  EXPECT_FLOAT_EQ(0, event.x);
  EXPECT_FLOAT_EQ(0, event.y);
}

TEST(OsrInputUtilTest, TouchFractionalCoordinates) {
  CefTouchEvent event;
  event.x = 10.75f;
  event.y = -0.25f;
  event.radius_x = 3.125f;
  event.radius_y = 6.25f;
  event.rotation_angle = 45;
  event.pressure = 0.5f;
  client::DeviceToLogical(event, 1.25f);
  EXPECT_FLOAT_EQ(8.6f, event.x);
  EXPECT_FLOAT_EQ(-0.2f, event.y);
  EXPECT_FLOAT_EQ(2.5f, event.radius_x);
  EXPECT_FLOAT_EQ(5, event.radius_y);
  EXPECT_FLOAT_EQ(45, event.rotation_angle);
  EXPECT_FLOAT_EQ(0.5f, event.pressure);
}

TEST(OsrInputUtilTest, TouchContactLifecycle) {
  client::OsrTouchInputState state;
  auto update = [&](uint64_t native_id, cef_touch_event_type_t type) {
    CefTouchEvent event;
    event.type = type;
    return state.Update(native_id, event, false, 0, 0) ? event.id : -1;
  };
  // Native identifiers need not fit in a signed int or form a dense range.
  constexpr uint64_t first = std::numeric_limits<uint64_t>::max();
  const int primary = update(first, CEF_TET_PRESSED);
  const int secondary = update(12345, CEF_TET_PRESSED);
  EXPECT_GE(primary, 0);
  EXPECT_GE(secondary, 0);
  EXPECT_NE(primary, secondary);
  EXPECT_EQ(-1, update(first, CEF_TET_PRESSED));
  EXPECT_EQ(-1, update(67890, CEF_TET_MOVED));
  EXPECT_EQ(primary, update(first, CEF_TET_MOVED));
  EXPECT_EQ(primary, update(first, CEF_TET_RELEASED));
  EXPECT_EQ(-1, update(first, CEF_TET_MOVED));
  EXPECT_EQ(primary, update(67890, CEF_TET_PRESSED));
  EXPECT_EQ(secondary, update(12345, CEF_TET_CANCELLED));
  EXPECT_EQ(-1, update(12345, CEF_TET_RELEASED));
  EXPECT_EQ(primary, update(67890, CEF_TET_RELEASED));
  EXPECT_TRUE(state.Cancel().empty());
}

TEST(OsrInputUtilTest, TouchContactLimitAndCancellation) {
  client::OsrTouchInputState state;
  CefTouchEvent event;
  event.type = CEF_TET_PRESSED;
  for (uint64_t id = 0; id < 16; ++id) {
    ASSERT_TRUE(state.Update(id, event, false, 0, 0));
    EXPECT_EQ(static_cast<int>(id), event.id);
  }
  EXPECT_FALSE(state.Update(16, event, false, 0, 0));
  event.type = CEF_TET_RELEASED;
  ASSERT_TRUE(state.Update(0, event, false, 0, 0));
  event.type = CEF_TET_PRESSED;
  ASSERT_TRUE(state.Update(16, event, false, 0, 0));
  EXPECT_EQ(0, event.id);
  event.type = CEF_TET_MOVED;
  event.x = 10.75f;
  event.y = -0.25f;
  event.modifiers = EVENTFLAG_SHIFT_DOWN;
  ASSERT_TRUE(state.Update(16, event, false, 0, 0));
  const auto cancelled = state.Cancel();
  ASSERT_EQ(16U, cancelled.size());
  for (const auto& touch : cancelled) {
    EXPECT_EQ(static_cast<int>(CEF_TET_CANCELLED), touch.type);
    if (touch.id == 0) {
      EXPECT_FLOAT_EQ(10.75f, touch.x);
      EXPECT_FLOAT_EQ(-0.25f, touch.y);
      EXPECT_EQ(static_cast<uint32_t>(EVENTFLAG_SHIFT_DOWN), touch.modifiers);
    }
  }
  EXPECT_TRUE(state.Cancel().empty());
  EXPECT_FALSE(state.Update(16, event, false, 0, 0));
  event.type = CEF_TET_PRESSED;
  EXPECT_TRUE(state.Update(16, event, false, 0, 0));
  EXPECT_EQ(0, event.id);
}

TEST(OsrInputUtilTest, TouchPopupSequence) {
  client::OsrTouchInputState state;
  auto update = [&](uint64_t native_id, cef_touch_event_type_t type,
                    bool over_popup, int offset) {
    CefTouchEvent event;
    event.type = type;
    event.x = 10.75f;
    event.y = 20.25f;
    EXPECT_TRUE(state.Update(native_id, event, over_popup, offset, -offset));
    return event;
  };
  auto event = update(0, CEF_TET_PRESSED, true, 100);
  EXPECT_FLOAT_EQ(110.75f, event.x);
  EXPECT_FLOAT_EQ(-79.75f, event.y);
  // Moving outside or changing the popup does not change the latched offset.
  event = update(0, CEF_TET_MOVED, false, 200);
  EXPECT_FLOAT_EQ(110.75f, event.x);
  event = update(1, CEF_TET_PRESSED, false, 200);
  EXPECT_FLOAT_EQ(110.75f, event.x);
  update(0, CEF_TET_RELEASED, false, 200);
  event = update(1, CEF_TET_MOVED, false, 200);
  EXPECT_FLOAT_EQ(110.75f, event.x);
  const auto cancelled = state.Cancel();
  ASSERT_EQ(1U, cancelled.size());
  EXPECT_FLOAT_EQ(110.75f, cancelled[0].x);
  EXPECT_FLOAT_EQ(-79.75f, cancelled[0].y);
  // A sequence starting on the main view stays there when entering a popup.
  event = update(0, CEF_TET_PRESSED, false, 100);
  EXPECT_FLOAT_EQ(10.75f, event.x);
  event = update(1, CEF_TET_PRESSED, true, 100);
  EXPECT_FLOAT_EQ(10.75f, event.x);
  update(0, CEF_TET_RELEASED, true, 100);
  update(1, CEF_TET_RELEASED, true, 100);
  event = update(0, CEF_TET_PRESSED, true, 200);
  EXPECT_FLOAT_EQ(210.75f, event.x);
}

}  // namespace
