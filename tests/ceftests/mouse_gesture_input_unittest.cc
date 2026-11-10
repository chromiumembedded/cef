// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <array>
#include <chrono>
#include <cmath>
#include <deque>
#include <sstream>
#include <vector>

#include "include/cef_input_handler.h"
#include "include/cef_parser.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/osr_input_util.h"

namespace {

#if CEF_API_ADDED(15800)
using TestMouseEvent = CefMouseInputEvent;
constexpr auto kMouseDown = CEF_MOUSE_EVENT_DOWN;
constexpr auto kMouseUp = CEF_MOUSE_EVENT_UP;
constexpr auto kMouseMove = CEF_MOUSE_EVENT_MOVE;
constexpr float kCoordinateTolerance = 0.001f;
#else
// Expected values for legacy-only tests, following keyboard_input_unittest.cc.
// This is test data, not a substitute public API type.
struct TestMouseEvent {
  enum Type { Down, Up, Move } type = Down;
  uint32_t modifiers = 0;
  int x = 0;
  int y = 0;
  cef_mouse_button_type_t button = MBT_LEFT;
  int click_count = 0;
};
constexpr auto kMouseDown = TestMouseEvent::Down;
constexpr auto kMouseUp = TestMouseEvent::Up;
constexpr auto kMouseMove = TestMouseEvent::Move;
#endif

constexpr char kUrl[] = "https://tests/mouse-gesture-input";
enum class Mode { Portable, LegacySend };
enum class Scenario { Buttons, Movement, Tap, Scroll, LongPress, Pinch };
enum class FrameMode { Main, SameOrigin, CrossOrigin };
constexpr int kFrameX = 80;
// Keep the iframe origin on device-pixel boundaries at fractional scales.
constexpr int kFrameY = 80;

constexpr cef_mouse_button_type_t kButtons[] = {
    MBT_LEFT,
    MBT_MIDDLE,
    MBT_RIGHT,
#if CEF_API_ADDED(15800)
    CEF_MOUSE_BUTTON_BACK,
    CEF_MOUSE_BUTTON_FORWARD,
#endif
};
constexpr uint32_t kButtonFlags[] = {
    EVENTFLAG_LEFT_MOUSE_BUTTON,     EVENTFLAG_MIDDLE_MOUSE_BUTTON,
    EVENTFLAG_RIGHT_MOUSE_BUTTON,
#if CEF_API_ADDED(15800)
    CEF_EVENTFLAG_BACK_MOUSE_BUTTON, CEF_EVENTFLAG_FORWARD_MOUSE_BUTTON,
#endif
};

class MouseGestureInputTestHandler : public RoutingTestHandler,
#if CEF_API_ADDED(15800)
                                     public CefInputHandler,
#endif
                                     public CefRenderHandler {
 public:
  MouseGestureInputTestHandler(Mode mode,
                               Scenario scenario,
                               bool osr,
                               bool consume,
                               float scale,
                               FrameMode frame_mode)
      : mode_(mode),
        scenario_(scenario),
        osr_(osr),
        consume_(consume),
        scale_(scale),
        frame_mode_(frame_mode) {}

  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
#if CEF_API_ADDED(15800)
  CefRefPtr<CefInputHandler> GetInputHandler() override { return this; }
#endif

  void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override {
    rect = CefRect(0, 0, 400, 300);
  }

  bool GetScreenInfo(CefRefPtr<CefBrowser>, CefScreenInfo& info) override {
    info.device_scale_factor = scale_;
    return true;
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType,
               const RectList&,
               const void*,
               int,
               int) override {
    if (waiting_for_paint_) {
      waiting_for_paint_ = false;
      WaitForInput(browser);
    }
  }

  void RunTest() override {
    SetTestTimeout(10000);
    std::string html = R"(
      <style>body{margin:0;height:3000px}
      #finish{position:fixed;left:180px;top:20px;width:80px;height:50px}
      #setup{position:fixed;left:180px;top:100px;width:80px;height:50px}</style>
      <button id="finish">Report</button><button id="setup">Ready</button><script>
      const scenario = SCENARIO;
      const inFrame = IN_FRAME;
      const lastButton = LAST_BUTTON;
      const events = [];
      let completed = false;
      function inputComplete() {
        if (!completed) {
          completed = true;
          window.testQuery({request:'input-complete'});
        }
      }
      for (const type of ['mousedown','mouseup','mousemove','click','auxclick','contextmenu']) {
        document.addEventListener(type, e => {
          e.preventDefault();
          if (e.clientX !== 40 || e.clientY !== 60) return;
          events.push({type:e.type, button:e.button, buttons:e.buttons,
                       x:e.clientX, y:e.clientY, shift:e.shiftKey, detail:e.detail});
          if ((scenario === 0 && e.type === 'mouseup' && e.button === lastButton) ||
              (scenario === 1 && e.type === 'mousemove' && e.shiftKey) ||
              (scenario === 2 && e.type === 'click')) inputComplete();
        });
      }
      window.addEventListener('scroll', () => {
        if (scenario === 3 && scrollY > 0) inputComplete();
      });
      finish.addEventListener('click', () => {
        window.testQuery({request:JSON.stringify({events, scroll:scrollY})});
      });
      setup.addEventListener('click', () => {
        window.testQuery({request:'input-ready'});
      });
      let loaded = false;
      let parentReady = !inFrame;
      let readySent = false;
      function notifyReady() {
        if (loaded && parentReady && !readySent) {
          readySent = true;
          requestAnimationFrame(() => requestAnimationFrame(() =>
            window.testQuery({request:'ready'})));
        }
      }
      window.addEventListener('message', e => {
        if (e.source === parent && e.data === 'parent-ready') {
          parentReady = true;
          notifyReady();
        }
      });
      window.addEventListener('load', () => { loaded = true; notifyReady(); });
      </script>)";
    html.replace(html.find("SCENARIO"), 8,
                 std::to_string(static_cast<int>(scenario_)));
    html.replace(html.find("LAST_BUTTON"), 11,
                 std::to_string(std::size(kButtons) - 1));
    html.replace(html.find("IN_FRAME"), 8, IsIframe() ? "true" : "false");
    if (IsIframe()) {
      target_url_ =
          frame_mode_ == FrameMode::SameOrigin
              ? "https://tests/mouse-gesture-input-child"
              : "https://tests-frame-other.com/mouse-gesture-input-child";
      AddResource(target_url_, html, "text/html");
      AddResource(
          kUrl,
          "<style>body{margin:0;overflow:hidden}iframe{position:absolute;"
          "left:80px;top:80px;width:300px;height:200px;border:0}</style>"
          "<iframe id='child' name='input-child' src='" +
              target_url_ +
              "'></iframe><script>"
              "window.addEventListener('load',()=>child.contentWindow."
              "postMessage('parent-ready','*'));"
              "for(const type of ['mousedown','mouseup','click']) {"
              "document.addEventListener(type,e=> {"
              "if(e.clientX===120 && e.clientY===140) "
              "window.testQuery({request:'parent-event'});"
              "});}</script>",
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
    waiting_for_input_ = false;
    waiting_for_paint_ = false;
    RoutingTestHandler::DestroyTest();
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t,
               const CefString& request,
               bool,
               CefRefPtr<Callback> callback) override {
    callback->Success("");
    if (request == "parent-event") {
      ADD_FAILURE() << "Input reached the parent instead of the target iframe";
      DestroyTest();
      return true;
    }
    EXPECT_EQ(!IsIframe(), frame->IsMain());
    EXPECT_EQ(target_url_, frame->GetURL().ToString());
    if (IsIframe()) {
      auto parent = frame->GetParent();
      EXPECT_TRUE(parent && parent->IsMain());
    }
    if (request == "ready") {
      target_frame_id_ = frame->GetIdentifier().ToString();
      browser->GetHost()->SetFocus(true);
      if (osr_) {
        waiting_for_paint_ = true;
        browser->GetHost()->Invalidate(PET_VIEW);
      } else {
        WaitForInput(browser);
      }
      return true;
    }
    EXPECT_EQ(target_frame_id_, frame->GetIdentifier().ToString());
    if (request == "input-ready") {
      if (!started_) {
        waiting_for_input_ = false;
        Start(browser);
      }
      return true;
    }
    if (request == "input-complete") {
      EXPECT_FALSE(consume_);
      renderer_done_ = true;
      MaybeReport(browser);
      return true;
    }
    auto value = CefParseJSON(request, JSON_PARSER_RFC);
    EXPECT_TRUE(value && value->GetType() == VTYPE_DICTIONARY);
    if (value && value->GetType() == VTYPE_DICTIONARY) {
      auto report = value->GetDictionary();
      auto events = report->GetList("events");
      EXPECT_TRUE(events);
      if (events) {
        int downs = 0, ups = 0, clicks = 0, moves = 0;
        for (size_t i = 0; i < events->GetSize(); ++i) {
          auto event = events->GetDictionary(i);
          auto type = event->GetString("type");
          if (scenario_ == Scenario::Buttons &&
              (type == "mousedown" || type == "mouseup")) {
            const bool down = type == "mousedown";
            const size_t index = down ? downs++ : ups++;
            EXPECT_LT(index, std::size(kButtons));
            if (index < std::size(kButtons)) {
              EXPECT_EQ(static_cast<int>(kButtons[index]),
                        event->GetInt("button"));
              constexpr int dom_buttons[] = {1, 4, 2, 8, 16};
              EXPECT_EQ(down ? dom_buttons[index] : 0,
                        event->GetInt("buttons"));
            }
            EXPECT_EQ(1, event->GetInt("detail"));
            EXPECT_TRUE(event->GetBool("shift"));
          }
          if (type == "click") {
            ++clicks;
          }
          if (type == "mousemove" && event->GetBool("shift")) {
            ++moves;
            EXPECT_EQ(0, event->GetInt("buttons"));
          }
          EXPECT_EQ(40, event->GetInt("x"));
          EXPECT_EQ(60, event->GetInt("y"));
        }
        switch (scenario_) {
          case Scenario::Buttons:
            EXPECT_EQ(consume_ ? 0 : std::size(kButtons),
                      static_cast<size_t>(downs));
            EXPECT_EQ(consume_ ? 0 : std::size(kButtons),
                      static_cast<size_t>(ups));
            EXPECT_EQ(consume_ ? 0 : 1, clicks);
            break;
          case Scenario::Movement:
            EXPECT_EQ(consume_ ? 0 : 1, moves);
            break;
          case Scenario::Tap:
            EXPECT_EQ(consume_ ? 0 : 1, clicks);
            break;
          case Scenario::Scroll: {
            // Fractional device scales can produce a non-integral scrollY.
            // GetInt returns zero for JSON doubles, even when scrolling worked.
            const double scroll = report->GetType("scroll") == VTYPE_INT
                                      ? report->GetInt("scroll")
                                      : report->GetDouble("scroll");
            EXPECT_EQ(0, clicks);
            if (consume_) {
              EXPECT_EQ(0, scroll);
            } else {
              EXPECT_GT(scroll, 0);
            }
            break;
          }
          case Scenario::LongPress:
          case Scenario::Pinch:
            EXPECT_EQ(0, clicks);
            break;
        }
      }
    }
#if CEF_API_ADDED(15800)
    EXPECT_EQ(mouse_.size(), mouse_callbacks_);
    if (mode_ == Mode::Portable) {
      EXPECT_EQ(gesture_.size(), gesture_callbacks_);
    } else if (!IsMouse()) {
      EXPECT_TRUE(legacy_gesture_finished_);
      EXPECT_GT(gesture_callbacks_, 0U);
    }
#else
    EXPECT_EQ(0U, mouse_callbacks_);
    EXPECT_EQ(0U, gesture_callbacks_);
#endif
    EXPECT_EQ(!consume_, renderer_done_);
    DestroyTest();
    return true;
  }

#if CEF_API_ADDED(15800)
  bool OnPreMouseEvent(CefRefPtr<CefBrowser> browser,
                       const CefMouseInputEvent& event) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    // Cross-widget transforms can introduce floating-point rounding even with
    // a device-pixel-aligned iframe origin.
    const auto at_position = [&event](int x, int y) {
      return std::abs(event.x - x) <= kCoordinateTolerance &&
             std::abs(event.y - y) <= kCoordinateTolerance;
    };
    // Select injected events independently of native hover and the report
    // click.
    if (!started_ || !IsMouse() ||
        (!at_position(CallbackX(), CallbackY()) &&
         !at_position(40 + OffsetX(), 60 + OffsetY())) ||
        (mode_ == Mode::Portable && event.pointer_id != 17) ||
        (scenario_ == Scenario::Movement && event.type != kMouseMove) ||
        ((mode_ == Mode::LegacySend || IsIframe()) &&
         scenario_ == Scenario::Buttons && event.type != kMouseDown &&
         event.type != kMouseUp)) {
      return false;
    }
    // Crossing widget boundaries can synthesize a move in the ancestor widget.
    // Count only the move delivered in the target child widget's coordinates.
    if (frame_mode_ == FrameMode::CrossOrigin &&
        scenario_ == Scenario::Movement &&
        at_position(40 + kFrameX, 60 + kFrameY)) {
      return false;
    }
    EXPECT_LT(mouse_callbacks_, mouse_.size());
    EXPECT_NEAR(CallbackX(), event.x, kCoordinateTolerance);
    EXPECT_NEAR(CallbackY(), event.y, kCoordinateTolerance);
    if (mouse_callbacks_ < mouse_.size()) {
      const auto& expected = mouse_[mouse_callbacks_++];
      EXPECT_EQ(expected.type, event.type);
      EXPECT_EQ(expected.button, event.button);
      EXPECT_EQ(expected.modifiers, event.modifiers);
      EXPECT_EQ(expected.click_count, event.click_count);
      if (mode_ == Mode::Portable) {
        EXPECT_EQ(expected.timestamp, event.timestamp);
        EXPECT_EQ(expected.screen_x + OffsetX(), event.screen_x);
        EXPECT_EQ(expected.screen_y + OffsetY(), event.screen_y);
        EXPECT_EQ(expected.pointer_type, event.pointer_type);
        EXPECT_EQ(expected.pointer_id, event.pointer_id);
        EXPECT_EQ(expected.pressure, event.pressure);
        EXPECT_EQ(expected.tilt_x, event.tilt_x);
        EXPECT_EQ(expected.tilt_y, event.tilt_y);
        EXPECT_EQ(expected.movement_x, event.movement_x);
        EXPECT_EQ(expected.movement_y, event.movement_y);
      }
    }
    MaybeReport(browser);
    return consume_;
  }

  bool OnPreGestureEvent(CefRefPtr<CefBrowser> browser,
                         const CefGestureInputEvent& event) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    if (!started_ || IsMouse()) {
      return false;
    }
    ++gesture_callbacks_;
    if (mode_ == Mode::Portable) {
      EXPECT_LE(gesture_callbacks_, gesture_.size());
      if (gesture_callbacks_ <= gesture_.size()) {
        const auto& expected = gesture_[gesture_callbacks_ - 1];
        EXPECT_EQ(expected.type, event.type);
        EXPECT_EQ(expected.modifiers, event.modifiers);
        EXPECT_EQ(expected.timestamp, event.timestamp);
        EXPECT_EQ(CallbackX(), event.x);
        EXPECT_EQ(CallbackY(), event.y);
        EXPECT_EQ(expected.screen_x + OffsetX(), event.screen_x);
        EXPECT_EQ(expected.screen_y + OffsetY(), event.screen_y);
        EXPECT_EQ(expected.source_device, event.source_device);
        EXPECT_EQ(expected.primary_pointer_type, event.primary_pointer_type);
        EXPECT_EQ(expected.tap_count, event.tap_count);
        EXPECT_EQ(expected.width, event.width);
        EXPECT_EQ(expected.height, event.height);
        EXPECT_EQ(expected.delta_y, event.delta_y);
        EXPECT_EQ(expected.pointer_count, event.pointer_count);
      }
    } else {
      // Touch injection goes through Chromium's gesture recognizer. Assert the
      // semantic result; incidental begin/show-press events are
      // platform-specific.
      EXPECT_EQ(CEF_GESTURE_DEVICE_TOUCHSCREEN, event.source_device);
      if (scenario_ == Scenario::Pinch) {
        if (event.type == CEF_GESTURE_EVENT_PINCH_BEGIN) {
          legacy_pinch_begin_ = true;
        } else if (event.type == CEF_GESTURE_EVENT_PINCH_UPDATE) {
          EXPECT_TRUE(legacy_pinch_begin_);
          EXPECT_GT(event.scale, 1);
          legacy_pinch_update_ = true;
        } else if (event.type == CEF_GESTURE_EVENT_PINCH_END) {
          EXPECT_TRUE(legacy_pinch_update_);
          legacy_gesture_finished_ = true;
        }
        MaybeReport(browser);
        return consume_;
      }
      if (event.type == CEF_GESTURE_EVENT_LONG_PRESS &&
          scenario_ == Scenario::LongPress) {
        CefPostTask(TID_UI, base::BindOnce(
                                &MouseGestureInputTestHandler::FinishLongPress,
                                this, browser));
        return consume_;
      }
      if (event.type == CEF_GESTURE_EVENT_TAP) {
        EXPECT_EQ(Scenario::Tap, scenario_);
        EXPECT_EQ(1, event.tap_count);
        legacy_gesture_finished_ = true;
      }
      if (event.type == CEF_GESTURE_EVENT_SCROLL_BEGIN) {
        EXPECT_EQ(Scenario::Scroll, scenario_);
        EXPECT_EQ(1, event.pointer_count);
        legacy_scroll_begin_ = true;
      }
      if (event.type == CEF_GESTURE_EVENT_SCROLL_UPDATE) {
        EXPECT_TRUE(legacy_scroll_begin_);
        EXPECT_LT(event.delta_y, 0);
        legacy_scroll_update_ = true;
      }
      if (event.type == CEF_GESTURE_EVENT_SCROLL_END) {
        EXPECT_TRUE(legacy_scroll_begin_);
        EXPECT_TRUE(legacy_scroll_update_);
        legacy_gesture_finished_ = true;
      }
    }
    MaybeReport(browser);
    return consume_;
  }
#endif

