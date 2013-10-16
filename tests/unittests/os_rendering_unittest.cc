// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_runnable.h"
#include "include/cef_v8.h"
#include "include/wrapper/cef_stream_resource_handler.h"

#include "tests/cefclient/client_app.h"
#include "tests/cefclient/resource_util.h"
#include "tests/unittests/test_handler.h"

#include "base/logging.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

#if defined(OS_MACOSX)
#include "tests/unittests/os_rendering_unittest_mac.h"
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

// #define DEBUGGER_ATTACHED

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
  CefRect(20, 278, 572, 18),  // LI10
};
#else
#error "Unsupported platform"
#endif  // defined(OS_WIN)

// bounding client rects for edit box and navigate button
#if defined(OS_WIN)
const CefRect kEditBoxRect(412, 245, 60, 22);
const CefRect kNavigateButtonRect(360, 271, 140, 22);
const CefRect kSelectRect(467, 22, 75, 20);
const CefRect kExpandedSelectRect(467, 42, 81, 322);
const int kDefaultVerticalScrollbarWidth = 17;
const int kVerticalScrollbarWidth = GetSystemMetrics(SM_CXVSCROLL);
const int kHorizontalScrollbarWidth = GetSystemMetrics(SM_CXHSCROLL);
#elif defined(OS_MACOSX)
const CefRect kEditBoxRect(429, 228, 60, 25);
const CefRect kNavigateButtonRect(375, 251, 138, 28);
const CefRect kSelectRect(461, 21, 87, 26);
const CefRect kExpandedSelectRect(467, 42, 80, 262);
#elif defined(OS_LINUX)
const CefRect kEditBoxRect(434, 246, 60, 20);
const CefRect kNavigateButtonRect(380, 271, 140, 22);
const CefRect kSelectRect(467, 22, 75, 20);
const CefRect kExpandedSelectRect(467, 42, 79, 322);
const int kDefaultVerticalScrollbarWidth = 14;
const int kVerticalScrollbarWidth = 14;
const int kHorizontalScrollbarWidth = 14;
#else
#error "Unsupported platform"
#endif  // defined(OS_WIN)

// adjusted expected rect regarding system vertical scrollbar width
CefRect ExpectedRect(int index) {
#if defined(OS_WIN) || defined(OS_LINUX)
  // this is the scrollbar width for all the kExpectedRectLI
  if (kDefaultVerticalScrollbarWidth == kVerticalScrollbarWidth)
    return kExpectedRectLI[index];

  CefRect adjustedRect(kExpectedRectLI[index]);
  adjustedRect.width += kDefaultVerticalScrollbarWidth -
                        kVerticalScrollbarWidth;
  return adjustedRect;
#elif defined(OS_MACOSX)
  return kExpectedRectLI[index];
#else
#error "Unsupported platform"
#endif
}

// word to be written into edit box
const char kKeyTestWord[] = "done";

#if defined(OS_MACOSX)
const ui::KeyboardCode kKeyTestCodes[] = {
  ui::VKEY_D,
  ui::VKEY_O,
  ui::VKEY_N,
  ui::VKEY_E
};
#elif defined(OS_LINUX)
const unsigned int kKeyTestCodes[] = {
  XK_d,
  XK_o,
  XK_n,
  XK_e
};
#endif

// width for the icon that appear on the screen when pressing
// middle mouse button
const int kMiddleButtonIconWidth = 16;

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
  // clicking middle mouse button, will draw the scroll icon
  OSR_TEST_CLICK_MIDDLE,
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
};

