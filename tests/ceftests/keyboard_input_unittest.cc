// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <algorithm>
#include <array>

#include "include/cef_input_event_utils.h"
#include "include/cef_input_handler.h"
#include "include/cef_parser.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/keyboard_input_test_util.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

#if CEF_API_ADDED(CEF_NEXT)
namespace {

constexpr char kUrl[] = "https://tests/keyboard-input";
enum class Scenario {
  Unicode,
  Alpha,
  Sequence,
  Keypad,
  LeftShift,
  RightShift,
  RepeatedShift,
  DeadKey,
  UnknownCode,
  Modifiers,
  AltGraph
};

enum class Mode {
  Portable,
  Native,
  NativeTextClient,
  PreHandled,
  PageHandled,
  Shortcut,
  LegacyPreHandled,
  LegacyShortcut,
  LegacyPostHandled,
  LegacySend,
  LegacySendPreHandled,
  WindowHandled,
  LegacyWindowHandled
};

void ExpectSnapshot(const CefKeyboardInputEvent& expected,
                    const CefKeyboardInputEvent& actual,
                    bool compare_timestamp = true) {
  EXPECT_EQ(expected.type, actual.type);
  EXPECT_EQ(expected.modifiers, actual.modifiers);
  // Legacy input has no timestamp field; native conversion selects it.
  if (compare_timestamp) {
    if (expected.timestamp) {
      EXPECT_EQ(expected.timestamp, actual.timestamp);
    } else {
      EXPECT_GT(actual.timestamp, 0);
    }
  }
  EXPECT_EQ(expected.dom_code, actual.dom_code);
  EXPECT_EQ(expected.dom_key, actual.dom_key);
  EXPECT_EQ(expected.windows_key_code, actual.windows_key_code);
  EXPECT_EQ(expected.is_system_key, actual.is_system_key);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(expected.text[i], actual.text[i]);
    EXPECT_EQ(expected.unmodified_text[i], actual.unmodified_text[i]);
  }
}

class KeyboardInputTestHandler : public RoutingTestHandler,
                                 public CefRenderHandler,
                                 public CefInputHandler,
                                 public CefKeyboardHandler {
 public:
  KeyboardInputTestHandler(Mode mode, bool osr, Scenario scenario)
      : mode_(mode), osr_(osr), scenario_(scenario) {}

#if defined(OS_MAC)
  void SetMacTextClientInput(MacTextClientInput input) { mac_input_ = input; }
#endif

  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

  CefRefPtr<CefInputHandler> GetInputHandler() override { return this; }

  // DEPRECATED: Use GetInputHandler(). Return both handlers here to verify
  // ordering and legacy fallthrough.
  CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }

  void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override {
    rect = CefRect(0, 0, 400, 300);
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType,
               const RectList&,
               const void*,
               int,
               int) override {
    if (waiting_for_paint_) {
      waiting_for_paint_ = false;
      ClickInput(browser);
    }
  }

  void RunTest() override {
    if (osr_) {
      SetUseAlloyStyle(true, true);
    }
    // No blur/refocus workaround: the initial editable focus must be reported
    // correctly, including Chromium's catch-all focus type (CEF #3184).
    std::string html =
        R"(<style>body{margin:0}input{width:200px;height:40px}</style>
      <input id='i'><script>
      const events = [];
      const cancel = CANCEL;
      let started = false;
      let loaded = false;
      let reportSent = false;
      let keyups = 0;
      function notifyReady() {
        if (loaded && document.hasFocus() && !started) {
          started = true;
          window.testQuery({request:'focused'});
        }
      }
      window.addEventListener('focus', notifyReady);
      i.addEventListener('click', () => window.testQuery({request:'ready'}),
                         {once:true});
      for (const type of ['keydown', 'keypress', 'keyup']) {
        document.addEventListener(type, e => {
          events.push({type:e.type, key:e.key, code:e.code, location:e.location,
                       repeat:e.repeat, shift:e.shiftKey, ctrl:e.ctrlKey, alt:e.altKey,
                       meta:e.metaKey, caps:e.getModifierState('CapsLock'),
                       num:e.getModifierState('NumLock'),
                       altgr:e.getModifierState('AltGraph'),
                       keyCode:e.keyCode, charCode:e.charCode, which:e.which});
          if (cancel) e.preventDefault();
          if (e.type === 'keyup' && ++keyups === EXPECTED_KEYUPS) report();
        });
      }
      window.addEventListener('load', () => {
        loaded = true;
        window.testQuery({request:'loaded'});
      });
      function report() {
        if (reportSent) return;
        reportSent = true;
        window.testQuery({request:JSON.stringify({events, value:i.value})});
      }
    </script>)";
    if (mode_ == Mode::NativeTextClient) {
      const std::string input = "<input id='i'>";
      html.replace(html.find(input), input.size(),
                   "<textarea id='i'></textarea>");
    }
    html.replace(html.find("CANCEL"), 6,
                 mode_ == Mode::PageHandled ? "true" : "false");
    html.replace(html.find("EXPECTED_KEYUPS"), 15,
                 scenario_ == Scenario::Sequence ? "3" : "1");
    AddResource(kUrl, html, "text/html");
    if (!osr_) {
      if (IsWindowTest()) {
        SetUseViews(true);
      }
      // Native-window path honors global runtime/style flags and --use-views.
      CreateBrowser(kUrl);
      SetTestTimeout(10000);
      return;
    }
    CefWindowInfo window_info;