 private:
  bool IsIframe() const { return frame_mode_ != FrameMode::Main; }
  int OffsetX() const { return IsIframe() ? kFrameX : 0; }
  int OffsetY() const { return IsIframe() ? kFrameY : 0; }

  // Same-origin frames share the root widget. Cross-site OOPIFs receive
  // widget-local coordinates, proving that the input crossed widget routing.
  int CallbackX() const {
    return 40 + (frame_mode_ == FrameMode::SameOrigin ? kFrameX : 0);
  }

  int CallbackY() const {
    return 60 + (frame_mode_ == FrameMode::SameOrigin ? kFrameY : 0);
  }

  bool IsMouse() const {
    return scenario_ == Scenario::Buttons || scenario_ == Scenario::Movement;
  }

  void OnTestTimeout(int timeout_ms, bool treat_as_error) override {
    waiting_for_input_ = false;
    ADD_FAILURE() << "started=" << started_
                  << " waiting_for_paint=" << waiting_for_paint_
                  << " mouse_callbacks=" << mouse_callbacks_ << "/"
                  << mouse_.size()
                  << " gesture_callbacks=" << gesture_callbacks_
                  << " renderer_done=" << renderer_done_
                  << " report_posted=" << report_posted_;
    TestHandler::OnTestTimeout(timeout_ms, treat_as_error);
  }

