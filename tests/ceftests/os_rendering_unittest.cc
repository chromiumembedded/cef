// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/base/cef_logging.h"
#include "include/cef_parser.h"
#include "include/cef_v8.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/geometry_util.h"
#include "tests/shared/browser/resource_util.h"

#if defined(OS_MAC)
#include <Carbon/Carbon.h>  // For character codes.

#include "tests/ceftests/os_rendering_unittest_mac.h"
#elif defined(OS_LINUX)
#include <X11/keysym.h>
#elif defined(OS_WIN)
// Required for resource_util_win, which uses this as an extern
HINSTANCE hInst = ::GetModuleHandle(nullptr);
#endif

// Set to 1 to enable verbose debugging info logging.
#define VERBOSE_DEBUGGING 0

namespace {

const char kTestUrl[] = "https://tests/osrtest";

// Must be both large enough for the drag/drop region to be visible and small
// enough for a little vertical offset with scrollbar.
const int kOsrWidth = 600;
const int kOsrHeight = 450;

// bounding client rects for edit box and navigate button
#if defined(OS_WIN)
const CefRect kExpandedSelectRect(462, 42, 81, 334);
#elif defined(OS_MAC)
const CefRect kExpandedSelectRect(462, 42, 75, 334);
#elif defined(OS_LINUX)
const CefRect kExpandedSelectRect(462, 42, 79, 334);
#else
#error "Unsupported platform"
#endif  // defined(OS_WIN)

// word to be written into edit box
const char kKeyTestWord[] = "done";

constexpr uint32_t kAllTouchHandleFlags =
    (CEF_THS_FLAG_ENABLED | CEF_THS_FLAG_ORIENTATION | CEF_THS_FLAG_ORIGIN |
     CEF_THS_FLAG_ALPHA);

#if defined(OS_LINUX)

// From ui/events/keycodes/keyboard_codes_posix.h
#define VKEY_D 0x44
#define VKEY_O 0x4F
#define VKEY_N 0x4E
#define VKEY_E 0x45
#define VKEY_ESCAPE 0x1B
#define VKEY_TAB 0x09

const unsigned int kNativeKeyTestCodes[] = {XK_d, XK_o, XK_n, XK_e};

const unsigned int kNativeKeyEscape = XK_Escape;
const unsigned int kNativeKeyTab = XK_Tab;

#elif defined(OS_MAC)

// See kKeyCodesMap in ui/events/keycodes/keyboard_code_conversion_mac.mm
#define VKEY_D 'd'
#define VKEY_O 'o'
#define VKEY_N 'n'
#define VKEY_E 'e'
#define VKEY_ESCAPE kEscapeCharCode
#define VKEY_TAB kTabCharCode

const unsigned int kNativeKeyTestCodes[] = {kVK_ANSI_D, kVK_ANSI_O, kVK_ANSI_N,
                                            kVK_ANSI_E};

const unsigned int kNativeKeyEscape = kVK_Escape;
const unsigned int kNativeKeyTab = kVK_Tab;

#endif

#if defined(OS_MAC) || defined(OS_LINUX)

const unsigned int kKeyTestCodes[] = {VKEY_D, VKEY_O, VKEY_N, VKEY_E};

#endif

// test type
enum OSRTestType {
  // IsWindowRenderingDisabled should be true
  OSR_TEST_IS_WINDOWLESS,
  // focusing webview, LI00 will get red & repainted
  OSR_TEST_FOCUS,
  // tab key traversal should iterate the focus across HTML element and
  // subsequently after last element CefFocusHandler::OnTakeFocus should be
  // called to allow giving focus to the next component.
  OSR_TEST_TAKE_FOCUS,
  // send focus event should set focus on the webview
  OSR_TEST_GOT_FOCUS,
  // loading webview should trigger a full paint (L01)
  OSR_TEST_PAINT,
  // same as OSR_TEST_PAINT but with alpha values
  OSR_TEST_TRANSPARENCY,
  // moving mouse over L02, OnCursorChange will be called
  OSR_TEST_CURSOR,
  // moving mouse on L03, OnPaint will be called for its bounding rectangle
  OSR_TEST_MOUSE_MOVE,
  // right clicking an element (L04), OnBeforeContextMenu should be called
  OSR_TEST_CLICK_RIGHT,
  // right clicking an element (L04), context menu will query screen point
  OSR_TEST_SCREEN_POINT,
  // left click in text box should query repainting edit box area
  OSR_TEST_CLICK_LEFT,
  // Resize should trigger a full repaint with the new given size
  OSR_TEST_RESIZE,
  // Invalidate should trigger repaint synchronously
  OSR_TEST_INVALIDATE,
  // write into editbox LI08, click to navigate on LI09
  OSR_TEST_KEY_EVENTS,
  // mouse over LI10 will show a tooltip
  OSR_TEST_TOOLTIP,
  // mouse wheel will trigger a scroll event
  OSR_TEST_SCROLLING,
  // Right click will trigger a context menu, and on destroying the test, it
  // should not crash
  OSR_TEST_CONTEXT_MENU,
  // In certain scenarios, the quick menu should be shown instead of the context
  // menu
  OSR_TEST_QUICK_MENU,
  // clicking on dropdown box, PET_POPUP OnPaint is triggered
  OSR_TEST_POPUP_PAINT,
  // clicking on dropdown box, a popup will show up
  OSR_TEST_POPUP_SHOW,
  // clicking on dropdown box, OnPopupSize should be called
  OSR_TEST_POPUP_SIZE,
  // taking focus away from the webview, will close popup
  OSR_TEST_POPUP_HIDE_ON_BLUR,
  // clicking outside the popup widget will close popup
  OSR_TEST_POPUP_HIDE_ON_CLICK,
  // scrolling outside the popup widget will close popup
  OSR_TEST_POPUP_HIDE_ON_SCROLL,
  // pressing ESC will close popup
  OSR_TEST_POPUP_HIDE_ON_ESC,
  // scrolling inside the popup should trigger repaint for popup area
  OSR_TEST_POPUP_SCROLL_INSIDE,
  // clicking and moving the mouse will call StartDragging
  OSR_TEST_DRAG_DROP_START_DRAGGING,
  // starting dragging over the drop region will call UpdateDragCursor
  OSR_TEST_DRAG_DROP_UPDATE_CURSOR,
  // dropping element inside drop region will move the element
  OSR_TEST_DRAG_DROP_DROP,
  // IMESetComposition will update the composition range
  OSR_TEST_IME_SET_COMPOSITION,
  // IMECommitText inserts the specified text
  OSR_TEST_IME_COMMIT_TEXT,
  // IMEFinishComposition will commit the text present composition text
  OSR_TEST_IME_FINISH_COMPOSITION,
  // IMECancelComposition will update the composition range
  OSR_TEST_IME_CANCEL_COMPOSITION,
  // text selection range changed
  OSR_TEST_TEXT_SELECTION_CHANGE,
  // clicking on text input will call OnVirtualKeyboardRequested
  OSR_TEST_VIRTUAL_KEYBOARD,
  // touchStart event is triggered and contains the touch point count
  OSR_TEST_TOUCH_START,
  // touchMove event is triggered and contains the changed touch points
  OSR_TEST_TOUCH_MOVE,
  // touchEnd is triggered on completion
  OSR_TEST_TOUCH_END,
  // touchCancel is triggered on dismissing
  OSR_TEST_TOUCH_CANCEL,
  // CEF_POINTER_TYPE_PEN is mapped to pen pointer event
  OSR_TEST_PEN,
  // Define the range for popup tests.
  OSR_TEST_POPUP_FIRST = OSR_TEST_POPUP_PAINT,
  OSR_TEST_POPUP_LAST = OSR_TEST_POPUP_SCROLL_INSIDE,
};

// Used in the browser process.
class OSRTestHandler : public RoutingTestHandler,
                       public CefFocusHandler,
                       public CefRenderHandler,
                       public CefContextMenuHandler {
 public:
  OSRTestHandler(OSRTestType test_type, float scale_factor)
      : test_type_(test_type), scale_factor_(scale_factor) {}

  // TestHandler methods
  void RunTest() override {
    CreateOSRBrowser(kTestUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    if (test_type_ == OSR_TEST_IS_WINDOWLESS) {
      EXPECT_TRUE(browser->GetHost()->IsWindowRenderingDisabled());
      DestroySucceededTestSoon();
    }
    RoutingTestHandler::OnAfterCreated(browser);
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "OnLoadStart started=" << started()
              << " finished=" << finished();
#endif

    // Only interested in the 2nd+ load.
    if (!started()) {
      return;
    }

    switch (test_type_) {
      case OSR_TEST_KEY_EVENTS: {
        const std::string& expected_url =
            std::string(kTestUrl) + "?k=" + kKeyTestWord;
        EXPECT_STREQ(expected_url.c_str(), frame->GetURL().ToString().c_str());
        DestroySucceededTestSoon();
      } break;
      case OSR_TEST_IME_COMMIT_TEXT: {
        const std::string& expected_url =
            std::string(kTestUrl) + "?k=osrimecommit";
        EXPECT_STREQ(expected_url.c_str(), frame->GetURL().ToString().c_str());
        DestroySucceededTestSoon();
      } break;
      case OSR_TEST_IME_FINISH_COMPOSITION: {
        const std::string& expected_url =
            std::string(kTestUrl) + "?k=" + kKeyTestWord;
        EXPECT_STREQ(expected_url.c_str(), frame->GetURL().ToString().c_str());
        DestroySucceededTestSoon();
      } break;
      case OSR_TEST_IME_CANCEL_COMPOSITION: {
        const std::string& expected_url = std::string(kTestUrl) + "?k=";
        EXPECT_STREQ(expected_url.c_str(), frame->GetURL().ToString().c_str());
        DestroySucceededTestSoon();
      } break;
      default:
        // Intentionally left blank
        break;
    }
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "OnLoadEnd started=" << started()
              << " finished=" << finished();
#endif

    // Only interested in the first load.
    if (started()) {
      return;
    }

    if (test_type_ >= OSR_TEST_POPUP_FIRST &&
        test_type_ <= OSR_TEST_POPUP_LAST) {
      StartTest();
      ExpandDropDown();
      return;
    }

    switch (test_type_) {
      case OSR_TEST_TOOLTIP:
        if (StartTest()) {
          CefMouseEvent mouse_event;
          const CefRect& expected_rect = GetElementBounds("LI10");
          mouse_event.x = MiddleX(expected_rect);
          mouse_event.y = MiddleY(expected_rect);
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
        }
        break;
      case OSR_TEST_FOCUS:
        if (StartTest()) {
          // body.onfocus will make LI00 red
          browser->GetHost()->SetFocus(true);
        }
        break;
      case OSR_TEST_TAKE_FOCUS:
        if (StartTest()) {
          // Give focus to the last input element.
          browser->GetMainFrame()->ExecuteJavaScript(
              "document.getElementById('email').focus()",
              browser->GetMainFrame()->GetURL(), 0);

          // Tab traversal across HTML element.
#if defined(OS_WIN)
          CefPostDelayedTask(TID_UI,
                             base::BindOnce(&OSRTestHandler::SendKeyEvent, this,
                                            browser, VK_TAB),
                             50);
#elif defined(OS_MAC) || defined(OS_LINUX)
          CefPostDelayedTask(TID_UI,
                             base::BindOnce(&OSRTestHandler::SendKeyEvent, this,
                                            browser, kNativeKeyTab, VKEY_TAB),
                             50);
#else
#error "Unsupported platform"
#endif
        }
        break;
      case OSR_TEST_GOT_FOCUS:
        if (StartTest()) {
          browser->GetHost()->SetFocus(true);
        }
        break;
      case OSR_TEST_CURSOR:
        if (StartTest()) {
          // enter mouse in the LI2 element having hand cursor
          CefMouseEvent mouse_event;
          const CefRect& expected_rect = GetElementBounds("LI02");
          mouse_event.x = MiddleX(expected_rect);
          mouse_event.y = MiddleY(expected_rect);
          browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
        }
        break;
      case OSR_TEST_MOUSE_MOVE:
        if (StartTest()) {
          CefMouseEvent mouse_event;
          const CefRect& expected_rect = GetElementBounds("LI03");
          mouse_event.x = MiddleX(expected_rect);
          mouse_event.y = MiddleY(expected_rect);
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
        }
        break;
      case OSR_TEST_CLICK_RIGHT:
      case OSR_TEST_SCREEN_POINT:
      case OSR_TEST_CONTEXT_MENU:
        if (StartTest()) {
          CefMouseEvent mouse_event;
          const CefRect& expected_rect = GetElementBounds("LI04");
          mouse_event.x = MiddleX(expected_rect);
          mouse_event.y = MiddleY(expected_rect);
          mouse_event.modifiers = 0;
          SendMouseClickEvent(browser, mouse_event, MBT_RIGHT);
        }
        break;
      case OSR_TEST_QUICK_MENU:
        if (StartTest()) {
          CefTouchEvent touch_event_pressed;
          touch_event_pressed.type = CEF_TET_PRESSED;
          const CefRect& expected_rect = GetElementBounds("quickmenu");
          touch_event_pressed.x = MiddleX(expected_rect);
          touch_event_pressed.y = MiddleY(expected_rect);
          browser->GetHost()->SendTouchEvent(touch_event_pressed);
        }
        break;
      case OSR_TEST_CLICK_LEFT:
        if (StartTest()) {
          CefMouseEvent mouse_event;
          const CefRect& expected_rect = GetElementBounds("LI00");
          mouse_event.x = MiddleX(expected_rect);
          mouse_event.y = MiddleY(expected_rect);
          mouse_event.modifiers = 0;
          SendMouseClickEvent(browser, mouse_event);
        }
        break;
      case OSR_TEST_DRAG_DROP_START_DRAGGING:
      case OSR_TEST_DRAG_DROP_UPDATE_CURSOR:
      case OSR_TEST_DRAG_DROP_DROP: {
        // trigger the StartDragging event
        if (StartTest()) {
          // move the mouse over the element to drag
          CefMouseEvent mouse_event;
          const CefRect& dragdiv = GetElementBounds("dragdiv");
          mouse_event.x = MiddleX(dragdiv);
          mouse_event.y = MiddleY(dragdiv);
          mouse_event.modifiers = 0;

          // The div drag point must be visible.
          EXPECT_LT(mouse_event.y, kOsrHeight);

          browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
          // click on the element to drag
          mouse_event.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
          CefPostDelayedTask(
              TID_UI,
              base::BindOnce(&CefBrowserHost::SendMouseClickEvent,
                             browser->GetHost(), mouse_event, MBT_LEFT, false,
                             1),
              50);
          // move the mouse to start dragging
          mouse_event.x -= 5;
          mouse_event.y -= 5;
          CefPostDelayedTask(
              TID_UI,
              base::BindOnce(&CefBrowserHost::SendMouseMoveEvent,
                             browser->GetHost(), mouse_event, false),
              100);
        }
      } break;
      case OSR_TEST_KEY_EVENTS:
      case OSR_TEST_IME_COMMIT_TEXT:
      case OSR_TEST_IME_FINISH_COMPOSITION:
      case OSR_TEST_IME_CANCEL_COMPOSITION:
      case OSR_TEST_IME_SET_COMPOSITION:
        if (StartTest()) {
          // Results in a call to OnQuery.
          FocusEditBox(browser);
        }
        break;
      case OSR_TEST_TEXT_SELECTION_CHANGE: {
        // trigger the text selection changed event
        if (StartTest()) {
          // click inside list element so text range will be selected.
          CefMouseEvent mouse_event;
          const CefRect& expected_rect = GetElementBounds("LI11");
          mouse_event.x = MiddleX(expected_rect);
          mouse_event.y = MiddleY(expected_rect);
          mouse_event.modifiers = 0;
          SendMouseClickEvent(browser, mouse_event);
        }
      } break;
      case OSR_TEST_VIRTUAL_KEYBOARD: {
        if (StartTest()) {
          CefMouseEvent mouse_event;
          const CefRect& input = GetElementBounds("email");
          mouse_event.x = MiddleX(input);
          mouse_event.y = MiddleY(input);
          mouse_event.modifiers = 0;
          SendMouseClickEvent(browser, mouse_event);
        }
      } break;
      case OSR_TEST_TOUCH_START:
      case OSR_TEST_TOUCH_MOVE:
      case OSR_TEST_TOUCH_END:
      case OSR_TEST_TOUCH_CANCEL: {
        // We trigger a valid Touch workflow sequence and close the tests
        // at seperate points for the 4 cases
        if (StartTest()) {
          const CefRect& touchdiv = GetElementBounds("touchdiv");
          std::vector<CefTouchEvent> touch_events;

          // click inside edit box so that text could be entered
          CefTouchEvent touch_event1;
          touch_event1.id = 0;
          touch_event1.x = MiddleX(touchdiv) - 45;
          touch_event1.y = MiddleY(touchdiv);
          touch_event1.modifiers = 0;
          touch_event1.type = CEF_TET_PRESSED;
          touch_events.push_back(touch_event1);

          CefTouchEvent touch_event2;
          touch_event2.id = 1;
          touch_event2.x = MiddleX(touchdiv) + 45;
          touch_event2.y = MiddleY(touchdiv);
          touch_event2.modifiers = 0;
          touch_event2.type = CEF_TET_PRESSED;
          touch_events.push_back(touch_event2);

          if (test_type_ >= OSR_TEST_TOUCH_MOVE) {
            // Move the Touch fingers closer
            touch_event1.type = touch_event2.type = CEF_TET_MOVED;
            for (size_t i = 0; i < 40; i++) {
              touch_event1.x++;
              touch_event2.x--;
              touch_events.push_back(touch_event1);
              touch_events.push_back(touch_event2);
            }
          }

          // Now release the Touch fingers or cancel them
          if (test_type_ == OSR_TEST_TOUCH_CANCEL) {
            touch_event1.type = touch_event2.type = CEF_TET_CANCELLED;
          } else {
            touch_event1.type = touch_event2.type = CEF_TET_RELEASED;
          }
          touch_events.push_back(touch_event1);
          touch_events.push_back(touch_event2);

          CefPostDelayedTask(TID_UI,
                             base::BindOnce(&OSRTestHandler::SendTouchEvents,
                                            this, std::move(touch_events)),
                             100);
        }
      } break;
      case OSR_TEST_PEN: {
        if (StartTest()) {
          const CefRect& pointerdiv = GetElementBounds("pointerdiv");
          std::vector<CefTouchEvent> touch_events;

          CefTouchEvent touch_event;
          touch_event.x = MiddleX(pointerdiv) - 45;
          touch_event.y = MiddleY(pointerdiv);
          touch_event.type = CEF_TET_PRESSED;
          touch_event.pointer_type = CEF_POINTER_TYPE_PEN;

          touch_events.push_back(touch_event);

          touch_event.type = CEF_TET_MOVED;
          for (size_t i = 0; i < 40; i++) {
            touch_event.x++;
            touch_events.push_back(touch_event);
          }

          touch_event.type = CEF_TET_RELEASED;
          touch_events.push_back(touch_event);

          CefPostDelayedTask(TID_UI,
                             base::BindOnce(&OSRTestHandler::SendTouchEvents,
                                            this, std::move(touch_events)),
                             100);
        }
      } break;
      default:
        break;
    }
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    EXPECT_TRUE(browser.get());

    const std::string& messageStr = request;
    if (messageStr.length() > 0 && messageStr[0] == '{') {
      return handleBoundsQuery(browser, frame, query_id, request, persistent,
                               callback);
    }

#if VERBOSE_DEBUGGING
    LOG(INFO) << "OnQuery message=" << messageStr;
#endif

    switch (test_type_) {
      case OSR_TEST_FOCUS:
        if (messageStr == "osrfocus") {
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_CLICK_LEFT:
        if (messageStr == "osrclick0") {
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_MOUSE_MOVE:
        if (messageStr == "osrmousemove") {
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_DRAG_DROP_DROP:
        if (messageStr == "osrdrop") {
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_KEY_EVENTS:
        if (messageStr == "osrfocuseditbox") {
          SendKeyEvents();
        }
        break;
      case OSR_TEST_IME_COMMIT_TEXT:
        if (messageStr == "osrfocuseditbox") {
          SendIMECommitText();
        }
        break;
      case OSR_TEST_IME_FINISH_COMPOSITION:
        if (messageStr == "osrfocuseditbox") {
          SendIMEFinishComposition();
        }
        break;
      case OSR_TEST_IME_CANCEL_COMPOSITION:
        if (messageStr == "osrfocuseditbox") {
          SendIMECancelComposition();
        }
        break;
      case OSR_TEST_IME_SET_COMPOSITION:
        if (messageStr == "osrfocuseditbox") {
          SendIMESetComposition();
        }
        break;
      case OSR_TEST_TOUCH_START:
      case OSR_TEST_TOUCH_MOVE:
      case OSR_TEST_TOUCH_END:
      case OSR_TEST_TOUCH_CANCEL: {
        switch (touch_state_) {
          case CEF_TET_CANCELLED: {
            // The first message expected is touchstart.
            // We expect multitouch, so touches length should be 2.
            // Ignore intermediate touch start events.
            if (messageStr == "osrtouchstart1") {
              break;
            }
            EXPECT_STREQ(messageStr.c_str(), "osrtouchstart2");
            // Close Touch Start Tests.
            if (test_type_ == OSR_TEST_TOUCH_START) {
              DestroySucceededTestSoon();
              touch_state_ = CEF_TET_RELEASED;
            } else {
              touch_state_ = CEF_TET_PRESSED;
            }
          } break;
          case CEF_TET_PRESSED: {
            // Touch Move include the touches that changed, should be 2.
            EXPECT_STREQ(messageStr.c_str(), "osrtouchmove2");
            if (test_type_ == OSR_TEST_TOUCH_MOVE) {
              DestroySucceededTestSoon();
              touch_state_ = CEF_TET_RELEASED;
            } else {
              touch_state_ = CEF_TET_MOVED;
            }
          } break;
          case CEF_TET_MOVED: {
            // There might be multiple touchmove events, ignore.
            if (messageStr != "osrtouchmove2") {
              if (test_type_ == OSR_TEST_TOUCH_END) {
                EXPECT_STREQ(messageStr.c_str(), "osrtouchend");
                DestroySucceededTestSoon();
                touch_state_ = CEF_TET_RELEASED;
              } else if (test_type_ == OSR_TEST_TOUCH_CANCEL) {
                EXPECT_STREQ(messageStr.c_str(), "osrtouchcancel");
                DestroySucceededTestSoon();
                touch_state_ = CEF_TET_RELEASED;
              }
            }
          } break;
          default:
            break;
        }
      } break;
      case OSR_TEST_PEN: {
        switch (touch_state_) {
          case CEF_TET_CANCELLED:
            // The first message expected is pointerdown.
            EXPECT_STREQ(messageStr.c_str(), "osrpointerdown pen");
            touch_state_ = CEF_TET_PRESSED;
            break;
          case CEF_TET_PRESSED:
            EXPECT_STREQ(messageStr.c_str(), "osrpointermove pen");
            touch_state_ = CEF_TET_MOVED;
            break;
          case CEF_TET_MOVED:
            // There might be multiple pointermove events, ignore.
            if (messageStr != "osrpointermove pen") {
              EXPECT_STREQ(messageStr.c_str(), "osrpointerup pen");
              DestroySucceededTestSoon();
            }
            break;
          default:
            break;
        }
      } break;
      default:
        // Intentionally left blank
        break;
    }
    callback->Success("");
    return true;
  }

  bool handleBoundsQuery(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int64_t query_id,
                         const CefString& request,
                         bool persistent,
                         CefRefPtr<Callback> callback) {
    CefRefPtr<CefValue> jsonObj = CefParseJSON(request, JSON_PARSER_RFC);
    if (jsonObj.get()) {
      CefRefPtr<CefDictionaryValue> dict = jsonObj->GetDictionary();
      const std::string& type = dict->GetString("type");
      if (type == "ElementBounds") {
        CefRefPtr<CefListValue> elems = dict->GetList("elems");

        for (size_t i = 0; i < elems->GetSize(); i++) {
          CefRefPtr<CefDictionaryValue> elem = elems->GetDictionary(i);
          std::string elementId = elem->GetString("id");
          CefRect bounds(elem->GetInt("x"), elem->GetInt("y"),
                         elem->GetInt("width"), elem->GetInt("height"));
          element_bounds_.insert(std::make_pair(elementId, bounds));
        }
        return true;
      }
    }

    return false;
  }

  // CefClient methods, providing handlers
  CefRefPtr<CefFocusHandler> GetFocusHandler() override { return this; }

  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

  CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override {
    return this;
  }

  // CefResourceRequestHandler methods
  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    std::string url = request->GetURL();

    if (url.find(kTestUrl) == 0) {
      // Show the osr test contents
      CefRefPtr<CefStreamReader> stream =
          client::GetBinaryResourceReader("osr_test.html");
      return new CefStreamResourceHandler("text/html", stream);
    }

    return nullptr;
  }

  // CefRenderHandler methods
  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
    if (test_type_ == OSR_TEST_RESIZE && started()) {
      rect = CefRect(0, 0, kOsrWidth * 2, kOsrHeight * 2);
      return;
    }
    rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
  }

  bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                      int viewX,
                      int viewY,
                      int& screenX,
                      int& screenY) override {
    if (test_type_ == OSR_TEST_SCREEN_POINT && started()) {
      const CefRect& expected_rect = GetElementBounds("LI04");
      EXPECT_EQ(viewX, MiddleX(expected_rect));
      EXPECT_EQ(viewY, MiddleY(expected_rect));
      DestroySucceededTestSoon();
    } else if (test_type_ == OSR_TEST_CONTEXT_MENU && started()) {
      screenX = 0;
      screenY = 0;
      return true;
    }
    // we don't want to see a contextual menu. stop here.
    return false;
  }

  bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                     CefScreenInfo& screen_info) override {
    screen_info.device_scale_factor = scale_factor_;

    // The screen info rectangles are used by the renderer to create and
    // position popups. If not overwritten in this function, the rectangle
    // returned from GetViewRect will be used to popuplate them.
    // The popup in the test fits without modifications in the test window, so
    // setting the screen to the test window size does not affect its
    // rectangle.
    screen_info.rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
    screen_info.available_rect = screen_info.rect;
    return true;
  }

  void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) override {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "OnPopupShow show=" << show << " started=" << started()
              << " finished=" << finished();
#endif

    if (finished()) {
      // OnPopupShow(show=false) may arrive after DestroyTest.
      EXPECT_FALSE(show);
      return;
    }

    EXPECT_TRUE(started());

    if (show) {
      switch (test_type_) {
        case OSR_TEST_POPUP_SHOW:
          DestroySucceededTestSoon();
          break;
        default:
          break;
      }
    } else {
      switch (test_type_) {
        case OSR_TEST_POPUP_HIDE_ON_BLUR:
        case OSR_TEST_POPUP_HIDE_ON_CLICK:
        case OSR_TEST_POPUP_HIDE_ON_ESC:
        case OSR_TEST_POPUP_HIDE_ON_SCROLL:
          DestroySucceededTestSoon();
          break;
        default:
          break;
      }
    }
  }

  void OnPopupSize(CefRefPtr<CefBrowser> browser,
                   const CefRect& rect) override {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "OnPopupSize started=" << started()
              << " finished=" << finished();
#endif

    if (finished()) {
      // For OSR_TEST_POPUP_SHOW, OnPopupSize may arrive after DestroyTest.
      EXPECT_EQ(OSR_TEST_POPUP_SHOW, test_type_);
      return;
    }

    EXPECT_TRUE(started());

    switch (test_type_) {
      case OSR_TEST_POPUP_SIZE:
        EXPECT_EQ(kExpandedSelectRect.x, rect.x);
        EXPECT_EQ(kExpandedSelectRect.y, rect.y);
        if (ExpectComputedPopupSize()) {
          EXPECT_EQ(kExpandedSelectRect.width, rect.width);
          EXPECT_EQ(kExpandedSelectRect.height, rect.height);
        } else {
          EXPECT_GE(rect.width, kExpandedSelectRect.width);
          EXPECT_GE(rect.height, kExpandedSelectRect.height);
        }
        DestroySucceededTestSoon();
        break;
      default:
        break;
    }
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) override {
    // bitmap must as big as GetViewRect said
    if (test_type_ != OSR_TEST_RESIZE && type == PET_VIEW) {
      EXPECT_EQ(GetScaledInt(kOsrWidth), width);
      EXPECT_EQ(GetScaledInt(kOsrHeight), height);
    } else if (type == PET_POPUP) {
      const CefRect& expanded_select_rect = GetScaledRect(kExpandedSelectRect);
      if (ExpectComputedPopupSize()) {
        EXPECT_EQ(expanded_select_rect.width, width);
        EXPECT_EQ(expanded_select_rect.height, height);
      } else {
        EXPECT_GT(width, kExpandedSelectRect.width);
        EXPECT_GT(height, kExpandedSelectRect.height);
      }
    }

    EXPECT_TRUE(browser->GetHost()->IsWindowRenderingDisabled());

    // start test only when painting something else then background
    if (IsBackgroundInBuffer(
            reinterpret_cast<const uint32_t*>(buffer), width * height,
            test_type_ == OSR_TEST_TRANSPARENCY ? 0x00000000 : 0xFFFFFFFF)) {
      return;
    }

    // Send events after the first full repaint
    switch (test_type_) {
      case OSR_TEST_PAINT:
        if (StartTest()) {
          // test that we have a full repaint
          EXPECT_EQ(dirtyRects.size(), 1U);
          EXPECT_TRUE(IsFullRepaint(dirtyRects[0], GetScaledInt(kOsrWidth),
                                    GetScaledInt(kOsrHeight)));
          EXPECT_EQ(0xffff7f7fU, *(reinterpret_cast<const uint32_t*>(buffer)));
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_TRANSPARENCY:
        if (StartTest()) {
          // test that we have a full repaint
          EXPECT_EQ(dirtyRects.size(), 1U);
          EXPECT_TRUE(IsFullRepaint(dirtyRects[0], GetScaledInt(kOsrWidth),
                                    GetScaledInt(kOsrHeight)));
          EXPECT_EQ(0x80800000U, *(reinterpret_cast<const uint32_t*>(buffer)));
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_RESIZE:
        if (StartTest()) {
          browser->GetHost()->WasResized();
        } else {
          // There may be some partial repaints before the full repaint at the
          // desired size.
          const int desired_width = GetScaledInt(kOsrWidth) * 2;
          const int desired_height = GetScaledInt(kOsrHeight) * 2;

          EXPECT_EQ(dirtyRects.size(), 1U);
          if (width == desired_width && height == desired_height &&
              IsFullRepaint(dirtyRects[0], width, height)) {
            DestroySucceededTestSoon();
          }
        }
        break;
      case OSR_TEST_INVALIDATE: {
        if (StartTest()) {
          browser->GetHost()->Invalidate(PET_VIEW);
        } else {
          EXPECT_EQ(dirtyRects.size(), 1U);
          const CefRect& expected_rect =
              GetScaledRect(CefRect(0, 0, kOsrWidth, kOsrHeight));
          // There may be some partial repaints before the full repaint.
          if (dirtyRects[0] == expected_rect) {
            DestroySucceededTestSoon();
          }
        }
        break;
      }
      case OSR_TEST_SCROLLING: {
        static const int deltaY = 10;
        if (StartTest()) {
          // scroll down once
          CefMouseEvent mouse_event;
          const CefRect& expected_rect = GetElementBounds("LI00");
          mouse_event.x = MiddleX(expected_rect);
          mouse_event.y = MiddleY(expected_rect);
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseWheelEvent(mouse_event, 0, -deltaY);
        } else {
          EXPECT_EQ(dirtyRects.size(), 1U);
          const CefRect& expected_rect =
              GetScaledRect(CefRect(0, 0, kOsrWidth, kOsrHeight));
          // There may be some partial repaints before the full repaint.
          if (dirtyRects[0] == expected_rect) {
            DestroySucceededTestSoon();
          }
        }
        break;
      }
      case OSR_TEST_POPUP_HIDE_ON_CLICK:
        // Wait for the first popup paint to occur
        if (type == PET_POPUP) {
          CefMouseEvent mouse_event;
          mouse_event.x = 1;
          mouse_event.y = 1;
          mouse_event.modifiers = 0;
          SendMouseClickEvent(browser, mouse_event);
        }
        break;
      case OSR_TEST_POPUP_HIDE_ON_SCROLL:
        // Wait for the first popup paint to occur
        if (type == PET_POPUP) {
          CefMouseEvent mouse_event;
          mouse_event.x = mouse_event.y = 1;
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseWheelEvent(mouse_event, 0, -10);
        }
        break;
      case OSR_TEST_POPUP_HIDE_ON_BLUR:
        // Wait for the first popup paint to occur
        if (type == PET_POPUP) {
          browser->GetHost()->SetFocus(false);
        }
        break;
      case OSR_TEST_POPUP_HIDE_ON_ESC:
        // Wait for the first popup paint to occur
        if (type == PET_POPUP) {
#if defined(OS_WIN)
          SendKeyEvent(browser, VK_ESCAPE);
#elif defined(OS_MAC) || defined(OS_LINUX)
          SendKeyEvent(browser, kNativeKeyEscape, VKEY_ESCAPE);
#else
#error "Unsupported platform"
#endif
        }
        break;
      case OSR_TEST_POPUP_PAINT:
        // Wait for the first popup paint to occur
        if (type == PET_POPUP) {
          EXPECT_EQ(dirtyRects.size(), 1U);
          const CefRect& expanded_select_rect =
              GetScaledRect(kExpandedSelectRect);
          EXPECT_EQ(0, dirtyRects[0].x);
          EXPECT_EQ(0, dirtyRects[0].y);
          if (ExpectComputedPopupSize()) {
            EXPECT_EQ(expanded_select_rect.width, dirtyRects[0].width);
            EXPECT_EQ(expanded_select_rect.height, dirtyRects[0].height);
          } else {
            EXPECT_GT(dirtyRects[0].width, kExpandedSelectRect.width);
            EXPECT_GT(dirtyRects[0].height, kExpandedSelectRect.height);
          }

          // Unselected option background color is cyan.
          // Go down 100 pixels to skip the selected option and over 5 pixels
          // to avoid hitting the border.
          const uint32_t offset = dirtyRects[0].width * 100 + 5;
          EXPECT_EQ(0xff00ffff,
                    *(reinterpret_cast<const uint32_t*>(buffer) + offset));

          if (ExpectComputedPopupSize()) {
            EXPECT_EQ(expanded_select_rect.width, width);
            EXPECT_EQ(expanded_select_rect.height, height);
          } else {
            EXPECT_GT(width, kExpandedSelectRect.width);
            EXPECT_GT(height, kExpandedSelectRect.height);
          }
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_POPUP_SCROLL_INSIDE: {
        static enum { Started, Scrolled } scroll_inside_state = Started;
        // Wait for the first popup paint to occur
        if (type == PET_POPUP) {
          if (scroll_inside_state == Started) {
            CefMouseEvent mouse_event;
            mouse_event.x = MiddleX(kExpandedSelectRect);
            mouse_event.y = MiddleY(kExpandedSelectRect);
            mouse_event.modifiers = 0;
            browser->GetHost()->SendMouseWheelEvent(mouse_event, 0, -10);
            scroll_inside_state = Scrolled;
          } else if (scroll_inside_state == Scrolled) {
            const CefRect& expanded_select_rect =
                GetScaledRect(kExpandedSelectRect);
            EXPECT_EQ(dirtyRects.size(), 1U);

            const int scaled_int_1 = GetScaledInt(1);
            EXPECT_NEAR(0, dirtyRects[0].x, scaled_int_1);
            EXPECT_NEAR(0, dirtyRects[0].y, scaled_int_1);
            if (ExpectComputedPopupSize()) {
              EXPECT_NEAR(expanded_select_rect.width, dirtyRects[0].width,
                          scaled_int_1 * 2);
              EXPECT_NEAR(expanded_select_rect.height, dirtyRects[0].height,
                          scaled_int_1 * 2);
            } else {
              EXPECT_GT(dirtyRects[0].width, kExpandedSelectRect.width);
              EXPECT_GT(dirtyRects[0].height, kExpandedSelectRect.height);
            }
            DestroySucceededTestSoon();
          }
        }
      } break;
      default:
        break;
    }
  }

  bool OnSetFocus(CefRefPtr<CefBrowser> browser, FocusSource source) override {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "OnSetFocus started=" << started();
#endif

    if (source == FOCUS_SOURCE_NAVIGATION) {
      got_navigation_focus_event_.yes();

      // Ignore focus from the original navigation when we're testing focus
      // event delivery.
      if (test_type_ == OSR_TEST_FOCUS) {
        return true;
      }
      return false;
    }

    EXPECT_EQ(source, FOCUS_SOURCE_SYSTEM);
    got_system_focus_event_.yes();
    return false;
  }

  void OnTakeFocus(CefRefPtr<CefBrowser> browser, bool next) override {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "OnTakeFocus next=" << next << " started=" << started();
#endif

    if (test_type_ == OSR_TEST_TAKE_FOCUS) {
      DestroySucceededTestSoon();
    }
  }

  void OnGotFocus(CefRefPtr<CefBrowser> browser) override {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "OnGotFocus started=" << started();
#endif

    if (test_type_ == OSR_TEST_GOT_FOCUS) {
      DestroySucceededTestSoon();
    }
  }

  bool OnCursorChange(CefRefPtr<CefBrowser> browser,
                      CefCursorHandle cursor,
                      cef_cursor_type_t type,
                      const CefCursorInfo& custom_cursor_info) override {
    if (test_type_ == OSR_TEST_CURSOR && started()) {
      EXPECT_EQ(CT_HAND, type);
      EXPECT_EQ(nullptr, custom_cursor_info.buffer);
      DestroySucceededTestSoon();
    }
    return true;
  }

  void OnImeCompositionRangeChanged(
      CefRefPtr<CefBrowser> browser,
      const CefRange& range,
      const CefRenderHandler::RectList& bounds) override {
    if (test_type_ == OSR_TEST_IME_SET_COMPOSITION && started()) {
      EXPECT_EQ(range.from, 0U);
      EXPECT_EQ(range.to, 1U);
      EXPECT_EQ(1U, bounds.size());
      DestroySucceededTestSoon();
    }
  }

  bool StartDragging(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefDragData> drag_data,
                     CefRenderHandler::DragOperationsMask allowed_ops,
                     int x,
                     int y) override {
    if (test_type_ == OSR_TEST_DRAG_DROP_START_DRAGGING && started()) {
      // Verify the drag image representation.
      const CefRect& dragdiv = GetElementBounds("dragdiv");
      EXPECT_TRUE(drag_data->HasImage());
      CefRefPtr<CefImage> image = drag_data->GetImage();
      EXPECT_TRUE(image.get() != nullptr);
      if (image.get()) {
        // Drag image height seems to always be + 1px greater than the drag
        // rect on Linux. Therefore allow it to be +/- 1px.
        EXPECT_NEAR(static_cast<int>(image->GetWidth()), dragdiv.width, 1);
        EXPECT_NEAR(static_cast<int>(image->GetHeight()), dragdiv.height, 1);
      }
      // During testing hotspot (x, y) was (15, 23) at 1x scale and (15, 18)
      // at 2x scale. Since the mechanism for determining this position is
      // unclear test only that it falls within the rect boundaries.
      CefPoint hotspot = drag_data->GetImageHotspot();
      EXPECT_GT(hotspot.x, 0);
      EXPECT_LE(hotspot.x, GetScaledInt(dragdiv.width));
      EXPECT_GT(hotspot.y, 0);
      EXPECT_LE(hotspot.y, GetScaledInt(dragdiv.height));

      DestroySucceededTestSoon();
      return false;
    } else if ((test_type_ == OSR_TEST_DRAG_DROP_UPDATE_CURSOR ||
                test_type_ == OSR_TEST_DRAG_DROP_DROP) &&
               started()) {
      // place the mouse over the drop area to trigger UpdateDragCursor
      CefRefPtr<CefDragData> data = drag_data->Clone();
      data->ResetFileContents();
      CefMouseEvent ev;
      const CefRect& dragdiv = GetElementBounds("dragdiv");
      ev.x = MiddleX(dragdiv) - 5;
      ev.y = MiddleY(dragdiv) - 5;
      ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
      browser->GetHost()->DragTargetDragEnter(data, ev, allowed_ops);

      const CefRect& dropdiv = GetElementBounds("dropdiv");
      ev.x = MiddleX(dropdiv);
      ev.y = MiddleY(dropdiv);
      browser->GetHost()->SendMouseMoveEvent(ev, false);
      browser->GetHost()->DragTargetDragOver(ev, allowed_ops);

      ev.x += 5;
      ev.y += 5;
      browser->GetHost()->SendMouseMoveEvent(ev, false);
      browser->GetHost()->DragTargetDragOver(ev, allowed_ops);
      return true;
    }
    return false;
  }

  void UpdateDragCursor(CefRefPtr<CefBrowser> browser,
                        DragOperation operation) override {
    if (operation == DRAG_OPERATION_NONE) {
      return;
    }

    if (test_type_ == OSR_TEST_DRAG_DROP_UPDATE_CURSOR && started()) {
      const CefRect& dropdiv = GetElementBounds("dropdiv");
      browser->GetHost()->DragSourceEndedAt(MiddleX(dropdiv), MiddleY(dropdiv),
                                            DRAG_OPERATION_NONE);
      browser->GetHost()->DragSourceSystemDragEnded();
      DestroySucceededTestSoon();
    } else if (test_type_ == OSR_TEST_DRAG_DROP_DROP && started()) {
      // Don't end the drag multiple times.
      if (got_update_cursor_) {
        return;
      }
      got_update_cursor_.yes();

      CefMouseEvent ev;
      const CefRect& dropdiv = GetElementBounds("dropdiv");
      ev.x = MiddleX(dropdiv);
      ev.y = MiddleY(dropdiv);
      ev.modifiers = 0;
      browser->GetHost()->SendMouseClickEvent(ev, MBT_LEFT, true, 1);
      browser->GetHost()->DragTargetDrop(ev);
      browser->GetHost()->DragSourceEndedAt(ev.x, ev.y, operation);
      browser->GetHost()->DragSourceSystemDragEnded();
    }
  }

  void OnTextSelectionChanged(CefRefPtr<CefBrowser> browser,
                              const CefString& selected_text,
                              const CefRange& selected_range) override {
    if (test_type_ == OSR_TEST_TEXT_SELECTION_CHANGE && started()) {
      if (!got_initial_text_selection_event_) {
        got_initial_text_selection_event_.yes();
      } else {
        EXPECT_STREQ("SELECTED_TEXT_RANGE", selected_text.ToString().c_str());
        DestroySucceededTestSoon();
      }
    }
  }

  void OnVirtualKeyboardRequested(CefRefPtr<CefBrowser> browser,
                                  TextInputMode input_mode) override {
    if (test_type_ == OSR_TEST_VIRTUAL_KEYBOARD && started()) {
      if (!got_virtual_keyboard_event_.isSet()) {
        got_virtual_keyboard_event_.yes();
        EXPECT_EQ(CEF_TEXT_INPUT_MODE_EMAIL, input_mode);

        CefMouseEvent mouse_event;
        const CefRect& input = GetElementBounds("LI01");
        mouse_event.x = MiddleX(input);
        mouse_event.y = MiddleY(input);
        mouse_event.modifiers = 0;
        SendMouseClickEvent(browser, mouse_event);
      } else {
        EXPECT_EQ(CEF_TEXT_INPUT_MODE_NONE, input_mode);
        DestroySucceededTestSoon();
      }
    }
  }

  bool OnTooltip(CefRefPtr<CefBrowser> browser, CefString& text) override {
    if (test_type_ == OSR_TEST_TOOLTIP && started()) {
      EXPECT_STREQ("EXPECTED_TOOLTIP", text.ToString().c_str());
      DestroySucceededTestSoon();
    }
    return false;
  }

  void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefContextMenuParams> params,
                           CefRefPtr<CefMenuModel> model) override {
    if (!started()) {
      return;
    }
    if (test_type_ == OSR_TEST_CLICK_RIGHT) {
      const CefRect& expected_rect = GetElementBounds("LI04");
      EXPECT_EQ(params->GetXCoord(), MiddleX(expected_rect));
      EXPECT_EQ(params->GetYCoord(), MiddleY(expected_rect));
      DestroySucceededTestSoon();
    } else if (test_type_ == OSR_TEST_CONTEXT_MENU) {
      // This test will pass if it does not crash on destruction
      DestroySucceededTestSoon();
    }
  }

  bool RunContextMenu(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefContextMenuParams> params,
                      CefRefPtr<CefMenuModel> model,
                      CefRefPtr<CefRunContextMenuCallback> callback) override {
    if (!started()) {
      return false;
    }

    EXPECT_UI_THREAD();

    auto current_browser = GetBrowser();
    EXPECT_TRUE(current_browser->IsSame(browser));
    EXPECT_STREQ(
        current_browser->GetFocusedFrame()->GetIdentifier().ToString().c_str(),
        frame->GetIdentifier().ToString().c_str());

    if (test_type_ == OSR_TEST_QUICK_MENU) {
      EXPECT_EQ(2U, got_touch_handle_enabled_ct_);
      EXPECT_EQ(2U, got_touch_handle_size_ct_);
      EXPECT_TRUE(got_quick_menu_);
      EXPECT_TRUE(got_quick_menu_command_);
      EXPECT_TRUE(got_quick_menu_dismissed_);
      EXPECT_EQ(2U, got_touch_handle_disabled_ct_);
      EXPECT_FALSE(got_context_menu_);

      got_context_menu_.yes();

      // Got all expected callbacks.
      DestroySucceededTestSoon();

      // Cancel the menu immediately.
      callback->Cancel();
      return true;
    }

    return false;
  }

  bool RunQuickMenu(CefRefPtr<CefBrowser> browser,
                    CefRefPtr<CefFrame> frame,
                    const CefPoint& location,
                    const CefSize& size,
                    QuickMenuEditStateFlags edit_state_flags,
                    CefRefPtr<CefRunQuickMenuCallback> callback) override {
    if (!started()) {
      return false;
    }

    EXPECT_UI_THREAD();

    auto current_browser = GetBrowser();
    EXPECT_TRUE(current_browser->IsSame(browser));
    EXPECT_STREQ(
        current_browser->GetFocusedFrame()->GetIdentifier().ToString().c_str(),
        frame->GetIdentifier().ToString().c_str());

    EXPECT_EQ(OSR_TEST_QUICK_MENU, test_type_);

    EXPECT_GT(location.x, 0);
    EXPECT_GT(location.y, 0);
    EXPECT_GT(size.width, 0);
    EXPECT_GT(size.height, 0);
    EXPECT_EQ(static_cast<QuickMenuEditStateFlags>(QM_EDITFLAG_CAN_ELLIPSIS |
                                                   QM_EDITFLAG_CAN_COPY),
              edit_state_flags);
    EXPECT_TRUE(callback.get());

    EXPECT_EQ(2U, got_touch_handle_enabled_ct_);
    EXPECT_EQ(2U, got_touch_handle_size_ct_);
    EXPECT_FALSE(got_quick_menu_);
    EXPECT_FALSE(got_quick_menu_command_);
    EXPECT_FALSE(got_quick_menu_dismissed_);
    EXPECT_EQ(0U, got_touch_handle_disabled_ct_);
    EXPECT_FALSE(got_context_menu_);

    got_quick_menu_.yes();

    // Proceed to OnQuickMenuCommand.
    callback->Continue(QM_EDITFLAG_CAN_ELLIPSIS,
                       static_cast<cef_event_flags_t>(EVENTFLAG_SHIFT_DOWN));
    return true;
  }

  bool OnQuickMenuCommand(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          int command_id,
                          EventFlags event_flags) override {
    EXPECT_UI_THREAD();

    auto current_browser = GetBrowser();
    EXPECT_TRUE(current_browser->IsSame(browser));
    EXPECT_STREQ(
        current_browser->GetFocusedFrame()->GetIdentifier().ToString().c_str(),
        frame->GetIdentifier().ToString().c_str());

    EXPECT_EQ(OSR_TEST_QUICK_MENU, test_type_);

    // Values passed to Continue() in RunQuickMenu.
    EXPECT_EQ(QM_EDITFLAG_CAN_ELLIPSIS, command_id);
    EXPECT_EQ(EVENTFLAG_SHIFT_DOWN, event_flags);

    EXPECT_EQ(2U, got_touch_handle_enabled_ct_);
    EXPECT_EQ(2U, got_touch_handle_size_ct_);
    EXPECT_TRUE(got_quick_menu_);
    EXPECT_FALSE(got_quick_menu_command_);
    EXPECT_FALSE(got_quick_menu_dismissed_);
    EXPECT_EQ(0U, got_touch_handle_disabled_ct_);
    EXPECT_FALSE(got_context_menu_);

    got_quick_menu_command_.yes();

    // Proceed to OnQuickMenuDismissed and RunContextMenu.
    return false;
  }

  void OnQuickMenuDismissed(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame) override {
    EXPECT_UI_THREAD();

    auto current_browser = GetBrowser();
    EXPECT_TRUE(current_browser->IsSame(browser));
    EXPECT_STREQ(
        current_browser->GetFocusedFrame()->GetIdentifier().ToString().c_str(),
        frame->GetIdentifier().ToString().c_str());

    EXPECT_EQ(OSR_TEST_QUICK_MENU, test_type_);

    EXPECT_EQ(2U, got_touch_handle_enabled_ct_);
    EXPECT_EQ(2U, got_touch_handle_size_ct_);
    EXPECT_TRUE(got_quick_menu_);
    EXPECT_TRUE(got_quick_menu_command_);
    EXPECT_FALSE(got_quick_menu_dismissed_);
    EXPECT_EQ(0U, got_touch_handle_disabled_ct_);
    EXPECT_FALSE(got_context_menu_);

    EXPECT_EQ(kAllTouchHandleFlags, touch_handle_flags_);

    got_quick_menu_dismissed_.yes();
  }

  void GetTouchHandleSize(CefRefPtr<CefBrowser> browser,
                          cef_horizontal_alignment_t orientation,
                          CefSize& size) override {
    size = CefSize(24, 24);
    got_touch_handle_size_ct_++;
  }

  void OnTouchHandleStateChanged(CefRefPtr<CefBrowser> browser,
                                 const CefTouchHandleState& state) override {
    if (!started()) {
      return;
    }

    EXPECT_UI_THREAD();

    auto current_browser = GetBrowser();
    EXPECT_TRUE(current_browser->IsSame(browser));

    if (test_type_ == OSR_TEST_QUICK_MENU) {
      if (state.flags & CEF_THS_FLAG_ENABLED) {
        EXPECT_EQ(state.orientation, 0);
        EXPECT_EQ(state.origin.x, 0);
        EXPECT_EQ(state.origin.y, 0);
        EXPECT_EQ(state.alpha, 0);

        if (state.enabled) {
          got_touch_handle_enabled_ct_++;
          EXPECT_FALSE(got_quick_menu_);
          EXPECT_FALSE(got_quick_menu_command_);
          EXPECT_FALSE(got_quick_menu_dismissed_);
          EXPECT_EQ(0U, got_touch_handle_disabled_ct_);
          EXPECT_FALSE(got_context_menu_);

          touch_handle_flags_ |= CEF_THS_FLAG_ENABLED;
        } else {
          got_touch_handle_disabled_ct_++;
          EXPECT_EQ(2U, got_touch_handle_enabled_ct_);
          EXPECT_EQ(2U, got_touch_handle_size_ct_);
          EXPECT_TRUE(got_quick_menu_);
          EXPECT_TRUE(got_quick_menu_command_);
          EXPECT_TRUE(got_quick_menu_dismissed_);
          EXPECT_FALSE(got_context_menu_);
        }
      }
      if (state.flags & CEF_THS_FLAG_ORIENTATION) {
        EXPECT_EQ(state.enabled, false);
        EXPECT_GE(state.orientation, 0);
        EXPECT_EQ(state.origin.x, 0);
        EXPECT_EQ(state.origin.y, 0);
        EXPECT_EQ(state.alpha, 0);
        touch_handle_flags_ |= CEF_THS_FLAG_ORIENTATION;
      }
      if (state.flags & CEF_THS_FLAG_ORIGIN) {
        EXPECT_EQ(state.enabled, false);
        EXPECT_EQ(state.orientation, 0);
        EXPECT_GT(state.origin.x, 0);
        EXPECT_GT(state.origin.y, 0);
        EXPECT_EQ(state.alpha, 0);
        touch_handle_flags_ |= CEF_THS_FLAG_ORIGIN;
      }
      if (state.flags & CEF_THS_FLAG_ALPHA) {
        EXPECT_EQ(state.enabled, false);
        EXPECT_EQ(state.orientation, 0);
        EXPECT_EQ(state.origin.x, 0);
        EXPECT_EQ(state.origin.y, 0);
        EXPECT_GE(state.alpha, 0);
        touch_handle_flags_ |= CEF_THS_FLAG_ALPHA;
      }
    }
  }

  // OSRTestHandler functions
  void CreateOSRBrowser(const CefString& url) {
    CefWindowInfo windowInfo;
    CefBrowserSettings settings;

    if (test_type_ != OSR_TEST_TRANSPARENCY) {
      // Explicitly set an opaque background color to disable transparency.
      settings.background_color = CefColorSetARGB(255, 255, 255, 255);
    }

#if defined(OS_WIN)
    windowInfo.SetAsWindowless(GetDesktopWindow());
#elif defined(OS_MAC)
    // An actual vies is needed only for the ContextMenu test. The menu runner
    // checks if the view is not nil before showing the context menu.
    if (test_type_ == OSR_TEST_CONTEXT_MENU) {
      windowInfo.SetAsWindowless(osr_unittests::GetFakeView());
    } else {
      windowInfo.SetAsWindowless(kNullWindowHandle);
    }
#elif defined(OS_LINUX)
    windowInfo.SetAsWindowless(kNullWindowHandle);
#else
#error "Unsupported platform"
#endif
    CefBrowserHost::CreateBrowser(windowInfo, this, url, settings, nullptr,
                                  nullptr);
  }

  CefRect GetScaledRect(const CefRect& rect) const {
    return client::LogicalToDevice(rect, scale_factor_);
  }

  int GetScaledInt(int value) const {
    return client::LogicalToDevice(value, scale_factor_);
  }

  CefRect GetElementBounds(const std::string& id) {
    ElementBoundsMap::const_iterator it = element_bounds_.find(id);
    if (it != element_bounds_.end()) {
      return it->second;
    }
    return CefRect();
  }

  static bool IsFullRepaint(const CefRect& rc, int width, int height) {
    return rc.width == width && rc.height == height;
  }

  static bool IsBackgroundInBuffer(const uint32_t* buffer,
                                   size_t size,
                                   uint32_t rgba) {
    for (size_t i = 0; i < size; i++) {
      if (buffer[i] != rgba) {
        return false;
      }
    }
    return true;
  }

  static inline int MiddleX(const CefRect& rect) {
    return rect.x + rect.width / 2;
  }

  static inline int MiddleY(const CefRect& rect) {
    return rect.y + rect.height / 2;
  }

  bool ExpectComputedPopupSize() const {
    // The device scale factor is ignored in Blink when computing
    // the default form control font size (see https://crbug.com/674663#c11).
    // This results in better font size display but also means that we won't
    // get the expected (scaled) width/height value for non-1.0 scale factor
    // select popups.
    // The non-1.0 scale factor size is off by a few pixels so we can't
    // perform an exact comparison.
    return scale_factor_ == 1.0;
  }

  static void FocusEditBox(CefRefPtr<CefBrowser> browser) {
    browser->GetMainFrame()->ExecuteJavaScript(
        "document.getElementById('editbox').focus()",
        browser->GetMainFrame()->GetURL(), 0);
  }

  static void ClickButtonToNavigate(CefRefPtr<CefBrowser> browser) {
    browser->GetMainFrame()->ExecuteJavaScript(
        "document.getElementById('btnnavigate').click()",
        browser->GetMainFrame()->GetURL(), 0);
  }

  void SendKeyEvents() {
    auto browser = GetBrowser();

    // write "done" word
    CefKeyEvent event;
    event.is_system_key = false;
    event.modifiers = 0;

    size_t word_length = strlen(kKeyTestWord);
    for (size_t i = 0; i < word_length; ++i) {
#if defined(OS_WIN)
      SendKeyEvent(browser, kKeyTestWord[i]);
#elif defined(OS_MAC) || defined(OS_LINUX)
      SendKeyEvent(browser, kNativeKeyTestCodes[i], kKeyTestCodes[i]);
#else
#error "Unsupported platform"
#endif
    }

    ClickButtonToNavigate(browser);
  }

  void SendIMECommitText() {
    auto browser = GetBrowser();

    size_t word_length = strlen(kKeyTestWord);
    // Add some input keys to edit box
    for (size_t i = 0; i < word_length; ++i) {
#if defined(OS_WIN)
      SendKeyEvent(browser, kKeyTestWord[i]);
#elif defined(OS_MAC) || defined(OS_LINUX)
      SendKeyEvent(browser, kNativeKeyTestCodes[i], kKeyTestCodes[i]);
#else
#error "Unsupported platform"
#endif
    }
    // This text should be honored instead of 'ka' added via key events
    CefString markedText("osrimecommit");

    CefRange range(0, static_cast<uint32_t>(markedText.length()));
    browser->GetHost()->ImeCommitText(markedText, range, 0);

    ClickButtonToNavigate(browser);
  }

  void SendIMEFinishComposition() {
    auto browser = GetBrowser();

    size_t word_length = strlen(kKeyTestWord);
    // Add some input keys to edit box
    for (size_t i = 0; i < word_length; ++i) {
#if defined(OS_WIN)
      SendKeyEvent(browser, kKeyTestWord[i]);
#elif defined(OS_MAC) || defined(OS_LINUX)
      SendKeyEvent(browser, kNativeKeyTestCodes[i], kKeyTestCodes[i]);
#else
#error "Unsupported platform"
#endif
    }

    // Finish Composition should set the existing composition
    browser->GetHost()->ImeFinishComposingText(true);

    ClickButtonToNavigate(browser);
  }

  void SendIMECancelComposition() {
    auto browser = GetBrowser();

    // Add some input keys to edit box
    CefString markedText(L"\u304B");
    std::vector<CefCompositionUnderline> underlines;

    // Use a thin black underline by default.
    CefRange range(0, static_cast<uint32_t>(markedText.length()));
    cef_composition_underline_t line = {sizeof(cef_composition_underline_t),
                                        range, 0xFF000000, 0, false};
    underlines.push_back(line);

    CefRange replacement_range(0, static_cast<uint32_t>(markedText.length()));
    CefRange selection_range(0, static_cast<uint32_t>(markedText.length()));

    // Composition should be updated
    browser->GetHost()->ImeSetComposition(markedText, underlines,
                                          replacement_range, selection_range);

    // CancelComposition should clean up the edit text
    browser->GetHost()->ImeCancelComposition();

    ClickButtonToNavigate(browser);
  }

  void SendIMESetComposition() {
    auto browser = GetBrowser();

    // Now set some intermediate text composition
    CefString markedText(L"\u304B");
    std::vector<CefCompositionUnderline> underlines;

    // Use a thin black underline by default.
    CefRange range(0, static_cast<uint32_t>(markedText.length()));
    cef_composition_underline_t line = {sizeof(cef_composition_underline_t),
                                        range, 0xFF000000, 0, false};
    underlines.push_back(line);

    CefRange replacement_range(0, static_cast<uint32_t>(markedText.length()));
    CefRange selection_range(0, static_cast<uint32_t>(markedText.length()));

    // This should update composition range and
    // trigger the compositionRangeChanged callback
    browser->GetHost()->ImeSetComposition(markedText, underlines,
                                          replacement_range, selection_range);
  }

  void SendTouchEvents(std::vector<CefTouchEvent> touch_events) {
    if (!GetBrowser() || !GetBrowser()->GetHost()) {
      return;
    }

    auto host = GetBrowser()->GetHost();
    for (const auto& te : touch_events) {
      host->SendTouchEvent(te);
    }
  }

  void DestroySucceededTestSoon() {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "DestroySucceededTestSoon finished=" << finished();
#endif

    if (finished()) {
      return;
    }
    finished_ = true;
    CefPostTask(TID_UI, base::BindOnce(&OSRTestHandler::DestroyTest, this));
  }

  void DestroyTest() override {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "DestroyTest";
#endif

    // Always get the OnSetFocus call for the initial navigation.
    EXPECT_TRUE(got_navigation_focus_event_);

    if (test_type_ == OSR_TEST_FOCUS) {
      // SetFocus is called by the system when we explicitly set the focus and
      // when popups are dismissed.
      EXPECT_TRUE(got_system_focus_event_);
    } else if (test_type_ == OSR_TEST_TEXT_SELECTION_CHANGE) {
      EXPECT_TRUE(got_initial_text_selection_event_);
    } else {
      EXPECT_FALSE(got_system_focus_event_);
    }

    if (test_type_ == OSR_TEST_QUICK_MENU) {
      EXPECT_EQ(2U, got_touch_handle_enabled_ct_);
      EXPECT_EQ(2U, got_touch_handle_size_ct_);
      EXPECT_TRUE(got_quick_menu_);
      EXPECT_TRUE(got_quick_menu_command_);
      EXPECT_TRUE(got_quick_menu_dismissed_);
      EXPECT_EQ(2U, got_touch_handle_disabled_ct_);
      EXPECT_TRUE(got_context_menu_);
      EXPECT_EQ(kAllTouchHandleFlags, touch_handle_flags_);
    }

    RoutingTestHandler::DestroyTest();
  }

  void ExpandDropDown() {
#if VERBOSE_DEBUGGING
    LOG(INFO) << "ExpandDropDown";
#endif

    EXPECT_TRUE(started());
    EXPECT_FALSE(finished());

    const CefRect& LI11select = GetElementBounds("LI11select");
    CefMouseEvent mouse_event;
    mouse_event.x = MiddleX(LI11select);
    mouse_event.y = MiddleY(LI11select);
    mouse_event.modifiers = 0;
    SendMouseClickEvent(GetBrowser(), mouse_event);
  }

  void SendKeyEvent(CefRefPtr<CefBrowser> browser,
#if defined(OS_LINUX) || defined(OS_MAC)
                    unsigned int native_key_code,
#endif
                    int key_code) {
    CefKeyEvent event;
    event.is_system_key = false;
    event.modifiers = 0;

#if defined(OS_WIN)
    BYTE VkCode = LOBYTE(VkKeyScanA(key_code));
    UINT scanCode = MapVirtualKey(VkCode, MAPVK_VK_TO_VSC);
    event.native_key_code = (scanCode << 16) |  // key scan code
                            1;                  // key repeat count
    event.windows_key_code = VkCode;
#elif defined(OS_MAC)
    event.native_key_code = native_key_code;
    // Note that this is only correct for lower-case characters. If |key_code|
    // was an upper-case character then |event.character| would be the upper-
    // case character and |event.unmodified_character| would be the lower-case
    // character (e.g. the character without the shift modifier applied).
    event.character = event.unmodified_character = key_code;
#elif defined(OS_LINUX)
    event.native_key_code = native_key_code;
    event.windows_key_code = key_code;
    event.character = event.unmodified_character = native_key_code;
#else
    NOTREACHED();
#endif
    event.type = KEYEVENT_RAWKEYDOWN;
    browser->GetHost()->SendKeyEvent(event);

#if defined(OS_WIN)
    event.windows_key_code = key_code;
#endif
    event.type = KEYEVENT_CHAR;
    browser->GetHost()->SendKeyEvent(event);

#if defined(OS_WIN)
    event.windows_key_code = VkCode;
    // bits 30 and 31 should be always 1 for WM_KEYUP
    event.native_key_code |= 0xC0000000;
#endif
    event.type = KEYEVENT_KEYUP;
    browser->GetHost()->SendKeyEvent(event);
  }

  // true if the events for this test are already sent
  bool started() const { return started_; }

  // true if the exit point was reached, even the result is not
  // the expected one
  bool finished() const { return finished_; }

  // will mark test as started and will return true only the first time
  // it is called
  bool StartTest() {
    if (started_) {
      return false;
    }
    started_ = true;
    return true;
  }

 private:
  const OSRTestType test_type_;
  const float scale_factor_;

  bool started_ = false;
  bool finished_ = false;
  cef_touch_event_type_t touch_state_ = CEF_TET_CANCELLED;

  TrackCallback got_update_cursor_;
  TrackCallback got_navigation_focus_event_;
  TrackCallback got_system_focus_event_;
  TrackCallback got_initial_text_selection_event_;
  TrackCallback got_virtual_keyboard_event_;

  uint32_t touch_handle_flags_ = 0U;
  size_t got_touch_handle_enabled_ct_ = 0U;
  size_t got_touch_handle_size_ct_ = 0U;
  TrackCallback got_quick_menu_;
  TrackCallback got_quick_menu_command_;
  TrackCallback got_quick_menu_dismissed_;
  size_t got_touch_handle_disabled_ct_ = 0U;
  TrackCallback got_context_menu_;

  typedef std::map<std::string, CefRect> ElementBoundsMap;
  ElementBoundsMap element_bounds_;

  IMPLEMENT_REFCOUNTING(OSRTestHandler);
};

}  // namespace

// generic test
#define OSR_TEST(name, test_mode, scale_factor)      \
  TEST(OSRTest, name) {                              \
    CefRefPtr<OSRTestHandler> handler =              \
        new OSRTestHandler(test_mode, scale_factor); \
    handler->ExecuteTest();                          \
    EXPECT_TRUE(handler->finished());                \
    ReleaseAndWaitForDestructor(handler);            \
  }

// tests
OSR_TEST(Windowless, OSR_TEST_IS_WINDOWLESS, 1.0f)
OSR_TEST(Windowless2x, OSR_TEST_IS_WINDOWLESS, 2.0f)
OSR_TEST(Focus, OSR_TEST_FOCUS, 1.0f)
OSR_TEST(Focus2x, OSR_TEST_FOCUS, 2.0f)
OSR_TEST(TakeFocus, OSR_TEST_TAKE_FOCUS, 1.0f)
OSR_TEST(TakeFocus2x, OSR_TEST_TAKE_FOCUS, 2.0f)
OSR_TEST(GotFocus, OSR_TEST_GOT_FOCUS, 1.0f)
OSR_TEST(GotFocus2x, OSR_TEST_GOT_FOCUS, 2.0f)
OSR_TEST(Paint, OSR_TEST_PAINT, 1.0f)
OSR_TEST(Paint2x, OSR_TEST_PAINT, 2.0f)
OSR_TEST(TransparentPaint, OSR_TEST_TRANSPARENCY, 1.0f)
OSR_TEST(TransparentPaint2x, OSR_TEST_TRANSPARENCY, 2.0f)
OSR_TEST(Cursor, OSR_TEST_CURSOR, 1.0f)
OSR_TEST(Cursor2x, OSR_TEST_CURSOR, 2.0f)
OSR_TEST(MouseMove, OSR_TEST_MOUSE_MOVE, 1.0f)
OSR_TEST(MouseMove2x, OSR_TEST_MOUSE_MOVE, 2.0f)
OSR_TEST(MouseRightClick, OSR_TEST_CLICK_RIGHT, 1.0f)
OSR_TEST(MouseRightClick2x, OSR_TEST_CLICK_RIGHT, 2.0f)
OSR_TEST(MouseLeftClick, OSR_TEST_CLICK_LEFT, 1.0f)
OSR_TEST(MouseLeftClick2x, OSR_TEST_CLICK_LEFT, 2.0f)
OSR_TEST(ScreenPoint, OSR_TEST_SCREEN_POINT, 1.0f)
OSR_TEST(ScreenPoint2x, OSR_TEST_SCREEN_POINT, 2.0f)
OSR_TEST(Resize, OSR_TEST_RESIZE, 1.0f)
OSR_TEST(Resize2x, OSR_TEST_RESIZE, 2.0f)
OSR_TEST(Invalidate, OSR_TEST_INVALIDATE, 1.0f)
OSR_TEST(Invalidate2x, OSR_TEST_INVALIDATE, 2.0f)
OSR_TEST(KeyEvents, OSR_TEST_KEY_EVENTS, 1.0f)
OSR_TEST(KeyEvents2x, OSR_TEST_KEY_EVENTS, 2.0f)
OSR_TEST(Tooltip, OSR_TEST_TOOLTIP, 1.0f)
OSR_TEST(Tooltip2x, OSR_TEST_TOOLTIP, 2.0f)
OSR_TEST(Scrolling, OSR_TEST_SCROLLING, 1.0f)
OSR_TEST(Scrolling2x, OSR_TEST_SCROLLING, 2.0f)
OSR_TEST(ContextMenu, OSR_TEST_CONTEXT_MENU, 1.0f)
OSR_TEST(ContextMenu2x, OSR_TEST_CONTEXT_MENU, 2.0f)
OSR_TEST(QuickMenu, OSR_TEST_QUICK_MENU, 1.0f)
OSR_TEST(QuickMenu2x, OSR_TEST_CONTEXT_MENU, 2.0f)
OSR_TEST(PopupPaint, OSR_TEST_POPUP_PAINT, 1.0f)
OSR_TEST(PopupPaint2x, OSR_TEST_POPUP_PAINT, 2.0f)
OSR_TEST(PopupShow, OSR_TEST_POPUP_SHOW, 1.0f)
OSR_TEST(PopupShow2x, OSR_TEST_POPUP_SHOW, 2.0f)
OSR_TEST(PopupSize, OSR_TEST_POPUP_SIZE, 1.0f)
OSR_TEST(PopupSize2x, OSR_TEST_POPUP_SIZE, 2.0f)
OSR_TEST(PopupHideOnBlur, OSR_TEST_POPUP_HIDE_ON_BLUR, 1.0f)
OSR_TEST(PopupHideOnBlur2x, OSR_TEST_POPUP_HIDE_ON_BLUR, 2.0f)
OSR_TEST(PopupHideOnClick, OSR_TEST_POPUP_HIDE_ON_CLICK, 1.0f)
OSR_TEST(PopupHideOnClick2x, OSR_TEST_POPUP_HIDE_ON_CLICK, 2.0f)
OSR_TEST(PopupHideOnScroll, OSR_TEST_POPUP_HIDE_ON_SCROLL, 1.0f)
OSR_TEST(PopupHideOnScroll2x, OSR_TEST_POPUP_HIDE_ON_SCROLL, 2.0f)
OSR_TEST(PopupHideOnEsc, OSR_TEST_POPUP_HIDE_ON_ESC, 1.0f)
OSR_TEST(PopupHideOnEsc2x, OSR_TEST_POPUP_HIDE_ON_ESC, 2.0f)
OSR_TEST(PopupScrollInside, OSR_TEST_POPUP_SCROLL_INSIDE, 1.0f)
OSR_TEST(PopupScrollInside2x, OSR_TEST_POPUP_SCROLL_INSIDE, 2.0f)
OSR_TEST(DragDropStartDragging, OSR_TEST_DRAG_DROP_START_DRAGGING, 1.0f)
OSR_TEST(DragDropStartDragging2x, OSR_TEST_DRAG_DROP_START_DRAGGING, 2.0f)
OSR_TEST(DragDropUpdateCursor, OSR_TEST_DRAG_DROP_UPDATE_CURSOR, 1.0f)
OSR_TEST(DragDropUpdateCursor2x, OSR_TEST_DRAG_DROP_UPDATE_CURSOR, 2.0f)
OSR_TEST(DragDropDropElement, OSR_TEST_DRAG_DROP_DROP, 1.0f)
OSR_TEST(DragDropDropElement2x, OSR_TEST_DRAG_DROP_DROP, 2.0f)
OSR_TEST(IMESetComposition, OSR_TEST_IME_SET_COMPOSITION, 1.0f)
OSR_TEST(IMESetComposition2x, OSR_TEST_IME_SET_COMPOSITION, 2.0f)
OSR_TEST(IMECommitText, OSR_TEST_IME_COMMIT_TEXT, 1.0f)
OSR_TEST(IMECommitText2x, OSR_TEST_IME_COMMIT_TEXT, 2.0f)
OSR_TEST(IMEFinishComposition, OSR_TEST_IME_FINISH_COMPOSITION, 1.0f)
OSR_TEST(IMEFinishComposition2x, OSR_TEST_IME_FINISH_COMPOSITION, 2.0f)
OSR_TEST(IMECancelComposition, OSR_TEST_IME_CANCEL_COMPOSITION, 1.0f)
OSR_TEST(IMECancelComposition2x, OSR_TEST_IME_CANCEL_COMPOSITION, 2.0f)
OSR_TEST(TextSelectionChanged, OSR_TEST_TEXT_SELECTION_CHANGE, 1.0f)
OSR_TEST(TextSelectionChanged2x, OSR_TEST_TEXT_SELECTION_CHANGE, 2.0f)
OSR_TEST(VirtualKeyboard, OSR_TEST_VIRTUAL_KEYBOARD, 1.0f)
OSR_TEST(TouchStart, OSR_TEST_TOUCH_START, 1.0f)
OSR_TEST(TouchStart2x, OSR_TEST_TOUCH_START, 2.0f)
OSR_TEST(TouchMove, OSR_TEST_TOUCH_MOVE, 1.0f)
OSR_TEST(TouchMove2x, OSR_TEST_TOUCH_MOVE, 2.0f)
OSR_TEST(TouchEnd, OSR_TEST_TOUCH_END, 1.0f)
OSR_TEST(TouchEnd2x, OSR_TEST_TOUCH_END, 2.0f)
OSR_TEST(TouchCancel, OSR_TEST_TOUCH_CANCEL, 1.0f)
OSR_TEST(TouchCancel2x, OSR_TEST_TOUCH_CANCEL, 2.0f)
OSR_TEST(PenEvent, OSR_TEST_PEN, 1.0f)