#if defined(OS_WIN)
    window_info.SetAsWindowless(GetDesktopWindow());
#else
    window_info.SetAsWindowless(kNullWindowHandle);
#endif
    CefBrowserSettings settings;
    window_info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
    CefBrowserHost::CreateBrowser(window_info, this, kUrl, settings, nullptr,
                                  nullptr);
    SetTestTimeout(10000);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame>,
               int64_t,
               const CefString& request,
               bool,
               CefRefPtr<Callback> callback) override {
    callback->Success("");
    if (request == "loaded") {
      browser->GetHost()->SetFocus(true);
      browser->GetMainFrame()->ExecuteJavaScript("notifyReady()", kUrl, 0);
      return true;
    }
    if (request == "focused") {
      if (osr_) {
        waiting_for_paint_ = true;
        browser->GetHost()->Invalidate(PET_VIEW);
      } else {
        ClickInput(browser);
      }
      return true;
    }
    if (request == "ready") {
      EXPECT_EQ(use_alloy_style_browser() ? CEF_RUNTIME_STYLE_ALLOY
                                          : CEF_RUNTIME_STYLE_CHROME,
                browser->GetHost()->GetRuntimeStyle());
      BuildEvents();
      CefPostTask(TID_UI, base::BindOnce(&KeyboardInputTestHandler::Start, this,
                                         browser));
      return true;
    }
    auto value = CefParseJSON(request, JSON_PARSER_RFC);
    if (!value || value->GetType() != VTYPE_DICTIONARY) {
      ADD_FAILURE() << "Invalid renderer report";
      DestroyTest();
      return true;
    }
    auto result = value->GetDictionary();
    auto events = result->GetList("events");
    const bool suppressed = IsPreHandled();
    const bool no_character = suppressed || mode_ == Mode::PageHandled ||
                              IsShortcut() || suppress_character_dom_;
    EXPECT_EQ(no_character ? "" : insertion_,
              result->GetString("value").ToString());
    std::vector<const CefKeyboardInputEvent*> dom_events;
    if (!suppressed) {
      for (const auto& event : expected_) {
        if (!no_character || event.type != CEF_KEYEVENT_CHAR) {
          dom_events.push_back(&event);
        }
      }
    }
    EXPECT_EQ(dom_events.size(), events->GetSize());
    for (size_t i = 0; i < std::min(dom_events.size(), events->GetSize());
         ++i) {
      const auto& expected = *dom_events[i];
      auto entry = events->GetDictionary(i);
      const bool character = expected.type == CEF_KEYEVENT_CHAR;
      EXPECT_EQ(character                             ? "keypress"
                : expected.type == CEF_KEYEVENT_KEYUP ? "keyup"
                                                      : "keydown",
                entry->GetString("type").ToString());
      const size_t sequence_index =
          static_cast<size_t>(dom_events[i] - expected_.data()) / 3;
      constexpr std::array sequence_codes = {"KeyA", "KeyB", "KeyC"};
      EXPECT_EQ(scenario_ == Scenario::Sequence ? sequence_codes[sequence_index]
                                                : code_,
                entry->GetString("code").ToString());
      EXPECT_EQ(scenario_ == Scenario::Sequence
                    ? std::string(1, "abc"[sequence_index])
                    : logical_key_,
                entry->GetString("key").ToString());
      EXPECT_EQ(location_, entry->GetInt("location"));
      EXPECT_EQ(!!(expected.modifiers & CEF_EVENTFLAG_IS_REPEAT),
                entry->GetBool("repeat"));
      for (const auto& modifier :
           {std::pair{"shift", CEF_EVENTFLAG_SHIFT_DOWN},
            std::pair{"ctrl", CEF_EVENTFLAG_CONTROL_DOWN},
            std::pair{"alt", CEF_EVENTFLAG_ALT_DOWN},
            std::pair{"meta", CEF_EVENTFLAG_COMMAND_DOWN},
            std::pair{"caps", CEF_EVENTFLAG_CAPS_LOCK_ON},
            std::pair{"num", CEF_EVENTFLAG_NUM_LOCK_ON},
            std::pair{"altgr", CEF_EVENTFLAG_ALTGR_DOWN}}) {
        EXPECT_EQ(!!(expected.modifiers & modifier.second),
                  entry->GetBool(modifier.first))
            << modifier.first;
      }
      const int character_code = scenario_ == Scenario::Sequence
                                     ? "abc"[sequence_index]
                                     : character_code_;
      const int key_code =
          character ? character_code : expected.windows_key_code;
      EXPECT_EQ(key_code, entry->GetInt("keyCode"));
      EXPECT_EQ(key_code, entry->GetInt("which"));
      EXPECT_EQ(character ? character_code : 0, entry->GetInt("charCode"));
    }
    reported_ = true;
    MaybeDone();
    return true;
  }

  bool OnPreKeyboardEvent(CefRefPtr<CefBrowser>,
                          const CefKeyboardInputEvent& event,
                          const CefInputEventContext& context,
                          bool* is_keyboard_shortcut) override {
    EXPECT_LT(pre_, expected_.size());
    if (pre_ < expected_.size()) {
      ExpectSnapshot(expected_[pre_], event, !IsLegacySend());
    }
    EXPECT_TRUE(context.focus_on_editable_field);
    ++pre_;
    EXPECT_FALSE(*is_keyboard_shortcut);
    if (mode_ == Mode::PreHandled || mode_ == Mode::LegacySendPreHandled) {
      return true;
    }
    if (mode_ == Mode::Shortcut && event.type == CEF_KEYEVENT_RAWKEYDOWN) {
      *is_keyboard_shortcut = true;
      return false;
    }
    ++pre_unhandled_;
    return false;
  }

  bool OnKeyboardEvent(CefRefPtr<CefBrowser> browser,
                       const CefKeyboardInputEvent& event,
                       const CefInputEventContext& context) override {
    EXPECT_TRUE(context.focus_on_editable_field);
    size_t index = post_cursor_;
    for (; index < expected_.size(); ++index) {
      if (expected_[index].type == event.type) {
        break;
      }
      // Character insertion and editing commands may be consumed by the
      // renderer. Other raw downs and all key ups must reach this callback.
      if (!renderer_handles_keydown_) {
        EXPECT_EQ(CEF_KEYEVENT_CHAR, expected_[index].type);
      }
    }
    EXPECT_LT(index, expected_.size());
    if (index < expected_.size()) {
      ExpectSnapshot(expected_[index], event, !IsLegacySend());
    }
    post_cursor_ = index + 1;
    if (event.type == CEF_KEYEVENT_KEYUP ||
        (event.type != CEF_KEYEVENT_CHAR && !renderer_handles_keydown_)) {
      ++post_;
    }
    if (event.type == CEF_KEYEVENT_KEYUP && post_cursor_ == expected_.size()) {
      // The acknowledgement guarantees the renderer processed the final
      // event. Also request a report when DOM dispatch itself is broken.
      Report(browser);
    }
    if (mode_ == Mode::LegacyPostHandled || IsWindowTest() || IsLegacySend()) {
      ++post_unhandled_;
      return false;
    }
    MaybeDone();
    return true;  // Prevent platform fallback from changing the test page.
  }

  // DEPRECATED: Use OnPreKeyboardEvent(); retained to test legacy fallthrough.
  bool OnPreKeyEvent(CefRefPtr<CefBrowser>,
                     const CefKeyEvent& event,
                     CefEventHandle,
                     bool* is_keyboard_shortcut) override {
    EXPECT_EQ(legacy_pre_ + 1, pre_unhandled_);
    EXPECT_GT(pre_, 0U);
    if (pre_ && pre_ <= expected_.size()) {
      EXPECT_EQ(expected_[pre_ - 1].type, event.type);
      if (IsLegacySend()) {
        ExpectLegacySnapshot(expected_[pre_ - 1], event);
      }
    }
    EXPECT_TRUE(event.focus_on_editable_field);
    ++legacy_pre_;
    if (mode_ == Mode::LegacyShortcut && event.type == KEYEVENT_RAWKEYDOWN) {
      *is_keyboard_shortcut = true;
    }
    return mode_ == Mode::LegacyPreHandled;
  }

  // DEPRECATED: Use OnKeyboardEvent(); retained to test legacy fallthrough.
  bool OnKeyEvent(CefRefPtr<CefBrowser>,
                  const CefKeyEvent& event,
                  CefEventHandle) override {
    EXPECT_EQ(legacy_post_ + 1, post_unhandled_);
    EXPECT_TRUE(mode_ == Mode::LegacyPostHandled || IsWindowTest() ||
                IsLegacySend());
    EXPECT_TRUE(event.focus_on_editable_field);
    if (post_cursor_ && post_cursor_ <= expected_.size()) {
      EXPECT_EQ(expected_[post_cursor_ - 1].type, event.type);
      if (IsLegacySend()) {
        ExpectLegacySnapshot(expected_[post_cursor_ - 1], event);
      }
    }
    ++legacy_post_;
    if (IsWindowTest()) {
      return false;
    }
    MaybeDone();
    return true;
  }

  bool OnWindowKeyboardEvent(int browser_id,
                             const CefKeyboardInputEvent& event) override {
    EXPECT_TRUE(IsWindowTest());
    EXPECT_GT(browser_id, 0);
    EXPECT_EQ(window_events_ + 1, legacy_post_);
    EXPECT_GT(post_cursor_, 0U);
    EXPECT_LE(post_cursor_, expected_.size());
    if (post_cursor_ && post_cursor_ <= expected_.size()) {
      ExpectSnapshot(expected_[post_cursor_ - 1], event);
    }
    ++window_events_;
    if (mode_ == Mode::LegacyWindowHandled) {
      return false;
    }
    MaybeDone();
    return true;
  }

  // DEPRECATED: Use OnWindowKeyboardEvent(); retained to test fallthrough.
  bool OnWindowKeyEvent(int browser_id, const CefKeyEvent& event) override {
    EXPECT_EQ(Mode::LegacyWindowHandled, mode_);
    EXPECT_GT(browser_id, 0);
    EXPECT_EQ(legacy_window_events_ + 1, window_events_);
    if (post_cursor_ && post_cursor_ <= expected_.size()) {
      const auto& expected = expected_[post_cursor_ - 1];
      EXPECT_EQ(expected.type, event.type);
      EXPECT_EQ(expected.windows_key_code, event.windows_key_code);
      EXPECT_EQ(expected.text[0], event.character);
    }
    ++legacy_window_events_;
    MaybeDone();
    return true;
  }

  void OnTestTimeout(int timeout_ms, bool treat_as_error) override {
    ADD_FAILURE() << "reported=" << reported_ << ", pre=" << pre_
                  << ", post=" << post_;
    RoutingTestHandler::OnTestTimeout(timeout_ms, treat_as_error);
  }

 private:
  void ClickInput(CefRefPtr<CefBrowser> browser) {
    CefMouseEvent event;
    event.x = event.y = 20;
    SendMouseClickEvent(browser, event);
  }

  void BuildEvents() {
    if (scenario_ == Scenario::Sequence) {
      // Distinct keys expose loss, duplication, and reordering across the
      // asynchronous send, renderer, and callback routes.
      expected_.clear();
      insertion_ = "abc";
      constexpr std::array codes = {CEF_DOM_CODE_US_A, CEF_DOM_CODE_US_B,
                                    CEF_DOM_CODE_US_C};
      for (size_t i = 0; i < insertion_.size(); ++i) {
        CefKeyboardInputEvent key;
        key.type = CEF_KEYEVENT_RAWKEYDOWN;
        key.dom_code = codes[i];
        key.dom_key = CEF_DOM_KEY_UNICODE_FLAG | insertion_[i];
        key.windows_key_code = "ABC"[i];
        key.text[0] = key.unmodified_text[0] = insertion_[i];
        expected_.push_back(key);
        key.type = CEF_KEYEVENT_CHAR;
        key.windows_key_code = insertion_[i];
        expected_.push_back(key);
        key.type = CEF_KEYEVENT_KEYUP;
        key.windows_key_code = "ABC"[i];
        expected_.push_back(key);
      }
      return;
    }
#if defined(OS_MAC)
    if (mode_ == Mode::NativeTextClient) {
      EXPECT_TRUE(BuildMacTextClientKeyboardInputEvents(mac_input_, expected_));
      code_ = "KeyA";
      logical_key_ = "a";
      insertion_ = "a";
      character_code_ = expected_[0].text[0];
      renderer_handles_keydown_ =
          mac_input_ == MacTextClientInput::Return ||
          mac_input_ == MacTextClientInput::ShiftReturn ||
          mac_input_ == MacTextClientInput::ControlCharacter ||
          mac_input_ == MacTextClientInput::CommandCharacter;
      if (mac_input_ == MacTextClientInput::OptionCharacter) {
        logical_key_ = insertion_ = "\xC3\xA5";
      } else if (mac_input_ == MacTextClientInput::Return ||
                 mac_input_ == MacTextClientInput::ShiftReturn) {
        code_ = "Enter";
        logical_key_ = "Enter";
        insertion_ = "\n";
      } else if (mac_input_ == MacTextClientInput::ControlCharacter ||
                 mac_input_ == MacTextClientInput::CommandCharacter ||
                 mac_input_ == MacTextClientInput::NonInsertionCommand) {
        insertion_.clear();
        suppress_character_dom_ =
            mac_input_ == MacTextClientInput::CommandCharacter;
      } else if (mac_input_ == MacTextClientInput::Clear) {
        code_ = "NumLock";
        logical_key_ = "Clear";
        insertion_.clear();
        location_ = 0;
      }
      return;
    }
#endif
    auto& down = expected_[0];
    down.type = CEF_KEYEVENT_RAWKEYDOWN;
    down.dom_code = CEF_DOM_CODE_US_A;
    down.dom_key = CEF_DOM_KEY_UNICODE_FLAG | 0x1f600;
    down.windows_key_code = 'A';
    down.modifiers = CEF_EVENTFLAG_SHIFT_DOWN | CEF_EVENTFLAG_IS_RIGHT |
                     CEF_EVENTFLAG_IS_REPEAT;
    down.text[0] = 0xd83d;
    down.text[1] = 0xde00;
    down.unmodified_text[0] = 'a';
    // A nonzero final unit after the terminator proves the callback copies
    // the entire native-compatible buffer, rather than rebuilding a string.
    down.unmodified_text[3] = 'Z';
    expected_[1] = down;
    expected_[1].type = CEF_KEYEVENT_CHAR;
    expected_[1].windows_key_code = down.text[0];
    expected_[1].modifiers &= ~CEF_EVENTFLAG_IS_REPEAT;
    expected_[2] = down;
    expected_[2].type = CEF_KEYEVENT_KEYUP;
    expected_[2].modifiers &= ~CEF_EVENTFLAG_IS_REPEAT;
    logical_key_ = insertion_ = "\xF0\x9F\x98\x80";
    location_ = 2;
#if defined(OS_WIN)
    if (mode_ == Mode::Native) {
      std::vector<int> state(256);
      // Explicit zero state is authoritative regardless of desktop modifiers.
      const auto layout = reinterpret_cast<uintptr_t>(GetKeyboardLayout(0));
      EXPECT_TRUE(CefKeyboardInputEventFromWindowsMessageEx(
          WM_KEYDOWN, 'A', 1 | (0x1e << 16), state, layout, 0, expected_[0]));
      EXPECT_TRUE(CefKeyboardInputEventFromWindowsMessageEx(
          WM_CHAR, 'a', 1 | (0x1e << 16), state, layout, 0, expected_[1]));
      EXPECT_TRUE(CefKeyboardInputEventFromWindowsMessageEx(
          WM_KEYUP, 'A',
          static_cast<LPARAM>(1LL | (0x1e << 16) | (1LL << 30) | (1LL << 31)),
          state, layout, 0, expected_[2]));
      EXPECT_EQ(static_cast<uint32_t>(CEF_DOM_CODE_US_A),
                expected_[0].dom_code);
      logical_key_ =
          CefString(std::u16string(1, static_cast<char16_t>(
                                          expected_[0].dom_key & 0x1fffff)))
              .ToString();
      insertion_ = "a";
      character_code_ = 'a';
      location_ = 0;
    }
#endif

#if defined(OS_MAC) || defined(CEF_TESTS_USE_GTK)
    if (mode_ == Mode::Native) {
      std::array<CefKeyboardInputEvent, 3> native;
      EXPECT_TRUE(BuildNativeKeyboardInputEvents(native));
      expected_.assign(native.begin(), native.end());
      logical_key_ = insertion_ = "a";
      character_code_ = 'a';
      location_ = 0;
    }
#endif
    if (scenario_ == Scenario::Unicode) {
      return;
    }
    CefKeyboardInputEvent key;
    key.type = CEF_KEYEVENT_RAWKEYDOWN;
    key.dom_code = CEF_DOM_CODE_US_A;
    key.dom_key = CEF_DOM_KEY_UNICODE_FLAG | 'a';
    key.windows_key_code = 'A';
    key.text[0] = key.unmodified_text[0] = 'a';
    code_ = "KeyA";
    logical_key_ = insertion_ = "a";
    character_code_ = 'a';
    location_ = 0;
    if (scenario_ == Scenario::AltGraph) {
      key.dom_code = CEF_DOM_CODE_DIGIT2;
      key.dom_key = CEF_DOM_KEY_UNICODE_FLAG | '@';
      key.windows_key_code = '2';
      key.modifiers = CEF_EVENTFLAG_ALTGR_DOWN;
      key.text[0] = '@';
      key.unmodified_text[0] = '2';
      code_ = "Digit2";
      logical_key_ = insertion_ = "@";
      character_code_ = '@';
    } else if (scenario_ == Scenario::Keypad) {
      key.dom_code = CEF_DOM_CODE_NUMPAD1;
      key.dom_key = CEF_DOM_KEY_UNICODE_FLAG | '1';
      key.windows_key_code = 0x61;
      key.modifiers = CEF_EVENTFLAG_IS_KEY_PAD | CEF_EVENTFLAG_NUM_LOCK_ON;
      key.text[0] = key.unmodified_text[0] = '1';
      code_ = "Numpad1";
      logical_key_ = insertion_ = "1";
      character_code_ = '1';
      location_ = 3;
    } else if (scenario_ == Scenario::UnknownCode) {
      key.dom_code = CEF_DOM_CODE_NONE;
      code_.clear();
    } else if (scenario_ == Scenario::DeadKey) {
      key.dom_code = CEF_DOM_CODE_QUOTE;
      key.dom_key = CEF_DOM_KEY_DEAD_FLAG | 0x0301;
      key.windows_key_code = 0xde;
      key.text[0] = key.unmodified_text[0] = 0;
      code_ = "Quote";
      logical_key_ = "Dead";
      insertion_.clear();
    } else if (scenario_ == Scenario::LeftShift ||
               scenario_ == Scenario::RightShift ||
               scenario_ == Scenario::RepeatedShift ||
               scenario_ == Scenario::Modifiers) {
      const bool right = scenario_ == Scenario::RightShift;
      key.dom_code = right ? CEF_DOM_CODE_SHIFT_RIGHT : CEF_DOM_CODE_SHIFT_LEFT;
      key.dom_key = CEF_DOM_KEY_SHIFT;
      key.windows_key_code = 16;
      key.modifiers = CEF_EVENTFLAG_SHIFT_DOWN |
                      (right ? CEF_EVENTFLAG_IS_RIGHT : CEF_EVENTFLAG_IS_LEFT);
      key.text[0] = key.unmodified_text[0] = 0;
      code_ = right ? "ShiftRight" : "ShiftLeft";
      logical_key_ = "Shift";
      insertion_.clear();
      location_ = right ? 2 : 1;
      if (scenario_ == Scenario::Modifiers) {
        key.modifiers |= CEF_EVENTFLAG_CONTROL_DOWN | CEF_EVENTFLAG_ALT_DOWN |
                         CEF_EVENTFLAG_COMMAND_DOWN |
                         CEF_EVENTFLAG_CAPS_LOCK_ON |
                         CEF_EVENTFLAG_NUM_LOCK_ON | CEF_EVENTFLAG_ALTGR_DOWN;
      }
    }
    expected_.clear();
    expected_.push_back(key);
    if (!insertion_.empty()) {
      CefKeyboardInputEvent character(key);
      character.type = CEF_KEYEVENT_CHAR;
      character.windows_key_code = key.text[0];
      expected_.push_back(character);
    } else if (scenario_ == Scenario::RepeatedShift) {
      CefKeyboardInputEvent repeated(key);
      repeated.modifiers |= CEF_EVENTFLAG_IS_REPEAT;
      expected_.push_back(repeated);
    }
    key.type = CEF_KEYEVENT_KEYUP;
    if (key.dom_key == CEF_DOM_KEY_SHIFT) {
      key.modifiers &= ~CEF_EVENTFLAG_SHIFT_DOWN;
    }
    expected_.push_back(key);
#if defined(OS_MAC)
    if (IsLegacySend() && expected_.size() == 3) {
      // Legacy macOS CHAR keeps the native builder's accelerator key code.
      expected_[1].windows_key_code = 'A';
    }
#endif
  }

  void Start(CefRefPtr<CefBrowser> browser) {
    // Exercise the asynchronous public send/C API route as well as UI-thread
    // dispatch. Conversion happened synchronously before this thread hop.
#if defined(OS_MAC)
    if (mode_ == Mode::NativeTextClient) {
      SendMacTextClientKeyboardInput(browser, mac_input_, expected_);
    } else
#endif
      CefPostTask(TID_IO, base::BindOnce(&KeyboardInputTestHandler::Send, this,
                                         browser));
  }

  void Send(CefRefPtr<CefBrowser> browser) {
    for (const auto& event : expected_) {
      if (IsLegacySend()) {
        CefKeyEvent legacy;
        legacy.type = event.type;
        legacy.modifiers = event.modifiers;
        legacy.windows_key_code = event.windows_key_code;
        legacy.character = event.text[0];
        legacy.unmodified_character = event.unmodified_text[0];
#if defined(OS_WIN)
        legacy.native_key_code = 0x1e;
#elif defined(OS_MAC)
        legacy.native_key_code = 0;
#else
        legacy.native_key_code = 38;
        if (event.type == CEF_KEYEVENT_CHAR) {
          // Legacy Linux CHAR uses the key-down accelerator key code. X11
          // derives the character from it; callbacks report the character.
          legacy.windows_key_code = expected_[0].windows_key_code;
        }
#endif
        browser->GetHost()->SendKeyEvent(legacy);
      } else {
        browser->GetHost()->SendKeyboardEvent(event);
      }
    }
    if (IsPreHandled()) {
      // All input dispatch tasks precede this task. No DOM event can trigger
      // the renderer report when pre-handling consumes the sequence.
      CefPostTask(TID_UI, base::BindOnce(&KeyboardInputTestHandler::Report,
                                         this, browser));
    }
  }

  void Report(CefRefPtr<CefBrowser> browser) {
    browser->GetMainFrame()->ExecuteJavaScript("report()", kUrl, 0);
  }

  void MaybeDone() {
    if (!reported_) {
      return;
    }
    const size_t expected_post =
        IsPreHandled() || mode_ == Mode::PageHandled ? 0
        : renderer_handles_keydown_
            ? 1
            : std::count_if(
                  expected_.begin(), expected_.end(),
                  [](const auto& e) { return e.type != CEF_KEYEVENT_CHAR; });
    if (post_ != expected_post) {
      return;
    }
    if (IsWindowTest() && window_events_ != post_unhandled_) {
      return;
    }
    if (mode_ == Mode::LegacyWindowHandled &&
        legacy_window_events_ != window_events_) {
      return;
    }
    // Chromium suppresses related CHAR/up before pre-handling after a
    // consumed raw down. Do not invent a symmetric callback lifecycle.
    EXPECT_EQ(IsPreHandled() ? 1U : expected_.size(), pre_);
    EXPECT_EQ(pre_unhandled_, legacy_pre_);
    EXPECT_EQ(post_unhandled_, legacy_post_);
    EXPECT_EQ(mode_ == Mode::LegacyWindowHandled ? window_events_ : 0U,
              legacy_window_events_);

    DestroyTest();
  }

  bool IsPreHandled() const {
    return mode_ == Mode::PreHandled || mode_ == Mode::LegacyPreHandled ||
           mode_ == Mode::LegacySendPreHandled;
  }

  bool IsLegacySend() const {
    return mode_ == Mode::LegacySend || mode_ == Mode::LegacySendPreHandled;
  }

  void ExpectLegacySnapshot(const CefKeyboardInputEvent& expected,
                            const CefKeyEvent& event) {
    EXPECT_EQ(expected.windows_key_code, event.windows_key_code);
    EXPECT_EQ(expected.is_system_key, event.is_system_key);
    EXPECT_EQ(expected.text[0], event.character);
    EXPECT_EQ(expected.unmodified_text[0], event.unmodified_character);
    EXPECT_EQ(expected.modifiers, static_cast<uint32_t>(event.modifiers));
  }

  bool IsShortcut() const {
    return mode_ == Mode::Shortcut || mode_ == Mode::LegacyShortcut;
  }

  bool IsWindowTest() const {
    return mode_ == Mode::WindowHandled || mode_ == Mode::LegacyWindowHandled;
  }

  const Mode mode_;
  const bool osr_;
  const Scenario scenario_;
