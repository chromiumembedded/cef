// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_bind.h"
#include "include/base/cef_logging.h"
#include "include/cef_v8.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/geometry_util.h"
#include "tests/shared/browser/resource_util.h"

#if defined(OS_MACOSX)
#include <Carbon/Carbon.h>  // For character codes.
#include "tests/ceftests/os_rendering_unittest_mac.h"
#elif defined(OS_LINUX)
#include <X11/keysym.h>
#elif defined(OS_WIN)
// Required for resource_util_win, which uses this as an extern
HINSTANCE hInst = ::GetModuleHandle(NULL);
#endif

namespace {

const char kTestUrl[] = "http://tests/osrtest";

// this html should render on a 600 x 400 window with a little vertical
// offset with scrollbar.

// default osr widget size
const int kOsrWidth = 600;
const int kOsrHeight = 400;

// precomputed bounding client rects for html elements (h1 and li).
#if defined(OS_WIN) || defined(OS_LINUX)
const CefRect kExpectedRectLI[] = {
  CefRect(8, 8, 567, 74),    // LI00
  CefRect(27, 103, 548, 20),  // LI01
  CefRect(27, 123, 548, 20),  // LI02
  CefRect(27, 143, 548, 20),  // LI03
  CefRect(27, 163, 548, 20),  // LI04
  CefRect(27, 183, 548, 20),  // LI05
  CefRect(27, 203, 548, 20),  // LI06
  CefRect(27, 223, 548, 20),  // LI07
  CefRect(27, 243, 548, 26),  // LI08
  CefRect(27, 269, 548, 26),  // LI09
  CefRect(27, 295, 548, 20),  // LI10
};
#elif defined(OS_MACOSX)
const CefRect kExpectedRectLI[] = {
  CefRect(8, 8, 584, 74),    // LI00
  CefRect(28, 103, 564, 18),  // LI01
  CefRect(28, 121, 564, 18),  // LI02
  CefRect(28, 139, 564, 18),  // LI03
  CefRect(28, 157, 564, 18),  // LI04
  CefRect(28, 175, 564, 18),  // LI05
  CefRect(28, 193, 564, 18),  // LI06
  CefRect(28, 211, 564, 18),  // LI07
  CefRect(28, 229, 564, 23),  // LI08
  CefRect(28, 252, 564, 26),  // LI09
  CefRect(18, 291, 360, 21),  // LI10
};
#else
#error "Unsupported platform"
#endif  // defined(OS_WIN)

// bounding client rects for edit box and navigate button
#if defined(OS_WIN)
const CefRect kEditBoxRect(412, 245, 60, 22);
const CefRect kNavigateButtonRect(360, 271, 140, 22);
const CefRect kSelectRect(467, 22, 75, 20);
const CefRect kExpandedSelectRect(463, 42, 81, 334);
const CefRect kDropDivRect(8, 332, 52, 52);
const CefRect kDragDivRect(71, 342, 30, 30);
const int kDefaultVerticalScrollbarWidth = 17;
const int kVerticalScrollbarWidth = GetSystemMetrics(SM_CXVSCROLL);
#elif defined(OS_MACOSX)
const CefRect kEditBoxRect(442, 251, 46, 16);
const CefRect kNavigateButtonRect(375, 275, 130, 20);
const CefRect kSelectRect(461, 21, 87, 26);
const CefRect kExpandedSelectRect(463, 42, 75, 286);
const CefRect kDropDivRect(9, 330, 52, 52);
const CefRect kDragDivRect(60, 330, 52, 52);
#elif defined(OS_LINUX)
const CefRect kEditBoxRect(434, 246, 60, 20);
const CefRect kNavigateButtonRect(380, 271, 140, 22);
const CefRect kSelectRect(467, 22, 75, 20);
const CefRect kExpandedSelectRect(463, 42, 79, 334);
const CefRect kDropDivRect(8, 332, 52, 52);
const CefRect kDragDivRect(71, 342, 30, 30);
const int kDefaultVerticalScrollbarWidth = 14;
const int kVerticalScrollbarWidth = 14;
#else
#error "Unsupported platform"
#endif  // defined(OS_WIN)

// word to be written into edit box
const char kKeyTestWord[] = "done";

#if defined(OS_LINUX)

// From ui/events/keycodes/keyboard_codes_posix.h
#define VKEY_D        0x44
#define VKEY_O        0x4F
#define VKEY_N        0x4E
#define VKEY_E        0x45
#define VKEY_ESCAPE   0x1B

const unsigned int kNativeKeyTestCodes[] = {
  XK_d,
  XK_o,
  XK_n,
  XK_e
};

const unsigned int kNativeKeyEscape = XK_Escape;

#elif defined(OS_MACOSX)

// See kKeyCodesMap in ui/events/keycodes/keyboard_code_conversion_mac.mm
#define VKEY_D        'd'
#define VKEY_O        'o'
#define VKEY_N        'n'
#define VKEY_E        'e'
#define VKEY_ESCAPE   kEscapeCharCode

const unsigned int kNativeKeyTestCodes[] = {
  kVK_ANSI_D,
  kVK_ANSI_O,
  kVK_ANSI_N,
  kVK_ANSI_E
};

const unsigned int kNativeKeyEscape = kVK_Escape;

#endif

#if defined(OS_MACOSX) || defined(OS_LINUX)

const unsigned int kKeyTestCodes[] = {
  VKEY_D,
  VKEY_O,
  VKEY_N,
  VKEY_E
};

#endif

// test type
enum OSRTestType {
  // IsWindowRenderingDisabled should be true
  OSR_TEST_IS_WINDOWLESS,
  // focusing webview, LI00 will get red & repainted
  OSR_TEST_FOCUS,
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

