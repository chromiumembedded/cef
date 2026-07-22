// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

// Tests that CefBrowserHost::SendKeyEvent correctly populates the DOM
// keyboard event properties (event.code, event.location, event.repeat) when
// received by JavaScript in the renderer.

#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const char kTestUrl[] = "https://tests/keyboard_dom_code_test.html";

// The HTML page installs keydown/keyup/keypress listeners and reports each
// event back via window.testQuery as "type|code|location|repeat".
//
// event.location values:
//   0 = DOM_KEY_LOCATION_STANDARD
//   1 = DOM_KEY_LOCATION_LEFT
//   2 = DOM_KEY_LOCATION_RIGHT
//   3 = DOM_KEY_LOCATION_NUMPAD
const char kTestHtml[] =
    "<html><head><script>\n"
    "function sendEvent(e) {\n"
    "  var msg = e.type+'|'+e.code+'|'+e.location+'|'+e.repeat;\n"
    "  window.testQuery({request: msg});\n"
    "}\n"
    "document.addEventListener('keydown',  sendEvent);\n"
    "document.addEventListener('keyup',    sendEvent);\n"
    "document.addEventListener('keypress', sendEvent);\n"
    "</script></head><body>KeyboardDomCodeTest</body></html>\n";

// ---------------------------------------------------------------------------
// Helper: build a Windows lParam for a key event.
//
//  Bit layout (from MSDN WM_KEYDOWN/WM_KEYUP):
//   0-15  repeat count (always 1 here)
//   16-23 scan code
//   24    extended-key flag
//   25-28 reserved
//   29    context code (0 for WM_KEYDOWN/UP)
//   30    previous key-state (0 = was up, 1 = was down -> auto-repeat)
//   31    transition state (0 = key being pressed, 1 = key being released)
// ---------------------------------------------------------------------------
#if defined(OS_WIN)
static int MakeLParam(UINT scanCode, bool extended, bool prevDown,
                      bool releasing) {
  // Build a DWORD with the Windows lParam bit layout, then cast to int
  // (which is how CEF's native_key_code is typed).
  DWORD lp = 1u;                              // repeat count
  lp |= (static_cast<DWORD>(scanCode) << 16);
  if (extended) {
    lp |= (1u << 24);
  }
  if (prevDown) {
    lp |= (1u << 30);  // "previous key state" -> auto-repeat
  }
  if (releasing) {
    lp |= (1u << 31);  // transition state -> key up
  }
  return static_cast<int>(lp);
}

// Send RAWKEYDOWN + CHAR + KEYUP for a given virtual key / scan code,
// mirroring the three-message sequence produced by a real WM_KEY* message.
// |repeat| sets lParam bit 30 on the RAWKEYDOWN event.
static void SendKeyDownCharKeyUp(CefRefPtr<CefBrowser> browser,
                              UINT vk,
                              UINT scanCode,
                              char16_t character,
                              bool repeat) {
  CefKeyEvent event;
  event.is_system_key = false;
  event.modifiers = 0;
  event.windows_key_code = static_cast<int>(vk);

  // --- KEYEVENT_RAWKEYDOWN ---
  event.native_key_code = MakeLParam(scanCode, /*extended=*/false,
                                     /*prevDown=*/repeat,
                                     /*releasing=*/false);
  event.type = KEYEVENT_RAWKEYDOWN;
  browser->GetHost()->SendKeyEvent(event);

  // --- KEYEVENT_CHAR ---
  // For WM_CHAR: windows_key_code is the character value, lParam is the
  // same as for WM_KEYDOWN.  The character value must NOT be interpreted as
  // a VK code for modifier purposes
  event.windows_key_code = static_cast<int>(character);
  event.character = character;
  event.unmodified_character = character;
  event.native_key_code = MakeLParam(scanCode, /*extended=*/false,
                                     /*prevDown=*/repeat,
                                     /*releasing=*/false);
  event.type = KEYEVENT_CHAR;
  browser->GetHost()->SendKeyEvent(event);

  // --- KEYEVENT_KEYUP ---
  // For WM_KEYUP bits 30 and 31 are always 1.
  event.windows_key_code = static_cast<int>(vk);
  event.character = 0;
  event.unmodified_character = 0;
  event.native_key_code = MakeLParam(scanCode, /*extended=*/false,
                                     /*prevDown=*/true,
                                     /*releasing=*/true);
  event.type = KEYEVENT_KEYUP;
  browser->GetHost()->SendKeyEvent(event);
}
#endif  // OS_WIN

// ---------------------------------------------------------------------------
// Test handler
// ---------------------------------------------------------------------------