// Used in the browser process.
class OSRTestHandler : public TestHandler,
                       public CefRenderHandler,
                       public CefContextMenuHandler {
 public:
  explicit OSRTestHandler(OSRTestType test)
      : test_type_(test),
        event_count_(0),
        event_total_(1),
        started_(false) {
    if (test == OSR_TEST_SCROLLING) {
      // Wait for both the paint event and the scroll offset event.
      event_total_ = 2;
    }
  }

  virtual ~OSRTestHandler() {}

  // TestHandler methods
  virtual void RunTest() OVERRIDE {
    CreateOSRBrowser(kTestUrl);
#if !defined(DEBUGGER_ATTACHED)
    // Each test has a 5 second timeout. After this timeout it will be destroyed
    // and the test will fail. DestroyTest will be called at the timeout even
    // if the test is already destroyed and this is fine.
    CefPostDelayedTask(TID_UI, NewCefRunnableMethod(this,
        &OSRTestHandler::DestroyTest), 5000);
#endif  // DEBUGGER_ATTACHED
  }

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE {
    if (test_type_ == OSR_TEST_IS_WINDOWLESS) {
      EXPECT_TRUE(browser->GetHost()->IsWindowRenderingDisabled());
      DestroySucceededTestSoon();
    }
    TestHandler::OnAfterCreated(browser);
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      int httpStatusCode) OVERRIDE {
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
      default:
        // Intentionally left blank
        break;
    }
  }

  virtual bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) OVERRIDE {
    EXPECT_TRUE(browser.get());
    EXPECT_EQ(PID_RENDERER, source_process);
    EXPECT_TRUE(message.get());
    EXPECT_TRUE(message->IsReadOnly());

    if (!started())
      return false;

    const CefString kMessageName = "osrtest";
    EXPECT_EQ(kMessageName, message->GetName());

    CefString stringParam = message->GetArgumentList()->GetString(0);
    switch(test_type_) {
      case OSR_TEST_FOCUS:
        EXPECT_EQ(stringParam, std::string("focus"));
        DestroySucceededTestSoon();
        break;
      case OSR_TEST_CLICK_LEFT:
        EXPECT_EQ(stringParam, std::string("click0"));
        DestroySucceededTestSoon();
        break;
      case OSR_TEST_CLICK_MIDDLE:
        EXPECT_EQ(stringParam, std::string("click1"));
        DestroySucceededTestSoon();
        break;
      case OSR_TEST_MOUSE_MOVE:
        EXPECT_EQ(stringParam, std::string("mousemove"));
        DestroySucceededTestSoon();
        break;
      default:
        // Intentionally left blank
        break;
    }

    return true;
  }

  // CefClient methods, providing handlers
  virtual CefRefPtr<CefRenderHandler> GetRenderHandler() OVERRIDE {
    return this;
  }

  virtual CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() OVERRIDE {
    return this;
  }

  virtual CefRefPtr<CefRequestHandler> GetRequestHandler() OVERRIDE {
    return this;
  }

  virtual CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) OVERRIDE {
    std::string url = request->GetURL();

    if (url.find("http://tests/osrtest") == 0) {
      // Show the osr test contents
      CefRefPtr<CefStreamReader> stream =
         GetBinaryResourceReader("osr_test.html");
      return new CefStreamResourceHandler("text/html", stream);
    }

    return NULL;
  }

  // CefRenderHandler methods
  virtual bool GetViewRect(CefRefPtr<CefBrowser> browser,
                           CefRect& rect) OVERRIDE {
    if (test_type_ == OSR_TEST_RESIZE && started()) {
      rect = CefRect(0, 0, kOsrWidth * 2, kOsrHeight * 2);
      return true;
    }
    rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
    return true;
  }

  virtual bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                              int viewX,
                              int viewY,
                              int& screenX,
                              int& screenY) OVERRIDE {
    if (test_type_ == OSR_TEST_SCREEN_POINT && started()) {
      EXPECT_EQ(viewX, MiddleX(ExpectedRect(4)));
      EXPECT_EQ(viewY, MiddleY(ExpectedRect(4)));
      DestroySucceededTestSoon();
    } else if (test_type_ == OSR_TEST_CONTEXT_MENU && started()){
      screenX = 0;
      screenY = 0;
      return true;
    }
    // we don't want to see a contextual menu. stop here.
    return false;
  }

  virtual bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                             CefScreenInfo& screen_info) {
    screen_info.device_scale_factor = 1;

    // The screen info rectangles are used by the renderer to create and
    // position popups. If not overwritten in this function, the rectangle from
    // returned GetViewRect will be used to popuplate them.
    // The popup in the test fits without modifications in the test window, so
    // setting the screen to the test window size does not affect its rectangle.
    screen_info.rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
    screen_info.available_rect = CefRect(0, 0, kOsrWidth, kOsrHeight);
    return true;
  }

  virtual void OnPopupShow(CefRefPtr<CefBrowser> browser,
                           bool show) OVERRIDE {
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

  virtual void OnPopupSize(CefRefPtr<CefBrowser> browser,
                           const CefRect& rect) OVERRIDE {
    if (started()) {
      switch (test_type_) {
      case OSR_TEST_POPUP_SIZE:
        EXPECT_EQ(kExpandedSelectRect, rect);
        DestroySucceededTestSoon();
        break;
      default:
        break;
      }
    }
  }

  virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                       PaintElementType type,
                       const RectList& dirtyRects,
                       const void* buffer,
                       int width, int height) OVERRIDE {
    // bitmap must as big as GetViewRect said
    if (test_type_ != OSR_TEST_RESIZE && type == PET_VIEW) {
      EXPECT_EQ(kOsrWidth, width);
      EXPECT_EQ(kOsrHeight, height);
    } else if (type == PET_POPUP) {
      EXPECT_EQ(kExpandedSelectRect.width, width);
      EXPECT_EQ(kExpandedSelectRect.height, height);
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
        // test that we have a full repaint
        EXPECT_EQ(dirtyRects.size(), 1U);
        EXPECT_TRUE(IsFullRepaint(dirtyRects[0]));
        EXPECT_EQ(*(reinterpret_cast<const uint32*>(buffer)), 0xffff8080);
        DestroySucceededTestSoon();
        break;
      case OSR_TEST_TRANSPARENCY:
        // test that we have a full repaint
        EXPECT_EQ(dirtyRects.size(), 1U);
        EXPECT_TRUE(IsFullRepaint(dirtyRects[0]));
        EXPECT_EQ(*(reinterpret_cast<const uint32*>(buffer)), 0x7f7f0000U);
        DestroySucceededTestSoon();
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
          mouse_event.x = MiddleX(ExpectedRect(2));
          mouse_event.y = MiddleY(ExpectedRect(2));
          browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
        }
        break;
      case OSR_TEST_MOUSE_MOVE:
        if (StartTest()) {
          CefMouseEvent mouse_event;
          mouse_event.x = MiddleX(ExpectedRect(3));
          mouse_event.y = MiddleY(ExpectedRect(3));
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
        }
        break;
      case OSR_TEST_CLICK_RIGHT:
      case OSR_TEST_SCREEN_POINT:
      case OSR_TEST_CONTEXT_MENU:
        if (StartTest()) {
          CefMouseEvent mouse_event;
          mouse_event.x = MiddleX(ExpectedRect(4));
          mouse_event.y = MiddleY(ExpectedRect(4));
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
          mouse_event.x = MiddleX(ExpectedRect(0));
          mouse_event.y = MiddleY(ExpectedRect(0));

          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseClickEvent(
              mouse_event, MBT_LEFT, false, 1);
          browser->GetHost()->SendMouseClickEvent(
              mouse_event, MBT_LEFT, true, 1);
        }
        break;
      case OSR_TEST_CLICK_MIDDLE:
        if (StartTest()) {
          CefMouseEvent mouse_event;
          mouse_event.x = MiddleX(ExpectedRect(0));
          mouse_event.y = MiddleY(ExpectedRect(0));
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseClickEvent(
              mouse_event, MBT_MIDDLE, false, 1);
          browser->GetHost()->SendMouseClickEvent(
              mouse_event, MBT_MIDDLE, true, 1);
        } else {
          EXPECT_EQ(dirtyRects.size(), 1U);
          CefRect expectedRect(
              MiddleX(ExpectedRect(0)) - kMiddleButtonIconWidth / 2,
              MiddleY(ExpectedRect(0)) - kMiddleButtonIconWidth / 2,
              kMiddleButtonIconWidth, kMiddleButtonIconWidth);
          EXPECT_EQ(dirtyRects[0], expectedRect);
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_RESIZE:
        if (StartTest()) {
          browser->GetHost()->WasResized();
        } else {
          EXPECT_EQ(kOsrWidth * 2, width);
          EXPECT_EQ(kOsrHeight * 2, height);
          EXPECT_EQ(dirtyRects.size(), 1U);
          EXPECT_TRUE(IsFullRepaint(dirtyRects[0], width, height));
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_INVALIDATE: {
        static bool invalidating = false;
        static const CefRect invalidate_rect(0, 0, 1, 1);
        if (StartTest()) {
          invalidating = true;
          browser->GetHost()->Invalidate(invalidate_rect, PET_VIEW);
          invalidating = false;
        } else {
          EXPECT_TRUE(invalidating);
          EXPECT_EQ(dirtyRects.size(), 1U);
          EXPECT_EQ(dirtyRects[0], invalidate_rect);
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
            BYTE VkCode = LOBYTE(VkKeyScanA(kKeyTestWord[i]));
            UINT scanCode = MapVirtualKey(VkCode, MAPVK_VK_TO_VSC);
            event.native_key_code = (scanCode << 16) |  // key scan code
                                                    1;  // key repeat count
            event.windows_key_code = VkCode;
#elif defined(OS_MACOSX)
            osr_unittests::GetKeyEvent(event, kKeyTestCodes[i], 0);
#elif defined(OS_LINUX)
            event.native_key_code = kKeyTestCodes[i];
#else
            NOTREACHED();
#endif
            event.type = KEYEVENT_RAWKEYDOWN;
            browser->GetHost()->SendKeyEvent(event);
#if defined(OS_WIN)
            event.windows_key_code = kKeyTestWord[i];
#elif defined(OS_MACOSX)
            osr_unittests::GetKeyEvent(event, kKeyTestCodes[i], 0);
#elif defined(OS_LINUX)
            event.native_key_code = kKeyTestCodes[i];
#endif
            event.type = KEYEVENT_CHAR;
            browser->GetHost()->SendKeyEvent(event);
#if defined(OS_WIN)
            event.windows_key_code = VkCode;
            // bits 30 and 31 should be always 1 for WM_KEYUP
            event.native_key_code |= 0xC0000000;
#elif defined(OS_MACOSX)
            osr_unittests::GetKeyEvent(event, kKeyTestCodes[i], 0);
#elif defined(OS_LINUX)
            event.native_key_code = kKeyTestCodes[i];
#endif
            event.type = KEYEVENT_KEYUP;
            browser->GetHost()->SendKeyEvent(event);
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
          mouse_event.x = MiddleX(ExpectedRect(10));
          mouse_event.y = MiddleY(ExpectedRect(10));
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
        }
        break;
      case OSR_TEST_SCROLLING: {
        static const int deltaY = 10;
        if (StartTest()) {
          // scroll down once
          CefMouseEvent mouse_event;
          mouse_event.x = MiddleX(ExpectedRect(0));
          mouse_event.y = MiddleY(ExpectedRect(0));
          mouse_event.modifiers = 0;
          browser->GetHost()->SendMouseWheelEvent(mouse_event, 0, - deltaY);
        } else {
#if defined(OS_WIN)
          // there should be 3 update areas:
          // 1) vertical scrollbar
          // 2) discovered new area (bottom side)
          // 3) the whole visible rect.
          EXPECT_EQ(dirtyRects.size(), 3U);
          EXPECT_EQ(dirtyRects[0], CefRect(0, 0,
              kOsrWidth - kVerticalScrollbarWidth, kHorizontalScrollbarWidth));

          EXPECT_EQ(dirtyRects[1], CefRect(0, kHorizontalScrollbarWidth,
              kOsrWidth, kOsrHeight - 2 * kHorizontalScrollbarWidth));

          EXPECT_EQ(dirtyRects[2],
              CefRect(0, kOsrHeight - kHorizontalScrollbarWidth,
                         kOsrWidth - kVerticalScrollbarWidth,
                         kHorizontalScrollbarWidth));
#elif defined(OS_MACOSX)
          // On Mac, when scrollbars are Always on, there is a single update of
          // the whole view
          // When scrollbars are set to appear 'When scrolling', the first
          // rectangle is of the whole view, and next comes a longer sequence of
          // updates, each with one rectangle update for the whole scrollbar
          // rectangle(584,0,16,400)
          // Validating the first, full update passes in both cases
          EXPECT_EQ(dirtyRects.size(), 1U);
          EXPECT_EQ(dirtyRects[0], CefRect(0, 0,
              kOsrWidth, kOsrHeight));
#elif defined(OS_LINUX)
          EXPECT_EQ(dirtyRects.size(), 1U);
          EXPECT_EQ(dirtyRects[0], CefRect(0, 0,
              kOsrWidth, kOsrHeight));
#else
#error "Unsupported platform"
#endif  // defined(OS_WIN)
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
          CefKeyEvent event;
          event.is_system_key = false;
#if defined(OS_WIN)
          BYTE VkCode = LOBYTE(VK_ESCAPE);
          UINT scanCode = MapVirtualKey(VkCode, MAPVK_VK_TO_VSC);
          event.native_key_code = (scanCode << 16) |  // key scan code
                                                  1;  // key repeat count
          event.windows_key_code = VkCode;
#elif defined(OS_MACOSX)
          osr_unittests::GetKeyEvent(event, ui::VKEY_ESCAPE, 0);
#elif defined(OS_LINUX)
          event.native_key_code = XK_Escape;
#else
#error "Unsupported platform"
#endif  // defined(OS_WIN)
          event.type = KEYEVENT_CHAR;
          browser->GetHost()->SendKeyEvent(event);
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
          EXPECT_EQ(dirtyRects[0],
              CefRect(0, 0,
                      kExpandedSelectRect.width,
                      kExpandedSelectRect.height));
          // first pixel of border
          EXPECT_EQ(*(reinterpret_cast<const uint32*>(buffer)), 0xff7f9db9);
          EXPECT_EQ(kExpandedSelectRect.width, width);
          EXPECT_EQ(kExpandedSelectRect.height, height);
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
              EXPECT_EQ(dirtyRects.size(), 1U);
#if defined(OS_WIN)
              // border is not redrawn
              EXPECT_EQ(dirtyRects[0],
                        CefRect(1,
                                1,
                                kExpandedSelectRect.width - 3,
                                kExpandedSelectRect.height - 2));
#elif defined(OS_MACOSX) || defined(OS_LINUX)
              EXPECT_EQ(dirtyRects[0],
                        CefRect(1,
                                0,
                                kExpandedSelectRect.width - 3,
                                kExpandedSelectRect.height - 1));
#else
#error "Unsupported platform"
#endif  // defined(OS_WIN)

              DestroySucceededTestSoon();
            }
          }
        }
        break;
      default:
        break;
    }
  }

  virtual void OnCursorChange(CefRefPtr<CefBrowser> browser,
                              CefCursorHandle cursor) OVERRIDE {
      if (test_type_ == OSR_TEST_CURSOR && started()) {
        DestroySucceededTestSoon();
      }
  }

  virtual void OnScrollOffsetChanged(CefRefPtr<CefBrowser> browser) OVERRIDE {
    if (test_type_ == OSR_TEST_SCROLLING && started()) {
      DestroySucceededTestSoon();
    }
  }

  virtual bool OnTooltip(CefRefPtr<CefBrowser> browser,
                         CefString& text) OVERRIDE {
    if (test_type_ == OSR_TEST_TOOLTIP && started()) {
      EXPECT_STREQ("EXPECTED_TOOLTIP", text.ToString().c_str());
      DestroySucceededTestSoon();
    }
    return false;
  }

  virtual void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefContextMenuParams> params,
                                   CefRefPtr<CefMenuModel> model) OVERRIDE {
    if (!started())
      return;
    if (test_type_ == OSR_TEST_CLICK_RIGHT) {
      EXPECT_EQ(params->GetXCoord(), MiddleX(ExpectedRect(4)));
      EXPECT_EQ(params->GetYCoord(), MiddleY(ExpectedRect(4)));
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
#if defined(OS_WIN)
    windowInfo.SetAsOffScreen(GetDesktopWindow());
#elif defined(OS_MACOSX)
    // An actual vies is needed only for the ContextMenu test. The menu runner
    // checks if the view is not nil before showing the context menu.
    if (test_type_ == OSR_TEST_CONTEXT_MENU)
      windowInfo.SetAsOffScreen(osr_unittests::GetFakeView());
    else
      windowInfo.SetAsOffScreen(NULL);
#elif defined(OS_LINUX)
    windowInfo.SetAsOffScreen(NULL);
#else
#error "Unsupported platform"
#endif
    if (test_type_ ==  OSR_TEST_TRANSPARENCY)
      windowInfo.SetTransparentPainting(TRUE);
    CefBrowserHost::CreateBrowser(windowInfo, this, url, settings, NULL);
  }

  bool IsFullRepaint(const CefRect& rc, int width = kOsrWidth,
                     int height = kOsrHeight) {
    return rc.width == width && rc.height == height;
  }

  static
  bool IsBackgroundInBuffer(const uint32* buffer, size_t size, uint32 rgba) {
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

  void DestroySucceededTestSoon() {
    if (succeeded())
      return;
    if (++event_count_ == event_total_) {
      CefPostTask(TID_UI, NewCefRunnableMethod(this,
          &OSRTestHandler::DestroyTest));
    }
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
  int event_count_;
  int event_total_;
  bool started_;
  IMPLEMENT_REFCOUNTING(OSRTestHandler);
};

}  // namespace

// generic test
#define OSR_TEST(name, test_mode)\
TEST(OSRTest, name) {\
  CefRefPtr<OSRTestHandler> handler = \
      new OSRTestHandler(test_mode);\
  handler->ExecuteTest();\
  EXPECT_TRUE(handler->succeeded());\
}

// tests
OSR_TEST(Windowless, OSR_TEST_IS_WINDOWLESS);
OSR_TEST(Focus, OSR_TEST_FOCUS);
OSR_TEST(Paint, OSR_TEST_PAINT);
OSR_TEST(TransparentPaint, OSR_TEST_TRANSPARENCY);
OSR_TEST(Cursor, OSR_TEST_CURSOR);
OSR_TEST(MouseMove, OSR_TEST_MOUSE_MOVE);
OSR_TEST(MouseRightClick, OSR_TEST_CLICK_RIGHT);
OSR_TEST(MouseLeftClick, OSR_TEST_CLICK_LEFT);
OSR_TEST(MouseMiddleClick, OSR_TEST_CLICK_MIDDLE);
OSR_TEST(ScreenPoint, OSR_TEST_SCREEN_POINT);
OSR_TEST(Resize, OSR_TEST_RESIZE);
OSR_TEST(Invalidate, OSR_TEST_INVALIDATE);
OSR_TEST(KeyEvents, OSR_TEST_KEY_EVENTS);
OSR_TEST(Tooltip, OSR_TEST_TOOLTIP);
OSR_TEST(Scrolling, OSR_TEST_SCROLLING);
OSR_TEST(ContextMenu, OSR_TEST_CONTEXT_MENU);
OSR_TEST(PopupPaint, OSR_TEST_POPUP_PAINT);
OSR_TEST(PopupShow, OSR_TEST_POPUP_SHOW);
OSR_TEST(PopupSize, OSR_TEST_POPUP_SIZE);
OSR_TEST(PopupHideOnBlur, OSR_TEST_POPUP_HIDE_ON_BLUR);
OSR_TEST(PopupHideOnClick, OSR_TEST_POPUP_HIDE_ON_CLICK);
OSR_TEST(PopupHideOnScroll, OSR_TEST_POPUP_HIDE_ON_SCROLL);
OSR_TEST(PopupHideOnEsc, OSR_TEST_POPUP_HIDE_ON_ESC);
OSR_TEST(PopupScrollInside, OSR_TEST_POPUP_SCROLL_INSIDE);
