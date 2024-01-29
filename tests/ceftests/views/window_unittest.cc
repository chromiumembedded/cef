// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_layout.h"
#include "include/views/cef_panel.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/image_util.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/ceftests/views/test_window_delegate.h"
#include "tests/gtest/include/gtest/gtest.h"

#define WINDOW_TEST_ASYNC(name) UI_THREAD_TEST_ASYNC(ViewsWindowTest, name)

#if !defined(OS_WIN)
#define VK_MENU 0x12  // ALT key.
#endif

namespace {

// Window state change delay in MS.
const int kStateDelayMS = 200;

const int kWSize = TestWindowDelegate::kWSize;

// Test that |expected| and |actual| are within |allowed_deviance| of each
// other.
void ExpectCloseRects(const CefRect& expected,
                      const CefRect& actual,
                      int allowed_deviance) {
  EXPECT_LE(abs(expected.x - actual.x), allowed_deviance);
  EXPECT_LE(abs(expected.y - actual.y), allowed_deviance);
  EXPECT_LE(abs(expected.width - actual.width), allowed_deviance);
  EXPECT_LE(abs(expected.height - actual.height), allowed_deviance);
}

void ExpectClosePoints(const CefPoint& expected,
                       const CefPoint& actual,
                       int allowed_deviance) {
  EXPECT_LE(abs(expected.x - actual.x), allowed_deviance);
  EXPECT_LE(abs(expected.y - actual.y), allowed_deviance);
}

void WindowCreateImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  TestWindowDelegate::RunTest(event, std::move(config));
}

void WindowCreateFramelessImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->frameless = true;
  TestWindowDelegate::RunTest(event, std::move(config));
}

void RunWindowShow(cef_show_state_t initial_show_state,
                   CefRefPtr<CefWindow> window) {
#if defined(OS_MAC)
  if (initial_show_state == CEF_SHOW_STATE_FULLSCREEN) {
    // On MacOS, starting in fullscreen mode also shows the window on creation.
    EXPECT_TRUE(window->IsVisible());
    EXPECT_TRUE(window->IsDrawn());
  } else
#endif
  {
    EXPECT_FALSE(window->IsVisible());
    EXPECT_FALSE(window->IsDrawn());
    window->Show();
  }

  if (initial_show_state == CEF_SHOW_STATE_MINIMIZED) {
#if !defined(OS_MAC)
    // This result is a bit unexpected, but I guess the platform considers a
    // window to be visible even when it's minimized.
    EXPECT_TRUE(window->IsVisible());
    EXPECT_TRUE(window->IsDrawn());
#else
    EXPECT_FALSE(window->IsVisible());
    EXPECT_FALSE(window->IsDrawn());
#endif
  } else {
    EXPECT_TRUE(window->IsVisible());
    EXPECT_TRUE(window->IsDrawn());
  }

  switch (initial_show_state) {
    case CEF_SHOW_STATE_NORMAL:
      EXPECT_FALSE(window->IsMaximized());
      EXPECT_FALSE(window->IsMinimized());
      EXPECT_FALSE(window->IsFullscreen());
      break;
    case CEF_SHOW_STATE_MINIMIZED:
      EXPECT_FALSE(window->IsMaximized());
#if defined(OS_WIN)
      // On MacOS, IsMinimized() state isn't reliable in this callback due to a
      // timing issue between NativeWidgetMac::Minimize requesting the minimize
      // state change (before this callback) and
      // NativeWidgetMacNSWindowHost::OnWindowMiniaturizedChanged indicating the
      // completed state change (after this callback).
      // On Linux, there's likely a similar timing issue.
      EXPECT_TRUE(window->IsMinimized());
#endif
      EXPECT_FALSE(window->IsFullscreen());
      break;
    case CEF_SHOW_STATE_MAXIMIZED:
#if !defined(OS_LINUX)
      // On Linux, there's likely a similar timing issue.
      EXPECT_TRUE(window->IsMaximized());
#endif
      EXPECT_FALSE(window->IsMinimized());
      EXPECT_FALSE(window->IsFullscreen());
      break;
    case CEF_SHOW_STATE_FULLSCREEN:
      EXPECT_FALSE(window->IsMaximized());
      EXPECT_FALSE(window->IsMinimized());
      EXPECT_TRUE(window->IsFullscreen());
      break;
    case CEF_SHOW_STATE_HIDDEN:
      break;
  }
}

void WindowCreateWithOriginImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->window_origin = {100, 200};
  config->on_window_created =
      base::BindOnce(RunWindowShow, config->initial_show_state);
  TestWindowDelegate::RunTest(event, std::move(config));
}

void WindowCreateMinimizedImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->initial_show_state = CEF_SHOW_STATE_MINIMIZED;
  config->on_window_created =
      base::BindOnce(RunWindowShow, config->initial_show_state);
  TestWindowDelegate::RunTest(event, std::move(config));
}

void WindowCreateMaximizedImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->initial_show_state = CEF_SHOW_STATE_MAXIMIZED;
  config->on_window_created =
      base::BindOnce(RunWindowShow, config->initial_show_state);
  TestWindowDelegate::RunTest(event, std::move(config));
}

void WindowCreateFullscreenImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->initial_show_state = CEF_SHOW_STATE_FULLSCREEN;
  config->on_window_created =
      base::BindOnce(RunWindowShow, config->initial_show_state);
  TestWindowDelegate::RunTest(event, std::move(config));
}

void RunWindowShowHide(cef_show_state_t initial_show_state,
                       CefRefPtr<CefWindow> window) {
  RunWindowShow(initial_show_state, window);
  window->Hide();
  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->IsDrawn());
}

void WindowShowHideImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created =
      base::BindOnce(RunWindowShowHide, config->initial_show_state);
  TestWindowDelegate::RunTest(event, std::move(config));
}

void WindowShowHideFramelessImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created =
      base::BindOnce(RunWindowShowHide, config->initial_show_state);
  config->frameless = true;
  TestWindowDelegate::RunTest(event, std::move(config));
}

const int kWPanel1ID = 1;
const int kWPanel2ID = 2;

void CreateBoxLayout(CefRefPtr<CefWindow> parent) {
  CefRefPtr<CefPanel> panel_child1 = CefPanel::CreatePanel(nullptr);
  panel_child1->SetID(kWPanel1ID);
  panel_child1->SetBackgroundColor(CefColorSetARGB(255, 0, 0, 255));
  EXPECT_TRUE(panel_child1->IsVisible());
  EXPECT_FALSE(panel_child1->IsDrawn());

  CefRefPtr<CefPanel> panel_child2 = CefPanel::CreatePanel(nullptr);
  panel_child2->SetID(kWPanel2ID);
  panel_child2->SetBackgroundColor(CefColorSetARGB(255, 0, 255, 0));
  EXPECT_TRUE(panel_child2->IsVisible());
  EXPECT_FALSE(panel_child2->IsDrawn());

  // Set to BoxLayout. Default layout is vertical with children stretched along
  // the horizontal axis.
  CefBoxLayoutSettings settings;
  parent->SetToBoxLayout(settings);

  parent->AddChildView(panel_child1);
  parent->AddChildView(panel_child2);

  // IsDrawn() returns true because the Panels now have a RootView from the
  // Window.
  EXPECT_TRUE(panel_child1->IsDrawn());
  EXPECT_TRUE(panel_child1->IsDrawn());

  // Stretch children equally along the vertical axis using flex.
  CefRefPtr<CefBoxLayout> layout = parent->GetLayout()->AsBoxLayout();
  layout->SetFlexForView(panel_child1, 1);
  layout->SetFlexForView(panel_child2, 1);

  // Force layout.
  parent->Layout();

  // The children should each take up 50% of the client area.
  ExpectCloseRects(CefRect(0, 0, kWSize, kWSize / 2), panel_child1->GetBounds(),
                   2);
  ExpectCloseRects(CefRect(0, kWSize / 2, kWSize, kWSize / 2),
                   panel_child2->GetBounds(), 2);
}