// Stages of the test state machine.
// The sequence mirrors what a real key press produces:
//   keydown -> keypress (from KEYEVENT_CHAR) -> keyup
// followed by the same sequence with the repeat flag set on the keydown.
enum KeyDomCodeTestStage {
  // --- first key press (non-repeat) ---
  STAGE_KEYDOWN_A,          // RAWKEYDOWN -> DOM keydown
  STAGE_KEYPRESS_A,         // CHAR      -> DOM keypress (location must be 0)
  STAGE_KEYUP_A,            // KEYUP     -> DOM keyup
  // --- second key press (auto-repeat) ---
  STAGE_KEYDOWN_A_REPEAT,   // RAWKEYDOWN with bit30 -> DOM keydown repeat=true
  STAGE_KEYPRESS_A_REPEAT,  // CHAR -> DOM keypress repeat=true, location=0
  STAGE_KEYUP_A_REPEAT,     // KEYUP -> DOM keyup
  // --- done ---
  STAGE_DONE,
};

class KeyboardDomCodeTestHandler : public RoutingTestHandler,
                                    public CefRenderHandler {
 public:
  KeyboardDomCodeTestHandler() = default;

  KeyboardDomCodeTestHandler(const KeyboardDomCodeTestHandler&) = delete;
  KeyboardDomCodeTestHandler& operator=(const KeyboardDomCodeTestHandler&) =
      delete;

  // CefClient override: return this as the render handler for OSR.
  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

  // CefRenderHandler methods:
  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
    rect = CefRect(0, 0, 800, 600);
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) override {
    if (!got_paint_) {
      got_paint_.yes();
      CefPostDelayedTask(
          TID_UI,
          base::BindOnce(&KeyboardDomCodeTestHandler::SendNextKey, this,
                         browser),
          100);
    }
  }

  void RunTest() override {
    AddResource(kTestUrl, kTestHtml, "text/html");

    // Create an OSR browser
    CefWindowInfo windowInfo;
#if defined(OS_WIN)
    windowInfo.SetAsWindowless(GetDesktopWindow());
#else
    windowInfo.SetAsWindowless(kNullWindowHandle);
#endif
    CefBrowserSettings settings;
    CefBrowserHost::CreateBrowser(windowInfo, this, kTestUrl, settings, nullptr,
                                  nullptr);

    SetTestTimeout();
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    callback->Success("");

    const std::string msg = request.ToString();
    HandleKeyEvent(browser, msg);
    return true;
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_paint_);
    EXPECT_EQ(STAGE_DONE, stage_);

    RoutingTestHandler::DestroyTest();
  }

 private:
#if defined(OS_WIN)
  // 'A' key: VK code, scan code, and character on a standard US keyboard.
  static constexpr UINT kVkA = 'A';
  static constexpr UINT kScanA = 0x1E;
  static constexpr char16_t kCharA = u'a';  // lowercase; no shift modifier
