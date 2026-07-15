// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

// Tests that CefBrowserHost::SendKeyEvent correctly populates the DOM
// keyboard event properties (event.code, event.key, event.repeat) when
// received by JavaScript in the renderer.
//

#include "include/cef_render_handler.h"
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

// The HTML page installs keydown/keyup listeners on the document and reports
// each event back via window.testQuery (the RoutingTestHandler query function)
// so the C++ side can verify the values.
// We report: eventType|code|key|repeat
const char kTestHtml[] =
    "<html><head><script>\n"
    "function sendEvent(e) {\n"
    "  var msg = e.type + '|' + e.code + '|' + e.key + '|' + e.repeat;\n"
    "  window.testQuery({request: msg});\n"
    "}\n"
    "document.addEventListener('keydown', sendEvent);\n"
    "document.addEventListener('keyup',   sendEvent);\n"
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

// Send a single key-down followed by a key-up for a given virtual key.
// |repeat| controls whether the key-down is flagged as an auto-repeat.
static void SendKeyDownUp(CefRefPtr<CefBrowser> browser,
                          UINT vk,
                          UINT scanCode,
                          bool repeat) {
  CefKeyEvent event;
  event.is_system_key = false;
  event.windows_key_code = vk;

  // --- keydown (RAWKEYDOWN) ---
  event.native_key_code = MakeLParam(scanCode, /*extended=*/false,
                                     /*prevDown=*/repeat,
                                     /*releasing=*/false);
  event.modifiers = 0;
  event.type = KEYEVENT_RAWKEYDOWN;
  browser->GetHost()->SendKeyEvent(event);

  // --- keyup ---
  // For WM_KEYUP bits 30 and 31 are always 1.
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

// What we are currently waiting for.
enum KeyDomCodeTestStage {
  // Waiting for: keydown for 'A' (not repeat)
  STAGE_KEYDOWN_A,
  // Waiting for: keyup for 'A'
  STAGE_KEYUP_A,
  // Waiting for: keydown for 'A' with repeat=true
  STAGE_KEYDOWN_A_REPEAT,
  // Waiting for: keyup after the repeated keydown
  STAGE_KEYUP_A_REPEAT,
  // All stages done.
  STAGE_DONE,
};

// Minimal CefRenderHandler required for OSR browser creation.
class KeyboardTestRenderHandler : public CefRenderHandler {
 public:
  KeyboardTestRenderHandler() = default;

  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
    rect = CefRect(0, 0, 800, 600);
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) override {}

  IMPLEMENT_REFCOUNTING(KeyboardTestRenderHandler);
};

class KeyboardDomCodeTestHandler : public RoutingTestHandler {
 public:
  KeyboardDomCodeTestHandler() = default;

  KeyboardDomCodeTestHandler(const KeyboardDomCodeTestHandler&) = delete;
  KeyboardDomCodeTestHandler& operator=(const KeyboardDomCodeTestHandler&) =
      delete;

  // CefClient override: return our render handler for OSR.
  CefRefPtr<CefRenderHandler> GetRenderHandler() override {
    return render_handler_;
  }

  void RunTest() override {
    AddResource(kTestUrl, kTestHtml, "text/html");

    // Create an OSR (windowless) browser so SendKeyEvent delivers events to
    // the renderer without requiring real OS window focus.
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

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (!frame->IsMain()) {
      return;
    }
    EXPECT_EQ(200, httpStatusCode);

    got_load_end_.yes();

    // Give the renderer a moment to register the event listeners before
    // sending key events.
    CefPostDelayedTask(
        TID_UI,
        base::BindOnce(&KeyboardDomCodeTestHandler::SendNextKey, this, browser),
        100);
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
    EXPECT_TRUE(got_load_end_);
    EXPECT_EQ(STAGE_DONE, stage_);

    RoutingTestHandler::DestroyTest();
  }

 private:
#if defined(OS_WIN)
  // 'A' key: VK code and scan code on a standard keyboard.
  static constexpr UINT kVkA = 'A';
  static constexpr UINT kScanA = 0x1E;  // scan code for 'A'
#endif

