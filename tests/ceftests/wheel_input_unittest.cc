// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <algorithm>
#include <limits>

#include "include/cef_parser.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/osr_input_util.h"

namespace {

constexpr char kUrl[] = "https://tests/wheel-input";
enum class Scenario { Dom, Scroll, PageScroll, Momentum, MomentumScroll };
enum class FrameMode { Main, SameOrigin, CrossOrigin };

double GetNumber(CefRefPtr<CefDictionaryValue> value, const char* key) {
  return value->GetType(key) == VTYPE_INT ? value->GetInt(key)
                                          : value->GetDouble(key);
}

class WheelInputTestHandler : public RoutingTestHandler,
                              public CefRenderHandler {
 public:
  WheelInputTestHandler(bool osr,
                        Scenario scenario,
                        FrameMode frame,
                        float scale)
      : osr_(osr), scenario_(scenario), frame_mode_(frame), scale_(scale) {}

  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser>,
      CefRefPtr<CefFrame>,
      CefRefPtr<CefRequest>,
      bool,
      bool,
      const CefString&,
      bool& disable_default_handling) override {
    // These pages use only registered resources. Do not let favicon requests
    // fall through to the network and retain a completed fixture.
    disable_default_handling = true;
    return this;
  }
  void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override {
    rect = CefRect(0, 0, 400, 300);
  }
  bool GetScreenInfo(CefRefPtr<CefBrowser>, CefScreenInfo& info) override {
    info.device_scale_factor = scale_;
    return true;
  }
  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirty,
               const void*,
               int width,
               int height) override {
    if (waiting_for_paint_) {
      waiting_for_paint_ = false;
      Arm(browser);
    }
    if (started_ && NeedsScrollPaint() && type == PET_VIEW &&
        dirty.size() == 1U && dirty[0] == CefRect(0, 0, width, height)) {
      EXPECT_EQ(static_cast<int>(400 * scale_), width);
      EXPECT_EQ(static_cast<int>(300 * scale_), height);
      // Preserve OSRTest.Scrolling's full repaint assertion while also
      // requiring the renderer to report an actual change in scroll offset.
      scroll_painted_ = true;
      MaybeFinish();
    }
  }

  void RunTest() override {
    SetTestTimeout(10000);
    const bool cancel = CancelWheel();
    // Unchanged scrollbars need not repaint during a page scroll. Hide them so
    // the scrolling content fills the view required by the full-paint check.
    std::string html = R"(
      <style>html{scrollbar-width:none}html,body{margin:0}
      body{width:3000px;height:3000px}
      button{position:absolute;left:20px;top:20px;width:140px;height:50px}</style>
      <button id="setup">Ready</button><script>
      const cancel = CANCEL;
      const requiredWheels = REQUIRED_WHEELS, minY = MIN_Y, both = BOTH;
      let armed = false, wheelCount = 0, doneSent = false;
      function done() {
        if (!doneSent) {
          doneSent = true;
          window.testQuery({request:'done:'+JSON.stringify({x:scrollX,y:scrollY})});
        }
      }
      setup.onclick = () => {
        if (!armed) { armed = true; window.testQuery({request:'armed'}); }
      };
      document.addEventListener('wheel', e => {
        if (!e.deltaX && !e.deltaY) return;
        ++wheelCount;
        if (cancel) e.preventDefault();
        window.testQuery({request:'wheel:'+JSON.stringify({
          x:e.clientX,y:e.clientY,dx:e.deltaX,dy:e.deltaY,
          mode:e.deltaMode,shift:e.shiftKey,cancelable:e.cancelable})});
      }, {passive:false});
      window.addEventListener('scroll', () => {
        if (!cancel && wheelCount >= requiredWheels && scrollY > minY &&
            (!both || scrollX > 0)) done();
      });
      let loaded = false, parentReady = window === top, readySent = false;
      function ready() {
        if (loaded && parentReady && !readySent) {
          readySent = true;
          requestAnimationFrame(() => requestAnimationFrame(() =>
            window.testQuery({request:'ready'})));
        }
      }
      window.addEventListener('message', e => {
        if (e.source === parent && e.data === 'parent-ready') {
          parentReady = true; ready();
        }
      });
      window.addEventListener('load', () => {loaded = true; ready();});
      </script>)";
    html.replace(html.find("CANCEL"), 6, cancel ? "true" : "false");
    html.replace(html.find("REQUIRED_WHEELS"), 15, IsMomentum() ? "2" : "1");
    html.replace(html.find("MIN_Y"), 5,
                 scenario_ == Scenario::MomentumScroll ? "10" : "0");