#endif

  // Parse "type|code|location|repeat" and advance the state machine.
  void HandleKeyEvent(CefRefPtr<CefBrowser> browser, const std::string& msg) {
    auto Split = [](const std::string& s,
                    char delim) -> std::vector<std::string> {
      std::vector<std::string> parts;
      std::string cur;
      for (char c : s) {
        if (c == delim) {
          parts.push_back(cur);
          cur.clear();
        } else {
          cur += c;
        }
      }
      parts.push_back(cur);
      return parts;
    };

    const auto parts = Split(msg, '|');
    if (parts.size() != 4) {
      ADD_FAILURE() << "Unexpected message format: " << msg;
      return;
    }

    const std::string& event_type = parts[0];
    const std::string& code       = parts[1];
    const int location            = std::stoi(parts[2]);
    const bool is_repeat          = (parts[3] == "true");

    switch (stage_) {
      case STAGE_KEYDOWN_A:
        EXPECT_EQ("keydown", event_type) << "STAGE_KEYDOWN_A: wrong type";
        // scan-code extraction gives the correct physical key code.
        EXPECT_EQ("KeyA", code)
            << "STAGE_KEYDOWN_A: wrong event.code (scan-code fix)";
        EXPECT_EQ(0, location) << "STAGE_KEYDOWN_A: wrong event.location";
        EXPECT_FALSE(is_repeat) << "STAGE_KEYDOWN_A: event.repeat should be false";
        stage_ = STAGE_KEYPRESS_A;
        break;

      case STAGE_KEYPRESS_A:
        EXPECT_EQ("keypress", event_type) << "STAGE_KEYPRESS_A: wrong type";
        EXPECT_EQ("KeyA", code) << "STAGE_KEYPRESS_A: wrong event.code";
        // character value must not be treated as a VK code.
        EXPECT_EQ(0, location)
            << "STAGE_KEYPRESS_A: event.location should be 0 (standard), "
               "not 3 (numpad) - char modifier fix (#2597 case 4)";
        EXPECT_FALSE(is_repeat) << "STAGE_KEYPRESS_A: event.repeat should be false";
        stage_ = STAGE_KEYUP_A;
        break;

      case STAGE_KEYUP_A:
        EXPECT_EQ("keyup", event_type) << "STAGE_KEYUP_A: wrong type";
        EXPECT_EQ("KeyA", code) << "STAGE_KEYUP_A: wrong event.code";
        EXPECT_EQ(0, location) << "STAGE_KEYUP_A: wrong event.location";
        EXPECT_FALSE(is_repeat) << "STAGE_KEYUP_A: event.repeat should be false";
        stage_ = STAGE_KEYDOWN_A_REPEAT;
        CefPostDelayedTask(
            TID_UI,
            base::BindOnce(&KeyboardDomCodeTestHandler::SendRepeatKey, this,
                           browser),
            50);
        break;

      case STAGE_KEYDOWN_A_REPEAT:
        EXPECT_EQ("keydown", event_type) << "STAGE_KEYDOWN_A_REPEAT: wrong type";
        EXPECT_EQ("KeyA", code) << "STAGE_KEYDOWN_A_REPEAT: wrong event.code";
        EXPECT_EQ(0, location) << "STAGE_KEYDOWN_A_REPEAT: wrong event.location";
        // lParam bit 30 -> EF_IS_REPEAT -> event.repeat=true.
        EXPECT_TRUE(is_repeat)
            << "STAGE_KEYDOWN_A_REPEAT: event.repeat should be true "
               "(lParam bit-30 repeat fix)";
        stage_ = STAGE_KEYPRESS_A_REPEAT;
        break;

      case STAGE_KEYPRESS_A_REPEAT:
        EXPECT_EQ("keypress", event_type)
            << "STAGE_KEYPRESS_A_REPEAT: wrong type";
        EXPECT_EQ("KeyA", code)
            << "STAGE_KEYPRESS_A_REPEAT: wrong event.code";
        EXPECT_EQ(0, location)
            << "STAGE_KEYPRESS_A_REPEAT: event.location should be 0 (standard)";
        // Note: keypress.repeat reflects the keydown repeat state, but
        // Chromium does not propagate EF_IS_REPEAT through KEYEVENT_CHAR to
        // the keypress event, so we do not assert repeat here.
        stage_ = STAGE_KEYUP_A_REPEAT;
        break;

      case STAGE_KEYUP_A_REPEAT:
        EXPECT_EQ("keyup", event_type) << "STAGE_KEYUP_A_REPEAT: wrong type";
        EXPECT_EQ("KeyA", code) << "STAGE_KEYUP_A_REPEAT: wrong event.code";
        EXPECT_EQ(0, location) << "STAGE_KEYUP_A_REPEAT: wrong event.location";
        EXPECT_FALSE(is_repeat)
            << "STAGE_KEYUP_A_REPEAT: event.repeat should be false for keyup";
        stage_ = STAGE_DONE;
        DestroyTest();
        break;

      case STAGE_DONE:
        // Ignore any trailing events.
        break;
    }
  }

  void SendNextKey(CefRefPtr<CefBrowser> browser) {
#if defined(OS_WIN)
    // Send RAWKEYDOWN + CHAR + KEYUP for 'A' (non-repeat).
    SendKeyDownCharKeyUp(browser, kVkA, kScanA, kCharA, /*repeat=*/false);
#else
    // The scan-code and char-modifier fixes are Windows-specific.
    (void)browser;
    stage_ = STAGE_DONE;
    DestroyTest();
#endif
  }

  void SendRepeatKey(CefRefPtr<CefBrowser> browser) {
#if defined(OS_WIN)
    // Send RAWKEYDOWN + CHAR + KEYUP for 'A' (auto-repeat).
    SendKeyDownCharKeyUp(browser, kVkA, kScanA, kCharA, /*repeat=*/true);
#else
    (void)browser;
#endif
  }

  TrackCallback got_paint_;
  KeyDomCodeTestStage stage_ = STAGE_KEYDOWN_A;

  IMPLEMENT_REFCOUNTING(KeyboardDomCodeTestHandler);
};

}  // namespace

// ---------------------------------------------------------------------------
// Test entry point
// ---------------------------------------------------------------------------

// Verifies correct DOM codes for Windows OSR key events:
//   1. event.code is correctly derived from the scan-code field of
//      native_key_code (lParam) via GetScanCodeFromLParam().
//   2. event.repeat is true when lParam bit 30 ("previous key state") is set.
//   3. event.location is 0 (standard) for a KEYEVENT_CHAR carrying a regular
//      character value; it must not be misidentified as numpad (location=3)
//      due to the character value coincidentally matching a numpad VK code
//      in GetCefKeyboardModifiersFromKeyEvent().
TEST(KeyboardDomCodeTest, DomCodeAndRepeatFlags) {
  CefRefPtr<KeyboardDomCodeTestHandler> handler =
      new KeyboardDomCodeTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