  TestMouseEvent MouseEvent() const {
    TestMouseEvent event;
    event.x = 40;
    event.y = 60;
    event.modifiers = EVENTFLAG_SHIFT_DOWN;
#if CEF_API_ADDED(15800)
    event.timestamp = 123456789;
    event.screen_x = 140;
    event.screen_y = 260;
    event.pointer_type = CEF_POINTER_TYPE_MOUSE;
    event.pointer_id = 17;
    event.pressure = 0.5f;
    event.tilt_x = -15;
    event.tilt_y = 25;
    event.movement_x = 3;
    event.movement_y = -4;
#endif
    return event;
  }

  void WaitForInput(CefRefPtr<CefBrowser> browser) {
    waiting_for_input_ = true;
    FocusInput(browser);
  }

  void FocusInput(CefRefPtr<CefBrowser> browser) {
    if (!waiting_for_input_) {
      return;
    }
    // As in the keyboard tests, a DOM click confirms that navigation paint
    // holding has ended and renderer input is active.
    CefMouseEvent event;
    event.x = 200 + OffsetX();
    event.y = 120 + OffsetY();
    // Establish the hover target before counting the injected events. Crossing
    // widget boundaries can synthesize additional moves in ancestor widgets.
    browser->GetHost()->SendMouseMoveEvent(event, false);
    browser->GetHost()->SendMouseClickEvent(event, MBT_LEFT, false, 1);
    browser->GetHost()->SendMouseClickEvent(event, MBT_LEFT, true, 1);
    CefPostDelayedTask(TID_UI,
                       base::BindOnce(&MouseGestureInputTestHandler::FocusInput,
                                      this, browser),
                       50);
  }

  void Start(CefRefPtr<CefBrowser> browser) {
    EXPECT_FALSE(started_);
    started_ = true;
    if (IsMouse()) {
      if (scenario_ == Scenario::Buttons) {
        for (size_t i = 0; i < std::size(kButtons); ++i) {
          for (bool up : {false, true}) {
            auto event = MouseEvent();
            event.type = up ? kMouseUp : kMouseDown;
            event.button = kButtons[i];
            event.modifiers |= up ? 0 : kButtonFlags[i];
            event.click_count = 1;
            mouse_.push_back(event);
          }
        }
#if CEF_API_ADDED(15800)
        if (mode_ == Mode::Portable && consume_ && !IsIframe()) {
          for (auto type :
               {CEF_MOUSE_EVENT_MOVE, CEF_MOUSE_EVENT_ENTER,
                CEF_MOUSE_EVENT_LEAVE, CEF_MOUSE_EVENT_CONTEXT_MENU}) {
            auto event = MouseEvent();
            event.type = type;
            event.button = CEF_MOUSE_BUTTON_NONE;
            mouse_.push_back(event);
          }
        }
#endif
      } else {
        auto event = MouseEvent();
        event.type = kMouseMove;
#if CEF_API_ADDED(15800)
        event.button = CEF_MOUSE_BUTTON_NONE;
#endif
        mouse_.push_back(event);
      }
#if CEF_API_ADDED(15800)
      if (mode_ == Mode::Portable) {
        CefPostTask(TID_IO, base::BindOnce(
                                &MouseGestureInputTestHandler::SendMouseInput,
                                this, browser));
      } else
#endif
      {
        for (const auto& event : mouse_) {
          SendMouse(browser, event);
        }
      }
    } else if (mode_ == Mode::LegacySend) {
      // The legacy gesture-producing API is SendTouchEvent. Cancellation ends
      // the scroll without a velocity-dependent fling on release.
      CefTouchEvent touch;
      touch.id = 1;
      touch.x = 40 + OffsetX();
      touch.y = 60 + OffsetY();
      touch.radius_x = 6;
      touch.radius_y = 7;
      touch.pressure = 0.5f;
      touch.pointer_type = CEF_POINTER_TYPE_TOUCH;
      touch.type = CEF_TET_PRESSED;
      browser->GetHost()->SendTouchEvent(touch);
      if (scenario_ == Scenario::Pinch) {
        auto second = touch;
        second.id = 2;
        second.x = 240;
        browser->GetHost()->SendTouchEvent(second);
        touch.type = second.type = CEF_TET_MOVED;
        touch.x = 20;
        second.x = 260;
        browser->GetHost()->SendTouchEvent(touch);
        browser->GetHost()->SendTouchEvent(second);
        // Cancellation must terminate a multi-contact sequence as well as
        // the generated pinch and its enclosing touchscreen scroll.
        touch.type = second.type = CEF_TET_CANCELLED;
        browser->GetHost()->SendTouchEvent(touch);
        browser->GetHost()->SendTouchEvent(second);
        return;
      }
      if (scenario_ == Scenario::LongPress) {
        // Leave the contact down until Chromium recognizes the long press.
        return;
      }
      if (scenario_ == Scenario::Scroll) {
        touch.y = 20 + OffsetY();
        touch.type = CEF_TET_MOVED;
        browser->GetHost()->SendTouchEvent(touch);
        touch.type = CEF_TET_CANCELLED;
      } else {
        touch.type = CEF_TET_RELEASED;
      }
      browser->GetHost()->SendTouchEvent(touch);
#if CEF_API_ADDED(15800)
    } else {
      const std::vector<cef_gesture_input_event_type_t> types =
          scenario_ == Scenario::Tap
              ? std::vector<
                    cef_gesture_input_event_type_t>{CEF_GESTURE_EVENT_TAP_DOWN,
                                                    CEF_GESTURE_EVENT_TAP}
              : std::vector<cef_gesture_input_event_type_t>{
                    CEF_GESTURE_EVENT_SCROLL_BEGIN,
                    CEF_GESTURE_EVENT_SCROLL_UPDATE,
                    CEF_GESTURE_EVENT_SCROLL_END};
      for (auto type : types) {
        CefGestureInputEvent event;
        event.type = type;
        event.timestamp = 123456789;
        event.source_device = CEF_GESTURE_DEVICE_TOUCHSCREEN;
        event.primary_pointer_type = CEF_POINTER_TYPE_TOUCH;
        event.x = 40;
        event.y = 60;
        event.screen_x = 140;
        event.screen_y = 260;
        if (scenario_ == Scenario::Tap) {
          event.tap_count = 1;
          event.width = 12;
          event.height = 14;
        } else if (type == CEF_GESTURE_EVENT_SCROLL_BEGIN) {
          event.pointer_count = 1;
          event.delta_y = -80;
        } else if (type == CEF_GESTURE_EVENT_SCROLL_UPDATE) {
          event.delta_y = -80;
          event.delta_y_unconstrained = -80;
        }
        gesture_.push_back(event);
      }
      CefPostTask(TID_IO, base::BindOnce(
                              &MouseGestureInputTestHandler::SendGestureInput,
                              this, browser));
#endif
    }
  }

#if CEF_API_ADDED(15800)
  void SendMouseInput(CefRefPtr<CefBrowser> browser) {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    if (scenario_ == Scenario::Buttons && !consume_ && !IsIframe() &&
        scale_ == 1) {
      auto invalid = MouseEvent();
      invalid.type = static_cast<cef_mouse_input_event_type_t>(999);
      SendMouse(browser, invalid);
      invalid = MouseEvent();
      invalid.pressure = 2;
      SendMouse(browser, invalid);
    }
    for (const auto& event : mouse_) {
      SendMouse(browser, event);
    }
  }

  void SendGestureInput(CefRefPtr<CefBrowser> browser) {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    if (scenario_ == Scenario::Tap && !consume_ && !IsIframe() && scale_ == 1) {
      CefGestureInputEvent invalid(gesture_.front());
      invalid.source_device = CEF_GESTURE_DEVICE_UNINITIALIZED;
      browser->GetHost()->SendGestureEvent(invalid);
      invalid = gesture_.front();
      invalid.type = static_cast<cef_gesture_input_event_type_t>(999);
      browser->GetHost()->SendGestureEvent(invalid);
    }
    for (const auto& event : gesture_) {
      CefGestureInputEvent root_event(event);
      root_event.x += OffsetX();
      root_event.y += OffsetY();
      root_event.screen_x += OffsetX();
      root_event.screen_y += OffsetY();
      browser->GetHost()->SendGestureEvent(root_event);
      // The asynchronous send must own its snapshot before returning.
      root_event.x = -1000;
      root_event.delta_y = 999;
    }
  }
#endif

#if CEF_API_ADDED(15800)
  void FinishLongPress(CefRefPtr<CefBrowser> browser) {
    CefTouchEvent touch;
    touch.id = 1;
    touch.x = 40;
    touch.y = 60;
    touch.pointer_type = CEF_POINTER_TYPE_TOUCH;
    touch.type = CEF_TET_RELEASED;
    browser->GetHost()->SendTouchEvent(touch);
    legacy_gesture_finished_ = true;
    MaybeReport(browser);
  }
#endif

  void SendMouse(CefRefPtr<CefBrowser> browser, const TestMouseEvent& event) {
#if CEF_API_ADDED(15800)
    if (mode_ == Mode::Portable) {
      CefMouseInputEvent root_event(event);
      root_event.x += OffsetX();
      root_event.y += OffsetY();
      root_event.screen_x += OffsetX();
      root_event.screen_y += OffsetY();
      browser->GetHost()->SendMouseEvent(root_event);
      root_event.x = -1000;
      root_event.click_count = 999;
      return;
    }
#endif
    CefMouseEvent legacy;
    legacy.x = event.x + OffsetX();
    legacy.y = event.y + OffsetY();
    legacy.modifiers = event.modifiers;
    if (event.type == kMouseMove) {
      browser->GetHost()->SendMouseMoveEvent(legacy, false);
    } else {
      browser->GetHost()->SendMouseClickEvent(
          legacy, event.button, event.type == kMouseUp, event.click_count);
    }
  }