void RunWindowLayoutAndCoords(CefRefPtr<CefWindow> window) {
  CreateBoxLayout(window);

  CefRefPtr<CefView> view1 = window->GetViewForID(kWPanel1ID);
  EXPECT_TRUE(view1.get());
  CefRefPtr<CefView> view2 = window->GetViewForID(kWPanel2ID);
  EXPECT_TRUE(view2.get());

  window->Show();

  CefRect client_bounds_in_screen = window->GetClientAreaBoundsInScreen();
  CefPoint point;

  // Test view to screen coordinate conversions.
  point = CefPoint(0, 0);
  EXPECT_TRUE(view1->ConvertPointToScreen(point));
  EXPECT_EQ(CefPoint(client_bounds_in_screen.x, client_bounds_in_screen.y),
            point);
  point = CefPoint(0, 0);
  EXPECT_TRUE(view2->ConvertPointToScreen(point));
  ExpectClosePoints(CefPoint(client_bounds_in_screen.x,
                             client_bounds_in_screen.y + kWSize / 2),
                    point, 1);

  // Test view from screen coordinate conversions.
  point = CefPoint(client_bounds_in_screen.x, client_bounds_in_screen.y);
  EXPECT_TRUE(view1->ConvertPointFromScreen(point));
  EXPECT_EQ(CefPoint(0, 0), point);
  point = CefPoint(client_bounds_in_screen.x,
                   client_bounds_in_screen.y + kWSize / 2);
  EXPECT_TRUE(view2->ConvertPointFromScreen(point));
  ExpectClosePoints(CefPoint(0, 0), point, 1);

  // Test view to window coordinate conversions.
  point = CefPoint(0, 0);
  EXPECT_TRUE(view1->ConvertPointToWindow(point));
  EXPECT_EQ(CefPoint(0, 0), point);
  point = CefPoint(0, 0);
  EXPECT_TRUE(view2->ConvertPointToWindow(point));
  ExpectClosePoints(CefPoint(0, kWSize / 2), point, 1);

  // Test view from window coordinate conversions.
  point = CefPoint(0, 0);
  EXPECT_TRUE(view1->ConvertPointFromWindow(point));
  EXPECT_EQ(CefPoint(0, 0), point);
  point = CefPoint(0, kWSize / 2);
  EXPECT_TRUE(view2->ConvertPointFromWindow(point));
  ExpectClosePoints(CefPoint(0, 0), point, 1);

  // Test view to view coordinate conversions.
  point = CefPoint(0, 0);
  EXPECT_TRUE(view1->ConvertPointToView(view2, point));
  ExpectClosePoints(CefPoint(0, -kWSize / 2), point, 1);
  point = CefPoint(0, 0);
  EXPECT_TRUE(view2->ConvertPointToView(view1, point));
  ExpectClosePoints(CefPoint(0, kWSize / 2), point, 1);

  // Test view from view coordinate conversions.
  point = CefPoint(0, -kWSize / 2);
  EXPECT_TRUE(view1->ConvertPointFromView(view2, point));
  ExpectClosePoints(CefPoint(0, 0), point, 1);
  point = CefPoint(0, kWSize / 2);
  EXPECT_TRUE(view2->ConvertPointFromView(view1, point));
  ExpectClosePoints(CefPoint(0, 0), point, 1);

  CefRefPtr<CefDisplay> display = window->GetDisplay();
  EXPECT_TRUE(display.get());

  // We don't know what the pixel values will be, but they should be reversable.
  point = CefPoint(client_bounds_in_screen.x, client_bounds_in_screen.y);
  display->ConvertPointToPixels(point);
  display->ConvertPointFromPixels(point);
  ExpectClosePoints(
      CefPoint(client_bounds_in_screen.x, client_bounds_in_screen.y), point, 1);

  // We don't know what the pixel values will be, but they should be reversable.
  point = CefPoint(client_bounds_in_screen.x, client_bounds_in_screen.y);
  const auto pixels = CefDisplay::ConvertScreenPointToPixels(point);
  const auto dip = CefDisplay::ConvertScreenPointFromPixels(pixels);
  EXPECT_EQ(point, dip);
}

void WindowLayoutAndCoordsImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunWindowLayoutAndCoords);
  TestWindowDelegate::RunTest(event, std::move(config));
}

void WindowLayoutAndCoordsFramelessImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunWindowLayoutAndCoords);
  config->frameless = true;
  TestWindowDelegate::RunTest(event, std::move(config));
}

void VerifyRestore(CefRefPtr<CefWindow> window) {
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_FALSE(window->IsFullscreen());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->IsDrawn());

  // End the test by closing the Window.
  window->Close();
}

void VerifyMaximize(CefRefPtr<CefWindow> window) {
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_TRUE(window->IsMaximized());
  EXPECT_FALSE(window->IsFullscreen());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->IsDrawn());

  window->Restore();
  CefPostDelayedTask(TID_UI, base::BindOnce(VerifyRestore, window),
                     kStateDelayMS);
}

void RunWindowMaximize(CefRefPtr<CefWindow> window) {
  CreateBoxLayout(window);
  window->Show();
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_FALSE(window->IsFullscreen());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->IsDrawn());

  window->Maximize();
  CefPostDelayedTask(TID_UI, base::BindOnce(VerifyMaximize, window),
                     kStateDelayMS);
}

void WindowMaximizeImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunWindowMaximize);
  config->close_window = false;
  TestWindowDelegate::RunTest(event, std::move(config));
}

void WindowMaximizeFramelessImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunWindowMaximize);
  config->frameless = true;
  config->close_window = false;
  TestWindowDelegate::RunTest(event, std::move(config));
}

void VerifyMinimize(CefRefPtr<CefWindow> window) {
  EXPECT_TRUE(window->IsMinimized());
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_FALSE(window->IsFullscreen());

#if defined(OS_WIN)
  // This result is a bit unexpected, but I guess the platform considers a
  // window to be visible even when it's minimized.
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->IsDrawn());
#else
  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->IsDrawn());
#endif

  window->Restore();
  CefPostDelayedTask(TID_UI, base::BindOnce(VerifyRestore, window),
                     kStateDelayMS);
}

void RunWindowMinimize(CefRefPtr<CefWindow> window) {
  CreateBoxLayout(window);
  window->Show();
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_FALSE(window->IsFullscreen());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->IsDrawn());

  window->Minimize();
  CefPostDelayedTask(TID_UI, base::BindOnce(VerifyMinimize, window),
                     kStateDelayMS);
}

void WindowMinimizeImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunWindowMinimize);
  config->close_window = false;
  TestWindowDelegate::RunTest(event, std::move(config));
}

void WindowMinimizeFramelessImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunWindowMinimize);
  config->frameless = true;
  config->close_window = false;
  TestWindowDelegate::RunTest(event, std::move(config));
}
void WindowFullscreenTransitionComplete(CefRefPtr<CefWindow> window,
                                        size_t count) {
  EXPECT_FALSE(window->IsMinimized());

#if defined(OS_MAC)
  // On MacOS, IsMaximized() returns true when IsFullscreen() returns true.
  EXPECT_EQ(window->IsFullscreen(), window->IsMaximized());
#else
  EXPECT_FALSE(window->IsMaximized());
#endif

  if (window->IsFullscreen()) {
    EXPECT_EQ(1U, count);
    window->SetFullscreen(false);
  } else {
    EXPECT_EQ(2U, count);

    // End the test by closing the Window.
    window->Close();
  }
}

void RunWindowFullscreen(CefRefPtr<CefWindow> window) {
  CreateBoxLayout(window);
  window->Show();
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_FALSE(window->IsFullscreen());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->IsDrawn());

  window->SetFullscreen(true);
}

void WindowFullscreenImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunWindowFullscreen);
  config->on_window_fullscreen_transition_complete =
      base::BindRepeating(WindowFullscreenTransitionComplete);
  config->close_window = false;
  TestWindowDelegate::RunTest(event, std::move(config));
}

void WindowFullscreenFramelessImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunWindowFullscreen);
  config->on_window_fullscreen_transition_complete =
      base::BindRepeating(WindowFullscreenTransitionComplete);
  config->frameless = true;
  config->close_window = false;
  TestWindowDelegate::RunTest(event, std::move(config));
}

void RunWindowIcon(CefRefPtr<CefWindow> window) {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  image_util::LoadIconImage(image, 1.0);
  image_util::LoadIconImage(image, 2.0);

  EXPECT_FALSE(window->GetWindowIcon().get());
  window->SetWindowIcon(image);
  EXPECT_TRUE(image->IsSame(window->GetWindowIcon()));

  EXPECT_FALSE(window->GetWindowAppIcon().get());
  window->SetWindowAppIcon(image);
  EXPECT_TRUE(image->IsSame(window->GetWindowAppIcon()));

  window->Show();
}

void WindowIconImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunWindowIcon);
  TestWindowDelegate::RunTest(event, std::move(config));
}

void WindowIconFramelessImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunWindowIcon);
  config->frameless = true;
  TestWindowDelegate::RunTest(event, std::move(config));
}

const int kChar = 'A';
const int kCloseWindowId = 2;
bool got_accelerator;
int got_key_event_alt_count;
bool got_key_event_char;

void TriggerAccelerator(CefRefPtr<CefWindow> window) {
  window->SendKeyPress(kChar, EVENTFLAG_ALT_DOWN);
}