#if CEF_API_ADDED(CEF_NEXT)
    html.replace(html.find("BOTH"), 4,
                 scenario_ == Scenario::Scroll ? "true" : "false");
#else
    html.replace(html.find("BOTH"), 4, "false");
#endif
    if (IsIframe()) {
      target_url_ = frame_mode_ == FrameMode::SameOrigin
                        ? "https://tests/wheel-child"
                        : "https://wheel-other.test/wheel-child";
      AddResource(target_url_, html, "text/html");
      AddResource(
          kUrl,
          "<style>body{margin:0;overflow:hidden}iframe{position:absolute;"
          "left:80px;top:80px;width:300px;height:200px;border:0}</style>"
          "<iframe id='child' src='" +
              target_url_ +
              "'></iframe><script>"
              "window.addEventListener('load',()=>child.contentWindow."
              "postMessage('parent-ready','*'));"
              "document.addEventListener('wheel',()=>window.testQuery({"
              "request:'parent-wheel'}));</script>",
          "text/html");
    } else {
      target_url_ = kUrl;
      AddResource(kUrl, html, "text/html");
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

  void DestroyTest() override {
    finished_ = true;
    waiting_for_paint_ = false;
    RoutingTestHandler::DestroyTest();
  }

  void OnTestTimeout(int timeout_ms, bool treat_as_error) override {
    LOG(ERROR) << "Wheel test state: started=" << started_
               << " wheels=" << wheel_count_
               << " renderer_done=" << renderer_done_
               << " scroll_painted=" << scroll_painted_;
    RoutingTestHandler::OnTestTimeout(timeout_ms, treat_as_error);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t,
               const CefString& request,
               bool,
               CefRefPtr<Callback> callback) override {
    callback->Success("");
    const std::string text = request;
    if (text == "parent-wheel") {
      ADD_FAILURE() << "Wheel reached the parent instead of the iframe";
      DestroyTest();
      return true;
    }
    EXPECT_EQ(target_url_, frame->GetURL().ToString());
    EXPECT_EQ(!IsIframe(), frame->IsMain());
    if (text == "ready") {
      browser->GetHost()->SetFocus(true);
      if (osr_) {
        waiting_for_paint_ = true;
        browser->GetHost()->Invalidate(PET_VIEW);
      } else {
        Arm(browser);
      }
    } else if (text == "armed") {
      if (!started_) {
        started_ = true;
        Send(browser);
      }
    } else if (text.starts_with("wheel:")) {
      auto value = CefParseJSON(text.substr(6), JSON_PARSER_RFC);
      EXPECT_TRUE(value && value->GetType() == VTYPE_DICTIONARY);
      if (!value || value->GetType() != VTYPE_DICTIONARY) {
        DestroyTest();
        return true;
      }
      auto data = value->GetDictionary();
      // Input and deltas are DIPs. Surface hit testing can round positions to
      // device pixels (notably at fractional DPI), and DOM wheel coordinates
      // can be integral. Allow one device pixel or half a DIP, respectively.
#if CEF_API_ADDED(CEF_NEXT)
      const double tolerance = std::max(0.5, 1.0 / scale_) + 0.001;
      EXPECT_NEAR(40.5, GetNumber(data, "x"), tolerance);
      EXPECT_NEAR(40.5, GetNumber(data, "y"), tolerance);
#else
      EXPECT_NEAR(40, GetNumber(data, "x"), 1);
      EXPECT_NEAR(40, GetNumber(data, "y"), 1);
#endif
#if CEF_API_ADDED(CEF_NEXT)
      const bool page = scenario_ == Scenario::PageScroll;
      const double dx = page ? 0 : scenario_ == Scenario::Scroll ? 4.5 : -0.5;
      // Chromium scales float deltas to device pixels and back before exposing
      // DOM doubles. Check float precision across that round trip.
      EXPECT_FLOAT_EQ(dx, GetNumber(data, "dx"));
      EXPECT_FLOAT_EQ(page ? 1 : 7.25, GetNumber(data, "dy"));
      EXPECT_EQ(page ? 2 : 0, data->GetInt("mode"));
      EXPECT_EQ(scenario_ == Scenario::Dom, data->GetBool("shift"));
#else
      EXPECT_DOUBLE_EQ(0, GetNumber(data, "dx"));
      EXPECT_DOUBLE_EQ(60, GetNumber(data, "dy"));
      EXPECT_EQ(0, data->GetInt("mode"));
#endif
      if (CancelWheel() || wheel_count_ == 0) {
        EXPECT_TRUE(data->GetBool("cancelable"));
      }
      ++wheel_count_;
#if CEF_API_ADDED(CEF_NEXT)
      if (scenario_ != Scenario::PageScroll) {
        SendEnd(browser, wheel_count_ == 2);
      }
      if (IsMomentum() && wheel_count_ == 1) {
        Send(browser, true);
        return true;
      }
#endif
      if (CancelWheel()) {
        frame->ExecuteJavaScript(
            "requestAnimationFrame(()=>requestAnimationFrame(done))",
            target_url_, 0);
      }
    } else if (text.starts_with("done:")) {
      auto value = CefParseJSON(text.substr(5), JSON_PARSER_RFC);
      EXPECT_TRUE(value && value->GetType() == VTYPE_DICTIONARY);
      EXPECT_EQ(IsMomentum() ? 2 : 1, wheel_count_);
      if (value && value->GetType() == VTYPE_DICTIONARY) {
        const double y = GetNumber(value->GetDictionary(), "y");
        if (CancelWheel()) {
          EXPECT_DOUBLE_EQ(0, y);
          EXPECT_DOUBLE_EQ(0, GetNumber(value->GetDictionary(), "x"));
        } else {
          EXPECT_GT(y, scenario_ == Scenario::MomentumScroll ? 10 : 0);
#if CEF_API_ADDED(CEF_NEXT)
          if (scenario_ == Scenario::Scroll) {
            EXPECT_GT(GetNumber(value->GetDictionary(), "x"), 0);
          }
#endif
        }
      }
      renderer_done_ = true;
      MaybeFinish();
    } else {
      ADD_FAILURE() << text;
      DestroyTest();
    }
    return true;
  }

 private:
  bool IsIframe() const { return frame_mode_ != FrameMode::Main; }
  bool IsMomentum() const {
    return scenario_ == Scenario::Momentum ||
           scenario_ == Scenario::MomentumScroll;
  }
  bool CancelWheel() const {
    return scenario_ == Scenario::Dom || scenario_ == Scenario::Momentum;
  }
  bool NeedsScrollPaint() const {
    return osr_ && !IsIframe() &&
           (scenario_ == Scenario::Scroll || scenario_ == Scenario::PageScroll);
  }
  void MaybeFinish() {
    if (renderer_done_ && (!NeedsScrollPaint() || scroll_painted_)) {
      DestroyTest();
    }
  }
  void Arm(CefRefPtr<CefBrowser> browser) {
    if (started_ || finished_) {
      return;
    }
    // Successful renderer click delivery also establishes compositor hit-test
    // readiness for an out-of-process iframe. Stop retries after the reply.
    CefMouseEvent mouse;
    mouse.x = mouse.y = IsIframe() ? 120 : 40;
    browser->GetHost()->SendMouseClickEvent(mouse, MBT_LEFT, false, 1);
    browser->GetHost()->SendMouseClickEvent(mouse, MBT_LEFT, true, 1);
    GetUIThreadHelper()->PostDelayedTask(
        base::BindOnce(&WheelInputTestHandler::Arm, base::Unretained(this),
                       browser),
        25);
  }

  void Send(CefRefPtr<CefBrowser> browser, bool momentum = false) {
#if CEF_API_ADDED(CEF_NEXT)
    CefWheelInputEvent event;
    event.x = event.y = IsIframe() ? 120.5f : 40.5f;
    event.screen_x = 500.5f;
    event.screen_y = 600.5f;
    event.delta_x = scenario_ == Scenario::Scroll ? -4.5f : 0.5f;
    event.delta_y = -7.25f;
    event.wheel_ticks_x = event.delta_x / 40;
    event.wheel_ticks_y = event.delta_y / 40;
    if (scenario_ == Scenario::Scroll && frame_mode_ == FrameMode::Main) {
      event.delta_units = CEF_GESTURE_SCROLL_PIXEL;
    }
    if (scenario_ == Scenario::PageScroll) {
      event.delta_units = CEF_GESTURE_SCROLL_PAGE;
      event.delta_x = 0;
      event.delta_y = -1;
      event.wheel_ticks_x = 0;
      event.wheel_ticks_y = -1;
    } else if (momentum) {
      event.momentum_phase = CEF_WHEEL_PHASE_BEGAN;
    } else if (scenario_ != Scenario::Scroll) {
      event.phase = CEF_WHEEL_PHASE_BEGAN;
    }
    if (scenario_ == Scenario::Dom) {
      event.modifiers = CEF_EVENTFLAG_SHIFT_DOWN;
      // Also exercise BrowserHost's off-UI-thread dispatch and snapshot copy.
      CefPostTask(TID_IO, base::BindOnce(&CefBrowserHost::SendWheelEvent,
                                         browser->GetHost(), event));
    } else {
      browser->GetHost()->SendWheelEvent(event);
    }
#else
    CefMouseEvent event;
    event.x = event.y = IsIframe() ? 120 : 40;
    browser->GetHost()->SendMouseWheelEvent(event, 0, -60);
#endif
  }

#if CEF_API_ADDED(CEF_NEXT)
  void SendEnd(CefRefPtr<CefBrowser> browser, bool momentum) {
    CefWheelInputEvent event;
    event.x = event.y = IsIframe() ? 120.5f : 40.5f;
    event.screen_x = 500.5f;
    event.screen_y = 600.5f;
    if (momentum) {
      event.momentum_phase = CEF_WHEEL_PHASE_ENDED;
    } else {
      // Use the canceled DOM cases to cover native scroll cancellation too,
      // without adding another browser/frame matrix.
      event.phase = scenario_ == Scenario::Dom ? CEF_WHEEL_PHASE_CANCELLED
                                               : CEF_WHEEL_PHASE_ENDED;
    }
    browser->GetHost()->SendWheelEvent(event);
  }
#endif

  const bool osr_;
  const Scenario scenario_;
  const FrameMode frame_mode_;
  const float scale_;
  std::string target_url_;
  bool waiting_for_paint_ = false;
  bool started_ = false;
  bool finished_ = false;
  bool renderer_done_ = false;
  bool scroll_painted_ = false;
  int wheel_count_ = 0;
  IMPLEMENT_REFCOUNTING(WheelInputTestHandler);
};

