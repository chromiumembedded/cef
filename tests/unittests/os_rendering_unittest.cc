// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_runnable.h"
#include "tests/unittests/test_handler.h"

namespace {

const char kTestUrl[] = "http://tests/OSRTest";

// this html should render on a 600 x 400 window with a little vertical
// offset with scrollbar.

const char kOsrHtml[] =
"<html>                                                                       "
"  <head><title>OSR Test</title></head>                                       "
"  <style>                                                                    "
"  .red_hover:hover {color:red;}                                              "
"  </style>                                                                   "
"  <script>                                                                   "
"  function getElement(id) { return document.getElementById(id); }            "
"  function makeH1Red() { getElement('LI00').style.color='red'; }             "
"  function makeH1Black() { getElement('LI00').style.color='black'; }         "
"  function navigate() { location.href='?k='+getElement('editbox').value; }   "
"  </script>                                                                  "
"  <body onfocus='makeH1Red()' onblur='makeH1Black()'>                        "
"  <h1 id='LI00'>OSR Testing h1 - Focus and blur this page and will get this  "
"      red black</h1>                                                         "
"  <ol>                                                                       "
"  <li id='LI01'>OnPaint should be called each time a page loads</li>         "
"  <li id='LI02' style='cursor:pointer;'><span>Move mouse                     "
"      to require an OnCursorChange call</span></li>                          "
"  <li id='LI03' class='red_hover'><span>Hover will color this with           "
"      red. Will trigger OnPaint once on enter and once on leave</span></li>  "
"  <li id='LI04'>Right clicking will show contextual menu and will request    "
"      GetScreenPoint</li>                                                    "
"  <li id='LI05'>IsWindowRenderingDisabled should be true</li>                "
"  <li id='LI06'>WasResized should trigger full repaint if size changes.      "
"      </li>                                                                  "
"  <li id='LI07'>Invalidate should trigger OnPaint once</li>                  "
"  <li id='LI08'>Click and write here with SendKeyEvent to trigger repaints:  "
"      <input id='editbox' type='text' value=''></li>                         "
"  <li id='LI09'>Click here with SendMouseClickEvent to navigate:             "
"      <input id='btnnavigate' type='button' onclick='navigate()'             "
"      value='Click here to navigate' id='editbox' /></li>                    "
"  <li id='LI10' title='EXPECTED_TOOLTIP'>Mouse over this element will        "
"      trigger show a tooltip</li>                                            "
"  </ol>                                                                      "
"  <br />                                                                     "
"  <br />                                                                     "
"  <br />                                                                     "
"  <br />                                                                     "
"  </body>                                                                    "
"</html>                                                                      ";

// #define(DEBUGGER_ATTACHED)

// default osr widget size
const int kOsrWidth = 600;
const int kOsrHeight = 400;

// precomputed bounding client rects for html elements (h1 and li).
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

// bounding client rects for edit box and navigate button
const CefRect kEditBoxRect(412, 245, 153, 22);
const CefRect kNavigateButtonRect(360, 271, 140, 22);
const int kVerticalScrollbarWidth = GetSystemMetrics(SM_CXVSCROLL);
const int kHorizontalScrollbarWidth = GetSystemMetrics(SM_CXHSCROLL);

// adjusted expected rect regarding system vertical scrollbar width
CefRect ExpectedRect(int index) {
  // this is the scrollbar width for all the kExpectedRectLI
  const int kDefaultVerticalScrollbarWidth = 17;
  if (kDefaultVerticalScrollbarWidth == kVerticalScrollbarWidth)
    return kExpectedRectLI[index];

  CefRect adjustedRect(kExpectedRectLI[index]);
  adjustedRect.width += kDefaultVerticalScrollbarWidth -
                        kVerticalScrollbarWidth;
  return adjustedRect;
}

// word to be written into edit box
const char kKeyTestWord[] = "done";

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
};