#if defined(OS_MAC)
  MacTextClientInput mac_input_ = MacTextClientInput::Character;
#endif
  std::vector<CefKeyboardInputEvent> expected_{3};
  std::string code_ = "KeyA";
  // DOM's legacy character codes contain the first UTF-16 unit.
  int character_code_ = 0xd83d;
  std::string logical_key_, insertion_;
  int location_ = 0;
  bool reported_ = false;
  bool suppress_character_dom_ = false;
  bool renderer_handles_keydown_ = false;
  bool waiting_for_paint_ = false;
  size_t pre_ = 0, post_ = 0, legacy_pre_ = 0, legacy_post_ = 0;
  size_t pre_unhandled_ = 0, post_unhandled_ = 0;
  size_t post_cursor_ = 0;
  size_t window_events_ = 0, legacy_window_events_ = 0;
  IMPLEMENT_REFCOUNTING(KeyboardInputTestHandler);
};

void RunKeyboardTest(Mode mode, Scenario scenario = Scenario::Unicode) {
  for (bool osr : {true, false}) {
    // Native-window legacy injection retains its separate view/IME entry path.
    // This comparison targets OSR, where both APIs share the same dispatcher.
    if (!osr &&
        (mode == Mode::LegacySend || mode == Mode::LegacySendPreHandled)) {
      continue;
    }
    if (osr &&
        (mode == Mode::WindowHandled || mode == Mode::LegacyWindowHandled)) {
      continue;
    }
    SCOPED_TRACE(osr ? "Alloy OSR" : "Native window (global runtime style)");
    CefRefPtr<KeyboardInputTestHandler> handler =
        new KeyboardInputTestHandler(mode, osr, scenario);
    handler->ExecuteTest();
    ReleaseAndWaitForDestructor(handler);
  }
}