  void MaybeReport(CefRefPtr<CefBrowser> browser) {
    if (report_posted_ || (!consume_ && !renderer_done_)) {
      return;
    }
#if CEF_API_ADDED(15800)
    if (IsMouse()) {
      if (mouse_callbacks_ != mouse_.size()) {
        return;
      }
    } else if (mode_ == Mode::Portable) {
      if (gesture_callbacks_ != gesture_.size()) {
        return;
      }
    } else if (!legacy_gesture_finished_) {
      return;
    }
#endif
    report_posted_ = true;
    // A real page click after the final callback/DOM acknowledgment provides
    // the renderer barrier for both forwarding and consumption cases.
    CefPostTask(TID_UI, base::BindOnce(&MouseGestureInputTestHandler::Report,
                                       this, browser));
  }

  void Report(CefRefPtr<CefBrowser> browser) {
    auto event = MouseEvent();
    event.x = 200;
    event.y = 40;
    event.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
    event.button = MBT_LEFT;
    event.click_count = 1;
    event.type = kMouseDown;
    SendMouse(browser, event);
    event.type = kMouseUp;
    event.modifiers = 0;
    SendMouse(browser, event);
  }

  const Mode mode_;
  const Scenario scenario_;
  const bool osr_;
  const bool consume_;
  const float scale_;
  const FrameMode frame_mode_;
  std::string target_url_;
  std::string target_frame_id_;
  bool waiting_for_paint_ = false;
  bool waiting_for_input_ = false;
  bool started_ = false;
  bool report_posted_ = false;
  bool renderer_done_ = false;
  std::vector<TestMouseEvent> mouse_;
#if CEF_API_ADDED(15800)
  std::vector<CefGestureInputEvent> gesture_;
  bool legacy_gesture_finished_ = false;
  bool legacy_scroll_begin_ = false;
  bool legacy_scroll_update_ = false;
  bool legacy_pinch_begin_ = false;
  bool legacy_pinch_update_ = false;
#endif
  size_t mouse_callbacks_ = 0;
  size_t gesture_callbacks_ = 0;
  IMPLEMENT_REFCOUNTING(MouseGestureInputTestHandler);
};

struct MouseGestureTestCase {
  Mode mode;
  Scenario scenario;
  bool consume = false;
  bool osr = true;
  float scale = 1.0f;
  FrameMode frame_mode = FrameMode::Main;
};

class MouseGestureInputTestCase : public testing::Test {
 public:
  explicit MouseGestureInputTestCase(MouseGestureTestCase test_case)
      : test_case_(test_case) {}

 private:
  void TestBody() override {
    CefRefPtr<MouseGestureInputTestHandler> handler =
        new MouseGestureInputTestHandler(
            test_case_.mode, test_case_.scenario, test_case_.osr,
            test_case_.consume, test_case_.scale, test_case_.frame_mode);
    handler->ExecuteTest();
    ReleaseAndWaitForDestructor(handler);
  }

  const MouseGestureTestCase test_case_;
};

bool RegisterInputTests(const char* name,
                        MouseGestureTestCase test_case,
                        int line) {
  for (auto frame_mode :
       {FrameMode::Main, FrameMode::SameOrigin, FrameMode::CrossOrigin}) {
    for (bool osr : {true, false}) {
      // Keep one native case per additional gesture. OSRTest.QuickMenu
      // separately covers OSR long-press/touch-selection behavior.
      if ((test_case.scenario == Scenario::LongPress ||
           test_case.scenario == Scenario::Pinch) &&
          (osr || frame_mode != FrameMode::Main)) {
        continue;
      }
      for (float scale : {1.0f, 2.0f}) {
        // Test behavior at 1x in the main frame. Add only representative cases
        // for scaling and same-widget iframe targeting; OOPIFs additionally
        // exercise Chromium's cross-widget routing and gesture latching.
        const bool portable = test_case.mode == Mode::Portable;
        if (scale == 2.0f && !(osr && portable && !test_case.consume &&
                               frame_mode == FrameMode::Main &&
                               (test_case.scenario == Scenario::Buttons ||
                                test_case.scenario == Scenario::Scroll))) {
          continue;
        }
        if (frame_mode == FrameMode::SameOrigin &&
            !(osr && portable && !test_case.consume &&
              (test_case.scenario == Scenario::Movement ||
               test_case.scenario == Scenario::Tap))) {
          continue;
        }
        if (frame_mode == FrameMode::CrossOrigin &&
            ((test_case.scenario == Scenario::Movement &&
              !(portable && !test_case.consume)) ||
             (test_case.scenario == Scenario::Scroll && test_case.consume &&
              !(portable && osr)))) {
          continue;
        }
        std::string case_name = name;
        if (frame_mode == FrameMode::SameOrigin) {
          case_name += "SameOriginIframe";
        }
        if (frame_mode == FrameMode::CrossOrigin) {
          case_name += "CrossOriginIframe";
        }
        case_name += osr ? "Osr" : "Windowed";
        case_name += scale == 1.0f ? "1x" : "2x";
        test_case.osr = osr;
        test_case.scale = scale;
        test_case.frame_mode = frame_mode;
        testing::RegisterTest("MouseGestureInputTest", case_name.c_str(),
                              nullptr, nullptr, __FILE__, line,
                              [test_case]() -> testing::Test* {
                                return new MouseGestureInputTestCase(test_case);
                              });
      }
    }
  }
  return true;
}