// Used in the browser process.
class OSRTestHandler : public TestHandler,
                       public CefRenderHandler,
                       public CefContextMenuHandler {
 public:
  explicit OSRTestHandler(OSRTestType test)
      : test_type_(test),
        started_(false),
        succeeded_(false) {
  }

  virtual ~OSRTestHandler() {}

  // TestHandler methods
  virtual void RunTest() OVERRIDE {
    AddResource(kTestUrl, kOsrHtml, "text/html");
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
    if (test_type_ == OSR_TEST_KEY_EVENTS && started()) {
      std::string expectedUrl = std::string(kTestUrl) + "?k=" + kKeyTestWord;
      EXPECT_EQ(frame->GetURL(), expectedUrl);
      DestroySucceededTestSoon();
    }
  }

  // CefClient methods, providing handlers
  virtual CefRefPtr<CefRenderHandler> GetRenderHandler() OVERRIDE {
    return this;
  }

  virtual CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() OVERRIDE {
    return this;
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
    }
    // we don't want to see a contextual menu. stop here.
    return false;
  }

  virtual void OnPopupShow(CefRefPtr<CefBrowser> browser,
                           bool show) OVERRIDE {
    // popup widgets are not yet supported
    EXPECT_FALSE("OnPopupShow should not be reached");
  }

  virtual void OnPopupSize(CefRefPtr<CefBrowser> browser,
                           const CefRect& rect) OVERRIDE {
    // popup widgets are not yet supported
    EXPECT_FALSE("OnPopupSize should not be reached");
  }

  virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                       PaintElementType type,
                       const RectList& dirtyRects,
                       const void* buffer,
                       int width, int height) OVERRIDE {
    // bitmap must as big as GetViewRect said
    if (test_type_ != OSR_TEST_RESIZE || !started()) {
      EXPECT_EQ(kOsrWidth, width);
      EXPECT_EQ(kOsrHeight, height);
    }

    EXPECT_TRUE(browser->GetHost()->IsWindowRenderingDisabled());

    // We don't support popup widgets yet
    EXPECT_EQ(type, PET_VIEW);

    // start test only when painting something else then background
    if (IsBackgroundInBuffer(reinterpret_cast<const uint32*>(buffer),
                             width * height, RGB(255, 255, 255))) {
      return;
    }

    // Send events after the first full repaint
    switch (test_type_) {
      case OSR_TEST_PAINT:
        // test that we have a full repaint
        EXPECT_EQ(dirtyRects.size(), 1);
        EXPECT_TRUE(IsFullRepaint(dirtyRects[0]));
        DestroySucceededTestSoon();
        break;
      case OSR_TEST_FOCUS:
        if (StartTest()) {
          // body.onfocus will make LI00 red
          browser->GetHost()->SendFocusEvent(true);
        } else {
          EXPECT_EQ(dirtyRects.size(), 1);
          EXPECT_EQ(dirtyRects[0], ExpectedRect(0));
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_CURSOR:
        if (StartTest()) {
          // make mouse leave first
          browser->GetHost()->SendMouseMoveEvent(0, 0, true);
          // enter mouse in the LI2 element having hand cursor
          browser->GetHost()->SendMouseMoveEvent(MiddleX(ExpectedRect(2)),
              MiddleY(ExpectedRect(2)), false);
        }
        break;
      case OSR_TEST_MOUSE_MOVE:
        if (StartTest()) {
          browser->GetHost()->SendMouseMoveEvent(MiddleX(ExpectedRect(3)),
              MiddleY(ExpectedRect(3)), false);
        } else {
          EXPECT_EQ(dirtyRects.size(), 1);
          EXPECT_EQ(dirtyRects[0], ExpectedRect(3));
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_CLICK_RIGHT:
      case OSR_TEST_SCREEN_POINT:
        if (StartTest()) {
          browser->GetHost()->SendMouseClickEvent(MiddleX(ExpectedRect(4)),
              MiddleY(ExpectedRect(4)), MBT_RIGHT, false, 1);
          browser->GetHost()->SendMouseClickEvent(MiddleX(ExpectedRect(4)),
              MiddleY(ExpectedRect(4)), MBT_RIGHT, true, 1);
        }
        break;
      case OSR_TEST_CLICK_LEFT:
        if (StartTest()) {
          browser->GetHost()->SendMouseClickEvent(MiddleX(kEditBoxRect),
              MiddleY(kEditBoxRect), MBT_LEFT, false, 1);
          browser->GetHost()->SendMouseClickEvent(MiddleX(kEditBoxRect),
              MiddleY(kEditBoxRect), MBT_LEFT, true, 1);
        } else {
          EXPECT_EQ(dirtyRects.size(), 1);
          EXPECT_EQ(dirtyRects[0], kEditBoxRect);
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_CLICK_MIDDLE:
        if (StartTest()) {
          browser->GetHost()->SendMouseClickEvent(MiddleX(ExpectedRect(0)),
              MiddleY(ExpectedRect(0)), MBT_MIDDLE, false, 1);
          browser->GetHost()->SendMouseClickEvent(MiddleX(ExpectedRect(0)),
              MiddleY(ExpectedRect(0)), MBT_MIDDLE, true, 1);
        } else {
          EXPECT_EQ(dirtyRects.size(), 1);
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
          EXPECT_EQ(dirtyRects.size(), 1);
          EXPECT_TRUE(IsFullRepaint(dirtyRects[0], width, height));
          DestroySucceededTestSoon();
        }
        break;
      case OSR_TEST_INVALIDATE: {
        static bool invalidating = false;
        static const CefRect invalidate_rect(0, 0, 1, 1);
        if (StartTest()) {
          invalidating = true;
          browser->GetHost()->Invalidate(invalidate_rect);
          invalidating = false;
        } else {
          EXPECT_TRUE(invalidating);
          EXPECT_EQ(dirtyRects.size(), 1);
          EXPECT_EQ(dirtyRects[0], invalidate_rect);
          DestroySucceededTestSoon();
        }
        break;
      }
      case OSR_TEST_KEY_EVENTS:
        if (StartTest()) {
          // click inside edit box
          browser->GetHost()->SendMouseClickEvent(MiddleX(kEditBoxRect),
              MiddleY(kEditBoxRect), MBT_LEFT, false, 1);
          browser->GetHost()->SendMouseClickEvent(MiddleX(kEditBoxRect),
              MiddleY(kEditBoxRect), MBT_LEFT, true, 1);

          // write "done" word
          CefKeyEvent event;
          event.is_system_key = false;

          size_t word_lenght = strlen(kKeyTestWord);
          for (size_t i = 0; i < word_lenght; ++i) {
            BYTE VkCode = LOBYTE(VkKeyScanA(kKeyTestWord[i]));
            UINT scanCode = MapVirtualKey(VkCode, MAPVK_VK_TO_VSC);
            event.native_key_code = (scanCode << 16) |  // key scan code
                                    1;  // key repeat count
            event.windows_key_code = VkCode;
            event.type = KEYEVENT_RAWKEYDOWN;
            browser->GetHost()->SendKeyEvent(event);
            event.windows_key_code = kKeyTestWord[i];
            event.type = KEYEVENT_CHAR;
            browser->GetHost()->SendKeyEvent(event);
            event.windows_key_code = VkCode;
            // bits 30 and 31 should be always 1 for WM_KEYUP
            event.native_key_code |= 0xC0000000;
            event.type = KEYEVENT_KEYUP;
            browser->GetHost()->SendKeyEvent(event);
          }
          // click button to navigate
          browser->GetHost()->SendMouseClickEvent(MiddleX(kNavigateButtonRect),
              MiddleY(kNavigateButtonRect), MBT_LEFT, false, 1);
          browser->GetHost()->SendMouseClickEvent(MiddleX(kNavigateButtonRect),
              MiddleY(kNavigateButtonRect), MBT_LEFT, true, 1);
        }
        break;
      case OSR_TEST_TOOLTIP:
        if (StartTest()) {
          browser->GetHost()->SendMouseMoveEvent(MiddleX(ExpectedRect(10)),
              MiddleY(ExpectedRect(10)), false);
        }
        break;
      case OSR_TEST_SCROLLING: {
        static const int deltaY = 10;
        if (StartTest()) {
          // scroll down once
          browser->GetHost()->SendMouseWheelEvent(MiddleX(ExpectedRect(0)),
              MiddleY(ExpectedRect(0)), 0, - deltaY);
        } else {
          // there should be 3 update areas:
          // 1) vertical scrollbar
          // 2) discovered new area (bottom side)
          // 3) the whole visible rect.
          EXPECT_EQ(dirtyRects.size(), 3);
          EXPECT_EQ(dirtyRects[0], CefRect(0, 0,
              kOsrWidth - kVerticalScrollbarWidth, kHorizontalScrollbarWidth));
          EXPECT_EQ(dirtyRects[1], CefRect(0, kHorizontalScrollbarWidth,
              kOsrWidth, kOsrHeight - 2 * kHorizontalScrollbarWidth));
          EXPECT_EQ(dirtyRects[2],
              CefRect(0, kOsrHeight - kHorizontalScrollbarWidth,
                         kOsrWidth - kVerticalScrollbarWidth,
                         kHorizontalScrollbarWidth));
          DestroySucceededTestSoon();
        }
        break;
      }
    }
  }

  virtual void OnCursorChange(CefRefPtr<CefBrowser> browser,
                              CefCursorHandle cursor) OVERRIDE {
      if (test_type_ == OSR_TEST_CURSOR && started()) {
        DestroySucceededTestSoon();
      }
  }

  virtual bool OnTooltip(CefRefPtr<CefBrowser> browser,
                         CefString& text) OVERRIDE {
    if (test_type_ ==  OSR_TEST_TOOLTIP && started()) {
      EXPECT_EQ(text, "EXPECTED_TOOLTIP");
      DestroySucceededTestSoon();
    }
    return false;
  }

  virtual void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefContextMenuParams> params,
                                   CefRefPtr<CefMenuModel> model) OVERRIDE {
    if (test_type_ == OSR_TEST_CLICK_RIGHT && started()) {
      EXPECT_EQ(params->GetXCoord(), MiddleX(ExpectedRect(4)));
      EXPECT_EQ(params->GetYCoord(), MiddleY(ExpectedRect(4)));
      DestroySucceededTestSoon();
    }
  }

  // OSRTestHandler functions
  void CreateOSRBrowser(const CefString& url) {
    CefWindowInfo windowInfo;
    CefBrowserSettings settings;
    windowInfo.SetAsOffScreen(GetDesktopWindow());
    CefBrowserHost::CreateBrowser(windowInfo, this, url, settings);
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
    succeeded_ = true;
    CefPostTask(TID_IO, NewCefRunnableMethod(this,
        &OSRTestHandler::DestroyTest));
  }

  // true if the events for this test are already sent
  bool started() { return started_; }

  // true if the exit point was reached, even the result is not
  // the expected one
  bool succeeded() { return succeeded_; }

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
  bool started_;
  bool succeeded_;
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