TEST(KeyboardInputTest, PortableToWebAndCallbacks) {
  RunKeyboardTest(Mode::Portable);
}

TEST(KeyboardInputTest, ViewsWindowHandlesUnifiedKeyboardInput) {
  RunKeyboardTest(Mode::WindowHandled);
}

TEST(KeyboardInputTest, ViewsWindowLegacyCallbackHandlesUnhandledInput) {
  RunKeyboardTest(Mode::LegacyWindowHandled);
}

// Regression coverage for all four independently reported CEF #2597 failures.
TEST(KeyboardInputTest, OrdinaryCharacterHasStandardLocation) {
  RunKeyboardTest(Mode::Portable, Scenario::Alpha);
}

TEST(KeyboardInputTest, MultipleCharactersPreserveOrderAndCallbacks) {
  RunKeyboardTest(Mode::Portable, Scenario::Sequence);
}

TEST(KeyboardInputTest, LegacySendReachesUnifiedAndLegacyCallbacks) {
  RunKeyboardTest(Mode::LegacySend, Scenario::Alpha);
}

TEST(KeyboardInputTest, UnifiedPreHandlerConsumesLegacySend) {
  RunKeyboardTest(Mode::LegacySendPreHandled, Scenario::Alpha);
}

TEST(KeyboardInputTest, LeftShiftCodeAndLocationOnDownAndUp) {
  RunKeyboardTest(Mode::Portable, Scenario::LeftShift);
}