#define INPUT_TEST(name, mode, scenario, consume) \
  [[maybe_unused]] const bool kRegister##name =   \
      RegisterInputTests(#name, {mode, scenario, consume}, __LINE__);

INPUT_TEST(LegacyMouseButtonsReachDom,
           Mode::LegacySend,
           Scenario::Buttons,
           false)
INPUT_TEST(LegacyMouseMovementReachesDom,
           Mode::LegacySend,
           Scenario::Movement,
           false)
INPUT_TEST(LegacyTouchProducesTap, Mode::LegacySend, Scenario::Tap, false)
INPUT_TEST(LegacyTouchProducesScroll, Mode::LegacySend, Scenario::Scroll, false)

#if CEF_API_ADDED(15800)
INPUT_TEST(MouseButtonsReachDom, Mode::Portable, Scenario::Buttons, false)
INPUT_TEST(MouseMovementReachesDom, Mode::Portable, Scenario::Movement, false)
INPUT_TEST(GestureTapReachesDom, Mode::Portable, Scenario::Tap, false)
INPUT_TEST(GestureScrollReachesDom, Mode::Portable, Scenario::Scroll, false)
INPUT_TEST(MouseEventsConsumed, Mode::Portable, Scenario::Buttons, true)
INPUT_TEST(MouseMovementConsumed, Mode::Portable, Scenario::Movement, true)
INPUT_TEST(GestureTapConsumed, Mode::Portable, Scenario::Tap, true)
INPUT_TEST(GestureScrollConsumed, Mode::Portable, Scenario::Scroll, true)
INPUT_TEST(LegacyMouseButtonsConsumed,
           Mode::LegacySend,
           Scenario::Buttons,
           true)
INPUT_TEST(LegacyMouseMovementConsumed,
           Mode::LegacySend,
           Scenario::Movement,
           true)
INPUT_TEST(LegacyTouchTapConsumed, Mode::LegacySend, Scenario::Tap, true)
INPUT_TEST(LegacyTouchScrollConsumed, Mode::LegacySend, Scenario::Scroll, true)
INPUT_TEST(LegacyTouchLongPressConsumed,
           Mode::LegacySend,
           Scenario::LongPress,
           true)
INPUT_TEST(LegacyTouchPinchConsumed, Mode::LegacySend, Scenario::Pinch, true)
#endif
#undef INPUT_TEST

#if CEF_API_ADDED(15800)
TEST(OsrInputUtilTest, MouseButtonTransitions) {
  CefMouseEvent mouse;
  mouse.x = 12;
  mouse.y = 34;
  const CefPoint screen(112, 234);
  for (auto button :
       {CEF_MOUSE_BUTTON_LEFT, CEF_MOUSE_BUTTON_MIDDLE, CEF_MOUSE_BUTTON_RIGHT,
        CEF_MOUSE_BUTTON_BACK, CEF_MOUSE_BUTTON_FORWARD}) {
    const uint32_t flag = client::OsrMouseButtonFlag(button);
    // GTK can report pre-transition state; other frameworks report the
    // post-transition state. Both must produce the same portable snapshot.
    for (bool already_changed : {false, true}) {
      mouse.modifiers = CEF_EVENTFLAG_SHIFT_DOWN | (already_changed ? flag : 0);
      auto down = client::MakeOsrMouseInputEvent(
          mouse, screen, CEF_MOUSE_EVENT_DOWN, button, 2);
      EXPECT_EQ(static_cast<uint32_t>(CEF_EVENTFLAG_SHIFT_DOWN) | flag,
                down.modifiers);
      EXPECT_EQ(button, down.button);
      EXPECT_EQ(2, down.click_count);
      EXPECT_EQ(CEF_POINTER_TYPE_MOUSE, down.pointer_type);
      EXPECT_EQ(12, down.x);
      EXPECT_EQ(34, down.y);
      EXPECT_EQ(112, down.screen_x);
      EXPECT_EQ(234, down.screen_y);

      mouse.modifiers = CEF_EVENTFLAG_SHIFT_DOWN | (already_changed ? 0 : flag);
      auto up = client::MakeOsrMouseInputEvent(mouse, screen,
                                               CEF_MOUSE_EVENT_UP, button, 2);
      EXPECT_EQ(static_cast<uint32_t>(CEF_EVENTFLAG_SHIFT_DOWN), up.modifiers);
      EXPECT_EQ(button, up.button);
    }
  }
  // Releasing one chord button must preserve the others, and a subsequent
  // drag move must identify a still-held button.
  mouse.modifiers =
      CEF_EVENTFLAG_LEFT_MOUSE_BUTTON | CEF_EVENTFLAG_RIGHT_MOUSE_BUTTON;
  const auto released = client::MakeOsrMouseInputEvent(
      mouse, screen, CEF_MOUSE_EVENT_UP, CEF_MOUSE_BUTTON_RIGHT, 1);
  EXPECT_EQ(static_cast<uint32_t>(CEF_EVENTFLAG_LEFT_MOUSE_BUTTON),
            released.modifiers);
  mouse.modifiers = released.modifiers;
  const auto moved =
      client::MakeOsrMouseInputEvent(mouse, screen, CEF_MOUSE_EVENT_MOVE);
  EXPECT_EQ(CEF_MOUSE_BUTTON_LEFT, moved.button);
  EXPECT_EQ(static_cast<uint32_t>(CEF_EVENTFLAG_LEFT_MOUSE_BUTTON),
            moved.modifiers);
}

TEST(OsrInputUtilTest, WindowsZoomSequence) {
  uint64_t distance = 0;
  auto update = [&](uint64_t next, bool begin = false, bool end = false) {
    return client::OsrPinchInputEventsFromWindowsZoom(next, begin, end,
                                                      distance);
  };
  EXPECT_TRUE(update(100).empty());  // Ignore an update without a begin.
  EXPECT_TRUE(update(0, true).empty());
  const auto begin = update(100, true);
  ASSERT_EQ(2U, begin.size());
  EXPECT_EQ(CEF_GESTURE_EVENT_SCROLL_BEGIN, begin[0].type);
  EXPECT_EQ(2, begin[0].pointer_count);
  EXPECT_EQ(CEF_GESTURE_EVENT_PINCH_BEGIN, begin[1].type);
  EXPECT_EQ(CEF_GESTURE_DEVICE_TOUCHSCREEN, begin[1].source_device);
  EXPECT_EQ(CEF_POINTER_TYPE_TOUCH, begin[1].primary_pointer_type);
  EXPECT_FALSE(begin[1].needs_wheel_event);
  EXPECT_TRUE(update(100).empty());
  EXPECT_TRUE(update(0).empty());
  EXPECT_EQ(100U, distance);
  const auto grow = update(150);
  ASSERT_EQ(1U, grow.size());
  EXPECT_EQ(CEF_GESTURE_EVENT_PINCH_UPDATE, grow[0].type);
  EXPECT_FLOAT_EQ(1.5f, grow[0].scale);
  // The final message can contain one last scale change before ending.
  const auto end = update(75, false, true);
  ASSERT_EQ(3U, end.size());
  EXPECT_EQ(CEF_GESTURE_EVENT_PINCH_UPDATE, end[0].type);
  EXPECT_FLOAT_EQ(0.5f, end[0].scale);
  EXPECT_EQ(CEF_GESTURE_EVENT_PINCH_END, end[1].type);
  EXPECT_EQ(CEF_GESTURE_EVENT_SCROLL_END, end[2].type);
  EXPECT_EQ(0U, distance);
  EXPECT_TRUE(update(0, false, true).empty());
  update(100, true);
  const auto restart = update(200, true);
  ASSERT_EQ(4U, restart.size());
  EXPECT_EQ(CEF_GESTURE_EVENT_PINCH_END, restart[0].type);
  EXPECT_EQ(CEF_GESTURE_EVENT_SCROLL_END, restart[1].type);
  EXPECT_EQ(CEF_GESTURE_EVENT_SCROLL_BEGIN, restart[2].type);
  EXPECT_EQ(CEF_GESTURE_EVENT_PINCH_BEGIN, restart[3].type);
  const auto zero_end = update(0, false, true);
  ASSERT_EQ(2U, zero_end.size());
  EXPECT_EQ(CEF_GESTURE_EVENT_PINCH_END, zero_end[0].type);
  EXPECT_EQ(CEF_GESTURE_EVENT_SCROLL_END, zero_end[1].type);
  EXPECT_EQ(0U, distance);
}

// These sequence tests deliberately do not use the frame/scale matrix above.
// Each exercises a distinct renderer or browser input state machine.
enum class InputSequence {
  Mouse,
  Pinch,
  WheelFirst,
  Fling,
  Popup,
  PopupScroll
};

class InputSequenceTestHandler : public RoutingTestHandler,
                                 public CefRenderHandler,
                                 public CefInputHandler {
 public:
  InputSequenceTestHandler(InputSequence sequence, bool osr)
      : sequence_(sequence), osr_(osr) {}

  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
  CefRefPtr<CefInputHandler> GetInputHandler() override { return this; }

  void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override {
    rect = CefRect(0, 0, 400, 300);
  }

  bool GetScreenInfo(CefRefPtr<CefBrowser>, CefScreenInfo& info) override {
    info.device_scale_factor = 1;
    return true;
  }

  void RunTest() override {
    SetTestTimeout(10000);
    std::string html = R"(
      <meta name='viewport' content='width=device-width,initial-scale=1,
                                   minimum-scale=1,maximum-scale=4'>
      <style>body{margin:0;height:3000px}
      #target{position:absolute;left:20px;top:20px;width:100px;height:100px;
              background:blue;touch-action:none}
      #ready{position:fixed;left:200px;top:20px;width:80px;height:40px}
      #select{position:absolute;left:140px;top:160px;width:100px;height:30px}
      </style><div id='target'></div><button id='ready'>Ready</button>
      <select id='select'><option value='0'>Default</option>
      <option value='1'>One</option><option value='2'>Two</option>
      <option value='3'>Three</option><option value='4'>Four</option>
      <option value='5'>Five</option><option value='6'>Six</option>
      <option value='7'>Seven</option><option value='8'>Eight</option>
      <option value='9'>Nine</option></select><script>
      const query = request => window.testQuery({request});
      const sequence = SEQUENCE;
      if(sequence===5) {
        for(let i=10;i<40;++i) select.add(new Option('Option '+i, ''+i));
      }
      let active = false, phase = 'double', preventWheel = true, resetting = false;
      let blockedReported = false, zoomReported = false, scrollReported = false;
      const details = [], chord = [];
      let captured = null, wheels = 0, touches = 0;
      ready.onclick = () => { if(!active) { active=true; query('active'); } };
      document.addEventListener('contextmenu', e => e.preventDefault());
      target.addEventListener('click', e => {
        if(active && phase==='double') details.push(e.detail);
      });
      target.addEventListener('dblclick', e => {
        if(!active || sequence!==0) return;
        phase='chord'; query('double');
      });
      for(const type of ['mousedown','mouseup']) {
        target.addEventListener(type, e => {
          e.preventDefault();
          if(!active || phase!=='chord') return;
          chord.push({type:e.type,button:e.button,buttons:e.buttons});
          if(type==='mouseup' && e.button===0) {
            phase='capture'; query('chord');
          }
        });
      }
      target.addEventListener('pointerdown', e => {
        if(active && phase==='capture') target.setPointerCapture(e.pointerId);
      });
      target.addEventListener('gotpointercapture', () => query('captured'));
      target.addEventListener('pointerup', e => {
        if(active && phase==='capture') captured = {
          x:e.clientX,y:e.clientY,buttons:e.buttons,
          held:target.hasPointerCapture(e.pointerId)};
      });
      target.addEventListener('lostpointercapture', () => {
        if(captured) query(JSON.stringify({details,chord,captured}));
      });
      document.addEventListener('touchstart', () => ++touches, {passive:true});
      document.addEventListener('wheel', e => {
        if(!active || !e.ctrlKey) return;
        ++wheels;
        if(preventWheel) {
          e.preventDefault();
          if(!blockedReported) {
            blockedReported=true;
            requestAnimationFrame(()=>requestAnimationFrame(()=> {
              preventWheel=false; query('blocked:'+visualViewport.scale);
            }));
          }
        }
      }, {passive:false});
      visualViewport.addEventListener('resize', () => {
        if(resetting && Math.abs(visualViewport.scale-1)<0.01) {
          resetting=false; query('reset'); return;
        }
        if(active && !zoomReported && visualViewport.scale>1.1) {
          zoomReported=true;
          query(JSON.stringify({scale:visualViewport.scale,wheels,touches}));
        }
      });
      window.addEventListener('scroll', () => {
        if(active && sequence===3 && !scrollReported && scrollY>0) {
          scrollReported=true; query('scrolled');
        }
      });
      select.onchange = () => query('selected:'+select.value);
      window.addEventListener('load', () => {
        requestAnimationFrame(()=>requestAnimationFrame(()=>
          query('ready:'+visualViewport.scale)));
      });
      </script>)";
    html.replace(html.find("SEQUENCE"), 8,
                 std::to_string(static_cast<int>(sequence_)));
    AddResource(kSequenceUrl, html, "text/html");
    if (osr_) {
      SetUseAlloyStyle(true, true);
      CefWindowInfo info;
#if defined(OS_WIN)
      info.SetAsWindowless(GetDesktopWindow());
#else
      info.SetAsWindowless(kNullWindowHandle);
#endif
      info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
      CreateReusableBrowser(info, kSequenceUrl, CefBrowserSettings());
    } else {
      SetUseViews(true);
      CreateReusableBrowser(kSequenceUrl);
    }
  }

  void DestroyTest() override {
    waiting_ = false;
    RecordPopupEvent("DestroyTest");
    RoutingTestHandler::DestroyTest();
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t,
               const CefString& request,
               bool,
               CefRefPtr<Callback> callback) override {
    callback->Success("");
    EXPECT_TRUE(frame->IsMain());
    const auto text = request.ToString();
    RecordPopupEvent("query=" + text);
    if (text.rfind("ready:", 0) == 0) {
      EXPECT_EQ("ready:1", text);
      waiting_ = true;
      browser->GetHost()->SetFocus(true);
      FocusInput(browser);
    } else if (text == "active") {
      waiting_ = false;
      if (started_) {
        return true;
      }
      started_ = true;
      switch (sequence_) {
        case InputSequence::Mouse:
          for (int count : {1, 2}) {
            Mouse(browser, CEF_MOUSE_EVENT_DOWN, 40, 60, CEF_MOUSE_BUTTON_LEFT,
                  CEF_EVENTFLAG_LEFT_MOUSE_BUTTON, count);
            Mouse(browser, CEF_MOUSE_EVENT_UP, 40, 60, CEF_MOUSE_BUTTON_LEFT, 0,
                  count);
          }
          break;
        case InputSequence::Pinch: {
          // Exercise the native Windows adapter's complete sequence through
          // renderer zoom, without requiring Windows hardware for this test.
          uint64_t distance = 0;
          for (int i = 0; i < 3; ++i) {
            for (auto& event : client::OsrPinchInputEventsFromWindowsZoom(
                     i == 0 ? 100 : 150, i == 0, i == 2, distance)) {
              event.x = event.screen_x = 150;
              event.y = event.screen_y = 100;
              browser->GetHost()->SendGestureEvent(event);
            }
          }
        } break;
        case InputSequence::WheelFirst:
          Pinch(browser, true);
          break;
        case InputSequence::Fling:
          Gesture(browser, CEF_GESTURE_EVENT_SCROLL_BEGIN);
          Gesture(browser, CEF_GESTURE_EVENT_SCROLL_UPDATE);
          break;
        case InputSequence::Popup:
        case InputSequence::PopupScroll:
          Mouse(browser, CEF_MOUSE_EVENT_DOWN, 150, 175, CEF_MOUSE_BUTTON_LEFT,
                CEF_EVENTFLAG_LEFT_MOUSE_BUTTON);
          Mouse(browser, CEF_MOUSE_EVENT_UP, 150, 175, CEF_MOUSE_BUTTON_LEFT,
                0);
          break;
      }
    } else if (text == "double") {
      EXPECT_EQ(InputSequence::Mouse, sequence_);
      Mouse(browser, CEF_MOUSE_EVENT_DOWN, 40, 60, CEF_MOUSE_BUTTON_LEFT,
            CEF_EVENTFLAG_LEFT_MOUSE_BUTTON);
      Mouse(browser, CEF_MOUSE_EVENT_DOWN, 40, 60, CEF_MOUSE_BUTTON_RIGHT,
            CEF_EVENTFLAG_LEFT_MOUSE_BUTTON | CEF_EVENTFLAG_RIGHT_MOUSE_BUTTON);
      Mouse(browser, CEF_MOUSE_EVENT_UP, 40, 60, CEF_MOUSE_BUTTON_RIGHT,
            CEF_EVENTFLAG_LEFT_MOUSE_BUTTON);
      Mouse(browser, CEF_MOUSE_EVENT_UP, 40, 60, CEF_MOUSE_BUTTON_LEFT, 0);
    } else if (text == "chord") {
      Mouse(browser, CEF_MOUSE_EVENT_DOWN, 40, 60, CEF_MOUSE_BUTTON_LEFT,
            CEF_EVENTFLAG_LEFT_MOUSE_BUTTON);
      Mouse(browser, CEF_MOUSE_EVENT_MOVE, 41, 61, CEF_MOUSE_BUTTON_LEFT,
            CEF_EVENTFLAG_LEFT_MOUSE_BUTTON);
    } else if (text == "captured") {
      Mouse(browser, CEF_MOUSE_EVENT_MOVE, 350, 250, CEF_MOUSE_BUTTON_LEFT,
            CEF_EVENTFLAG_LEFT_MOUSE_BUTTON);
      Mouse(browser, CEF_MOUSE_EVENT_UP, 350, 250, CEF_MOUSE_BUTTON_LEFT, 0);
    } else if (text.rfind("blocked:", 0) == 0) {
      EXPECT_EQ(InputSequence::WheelFirst, sequence_);
      EXPECT_EQ("blocked:1", text);
      EXPECT_EQ(0, forwarded_pinches_);
      Pinch(browser, true);
    } else if (text == "scrolled") {
      EXPECT_EQ(InputSequence::Fling, sequence_);
      Gesture(browser, CEF_GESTURE_EVENT_FLING_START);
      Gesture(browser, CEF_GESTURE_EVENT_FLING_CANCEL);
      // A renderer round trip followed by consecutive frames checks that the
      // cancelled fling no longer advances the document.
      frame->ExecuteJavaScript(R"(
        requestAnimationFrame(()=>requestAnimationFrame(()=> {
          const before=scrollY;
          requestAnimationFrame(()=>requestAnimationFrame(()=>
            query(JSON.stringify({before,after:scrollY}))));
        }));)",
                               kSequenceUrl, 0);
    } else if (text.rfind("selected:", 0) == 0) {
      EXPECT_EQ(InputSequence::Popup, sequence_);
      EXPECT_NE("selected:0", text);
      EXPECT_EQ(1, popup_downs_);
      EXPECT_TRUE(popup_painted_);
      popup_selected_ = true;
      RecordPopupEvent("selection confirmed");
      if (popup_hidden_) {
        FinishPopup();
      }
    } else if (text == "reset-ready") {
      // Pinch scale is browser state. Restore it before releasing this lease,
      // and wait for a renderer-observed reset rather than relying on close.
      resetting_ = true;
      Gesture(browser, CEF_GESTURE_EVENT_SCROLL_BEGIN);
      Pinch(browser, false, 1.0f / page_scale_);
      Gesture(browser, CEF_GESTURE_EVENT_SCROLL_END);
    } else if (text == "reset") {
      EXPECT_TRUE(resetting_);
      DestroyTest();
    } else {
      auto value = CefParseJSON(request, JSON_PARSER_RFC);
      CheckReport(frame, value);
    }
    return true;
  }

  bool OnPreMouseEvent(CefRefPtr<CefBrowser>,
                       const CefMouseInputEvent& event) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    if (IsPopupSequence() && event.type != CEF_MOUSE_EVENT_MOVE) {
      std::ostringstream details;
      details << "OnPreMouseEvent type=" << event.type << " local=" << event.x
              << "," << event.y << " screen=" << event.screen_x << ","
              << event.screen_y << " pointer_id=" << event.pointer_id
              << " button=" << event.button << " modifiers=" << event.modifiers;
      RecordPopupEvent(details.str());
    }
    if (sequence_ == InputSequence::Popup && popup_clicked_ &&
        event.pointer_id == 17 && event.type == CEF_MOUSE_EVENT_DOWN) {
      ++popup_downs_;
      EXPECT_EQ(10, event.x);
      EXPECT_EQ(popup_rect_.height / 2, event.y);
    }
    // Native hover can report no buttons down during the injected drag,
    // releasing renderer pointer capture before the synthetic mouse-up.
    // Isolate this sequence using the injected events' pointer ID.
    return sequence_ == InputSequence::Mouse && started_ &&
           event.pointer_id != 17;
  }

  bool OnPreGestureEvent(CefRefPtr<CefBrowser>,
                         const CefGestureInputEvent& event) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_TRUE(started_);
    if (resetting_) {
      return false;
    }
    if (IsPopupSequence()) {
      std::ostringstream details;
      details << "OnPreGestureEvent type=" << event.type << " local=" << event.x
              << "," << event.y << " screen=" << event.screen_x << ","
              << event.screen_y << " delta=" << event.delta_x << ","
              << event.delta_y << " source=" << event.source_device;
      RecordPopupEvent(details.str());
    }
    if (sequence_ == InputSequence::PopupScroll && popup_clicked_ &&
        event.type == CEF_GESTURE_EVENT_SCROLL_UPDATE) {
      ++popup_updates_;
      EXPECT_EQ(10, event.x);
      EXPECT_EQ(popup_rect_.height / 2, event.y);
    }
    if (event.type == CEF_GESTURE_EVENT_PINCH_UPDATE) {
      EXPECT_FLOAT_EQ(1.5f, event.scale);
      if (!event.needs_wheel_event) {
        ++forwarded_pinches_;
      }
    }
    if (event.type == CEF_GESTURE_EVENT_FLING_START) {
      EXPECT_FLOAT_EQ(-500, event.velocity_y);
      ++fling_starts_;
    }
    if (event.type == CEF_GESTURE_EVENT_FLING_CANCEL) {
      ++fling_cancels_;
    }
    return false;
  }

  void OnPopupSize(CefRefPtr<CefBrowser>, const CefRect& rect) override {
    popup_rect_ = rect;
    std::ostringstream details;
    details << "OnPopupSize rect=" << rect.x << "," << rect.y << ","
            << rect.width << "," << rect.height;
    RecordPopupEvent(details.str());
  }

  void OnPopupShow(CefRefPtr<CefBrowser>, bool show) override {
    RecordPopupEvent(show ? "OnPopupShow true" : "OnPopupShow false");
    if (!show) {
      popup_hidden_ = true;
      if (popup_selected_ || popup_scrolled_) {
        FinishPopup();
      }
    }
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirty_rects,
               const void* buffer,
               int width,
               int height) override {
    // Inspect even frames excluded by the completion gate below, so the
    // timeout history distinguishes skipped paints from unchanged pixels.
    RecordPopupPaint(type, dirty_rects, buffer, width, height);
    if (IsPopupSequence() && type == PET_POPUP && !popup_clicked_) {
      popup_painted_ = true;
      popup_clicked_ = true;
      const auto* pixels = static_cast<const uint32_t*>(buffer);
      popup_pixels_.assign(pixels, pixels + width * height);
      popup_baseline_width_ = width;
      popup_baseline_height_ = height;
      EXPECT_GT(popup_rect_.x, 0);
      EXPECT_GT(popup_rect_.height, 40);
      CefPostTask(TID_UI, base::BindOnce(&InputSequenceTestHandler::SelectPopup,
                                         this, browser));
      RecordPopupEvent("posted SelectPopup");
    } else if (sequence_ == InputSequence::PopupScroll && type == PET_POPUP &&
               popup_updates_ == 1 && !popup_scrolled_) {
      const auto* pixels = static_cast<const uint32_t*>(buffer);
      const std::vector<uint32_t> current(pixels, pixels + width * height);
      if (current != popup_pixels_) {
        popup_scrolled_ = true;
        RecordPopupEvent("changed pixels accepted; posting ClosePopup");
        // Close the widget after the repaint, before returning this browser
        // to the reusable pool.
        CefPostTask(TID_UI,
                    base::BindOnce(&InputSequenceTestHandler::ClosePopup, this,
                                   browser));
      }
    }
  }

 private:
  void CheckReport(CefRefPtr<CefFrame> frame, CefRefPtr<CefValue> value) {
    ASSERT_TRUE(value && value->GetType() == VTYPE_DICTIONARY);
    const auto report = value->GetDictionary();
    if (sequence_ == InputSequence::Mouse) {
      auto details = report->GetList("details");
      ASSERT_TRUE(details && details->GetSize() == 2);
      EXPECT_EQ(1, details->GetInt(0));
      EXPECT_EQ(2, details->GetInt(1));
      auto chord = report->GetList("chord");
      ASSERT_TRUE(chord && chord->GetSize() == 4);
      constexpr int buttons[] = {0, 2, 2, 0};
      constexpr int held[] = {1, 3, 1, 0};
      for (size_t i = 0; i < 4; ++i) {
        auto event = chord->GetDictionary(i);
        EXPECT_EQ(i < 2 ? "mousedown" : "mouseup",
                  event->GetString("type").ToString());
        EXPECT_EQ(buttons[i], event->GetInt("button"));
        EXPECT_EQ(held[i], event->GetInt("buttons"));
      }
      auto captured = report->GetDictionary("captured");
      ASSERT_TRUE(captured);
      // Display scaling can leave fractional rounding in pointer coordinates.
      auto coordinate = [&](const char* key) -> double {
        const auto type = captured->GetType(key);
        EXPECT_TRUE(type == VTYPE_INT || type == VTYPE_DOUBLE);
        return type == VTYPE_INT ? captured->GetInt(key)
                                 : captured->GetDouble(key);
      };
      EXPECT_NEAR(350.0, coordinate("x"), 0.001);
      EXPECT_NEAR(250.0, coordinate("y"), 0.001);
      EXPECT_EQ(0, captured->GetInt("buttons"));
      EXPECT_TRUE(captured->GetBool("held"));
    } else if (sequence_ == InputSequence::Fling) {
      EXPECT_EQ(1, fling_starts_);
      EXPECT_EQ(1, fling_cancels_);
      EXPECT_GT(report->GetInt("before"), 0);
      EXPECT_EQ(report->GetInt("before"), report->GetInt("after"));
    } else {
      EXPECT_GT(report->GetDouble("scale"), 1.1);
      EXPECT_EQ(1, forwarded_pinches_);
      EXPECT_EQ(0, report->GetInt("touches"));
      if (sequence_ == InputSequence::WheelFirst) {
        EXPECT_GE(report->GetInt("wheels"), 2);
      } else {
        EXPECT_EQ(0, report->GetInt("wheels"));
      }
      page_scale_ = report->GetDouble("scale");
      frame->ExecuteJavaScript("resetting=true;query('reset-ready');",
                               kSequenceUrl, 0);
      return;
    }
    DestroyTest();
  }

  void FocusInput(CefRefPtr<CefBrowser> browser) {
    if (!waiting_) {
      return;
    }
    CefMouseEvent event;
    event.x = 220;
    event.y = 40;
    browser->GetHost()->SendMouseMoveEvent(event, false);
    browser->GetHost()->SendMouseClickEvent(event, MBT_LEFT, false, 1);
    browser->GetHost()->SendMouseClickEvent(event, MBT_LEFT, true, 1);
    CefPostDelayedTask(
        TID_UI,
        base::BindOnce(&InputSequenceTestHandler::FocusInput, this, browser),
        50);
  }

  void Mouse(CefRefPtr<CefBrowser> browser,
             cef_mouse_input_event_type_t type,
             int x,
             int y,
             cef_mouse_button_type_t button,
             uint32_t modifiers,
             int count = 1) {
    CefMouseInputEvent event;
    event.type = type;
    event.x = event.screen_x = x;
    event.y = event.screen_y = y;
    event.button = button;
    event.modifiers = modifiers;
    event.click_count = count;
    event.pointer_type = CEF_POINTER_TYPE_MOUSE;
    event.pointer_id = 17;
    if (IsPopupSequence()) {
      std::ostringstream details;
      details << "SendMouseEvent type=" << type << " position=" << x << "," << y
              << " button=" << button << " modifiers=" << modifiers
              << " click_count=" << count;
      RecordPopupEvent(details.str());
    }
    browser->GetHost()->SendMouseEvent(event);
    if (IsPopupSequence()) {
      RecordPopupEvent("SendMouseEvent returned type=" + std::to_string(type));
    }
  }

  void Gesture(CefRefPtr<CefBrowser> browser,
               cef_gesture_input_event_type_t type,
               bool wheel_first = false,
               float scale = 1.5f) {
    CefGestureInputEvent event;
    event.type = type;
    event.x = event.screen_x = 150;
    event.y = event.screen_y = 100;
    event.source_device = wheel_first ? CEF_GESTURE_DEVICE_TOUCHPAD
                                      : CEF_GESTURE_DEVICE_TOUCHSCREEN;
    event.primary_pointer_type = CEF_POINTER_TYPE_TOUCH;
    if (type == CEF_GESTURE_EVENT_SCROLL_BEGIN) {
      event.pointer_count = 1;
      event.delta_y = -40;
    } else if (type == CEF_GESTURE_EVENT_SCROLL_UPDATE) {
      event.delta_y = event.delta_y_unconstrained = -40;
    } else if (type == CEF_GESTURE_EVENT_FLING_START) {
      event.velocity_y = -500;
    } else if (type == CEF_GESTURE_EVENT_PINCH_UPDATE) {
      event.scale = scale;
    }
    event.needs_wheel_event = wheel_first;
    browser->GetHost()->SendGestureEvent(event);
  }

  void Pinch(CefRefPtr<CefBrowser> browser,
             bool wheel_first,
             float scale = 1.5f) {
    for (auto type :
         {CEF_GESTURE_EVENT_PINCH_BEGIN, CEF_GESTURE_EVENT_PINCH_UPDATE,
          CEF_GESTURE_EVENT_PINCH_END}) {
      Gesture(browser, type, wheel_first, scale);
    }
  }

  void SelectPopup(CefRefPtr<CefBrowser> browser) {
    RecordPopupEvent("SelectPopup task entered");
    const int x = popup_rect_.x + 10;
    const int y = popup_rect_.y + popup_rect_.height / 2;
    if (sequence_ == InputSequence::PopupScroll) {
      for (auto type :
           {CEF_GESTURE_EVENT_SCROLL_BEGIN, CEF_GESTURE_EVENT_SCROLL_UPDATE,
            CEF_GESTURE_EVENT_SCROLL_END}) {
        CefGestureInputEvent event;
        event.type = type;
        event.source_device = CEF_GESTURE_DEVICE_TOUCHSCREEN;
        event.primary_pointer_type = CEF_POINTER_TYPE_TOUCH;
        event.x = event.screen_x = x;
        event.y = event.screen_y = y;
        if (type == CEF_GESTURE_EVENT_SCROLL_BEGIN) {
          event.pointer_count = 1;
          event.delta_y = -80;
        } else if (type == CEF_GESTURE_EVENT_SCROLL_UPDATE) {
          event.delta_y = event.delta_y_unconstrained = -80;
        }
        std::ostringstream details;
        details << "SendGestureEvent type=" << type << " position=" << x << ","
                << y << " delta_y=" << event.delta_y
                << " pointers=" << event.pointer_count;
        RecordPopupEvent(details.str());
        browser->GetHost()->SendGestureEvent(event);
        RecordPopupEvent("SendGestureEvent returned type=" +
                         std::to_string(type));
      }
      return;
    }
    Mouse(browser, CEF_MOUSE_EVENT_MOVE, x, y, CEF_MOUSE_BUTTON_NONE, 0);
    Mouse(browser, CEF_MOUSE_EVENT_DOWN, x, y, CEF_MOUSE_BUTTON_LEFT,
          CEF_EVENTFLAG_LEFT_MOUSE_BUTTON);
    Mouse(browser, CEF_MOUSE_EVENT_UP, x, y, CEF_MOUSE_BUTTON_LEFT, 0);
  }

  void ClosePopup(CefRefPtr<CefBrowser> browser) {
    RecordPopupEvent("ClosePopup task entered");
    Mouse(browser, CEF_MOUSE_EVENT_DOWN, 1, 1, CEF_MOUSE_BUTTON_LEFT,
          CEF_EVENTFLAG_LEFT_MOUSE_BUTTON);
    Mouse(browser, CEF_MOUSE_EVENT_UP, 1, 1, CEF_MOUSE_BUTTON_LEFT, 0);
  }

  void FinishPopup() {
    RecordPopupEvent("FinishPopup");
    if (!popup_finish_posted_) {
      popup_finish_posted_ = true;
      // OnPopupShow can run inside popup widget destruction. Let that finish
      // before releasing the lease or closing the owning WebContents.
      CefPostTask(TID_UI,
                  base::BindOnce(&InputSequenceTestHandler::DestroyTest, this));
    }
  }

  void OnTestTimeout(int timeout_ms, bool treat_as_error) override {
    waiting_ = false;
    std::ostringstream details;
    details << "sequence=" << static_cast<int>(sequence_)
            << " started=" << started_ << " pinches=" << forwarded_pinches_
            << " fling=" << fling_starts_ << "/" << fling_cancels_
            << " popup_downs=" << popup_downs_
            << " popup_updates=" << popup_updates_
            << " popup_painted=" << popup_painted_
            << " popup_scrolled=" << popup_scrolled_
            << " popup_selected=" << popup_selected_
            << " popup_hidden=" << popup_hidden_
            << " popup_rect=" << popup_rect_.x << "," << popup_rect_.y << ","
            << popup_rect_.width << "," << popup_rect_.height;
    if (IsPopupSequence()) {
      details << "\nPopup browser-side history (steady-clock us; input "
                 "callbacks precede renderer handling):"
              << "\nview_paints=" << view_paints_
              << " view_paints_after_input=" << view_paints_after_input_
              << " popup_paints=" << popup_paints_
              << " paints_before_input=" << paints_before_input_
              << " paints_after_input=" << paints_after_input_
              << " changed_before_input=" << changed_before_input_
              << " unchanged_after_input=" << unchanged_after_input_
              << " skipped_update_count=" << skipped_update_count_
              << "\nevents=" << popup_event_count_;
      for (const auto& entry : popup_events_) {
        details << "\n" << entry;
      }
      if (popup_event_count_ >
          popup_events_.size() + popup_recent_events_.size()) {
        details << "\n... omitted "
                << popup_event_count_ - popup_events_.size() -
                       popup_recent_events_.size()
                << " middle events ...";
      }
      for (const auto& entry : popup_recent_events_) {
        details << "\n" << entry;
      }
    }
    ADD_FAILURE() << details.str();
    RoutingTestHandler::OnTestTimeout(timeout_ms, treat_as_error);
  }

  bool IsPopupSequence() const {
    return sequence_ == InputSequence::Popup ||
           sequence_ == InputSequence::PopupScroll;
  }

  // Opening the select also generates mouse callbacks. Count only the input
  // directed into the painted popup: a selection mouse-down or scroll update.
  bool GotPopupInput() const {
    return sequence_ == InputSequence::Popup ? popup_downs_ != 0
                                             : popup_updates_ != 0;
  }

  // Keep the first and last events so an animated popup cannot exhaust memory
  // or obscure setup/dispatch in the timeout log. Successful runs stay quiet.
  void RecordPopupEvent(const std::string& event) {
    if (!IsPopupSequence()) {
      return;
    }
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - popup_log_start_)
            .count();
    std::ostringstream entry;
    entry << "#" << ++popup_event_count_ << " +" << elapsed << "us " << event
          << " [clicked=" << popup_clicked_ << " updates=" << popup_updates_
          << " downs=" << popup_downs_ << " selected=" << popup_selected_
          << " scrolled=" << popup_scrolled_ << " hidden=" << popup_hidden_
          << "]";
    constexpr size_t kRetainedEvents = 32;
    if (popup_events_.size() < kRetainedEvents) {
      popup_events_.push_back(entry.str());
    } else {
      if (popup_recent_events_.size() == kRetainedEvents) {
        popup_recent_events_.pop_front();
      }
      popup_recent_events_.push_back(entry.str());
    }
  }

  void RecordPopupPaint(PaintElementType type,
                        const RectList& dirty_rects,
                        const void* buffer,
                        int width,
                        int height) {
    if (!IsPopupSequence()) {
      return;
    }
    if (type != PET_POPUP) {
      ++view_paints_;
      if (GotPopupInput()) {
        ++view_paints_after_input_;
      }
      if (view_paints_ == 1 || view_paints_after_input_ == 1) {
        RecordPopupEvent("PET_VIEW paint size=" + std::to_string(width) + "," +
                         std::to_string(height));
      }
      return;
    }
    ++popup_paints_;
    if (!GotPopupInput()) {
      ++paints_before_input_;
    } else {
      ++paints_after_input_;
    }
    const auto* pixels = static_cast<const uint32_t*>(buffer);
    const size_t count = static_cast<size_t>(width) * height;
    const bool same_size = count == popup_pixels_.size();
    size_t different = 0;
    size_t first_difference = 0;
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < count; ++i) {
      hash = (hash ^ pixels[i]) * 1099511628211ULL;
      if (same_size && pixels[i] != popup_pixels_[i]) {
        if (different++ == 0) {
          first_difference = i;
        }
      }
    }
    const bool changed = !same_size || different != 0;
    const char* disposition = "baseline";
    if (popup_clicked_) {
      if (!GotPopupInput() && changed) {
        ++changed_before_input_;
      } else if (GotPopupInput() && !changed) {
        ++unchanged_after_input_;
      }
      if (sequence_ == InputSequence::Popup) {
        if (popup_selected_) {
          disposition = "selection confirmed";
        } else if (GotPopupInput()) {
          disposition = "awaiting selection query";
        } else {
          disposition = "awaiting popup mouse-down";
        }
      } else if (popup_updates_ == 0) {
        disposition = "ignored: no update callback yet";
      } else if (popup_updates_ != 1) {
        disposition = "ignored: update count != 1";
        ++skipped_update_count_;
      } else if (popup_scrolled_) {
        disposition = "ignored: already scrolled";
      } else {
        disposition = changed ? "comparison: changed" : "comparison: equal";
      }
    }
    std::ostringstream details;
    details << "PET_POPUP paint=" << popup_paints_ << " size=" << width << ","
            << height << " rect=" << popup_rect_.x << "," << popup_rect_.y
            << "," << popup_rect_.width << "," << popup_rect_.height
            << " baseline_size=" << popup_baseline_width_ << ","
            << popup_baseline_height_
            << " baseline_pixels=" << popup_pixels_.size() << " hash=" << hash
            << " same_pixel_count=" << same_size
            << " different_pixels=" << different
            << " disposition=" << disposition;
    if (different) {
      details << " first_difference=" << first_difference % width << ","
              << first_difference / width
              << " old=" << popup_pixels_[first_difference]
              << " new=" << pixels[first_difference];
    }
    details << " dirty_rects=";
    for (const auto& rect : dirty_rects) {
      details << "(" << rect.x << "," << rect.y << "," << rect.width << ","
              << rect.height << ")";
    }
    RecordPopupEvent(details.str());
  }

  static constexpr char kSequenceUrl[] = "https://tests/input-sequence";
  const InputSequence sequence_;
  const bool osr_;
  bool waiting_ = false;
  bool started_ = false;
  bool popup_painted_ = false;
  bool popup_clicked_ = false;
  bool popup_selected_ = false;
  bool popup_hidden_ = false;
  bool popup_scrolled_ = false;
  bool popup_finish_posted_ = false;
  std::vector<uint32_t> popup_pixels_;
  bool resetting_ = false;
  float page_scale_ = 1;
  CefRect popup_rect_;
  int popup_downs_ = 0;
  int popup_updates_ = 0;
  int forwarded_pinches_ = 0;
  int fling_starts_ = 0;
  int fling_cancels_ = 0;

  // Logging-only state for popup timeout diagnostics.
  const std::chrono::steady_clock::time_point popup_log_start_ =
      std::chrono::steady_clock::now();
  size_t popup_event_count_ = 0;
  std::vector<std::string> popup_events_;
  std::deque<std::string> popup_recent_events_;
  int view_paints_ = 0;
  int view_paints_after_input_ = 0;
  int popup_paints_ = 0;
  int popup_baseline_width_ = 0;
  int popup_baseline_height_ = 0;
  int paints_before_input_ = 0;
  int paints_after_input_ = 0;
  int changed_before_input_ = 0;
  int unchanged_after_input_ = 0;
  int skipped_update_count_ = 0;

  IMPLEMENT_REFCOUNTING(InputSequenceTestHandler);
};

