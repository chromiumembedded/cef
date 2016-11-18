// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_bind.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_layout.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/image_util.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/ceftests/views/test_window_delegate.h"
#include "tests/gtest/include/gtest/gtest.h"

#define WINDOW_TEST_ASYNC(name) UI_THREAD_TEST_ASYNC(ViewsWindowTest, name)

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

void WindowCreateImpl(CefRefPtr<CefWaitableEvent> event) {
  TestWindowDelegate::RunTest(event, TestWindowDelegate::WindowTest(), false);
}

void WindowCreateFramelessImpl(CefRefPtr<CefWaitableEvent> event) {
  TestWindowDelegate::RunTest(event, TestWindowDelegate::WindowTest(), true);
}

void RunWindowShowHide(CefRefPtr<CefWindow> window) {
  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->IsDrawn());
  window->Show();
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->IsDrawn());
  window->Hide();
  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->IsDrawn());
}

void WindowShowHideImpl(CefRefPtr<CefWaitableEvent> event) {
  TestWindowDelegate::RunTest(event, base::Bind(RunWindowShowHide), false);
}

void WindowShowHideFramelessImpl(CefRefPtr<CefWaitableEvent> event) {
  TestWindowDelegate::RunTest(event, base::Bind(RunWindowShowHide), true);
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
  ExpectCloseRects(CefRect(0, 0, kWSize, kWSize / 2),
                   panel_child1->GetBounds(), 1);
  ExpectCloseRects(CefRect(0, kWSize / 2, kWSize, kWSize / 2),
                   panel_child2->GetBounds(), 1);
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
  EXPECT_EQ(CefPoint(client_bounds_in_screen.x,
                     client_bounds_in_screen.y + kWSize / 2),
            point);

  // Test view from screen coordinate conversions.
  point = CefPoint(client_bounds_in_screen.x, client_bounds_in_screen.y);
  EXPECT_TRUE(view1->ConvertPointFromScreen(point));
  EXPECT_EQ(CefPoint(0, 0), point);
  point = CefPoint(client_bounds_in_screen.x,
                   client_bounds_in_screen.y + kWSize / 2);
  EXPECT_TRUE(view2->ConvertPointFromScreen(point));
  EXPECT_EQ(CefPoint(0, 0), point);

  // Test view to window coordinate conversions.
  point = CefPoint(0, 0);
  EXPECT_TRUE(view1->ConvertPointToWindow(point));
  EXPECT_EQ(CefPoint(0, 0), point);
  point = CefPoint(0, 0);
  EXPECT_TRUE(view2->ConvertPointToWindow(point));
  EXPECT_EQ(CefPoint(0, kWSize / 2), point);

  // Test view from window coordinate conversions.
  point = CefPoint(0, 0);
  EXPECT_TRUE(view1->ConvertPointFromWindow(point));
  EXPECT_EQ(CefPoint(0, 0), point);
  point = CefPoint(0, kWSize / 2);
  EXPECT_TRUE(view2->ConvertPointFromWindow(point));
  EXPECT_EQ(CefPoint(0, 0), point);

  // Test view to view coordinate conversions.
  point = CefPoint(0, 0);
  EXPECT_TRUE(view1->ConvertPointToView(view2, point));
  EXPECT_EQ(CefPoint(0, -kWSize / 2), point);
  point = CefPoint(0, 0);
  EXPECT_TRUE(view2->ConvertPointToView(view1, point));
  EXPECT_EQ(CefPoint(0, kWSize / 2), point);

  // Test view from view coordinate conversions.
  point = CefPoint(0, -kWSize / 2);
  EXPECT_TRUE(view1->ConvertPointFromView(view2, point));
  EXPECT_EQ(CefPoint(0, 0), point);
  point = CefPoint(0, kWSize / 2);
  EXPECT_TRUE(view2->ConvertPointFromView(view1, point));
  EXPECT_EQ(CefPoint(0, 0), point);

  CefRefPtr<CefDisplay> display = window->GetDisplay();
  EXPECT_TRUE(display.get());

  // We don't know what the pixel values will be, but they should be reversable.
  point = CefPoint(client_bounds_in_screen.x, client_bounds_in_screen.y);
  display->ConvertPointToPixels(point);
  display->ConvertPointFromPixels(point);
  EXPECT_EQ(CefPoint(client_bounds_in_screen.x, client_bounds_in_screen.y),
            point);
}