TEST(KeyboardInputTest, RightShiftCodeAndLocationOnDownAndUp) {
  RunKeyboardTest(Mode::Portable, Scenario::RightShift);
}

TEST(KeyboardInputTest, RepeatedKeydownAndNonRepeatingKeyup) {
  RunKeyboardTest(Mode::Portable, Scenario::RepeatedShift);
}

TEST(KeyboardInputTest, TrueKeypadLocationOnAllKeyboardEvents) {
  RunKeyboardTest(Mode::Portable, Scenario::Keypad);
}

TEST(KeyboardInputTest, DeadKeyIdentityWithoutCharacterInsertion) {
  RunKeyboardTest(Mode::Portable, Scenario::DeadKey);
}

TEST(KeyboardInputTest, UnknownPhysicalCodePreservesLogicalInput) {
  RunKeyboardTest(Mode::Portable, Scenario::UnknownCode);
}

TEST(KeyboardInputTest, ModifierAndLockSnapshotReachesDom) {
  RunKeyboardTest(Mode::Portable, Scenario::Modifiers);
}

TEST(KeyboardInputTest, AltGraphCharacterInsertionAndCallbacks) {
  RunKeyboardTest(Mode::Portable, Scenario::AltGraph);
}

TEST(KeyboardInputTest, WindowsKeyCodesFromSharedDomMappings) {
  const struct {
    uint32_t code;
    uint32_t key;
    int windows;
  } cases[] = {
      {CEF_DOM_CODE_US_A, CEF_DOM_KEY_UNICODE_FLAG | 'q', 'Q'},
      {CEF_DOM_CODE_US_Q, CEF_DOM_KEY_UNICODE_FLAG | 'a', 'A'},
      {CEF_DOM_CODE_US_A, CEF_DOM_KEY_UNICODE_FLAG | 0x0444, 'A'},
      // A non-BMP scalar whose low UTF-16 bits equal 'a' must not map to A.
      {CEF_DOM_CODE_US_Q, CEF_DOM_KEY_UNICODE_FLAG | 0x10061, 'Q'},
      {CEF_DOM_CODE_SHIFT_RIGHT, CEF_DOM_KEY_SHIFT, 0x10},
      {CEF_DOM_CODE_CAPS_LOCK, CEF_DOM_KEY_CONTROL, 0x11},
      {CEF_DOM_CODE_CONTROL_RIGHT, CEF_DOM_KEY_CAPS_LOCK, 0x14},
      {CEF_DOM_CODE_META_RIGHT, CEF_DOM_KEY_META, 0x5c},
      {CEF_DOM_CODE_NUMPAD1, CEF_DOM_KEY_UNICODE_FLAG | '1', 0x61},
      {CEF_DOM_CODE_NUMPAD1, CEF_DOM_KEY_END, 0x23},
      {CEF_DOM_CODE_NUMPAD_ADD, CEF_DOM_KEY_UNICODE_FLAG | '+', 0x6b},
      {CEF_DOM_CODE_QUOTE, CEF_DOM_KEY_DEAD_FLAG | 0x0301, 0xde},
      {CEF_DOM_CODE_NONE, CEF_DOM_KEY_UNICODE_FLAG | '1', '1'},
      {CEF_DOM_CODE_NONE, CEF_DOM_KEY_UNIDENTIFIED, 0},
      {CEF_DOM_CODE_US_A, 'a', 0},
      {0xffffffff, CEF_DOM_KEY_CONTROL, 0},
  };
  for (const auto& item : cases) {
    EXPECT_EQ(item.windows,
              CefWindowsKeyCodeFromDomCodeAndKey(item.code, item.key))
        << "code=" << item.code << ", key=" << item.key;
  }
}