  // Define the range for popup tests.
  OSR_TEST_POPUP_FIRST = OSR_TEST_POPUP_PAINT,
  OSR_TEST_POPUP_LAST  = OSR_TEST_POPUP_SCROLL_INSIDE,
};

// Used in the browser process.
class OSRTestHandler : public RoutingTestHandler,
                       public CefFocusHandler,
                       public CefRenderHandler,
                       public CefContextMenuHandler {
 public:
  OSRTestHandler(OSRTestType test_type,
                 float scale_factor)
      : test_type_(test_type),
        scale_factor_(scale_factor),
        event_count_(0),
        event_total_(1),
        started_(false) {
  }

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

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      int httpStatusCode) override {
    if (!started())
      return;

    switch(test_type_) {
      case OSR_TEST_KEY_EVENTS: {
        const std::string& expected_url =
            std::string(kTestUrl) + "?k=" + kKeyTestWord;
        EXPECT_STREQ(expected_url.c_str(),
                     frame->GetURL().ToString().c_str());
        DestroySucceededTestSoon();
        } break;
      case OSR_TEST_IME_COMMIT_TEXT: {
        const std::string& expected_url =
            std::string(kTestUrl) + "?k=osrimecommit";
        EXPECT_STREQ(expected_url.c_str(),
                     frame->GetURL().ToString().c_str());
        DestroySucceededTestSoon();
      } break;
      case OSR_TEST_IME_FINISH_COMPOSITION: {
        const std::string& expected_url =
            std::string(kTestUrl) + "?k=" + kKeyTestWord;
        EXPECT_STREQ(expected_url.c_str(),
                     frame->GetURL().ToString().c_str());
        DestroySucceededTestSoon();
      } break;
      case OSR_TEST_IME_CANCEL_COMPOSITION: {
        const std::string& expected_url =
            std::string(kTestUrl) + "?k=";
        EXPECT_STREQ(expected_url.c_str(),
                     frame->GetURL().ToString().c_str());
        DestroySucceededTestSoon();
      } break;
      default:
        // Intentionally left blank
        break;
    }
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    EXPECT_TRUE(browser.get());

    if (!started())
      return false;

    const std::string& messageStr = request;
    switch(test_type_) {
      case OSR_TEST_FOCUS:
        EXPECT_STREQ(messageStr.c_str(), "osrfocus");
        DestroySucceededTestSoon();
        break;
      case OSR_TEST_CLICK_LEFT:
        EXPECT_STREQ(messageStr.c_str(), "osrclick0");
        DestroySucceededTestSoon();
        break;
      case OSR_TEST_MOUSE_MOVE:
        EXPECT_STREQ(messageStr.c_str(), "osrmousemove");
        DestroySucceededTestSoon();
        break;
      case OSR_TEST_DRAG_DROP_DROP:
        EXPECT_STREQ(messageStr.c_str(), "osrdrop");
        DestroySucceededTestSoon();
        break;
      default:
        // Intentionally left blank
        break;
    }

    callback->Success("");
    return true;
  }

  // CefClient methods, providing handlers
  CefRefPtr<CefFocusHandler> GetFocusHandler() override {
    return this;
  }

  CefRefPtr<CefRenderHandler> GetRenderHandler() override {
    return this;
  }

  CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override {
    return this;
  }

  CefRefPtr<CefRequestHandler> GetRequestHandler() override {
    return this;
  }

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