bool OnKeyEvent(CefRefPtr<CefWindow> window, const CefKeyEvent& event) {
  if (event.type != KEYEVENT_RAWKEYDOWN) {
    return false;
  }

  if (event.windows_key_code == VK_MENU) {
    // First we get the ALT key press in all cases.
    EXPECT_FALSE(got_key_event_char);
    if (got_key_event_alt_count == 0) {
      EXPECT_FALSE(got_accelerator);
    } else {
      EXPECT_TRUE(got_accelerator);
    }

    EXPECT_EQ(EVENTFLAG_ALT_DOWN, static_cast<int>(event.modifiers));
    got_key_event_alt_count++;
  } else if (event.windows_key_code == kChar) {
    // Then we get the char key press with the ALT modifier if the accelerator
    // isn't registered.
    EXPECT_TRUE(got_accelerator);
    EXPECT_EQ(got_key_event_alt_count, 2);
    EXPECT_FALSE(got_key_event_char);

    EXPECT_EQ(EVENTFLAG_ALT_DOWN, static_cast<int>(event.modifiers));
    got_key_event_char = true;

    // Call this method just to make sure it doesn't crash.
    window->RemoveAllAccelerators();

    // End the test by closing the Window.
    window->Close();

    return true;
  }

  return false;
}

bool OnAccelerator(CefRefPtr<CefWindow> window, int command_id) {
  EXPECT_FALSE(got_accelerator);
  EXPECT_EQ(got_key_event_alt_count, 1);
  EXPECT_FALSE(got_key_event_char);

  EXPECT_EQ(kCloseWindowId, command_id);
  got_accelerator = true;

  // Remove the accelerator.
  window->RemoveAccelerator(kCloseWindowId);

  // Now send the event without the accelerator registered. Should result in a
  // call to OnKeyEvent.
  TriggerAccelerator(window);

  return true;
}

void RunWindowAccelerator(CefRefPtr<CefWindow> window) {
  window->SetAccelerator(kCloseWindowId, kChar, false, false, true, false);
  window->Show();

  CefPostDelayedTask(TID_UI, base::BindOnce(TriggerAccelerator, window),
                     kStateDelayMS);
}

void VerifyWindowAccelerator(CefRefPtr<CefWindow> window) {
  EXPECT_TRUE(got_accelerator);
  EXPECT_EQ(got_key_event_alt_count, 2);
  EXPECT_TRUE(got_key_event_char);
}

// Expected order of events:
// 1. OnKeyEvent for ALT key press.
// 2. OnAccelerator for ALT+Char key press (with accelerator registered).
// 3. OnKeyEvent for ALT key press.
// 4. OnKeyEvent for ALT+Char key press (without accelerator registered).
void WindowAcceleratorImpl(CefRefPtr<CefWaitableEvent> event) {
  got_accelerator = false;
  got_key_event_alt_count = 0;
  got_key_event_char = false;

  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunWindowAccelerator);
  config->on_window_destroyed = base::BindOnce(VerifyWindowAccelerator);
  config->on_accelerator = base::BindRepeating(OnAccelerator);
  config->on_key_event = base::BindRepeating(OnKeyEvent);
  config->close_window = false;
  TestWindowDelegate::RunTest(event, std::move(config));
}

}  // namespace

// Test window functionality. This is primarily to exercise exposed CEF APIs
// and is not intended to comprehensively test window-related behavior (which
// we presume that Chromium is testing).
WINDOW_TEST_ASYNC(WindowCreate)
WINDOW_TEST_ASYNC(WindowCreateFrameless)
WINDOW_TEST_ASYNC(WindowCreateWithOrigin)
WINDOW_TEST_ASYNC(WindowCreateMinimized)
WINDOW_TEST_ASYNC(WindowCreateMaximized)
WINDOW_TEST_ASYNC(WindowCreateFullscreen)
WINDOW_TEST_ASYNC(WindowShowHide)
WINDOW_TEST_ASYNC(WindowShowHideFrameless)
WINDOW_TEST_ASYNC(WindowLayoutAndCoords)
WINDOW_TEST_ASYNC(WindowLayoutAndCoordsFrameless)
WINDOW_TEST_ASYNC(WindowMaximize)
WINDOW_TEST_ASYNC(WindowMaximizeFrameless)
WINDOW_TEST_ASYNC(WindowMinimize)
WINDOW_TEST_ASYNC(WindowMinimizeFrameless)
WINDOW_TEST_ASYNC(WindowFullscreen)
WINDOW_TEST_ASYNC(WindowFullscreenFrameless)
WINDOW_TEST_ASYNC(WindowIcon)
WINDOW_TEST_ASYNC(WindowIconFrameless)
WINDOW_TEST_ASYNC(WindowAccelerator)