void WindowLayoutAndCoordsImpl(CefRefPtr<CefWaitableEvent> event) {
  TestWindowDelegate::RunTest(event, base::Bind(RunWindowLayoutAndCoords), false);
}

void WindowLayoutAndCoordsFramelessImpl(CefRefPtr<CefWaitableEvent> event) {
  TestWindowDelegate::RunTest(event, base::Bind(RunWindowLayoutAndCoords), true);
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
  CefPostDelayedTask(TID_UI, base::Bind(VerifyRestore, window), kStateDelayMS);
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
  CefPostDelayedTask(TID_UI, base::Bind(VerifyMaximize, window), kStateDelayMS);
}

void WindowMaximizeImpl(CefRefPtr<CefWaitableEvent> event) {
  TestWindowDelegate::RunTest(event, base::Bind(RunWindowMaximize), false,
                              false);
}

void WindowMaximizeFramelessImpl(CefRefPtr<CefWaitableEvent> event) {
  TestWindowDelegate::RunTest(event, base::Bind(RunWindowMaximize), true,
                              false);
}

void VerifyMinimize(CefRefPtr<CefWindow> window) {
  EXPECT_TRUE(window->IsMinimized());
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_FALSE(window->IsFullscreen());

  // This result is a bit unexpected, but I guess the platform considers a
  // window to be visible even when it's minimized.
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->IsDrawn());

  window->Restore();
  CefPostDelayedTask(TID_UI, base::Bind(VerifyRestore, window), kStateDelayMS);
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
  CefPostDelayedTask(TID_UI, base::Bind(VerifyMinimize, window), kStateDelayMS);
}

void WindowMinimizeImpl(CefRefPtr<CefWaitableEvent> event) {
  TestWindowDelegate::RunTest(event, base::Bind(RunWindowMinimize), false,
                              false);
}

void WindowMinimizeFramelessImpl(CefRefPtr<CefWaitableEvent> event) {
  TestWindowDelegate::RunTest(event, base::Bind(RunWindowMinimize), true,
                              false);
}

void VerifyFullscreenExit(CefRefPtr<CefWindow> window) {
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_FALSE(window->IsFullscreen());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->IsDrawn());

  // End the test by closing the Window.
  window->Close();
}

void VerifyFullscreen(CefRefPtr<CefWindow> window) {
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_TRUE(window->IsFullscreen());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->IsDrawn());

  window->SetFullscreen(false);
  CefPostDelayedTask(TID_UI, base::Bind(VerifyFullscreenExit, window),
                     kStateDelayMS);
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
  CefPostDelayedTask(TID_UI, base::Bind(VerifyFullscreen, window),
                     kStateDelayMS);
}

void WindowFullscreenImpl(CefRefPtr<CefWaitableEvent> event) {
  TestWindowDelegate::RunTest(event, base::Bind(RunWindowFullscreen), false,
                              false);
}

void WindowFullscreenFramelessImpl(CefRefPtr<CefWaitableEvent> event) {
  TestWindowDelegate::RunTest(event, base::Bind(RunWindowFullscreen), true,
                              false);
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
  TestWindowDelegate::RunTest(event, base::Bind(RunWindowIcon), false);
}

void WindowIconFramelessImpl(CefRefPtr<CefWaitableEvent> event) {
  TestWindowDelegate::RunTest(event, base::Bind(RunWindowIcon), true);
}

}  // namespace

// Test window functionality. This is primarily to exercise exposed CEF APIs
// and is not intended to comprehensively test window-related behavior (which
// we presume that Chromium is testing).
WINDOW_TEST_ASYNC(WindowCreate);
WINDOW_TEST_ASYNC(WindowCreateFrameless);
WINDOW_TEST_ASYNC(WindowShowHide);
WINDOW_TEST_ASYNC(WindowShowHideFrameless);
WINDOW_TEST_ASYNC(WindowLayoutAndCoords);
WINDOW_TEST_ASYNC(WindowLayoutAndCoordsFrameless);
WINDOW_TEST_ASYNC(WindowMaximize);
WINDOW_TEST_ASYNC(WindowMaximizeFrameless);
WINDOW_TEST_ASYNC(WindowMinimize);
WINDOW_TEST_ASYNC(WindowMinimizeFrameless);
WINDOW_TEST_ASYNC(WindowFullscreen);
WINDOW_TEST_ASYNC(WindowFullscreenFrameless);
WINDOW_TEST_ASYNC(WindowIcon);
WINDOW_TEST_ASYNC(WindowIconFrameless);