TEST(KeyboardInputTest, PreHandledSuppressesRelatedEvents) {
  RunKeyboardTest(Mode::PreHandled);
}

TEST(KeyboardInputTest, PageHandledHasNoFallbackCallback) {
  RunKeyboardTest(Mode::PageHandled);
}

TEST(KeyboardInputTest, ShortcutSuppressesCharacter) {
  RunKeyboardTest(Mode::Shortcut);
}

TEST(KeyboardInputTest, LegacyPreHandlerConsumesUnhandledInput) {
  RunKeyboardTest(Mode::LegacyPreHandled);
}

TEST(KeyboardInputTest, LegacyPreHandlerClassifiesShortcut) {
  RunKeyboardTest(Mode::LegacyShortcut);
}

TEST(KeyboardInputTest, LegacyPostHandlerConsumesUnhandledInput) {
  RunKeyboardTest(Mode::LegacyPostHandled);
}

#if defined(OS_WIN) || defined(OS_MAC) || defined(CEF_TESTS_USE_GTK)
TEST(KeyboardInputTest, NativeToPortableToWebAndCallbacks) {
  RunKeyboardTest(Mode::Native);
}
#endif

#if defined(OS_MAC)
void RunMacTextClientTest(MacTextClientInput input) {
  for (bool osr : {true, false}) {
    SCOPED_TRACE(osr ? "Alloy OSR" : "Native window (global runtime style)");
    CefRefPtr<KeyboardInputTestHandler> handler = new KeyboardInputTestHandler(
        Mode::NativeTextClient, osr, Scenario::Unicode);
    handler->SetMacTextClientInput(input);
    handler->ExecuteTest();
    ReleaseAndWaitForDestructor(handler);
  }
}