void RunWheelTest(bool osr,
                  Scenario scenario,
                  FrameMode frame = FrameMode::Main,
                  float scale = 1) {
  CefRefPtr<WheelInputTestHandler> handler =
      new WheelInputTestHandler(osr, scenario, frame, scale);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

#if CEF_API_ADDED(CEF_NEXT)
TEST(OsrInputUtilTest, WheelSnapshot) {
  CefMouseEvent mouse;
  mouse.x = 17;
  mouse.y = 23;
  mouse.modifiers = CEF_EVENTFLAG_SHIFT_DOWN | CEF_EVENTFLAG_LEFT_MOUSE_BUTTON |
                    CEF_EVENTFLAG_PRECISION_SCROLLING_DELTA |
                    CEF_EVENTFLAG_SCROLL_BY_PAGE;
  const auto event = client::MakeOsrWheelInputEvent(mouse, -100.5f, 200.25f);
  EXPECT_EQ(sizeof(cef_wheel_input_event_t), event.size);
  EXPECT_FLOAT_EQ(17, event.x);
  EXPECT_FLOAT_EQ(23, event.y);
  EXPECT_FLOAT_EQ(-100.5f, event.screen_x);
  EXPECT_FLOAT_EQ(200.25f, event.screen_y);
  EXPECT_EQ(static_cast<uint32_t>(CEF_EVENTFLAG_SHIFT_DOWN |
                                  CEF_EVENTFLAG_LEFT_MOUSE_BUTTON),
            event.modifiers);
  EXPECT_EQ(0, event.timestamp);
  EXPECT_EQ(CEF_GESTURE_SCROLL_PRECISE_PIXEL, event.delta_units);
  EXPECT_EQ(CEF_WHEEL_PHASE_NONE, event.phase);
  EXPECT_EQ(CEF_WHEEL_PHASE_NONE, event.momentum_phase);
}

TEST(OsrInputUtilTest, WindowsWheelSettings) {
  // Exercise both axes/signs, partial detents, line/character settings,
  // disabled scrolling and page scrolling without a platform-specific test.
  constexpr uint32_t page_scroll = std::numeric_limits<uint32_t>::max();
  const struct {
    float ticks;
    uint32_t amount;
    float delta;
  } cases[] = {
      {1, 3, 100},        {-1, 3, -100},       {1, 1, 33.333333f},
      {.125f, 3, 12.5f},  {-.125f, 6, -25},    {1, 0, 0},
      {0, 3, 0},          {1, page_scroll, 1}, {-.125f, page_scroll, -1},
      {0, page_scroll, 0}};
  for (bool horizontal : {false, true}) {
    for (const auto& test : cases) {
      CefWheelInputEvent event;
      client::SetOsrWindowsWheelDelta(event, test.ticks, horizontal,
                                      test.amount);
      const bool page = test.amount == page_scroll;
      EXPECT_EQ(page ? CEF_GESTURE_SCROLL_PAGE : CEF_GESTURE_SCROLL_PIXEL,
                event.delta_units);
      EXPECT_FLOAT_EQ(horizontal ? test.ticks : 0, event.wheel_ticks_x);
      EXPECT_FLOAT_EQ(horizontal ? 0 : test.ticks, event.wheel_ticks_y);
      EXPECT_FLOAT_EQ(horizontal ? test.delta : 0, event.delta_x);
      EXPECT_FLOAT_EQ(horizontal ? 0 : test.delta, event.delta_y);
    }
  }
}

TEST(WheelInputTest, FractionalDomOsr125) {
  RunWheelTest(true, Scenario::Dom, FrameMode::Main, 1.25f);
}
TEST(WheelInputTest, FractionalDomWindowed) {
  RunWheelTest(false, Scenario::Dom);
}
TEST(WheelInputTest, ScrollWindowed) {
  RunWheelTest(false, Scenario::Scroll);
}
TEST(WheelInputTest, ScrollOsr125) {
  RunWheelTest(true, Scenario::Scroll, FrameMode::Main, 1.25f);
}
TEST(WheelInputTest, ScrollCrossOriginOsr) {
  RunWheelTest(true, Scenario::Scroll, FrameMode::CrossOrigin, 1.25f);
}
TEST(WheelInputTest, CancelCrossOriginWindowed) {
  RunWheelTest(false, Scenario::Dom, FrameMode::CrossOrigin);
}
TEST(WheelInputTest, PageScrollOsr) {
  RunWheelTest(true, Scenario::PageScroll);
}
TEST(WheelInputTest, MomentumScrollSameOriginWindowed) {
  RunWheelTest(false, Scenario::MomentumScroll, FrameMode::SameOrigin);
}
TEST(WheelInputTest, MomentumCrossOriginOsr) {
  RunWheelTest(true, Scenario::Momentum, FrameMode::CrossOrigin, 1.25f);
}
#else
TEST(WheelInputTest, LegacyDomOsr) {
  RunWheelTest(true, Scenario::Dom);
}
TEST(WheelInputTest, LegacyScrollWindowed) {
  RunWheelTest(false, Scenario::Scroll);
}
TEST(WheelInputTest, LegacyScrollOsr125) {
  RunWheelTest(true, Scenario::Scroll, FrameMode::Main, 1.25f);
}
#endif

}  // namespace
