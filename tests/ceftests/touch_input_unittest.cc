// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <algorithm>
#include <array>
#include <limits>

#include "include/cef_parser.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/geometry_util.h"
#include "tests/shared/browser/osr_input_util.h"

namespace {

constexpr char kUrl[] = "https://tests/touch-input";

// Preventing the DOM default action suppresses generated gestures. The existing
// MouseGestureInputTest cases cover unhandled touches producing tap/scroll
// gestures, including CefInputHandler consumption and iframe routing.
class TouchInputTestHandler : public RoutingTestHandler,
                              public CefRenderHandler {
 public:
  TouchInputTestHandler(bool osr, bool iframe) : osr_(osr), iframe_(iframe) {}

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
          EXPECT_DOUBLE_EQ(0.5, number("force"));
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
  std::string target_url_;
  size_t step_ = 0;
  int primary_ = -1;
  int secondary_ = -1;
  bool started_ = false;
  bool finished_ = false;
  IMPLEMENT_REFCOUNTING(TouchInputTestHandler);
};

void RunTouchTest(bool osr, bool iframe) {
  CefRefPtr<TouchInputTestHandler> handler =
      new TouchInputTestHandler(osr, iframe);
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