TEST(KeyboardInputTest, CefclientTextClientToWebAndCallbacks) {
  RunKeyboardTest(Mode::NativeTextClient);
}

TEST(KeyboardInputTest, MacTextClientPreservesOptionUnmodifiedText) {
  RunMacTextClientTest(MacTextClientInput::OptionCharacter);
}

TEST(KeyboardInputTest, MacTextClientReturnInsertsNewline) {
  RunMacTextClientTest(MacTextClientInput::Return);
}

TEST(KeyboardInputTest, MacTextClientShiftReturnInsertsNewline) {
  RunMacTextClientTest(MacTextClientInput::ShiftReturn);
}

TEST(KeyboardInputTest, MacTextClientControlCharacter) {
  RunMacTextClientTest(MacTextClientInput::ControlCharacter);
}

TEST(KeyboardInputTest, MacTextClientCommandCharacter) {
  RunMacTextClientTest(MacTextClientInput::CommandCharacter);
}

TEST(KeyboardInputTest, MacTextClientNonInsertionCommandHasNoCharacter) {
  RunMacTextClientTest(MacTextClientInput::NonInsertionCommand);
}

TEST(KeyboardInputTest, MacTextClientClearHasNoCharacter) {
  RunMacTextClientTest(MacTextClientInput::Clear);
}
#endif

}  // namespace
#endif  // CEF_API_ADDED(CEF_NEXT)