    return NULL;
  }

  // CefRenderHandler methods
  bool GetViewRect(CefRefPtr<CefBrowser> browser,
                   CefRect& rect) override {
    if (test_type_ == OSR_TEST_RESIZE && started()) {
      rect = CefRect(0, 0, kOsrWidth * 2, kOsrHeight * 2);
      return true;
    }
    rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
    return true;
  }

  bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                      int viewX,
                      int viewY,
                      int& screenX,
                      int& screenY) override {
    if (test_type_ == OSR_TEST_SCREEN_POINT && started()) {
      const CefRect& expected_rect = GetExpectedRect(4);
      EXPECT_EQ(viewX, MiddleX(expected_rect));
      EXPECT_EQ(viewY, MiddleY(expected_rect));
      DestroySucceededTestSoon();
    } else if (test_type_ == OSR_TEST_CONTEXT_MENU && started()){
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
    // setting the screen to the test window size does not affect its rectangle.
    screen_info.rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
    screen_info.available_rect = screen_info.rect;
    return true;
  }

  void OnPopupShow(CefRefPtr<CefBrowser> browser,
                   bool show) override {
    if (show && started()) {
      switch (test_type_) {
        case OSR_TEST_POPUP_SHOW:
          if (!succeeded()) {
            EXPECT_TRUE(show);
            DestroySucceededTestSoon();
          }
          break;
        default:
          break;
      }
    }
    if (!show && started()) {
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
    if (started()) {
      switch (test_type_) {
      case OSR_TEST_POPUP_SIZE:
        EXPECT_EQ(kExpandedSelectRect.x, rect.x);
        EXPECT_EQ(kExpandedSelectRect.y, rect.y);
        if (ExpectComputedPopupSize()) {
          EXPECT_EQ(kExpandedSelectRect.width, rect.width);
          EXPECT_EQ(kExpandedSelectRect.height, rect.height);
        } else {
          EXPECT_GT(rect.width, kExpandedSelectRect.width);
          EXPECT_GT(rect.height, kExpandedSelectRect.height);
        }
        DestroySucceededTestSoon();
        break;
      default:
        break;
      }
    }
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirtyRects,
               const void* buffer,
               int width, int height) override {
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
    if (IsBackgroundInBuffer(reinterpret_cast<const uint32*>(buffer),
                             width * height,
                             test_type_ == OSR_TEST_TRANSPARENCY ?
                                0x00000000 :
                                0xFFFFFFFF))
      return;

    // Send events after the first full repaint
    switch (test_type_) {
      case OSR_TEST_PAINT:
        if (StartTest()) {
          // test that we have a full repaint
          EXPECT_EQ(dirtyRects.size(), 1U);
          EXPECT_TRUE(IsFullRepaint(dirtyRects[0], GetScaledInt(kOsrWidth),
                                                   GetScaledInt(kOsrHeight)));
          EXPECT_EQ(0xffff7f7fU, *(reinterpret_cast<const uint32*>(buffer)));
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_TRANSPARENCY:
        if (StartTest()) {
          // test that we have a full repaint
          EXPECT_EQ(dirtyRects.size(), 1U);
          EXPECT_TRUE(IsFullRepaint(dirtyRects[0], GetScaledInt(kOsrWidth),
                                                   GetScaledInt(kOsrHeight)));
          EXPECT_EQ(0x80800000U, *(reinterpret_cast<const uint32*>(buffer)));
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_FOCUS:
        if (StartTest()) {
          // body.onfocus will make LI00 red
          browser->GetHost()->SendFocusEvent(true);
        }
        break;
      case OSR_TEST_CURSOR:
        if (StartTest()) {
          // make mouse leave first
          CefMouseEvent mouse_event;
          mouse_event.x = 0;
          mouse_event.y = 0;
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseMoveEvent(mouse_event, true);
          // enter mouse in the LI2 element having hand cursor
          const CefRect& expected_rect = GetExpectedRect(2);
          mouse_event.x = MiddleX(expected_rect);
          mouse_event.y = MiddleY(expected_rect);
          browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
        }
        break;
      case OSR_TEST_MOUSE_MOVE:
        if (StartTest()) {
          CefMouseEvent mouse_event;
          const CefRect& expected_rect = GetExpectedRect(3);
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
          const CefRect& expected_rect = GetExpectedRect(4);
          mouse_event.x = MiddleX(expected_rect);
          mouse_event.y = MiddleY(expected_rect);
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseClickEvent(
              mouse_event, MBT_RIGHT, false, 1);
          browser->GetHost()->SendMouseClickEvent(
              mouse_event, MBT_RIGHT, true, 1);
        }
        break;
      case OSR_TEST_CLICK_LEFT:
        if (StartTest()) {
          CefMouseEvent mouse_event;
          const CefRect& expected_rect = GetExpectedRect(0);
          mouse_event.x = MiddleX(expected_rect);
          mouse_event.y = MiddleY(expected_rect);

          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseClickEvent(
              mouse_event, MBT_LEFT, false, 1);
          browser->GetHost()->SendMouseClickEvent(
              mouse_event, MBT_LEFT, true, 1);
        }
        break;
      case OSR_TEST_RESIZE:
        if (StartTest()) {
          browser->GetHost()->WasResized();
          // There may be some partial repaints before the full repaint.
        } else if (IsFullRepaint(dirtyRects[0], width, height)) {
          EXPECT_EQ(GetScaledInt(kOsrWidth) * 2, width);
          EXPECT_EQ(GetScaledInt(kOsrHeight) * 2, height);
          EXPECT_EQ(dirtyRects.size(), 1U);
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_INVALIDATE: {
        if (StartTest()) {
          browser->GetHost()->Invalidate(PET_VIEW);
        } else {
          EXPECT_EQ(dirtyRects.size(), 1U);
          EXPECT_EQ(dirtyRects[0],
                    GetScaledRect(CefRect(0, 0, kOsrWidth, kOsrHeight)));
          DestroySucceededTestSoon();
        }
        break;
      }
      case OSR_TEST_KEY_EVENTS:
        if (StartTest()) {
          // click inside edit box
          CefMouseEvent mouse_event;
          mouse_event.x = MiddleX(kEditBoxRect);
          mouse_event.y = MiddleY(kEditBoxRect);
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseClickEvent(
              mouse_event, MBT_LEFT, false, 1);
          browser->GetHost()->SendMouseClickEvent(
              mouse_event, MBT_LEFT, true, 1);

          // write "done" word
          CefKeyEvent event;
          event.is_system_key = false;
          event.modifiers = 0;

          size_t word_length = strlen(kKeyTestWord);
          for (size_t i = 0; i < word_length; ++i) {
#if defined(OS_WIN)
            SendKeyEvent(browser, kKeyTestWord[i]);
#elif defined(OS_MACOSX) || defined(OS_LINUX)
            SendKeyEvent(browser, kNativeKeyTestCodes[i], kKeyTestCodes[i]);
#else
#error "Unsupported platform"
#endif
          }
          // click button to navigate
          mouse_event.x = MiddleX(kNavigateButtonRect);
          mouse_event.y = MiddleY(kNavigateButtonRect);
          browser->GetHost()->SendMouseClickEvent(
              mouse_event, MBT_LEFT, false, 1);
          browser->GetHost()->SendMouseClickEvent(
              mouse_event, MBT_LEFT, true, 1);
        }
        break;
      case OSR_TEST_TOOLTIP:
        if (StartTest()) {
          CefMouseEvent mouse_event;
          const CefRect& expected_rect = GetExpectedRect(10);
          mouse_event.x = MiddleX(expected_rect);
          mouse_event.y = MiddleY(expected_rect);
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
        }
        break;
      case OSR_TEST_SCROLLING: {
        static const int deltaY = 10;
        if (StartTest()) {
          // scroll down once
          CefMouseEvent mouse_event;
          const CefRect& expected_rect = GetExpectedRect(0);
          mouse_event.x = MiddleX(expected_rect);
          mouse_event.y = MiddleY(expected_rect);
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseWheelEvent(mouse_event, 0, - deltaY);
        } else {
          EXPECT_EQ(dirtyRects.size(), 1U);
          const CefRect& expected_rect =
              GetScaledRect(CefRect(0, 0, kOsrWidth, kOsrHeight));
          // There may be some partial repaints before the full repaint.
          if (dirtyRects[0] == expected_rect)
            DestroySucceededTestSoon();
        }
        break;
      }
      case OSR_TEST_POPUP_HIDE_ON_CLICK:
        if (StartTest()) {
          ExpandDropDown();
          // Wait for the first popup paint to occur
        } else if (type == PET_POPUP) {
          CefMouseEvent mouse_event;
          mouse_event.x = 1;
          mouse_event.y = 1;
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseClickEvent(
              mouse_event, MBT_LEFT, false, 1);
        }
        break;
      case OSR_TEST_POPUP_HIDE_ON_SCROLL:
        if (StartTest()) {
          ExpandDropDown();
          // Wait for the first popup paint to occur
        } else if (type == PET_POPUP) {
          CefMouseEvent mouse_event;
          mouse_event.x = mouse_event.y = 1;
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseWheelEvent(mouse_event, 0, -10);
        }
        break;
      case OSR_TEST_POPUP_HIDE_ON_BLUR:
        if (StartTest()) {
          ExpandDropDown();
          // Wait for the first popup paint to occur
        } else if (type == PET_POPUP) {
          browser->GetHost()->SendFocusEvent(false);
        }
        break;
      case OSR_TEST_POPUP_HIDE_ON_ESC:
        if (StartTest()) {
          ExpandDropDown();
          // Wait for the first popup paint to occur
        } else if (type == PET_POPUP) {
#if defined(OS_WIN)
          SendKeyEvent(browser, VK_ESCAPE);
#elif defined(OS_MACOSX) || defined(OS_LINUX)
          SendKeyEvent(browser, kNativeKeyEscape, VKEY_ESCAPE);
#else
#error "Unsupported platform"
#endif
        }
        break;
      case OSR_TEST_POPUP_SHOW:
      case OSR_TEST_POPUP_SIZE:
        if (StartTest()) {
          ExpandDropDown();
        }
        break;
      case OSR_TEST_POPUP_PAINT:
        if (StartTest()) {
          ExpandDropDown();
        } else if (type == PET_POPUP) {
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

          // first pixel of border
#if defined(OS_MACOSX)
          EXPECT_EQ(0xff5d99d6, *(reinterpret_cast<const uint32*>(buffer)));
#elif defined(OS_LINUX) || defined(OS_WIN)
          EXPECT_EQ(0xff6497ea, *(reinterpret_cast<const uint32*>(buffer)));
#else
#error "Unsupported platform"
#endif
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
      case OSR_TEST_POPUP_SCROLL_INSIDE:
        {
          static enum {NotStarted, Started, Scrolled}
              scroll_inside_state = NotStarted;
          if (StartTest()) {
            ExpandDropDown();
            scroll_inside_state = Started;
          } else if (type == PET_POPUP) {
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
              EXPECT_EQ(0, dirtyRects[0].x);
              EXPECT_EQ(0, dirtyRects[0].y);
              if (ExpectComputedPopupSize()) {
                EXPECT_EQ(expanded_select_rect.width, dirtyRects[0].width);
                EXPECT_EQ(expanded_select_rect.height, dirtyRects[0].height);
              } else {
                EXPECT_GT(dirtyRects[0].width, kExpandedSelectRect.width);
                EXPECT_GT(dirtyRects[0].height, kExpandedSelectRect.height);
              }
              DestroySucceededTestSoon();
            }
          }
        }
        break;
      case OSR_TEST_DRAG_DROP_START_DRAGGING:
      case OSR_TEST_DRAG_DROP_UPDATE_CURSOR:
      case OSR_TEST_DRAG_DROP_DROP:
        {
          // trigger the StartDragging event
          if (StartTest()) {
            // move the mouse over the element to drag
            CefMouseEvent mouse_event;
            mouse_event.x = MiddleX(kDragDivRect);
            mouse_event.y = MiddleY(kDragDivRect);
            mouse_event.modifiers = 0;
            browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
            // click on the element to drag
            mouse_event.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
            browser->GetHost()->SendMouseClickEvent(mouse_event, MBT_LEFT,
                                                    false, 1);
            // move the mouse to start dragging
            mouse_event.x -= 5;
            mouse_event.y -= 5;
            browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
          }
        }
        break;
      case OSR_TEST_IME_COMMIT_TEXT:
        {
          // trigger the IME Set Composition event
          if (StartTest()) {
            // click inside edit box so that text could be entered
            CefMouseEvent mouse_event;
            mouse_event.x = MiddleX(kEditBoxRect);
            mouse_event.y = MiddleY(kEditBoxRect);
            mouse_event.modifiers = 0;
            browser->GetHost()->SendMouseClickEvent(
                                      mouse_event, MBT_LEFT, false, 1);
            browser->GetHost()->SendMouseClickEvent(
                                      mouse_event, MBT_LEFT, true, 1);

            size_t word_length = strlen(kKeyTestWord);
            // Add some input keys to edit box
            for (size_t i = 0; i < word_length; ++i) {
#if defined(OS_WIN)
              SendKeyEvent(browser, kKeyTestWord[i]);
#elif defined(OS_MACOSX) || defined(OS_LINUX)
              SendKeyEvent(browser, kNativeKeyTestCodes[i], kKeyTestCodes[i]);
#else
#error "Unsupported platform"
#endif
            }
            // This text should be honored instead of 'ka' added via key events
            CefString markedText("osrimecommit");

            CefRange range(0, static_cast<int>(markedText.length()));
            browser->GetHost()->ImeCommitText(markedText, range, 0);

            // click button to navigate
            mouse_event.x = MiddleX(kNavigateButtonRect);
            mouse_event.y = MiddleY(kNavigateButtonRect);
            browser->GetHost()->SendMouseClickEvent(
                                mouse_event, MBT_LEFT, false, 1);
            browser->GetHost()->SendMouseClickEvent(
                                mouse_event, MBT_LEFT, true, 1);
          }
        }
        break;
      case OSR_TEST_IME_FINISH_COMPOSITION:
        {
          // trigger the IME Set Composition event
          if (StartTest()) {
            // click inside edit box so that text could be entered
            CefMouseEvent mouse_event;
            mouse_event.x = MiddleX(kEditBoxRect);
            mouse_event.y = MiddleY(kEditBoxRect);
            mouse_event.modifiers = 0;
            browser->GetHost()->SendMouseClickEvent(mouse_event, MBT_LEFT,
                                                    false, 1);
            browser->GetHost()->SendMouseClickEvent(mouse_event, MBT_LEFT,
                                                    true, 1);

            size_t word_length = strlen(kKeyTestWord);
            // Add some input keys to edit box
            for (size_t i = 0; i < word_length; ++i) {
#if defined(OS_WIN)
              SendKeyEvent(browser, kKeyTestWord[i]);
#elif defined(OS_MACOSX) || defined(OS_LINUX)
              SendKeyEvent(browser, kNativeKeyTestCodes[i], kKeyTestCodes[i]);
#else
#error "Unsupported platform"
#endif
            }

            // Finish Composition should set the existing composition
            browser->GetHost()->ImeFinishComposingText(true);

            // click button to navigate
            mouse_event.x = MiddleX(kNavigateButtonRect);
            mouse_event.y = MiddleY(kNavigateButtonRect);
            browser->GetHost()->SendMouseClickEvent(mouse_event, MBT_LEFT,
                                                    false, 1);
            browser->GetHost()->SendMouseClickEvent(mouse_event, MBT_LEFT,
                                                    true, 1);
          }
        }
        break;
      case OSR_TEST_IME_CANCEL_COMPOSITION:
        {
          // trigger the IME Set Composition event
          if (StartTest()) {
            // click inside edit box so that text could be entered
            CefMouseEvent mouse_event;
            mouse_event.x = MiddleX(kEditBoxRect);
            mouse_event.y = MiddleY(kEditBoxRect);
            mouse_event.modifiers = 0;
            browser->GetHost()->SendMouseClickEvent(
                                    mouse_event, MBT_LEFT, false, 1);
            browser->GetHost()->SendMouseClickEvent(
                                    mouse_event, MBT_LEFT, true, 1);
            // Add some input keys to edit box
            CefString markedText("か");
            std::vector<CefCompositionUnderline> underlines;

            // Use a thin black underline by default.
            CefRange range(0, static_cast<int>(markedText.length()));
            cef_composition_underline_t line = {
              range, 0xFF000000, 0, false
            };
            underlines.push_back(line);

            CefRange replacement_range(0,
                static_cast<int>(markedText.length()));
            CefRange selection_range(0, static_cast<int>(markedText.length()));

            // Composition should be updated
            browser->GetHost()->ImeSetComposition(markedText, underlines,
                                          replacement_range,selection_range);

            // CancelComposition should clean up the edit text
            browser->GetHost()->ImeCancelComposition();

            // click button to navigate and verify
            mouse_event.x = MiddleX(kNavigateButtonRect);
            mouse_event.y = MiddleY(kNavigateButtonRect);
            browser->GetHost()->SendMouseClickEvent(
                                    mouse_event, MBT_LEFT, false, 1);
            browser->GetHost()->SendMouseClickEvent(
                                    mouse_event, MBT_LEFT, true, 1);
          }
        }
        break;
      case OSR_TEST_IME_SET_COMPOSITION:
        {
          // trigger the IME Set Composition event
          if (StartTest()) {
            // click inside edit box so that text could be entered
            CefMouseEvent mouse_event;
            mouse_event.x = MiddleX(kEditBoxRect);
            mouse_event.y = MiddleY(kEditBoxRect);
            mouse_event.modifiers = 0;
            browser->GetHost()->SendMouseClickEvent(
                                 mouse_event, MBT_LEFT, false, 1);
            browser->GetHost()->SendMouseClickEvent(
                                 mouse_event, MBT_LEFT, true, 1);

            // Now set some intermediate text composition
            CefString markedText("か");
            std::vector<CefCompositionUnderline> underlines;

            // Use a thin black underline by default.
            CefRange range(0, static_cast<int>(markedText.length()));
            cef_composition_underline_t line = {
              range, 0xFF000000, 0, false
            };
            underlines.push_back(line);

            CefRange replacement_range(0,
                static_cast<int>(markedText.length()));
            CefRange selection_range(0, static_cast<int>(markedText.length()));

            // This should update composition range and
            // trigger the compositionRangeChanged callback
            browser->GetHost()->ImeSetComposition(markedText, underlines,
                                          replacement_range,selection_range);
          }
        }
        break;
      default:
        break;
    }
  }

  bool OnSetFocus(CefRefPtr<CefBrowser> browser,
                  FocusSource source) override {
    if (source == FOCUS_SOURCE_NAVIGATION) {
      got_navigation_focus_event_.yes();

      // Ignore focus from the original navigation when we're testing focus
      // event delivery.
      if (test_type_ == OSR_TEST_FOCUS)
        return true;
      return false;
    }

    EXPECT_EQ(source, FOCUS_SOURCE_SYSTEM);
    got_system_focus_event_.yes();
    return false;
  }

  void OnCursorChange(CefRefPtr<CefBrowser> browser,
                      CefCursorHandle cursor,
                      CursorType type,
                      const CefCursorInfo& custom_cursor_info) override {
    if (test_type_ == OSR_TEST_CURSOR && started()) {
      EXPECT_EQ(CT_HAND, type);
      EXPECT_EQ(NULL, custom_cursor_info.buffer);
      DestroySucceededTestSoon();
    }
  }

  void OnImeCompositionRangeChanged(CefRefPtr<CefBrowser> browser,
                      const CefRange& range,
                      const CefRenderHandler::RectList& bounds) override {
    if (test_type_ == OSR_TEST_IME_SET_COMPOSITION && started()) {
      EXPECT_EQ(range.from, 0);
      EXPECT_EQ(range.to, 1);
      EXPECT_EQ(1U, bounds.size());
      DestroySucceededTestSoon();
    }
  }

  bool StartDragging(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefDragData> drag_data,
                     CefRenderHandler::DragOperationsMask allowed_ops,
                     int x, int y) override {
    if (test_type_ == OSR_TEST_DRAG_DROP_START_DRAGGING && started()) {
      DestroySucceededTestSoon();
      return false;
    } else if ((test_type_ == OSR_TEST_DRAG_DROP_UPDATE_CURSOR ||
        test_type_ == OSR_TEST_DRAG_DROP_DROP) && started()) {
      // place the mouse over the drop area to trigger UpdateDragCursor
      CefRefPtr<CefDragData> data = drag_data->Clone();
      data->ResetFileContents();
      CefMouseEvent ev;
      ev.x = MiddleX(kDragDivRect) - 5;
      ev.y = MiddleY(kDragDivRect) - 5;
      ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
      browser->GetHost()->DragTargetDragEnter(data, ev, allowed_ops);

      ev.x = MiddleX(kDropDivRect);
      ev.y = MiddleY(kDropDivRect);
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
    if (test_type_ == OSR_TEST_DRAG_DROP_UPDATE_CURSOR && started()) {
      if (operation != DRAG_OPERATION_NONE) {
        browser->GetHost()->DragSourceEndedAt(MiddleX(kDropDivRect),
                                              MiddleY(kDropDivRect),
                                              DRAG_OPERATION_NONE);
        browser->GetHost()->DragSourceSystemDragEnded();
        DestroySucceededTestSoon();
      }
    } else if (test_type_ == OSR_TEST_DRAG_DROP_DROP && started()) {
      // Don't end the drag multiple times.
      if (got_update_cursor_)
        return;
      got_update_cursor_.yes();

      CefMouseEvent ev;
      ev.x = MiddleX(kDropDivRect);
      ev.y = MiddleY(kDropDivRect);
      ev.modifiers = 0;
      browser->GetHost()->SendMouseClickEvent(ev, MBT_LEFT, true, 1);
      browser->GetHost()->DragTargetDrop(ev);
      browser->GetHost()->DragSourceEndedAt(ev.x, ev.y, operation);
      browser->GetHost()->DragSourceSystemDragEnded();
    }
  }

  bool OnTooltip(CefRefPtr<CefBrowser> browser,
                 CefString& text) override {
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
    if (!started())
      return;
    if (test_type_ == OSR_TEST_CLICK_RIGHT) {
      const CefRect& expected_rect = GetExpectedRect(4);
      EXPECT_EQ(params->GetXCoord(), MiddleX(expected_rect));
      EXPECT_EQ(params->GetYCoord(), MiddleY(expected_rect));
      DestroySucceededTestSoon();
    } else if (test_type_ == OSR_TEST_CONTEXT_MENU) {
      // This test will pass if it does not crash on destruction
      DestroySucceededTestSoon();
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
#elif defined(OS_MACOSX)
    // An actual vies is needed only for the ContextMenu test. The menu runner
    // checks if the view is not nil before showing the context menu.
    if (test_type_ == OSR_TEST_CONTEXT_MENU)
      windowInfo.SetAsWindowless(osr_unittests::GetFakeView());
    else
      windowInfo.SetAsWindowless(kNullWindowHandle);
#elif defined(OS_LINUX)
    windowInfo.SetAsWindowless(kNullWindowHandle);
#else
#error "Unsupported platform"
#endif
    CefBrowserHost::CreateBrowser(windowInfo, this, url, settings, NULL);
  }

  CefRect GetScaledRect(const CefRect& rect) const {
    return client::LogicalToDevice(rect, scale_factor_);
  }

  int GetScaledInt(int value) const {
    return client::LogicalToDevice(value, scale_factor_);
  }

  CefRect GetExpectedRect(int index) {
    CefRect rect = kExpectedRectLI[index];
#if defined(OS_WIN) || defined(OS_LINUX)
     // Adjust the rect to include system vertical scrollbar width.
    rect.width += kDefaultVerticalScrollbarWidth - kVerticalScrollbarWidth;
#elif !defined(OS_MACOSX)
    #error "Unsupported platform"
#endif

    return rect;
  }

  static bool IsFullRepaint(const CefRect& rc, int width, int height) {
    return rc.width == width && rc.height == height;
  }

  static bool IsBackgroundInBuffer(const uint32* buffer, size_t size,
                                   uint32 rgba) {
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
#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MACOSX))
    // On Windows the device scale factor is ignored in Blink when computing
    // the default form control font size (see https://crbug.com/674663#c11).
    // This results in better font size display but also means that we won't
    // get the expected (scaled) width/height value for non-1.0 scale factor
    // select popups.
    // On both Windows and Linux the non-1.0 scale factor size is off by a few
    // pixels so we can't perform an exact comparison.
    return scale_factor_ == 1.0;
#else
    return true;
#endif
  }

  void DestroySucceededTestSoon() {
    if (succeeded())
      return;
    if (++event_count_ == event_total_)
      CefPostTask(TID_UI, base::Bind(&OSRTestHandler::DestroyTest, this));
  }

  void DestroyTest() override {
    // Always get the OnSetFocus call for the initial navigation.
    EXPECT_TRUE(got_navigation_focus_event_);

    if (test_type_ == OSR_TEST_FOCUS ||
        (test_type_ >= OSR_TEST_POPUP_FIRST &&
         test_type_ <= OSR_TEST_POPUP_LAST)) {
      // SetFocus is called by the system when we explicitly set the focus and
      // when popups are dismissed.
      EXPECT_TRUE(got_system_focus_event_);
    } else {
      EXPECT_FALSE(got_system_focus_event_);
    }

    RoutingTestHandler::DestroyTest();
  }

  void ExpandDropDown() {
    GetBrowser()->GetHost()->SendFocusEvent(true);
    CefMouseEvent mouse_event;
    mouse_event.x = MiddleX(kSelectRect);
    mouse_event.y = MiddleY(kSelectRect);
    mouse_event.modifiers = 0;
    GetBrowser()->GetHost()->SendMouseClickEvent(
        mouse_event, MBT_LEFT, false, 1);
  }

  void SendKeyEvent(CefRefPtr<CefBrowser> browser,
#if defined(OS_LINUX) || defined(OS_MACOSX)
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
                                            1;  // key repeat count
    event.windows_key_code = VkCode;
#elif defined(OS_MACOSX)
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
  bool started() { return started_; }

  // true if the exit point was reached, even the result is not
  // the expected one
  bool succeeded() { return (event_count_ == event_total_); }

  // will mark test as started and will return true only the first time
  // it is called
  bool StartTest() {
    if (started_)
      return false;
    started_ = true;
    return true;
  }

 private:
  OSRTestType test_type_;
  float scale_factor_;
  int event_count_;
  int event_total_;
  bool started_;
  TrackCallback got_update_cursor_;
  TrackCallback got_navigation_focus_event_;
  TrackCallback got_system_focus_event_;

  IMPLEMENT_REFCOUNTING(OSRTestHandler);
};

}  // namespace

// generic test
#define OSR_TEST(name, test_mode, scale_factor)\
TEST(OSRTest, name) {\
  CefRefPtr<OSRTestHandler> handler = \
      new OSRTestHandler(test_mode, scale_factor);\
  handler->ExecuteTest();\
  EXPECT_TRUE(handler->succeeded());\
  ReleaseAndWaitForDestructor(handler);\
}

// tests
OSR_TEST(Windowless, OSR_TEST_IS_WINDOWLESS, 1.0f);
OSR_TEST(Windowless2x, OSR_TEST_IS_WINDOWLESS, 2.0f);
OSR_TEST(Focus, OSR_TEST_FOCUS, 1.0f);
OSR_TEST(Focus2x, OSR_TEST_FOCUS, 2.0f);
OSR_TEST(Paint, OSR_TEST_PAINT, 1.0f);
OSR_TEST(Paint2x, OSR_TEST_PAINT, 2.0f);
OSR_TEST(TransparentPaint, OSR_TEST_TRANSPARENCY, 1.0f);
OSR_TEST(TransparentPaint2x, OSR_TEST_TRANSPARENCY, 2.0f);
OSR_TEST(Cursor, OSR_TEST_CURSOR, 1.0f);
OSR_TEST(Cursor2x, OSR_TEST_CURSOR, 2.0f);
OSR_TEST(MouseMove, OSR_TEST_MOUSE_MOVE, 1.0f);
OSR_TEST(MouseMove2x, OSR_TEST_MOUSE_MOVE, 2.0f);
OSR_TEST(MouseRightClick, OSR_TEST_CLICK_RIGHT, 1.0f);
OSR_TEST(MouseRightClick2x, OSR_TEST_CLICK_RIGHT, 2.0f);
OSR_TEST(MouseLeftClick, OSR_TEST_CLICK_LEFT, 1.0f);
OSR_TEST(MouseLeftClick2x, OSR_TEST_CLICK_LEFT, 2.0f);
OSR_TEST(ScreenPoint, OSR_TEST_SCREEN_POINT, 1.0f);
OSR_TEST(ScreenPoint2x, OSR_TEST_SCREEN_POINT, 2.0f);
OSR_TEST(Resize, OSR_TEST_RESIZE, 1.0f);
OSR_TEST(Resize2x, OSR_TEST_RESIZE, 2.0f);
OSR_TEST(Invalidate, OSR_TEST_INVALIDATE, 1.0f);
OSR_TEST(Invalidate2x, OSR_TEST_INVALIDATE, 2.0f);
OSR_TEST(KeyEvents, OSR_TEST_KEY_EVENTS, 1.0f);
OSR_TEST(KeyEvents2x, OSR_TEST_KEY_EVENTS, 2.0f);
OSR_TEST(Tooltip, OSR_TEST_TOOLTIP, 1.0f);
OSR_TEST(Tooltip2x, OSR_TEST_TOOLTIP, 2.0f);
OSR_TEST(Scrolling, OSR_TEST_SCROLLING, 1.0f);
OSR_TEST(Scrolling2x, OSR_TEST_SCROLLING, 2.0f);
OSR_TEST(ContextMenu, OSR_TEST_CONTEXT_MENU, 1.0f);
OSR_TEST(ContextMenu2x, OSR_TEST_CONTEXT_MENU, 2.0f);
OSR_TEST(PopupPaint, OSR_TEST_POPUP_PAINT, 1.0f);
OSR_TEST(PopupPaint2x, OSR_TEST_POPUP_PAINT, 2.0f);
OSR_TEST(PopupShow, OSR_TEST_POPUP_SHOW, 1.0f);
OSR_TEST(PopupShow2x, OSR_TEST_POPUP_SHOW, 2.0f);
OSR_TEST(PopupSize, OSR_TEST_POPUP_SIZE, 1.0f);
OSR_TEST(PopupSize2x, OSR_TEST_POPUP_SIZE, 2.0f);
OSR_TEST(PopupHideOnBlur, OSR_TEST_POPUP_HIDE_ON_BLUR, 1.0f);
OSR_TEST(PopupHideOnBlur2x, OSR_TEST_POPUP_HIDE_ON_BLUR, 2.0f);
OSR_TEST(PopupHideOnClick, OSR_TEST_POPUP_HIDE_ON_CLICK, 1.0f);
OSR_TEST(PopupHideOnClick2x, OSR_TEST_POPUP_HIDE_ON_CLICK, 2.0f);
OSR_TEST(PopupHideOnScroll, OSR_TEST_POPUP_HIDE_ON_SCROLL, 1.0f);
OSR_TEST(PopupHideOnScroll2x, OSR_TEST_POPUP_HIDE_ON_SCROLL, 2.0f);
OSR_TEST(PopupHideOnEsc, OSR_TEST_POPUP_HIDE_ON_ESC, 1.0f);
OSR_TEST(PopupHideOnEsc2x, OSR_TEST_POPUP_HIDE_ON_ESC, 2.0f);
OSR_TEST(PopupScrollInside, OSR_TEST_POPUP_SCROLL_INSIDE, 1.0f);
OSR_TEST(PopupScrollInside2x, OSR_TEST_POPUP_SCROLL_INSIDE, 2.0f);
OSR_TEST(DragDropStartDragging, OSR_TEST_DRAG_DROP_START_DRAGGING, 1.0f);
OSR_TEST(DragDropStartDragging2x, OSR_TEST_DRAG_DROP_START_DRAGGING, 2.0f);
OSR_TEST(DragDropUpdateCursor, OSR_TEST_DRAG_DROP_UPDATE_CURSOR, 1.0f);
OSR_TEST(DragDropUpdateCursor2x, OSR_TEST_DRAG_DROP_UPDATE_CURSOR, 2.0f);
OSR_TEST(DragDropDropElement, OSR_TEST_DRAG_DROP_DROP, 1.0f);
OSR_TEST(DragDropDropElement2x, OSR_TEST_DRAG_DROP_DROP, 2.0f);
OSR_TEST(IMESetComposition, OSR_TEST_IME_SET_COMPOSITION, 1.0f);
OSR_TEST(IMESetComposition2x, OSR_TEST_IME_SET_COMPOSITION, 2.0f);
OSR_TEST(IMECommitText, OSR_TEST_IME_COMMIT_TEXT, 1.0f);
OSR_TEST(IMECommitText2x, OSR_TEST_IME_COMMIT_TEXT, 2.0f);
OSR_TEST(IMEFinishComposition, OSR_TEST_IME_FINISH_COMPOSITION, 1.0f);
OSR_TEST(IMEFinishComposition2x, OSR_TEST_IME_FINISH_COMPOSITION, 2.0f);
OSR_TEST(IMECancelComposition, OSR_TEST_IME_CANCEL_COMPOSITION, 1.0f);
OSR_TEST(IMECancelComposition2x, OSR_TEST_IME_CANCEL_COMPOSITION, 2.0f);