void RunInputSequence(InputSequence sequence, bool osr) {
  CefRefPtr<InputSequenceTestHandler> handler =
      new InputSequenceTestHandler(sequence, osr);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(MouseGestureInputTest, MouseSequencesOsr) {
  RunInputSequence(InputSequence::Mouse, true);
}

TEST(MouseGestureInputTest, MouseSequencesWindowed) {
  RunInputSequence(InputSequence::Mouse, false);
}

TEST(MouseGestureInputTest, TouchscreenPinchOsr) {
  RunInputSequence(InputSequence::Pinch, true);
}

TEST(MouseGestureInputTest, WheelFirstPinchOsr) {
  RunInputSequence(InputSequence::WheelFirst, true);
}

TEST(MouseGestureInputTest, WheelFirstPinchWindowed) {
  RunInputSequence(InputSequence::WheelFirst, false);
}

TEST(MouseGestureInputTest, FlingCancelOsr) {
  RunInputSequence(InputSequence::Fling, true);
}

TEST(MouseGestureInputTest, PopupSelectionOsr) {
  RunInputSequence(InputSequence::Popup, true);
}

TEST(MouseGestureInputTest, PopupGestureScrollOsr) {
  RunInputSequence(InputSequence::PopupScroll, true);
}
#endif  // CEF_API_ADDED(15800)

}  // namespace