  // Parse the message string "eventType|code|key|repeat" and advance the
  // test state machine.
  void HandleKeyEvent(CefRefPtr<CefBrowser> browser, const std::string& msg) {
    // Parse the four pipe-separated fields.
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
    // parts[2] is event.key (keyboard-layout-dependent, not asserted here).
    const std::string& repeat_str = parts[3];
    const bool is_repeat = (repeat_str == "true");

    switch (stage_) {
      case STAGE_KEYDOWN_A:
        EXPECT_EQ("keydown", event_type)
            << "Stage KEYDOWN_A: unexpected event type";
        // event.code must reflect the physical key ('A' position).
        // This is what PR#336 fixed: the scan-code portion of native_key_code
        // is now correctly extracted via GetScanCodeFromLParam().
        EXPECT_EQ("KeyA", code)
            << "Stage KEYDOWN_A: wrong event.code (PR#336 scan-code fix)";
        // First keydown should NOT be a repeat.
        EXPECT_FALSE(is_repeat)
            << "Stage KEYDOWN_A: event.repeat should be false";
        stage_ = STAGE_KEYUP_A;
        break;

      case STAGE_KEYUP_A:
        EXPECT_EQ("keyup", event_type)
            << "Stage KEYUP_A: unexpected event type";
        EXPECT_EQ("KeyA", code)
            << "Stage KEYUP_A: wrong event.code";
        // keyup events never have repeat=true.
        EXPECT_FALSE(is_repeat)
            << "Stage KEYUP_A: event.repeat should be false for keyup";
        stage_ = STAGE_KEYDOWN_A_REPEAT;
        // Send the repeat keydown+keyup pair.
        CefPostDelayedTask(
            TID_UI,
            base::BindOnce(&KeyboardDomCodeTestHandler::SendRepeatKey, this,
                           browser),
            50);
        break;

      case STAGE_KEYDOWN_A_REPEAT:
        EXPECT_EQ("keydown", event_type)
            << "Stage KEYDOWN_A_REPEAT: unexpected event type";
        EXPECT_EQ("KeyA", code)
            << "Stage KEYDOWN_A_REPEAT: wrong event.code";
        // This keydown was flagged as auto-repeat via lParam bit 30.
        EXPECT_TRUE(is_repeat)
            << "Stage KEYDOWN_A_REPEAT: event.repeat should be true "
               "(PR#336 repeat-bit fix)";
        stage_ = STAGE_KEYUP_A_REPEAT;
        break;

      case STAGE_KEYUP_A_REPEAT:
        EXPECT_EQ("keyup", event_type)
            << "Stage KEYUP_A_REPEAT: unexpected event type";
        EXPECT_EQ("KeyA", code)
            << "Stage KEYUP_A_REPEAT: wrong event.code";
        EXPECT_FALSE(is_repeat)
            << "Stage KEYUP_A_REPEAT: event.repeat should be false for keyup";
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
    // Send a normal (non-repeat) keydown + keyup for 'A'.
    SendKeyDownUp(browser, kVkA, kScanA, /*repeat=*/false);
#else
    // On non-Windows platforms the native_key_code has a different layout
    // and the scan-code fix is Windows-specific. Skip cleanly.
    (void)browser;
    stage_ = STAGE_DONE;
    DestroyTest();
#endif
  }

  void SendRepeatKey(CefRefPtr<CefBrowser> browser) {
#if defined(OS_WIN)
    // Send a repeated (auto-repeat) keydown + keyup for 'A'.
    SendKeyDownUp(browser, kVkA, kScanA, /*repeat=*/true);
#else
    (void)browser;
#endif
  }

  CefRefPtr<KeyboardTestRenderHandler> render_handler_ =
      new KeyboardTestRenderHandler();
  TrackCallback got_load_end_;
  KeyDomCodeTestStage stage_ = STAGE_KEYDOWN_A;

  IMPLEMENT_REFCOUNTING(KeyboardDomCodeTestHandler);
};

}  // namespace

// ---------------------------------------------------------------------------
// Test entry point
// ---------------------------------------------------------------------------

// Verifies that:
//   1. event.code is correctly derived from the scan-code portion of
//      native_key_code 
//   2. event.repeat is true for a keydown whose lParam bit 30 is set
//   3. event.repeat is always false for keyup events.
TEST(KeyboardDomCodeTest, DomCodeAndRepeatFlags) {
  CefRefPtr<KeyboardDomCodeTestHandler> handler =
      new KeyboardDomCodeTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
