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
    case CEF_SHOW_STATE_NUM_VALUES:
      NOTREACHED();
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

#if defined(OS_MAC)
void WindowFullscreenCreationComplete(CefRefPtr<CefWindow> window,
                                      size_t count) {
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_EQ(window->IsFullscreen(), window->IsMaximized());

  if (window->IsFullscreen()) {
    EXPECT_EQ(1U, count);
    window->SetFullscreen(false);
  } else {
    EXPECT_EQ(2U, count);
    // End the test by closing the Window.
    window->Close();
  }
}
#endif

void WindowCreateFullscreenImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->initial_show_state = CEF_SHOW_STATE_FULLSCREEN;
  config->on_window_created =
      base::BindOnce(RunWindowShow, config->initial_show_state);
#if defined(OS_MAC)
  // On macOS, destroying a fullscreen window can take a long time.
  // To prevent the next test from starting before the window is fully closed,
  // we need to exit fullscreen mode before closing the window.
  config->on_window_fullscreen_transition_complete =
      base::BindRepeating(WindowFullscreenCreationComplete);
  config->close_window = false;
#endif
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
                   3);
  ExpectCloseRects(CefRect(0, kWSize / 2, kWSize, kWSize / 2),
                   panel_child2->GetBounds(), 3);
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
                    point, 3);

  // Test view from screen coordinate conversions.
  point = CefPoint(client_bounds_in_screen.x, client_bounds_in_screen.y);
  EXPECT_TRUE(view1->ConvertPointFromScreen(point));
  EXPECT_EQ(CefPoint(0, 0), point);
  point = CefPoint(client_bounds_in_screen.x,
                   client_bounds_in_screen.y + kWSize / 2);
  EXPECT_TRUE(view2->ConvertPointFromScreen(point));
  ExpectClosePoints(CefPoint(0, 0), point, 3);

  // Test view to window coordinate conversions.
  point = CefPoint(0, 0);
  EXPECT_TRUE(view1->ConvertPointToWindow(point));
  EXPECT_EQ(CefPoint(0, 0), point);
  point = CefPoint(0, 0);
  EXPECT_TRUE(view2->ConvertPointToWindow(point));
  ExpectClosePoints(CefPoint(0, kWSize / 2), point, 3);

  // Test view from window coordinate conversions.
  point = CefPoint(0, 0);
  EXPECT_TRUE(view1->ConvertPointFromWindow(point));
  EXPECT_EQ(CefPoint(0, 0), point);
  point = CefPoint(0, kWSize / 2);
  EXPECT_TRUE(view2->ConvertPointFromWindow(point));
  ExpectClosePoints(CefPoint(0, 0), point, 3);

  // Test view to view coordinate conversions.
  point = CefPoint(0, 0);
  EXPECT_TRUE(view1->ConvertPointToView(view2, point));
  ExpectClosePoints(CefPoint(0, -kWSize / 2), point, 3);
  point = CefPoint(0, 0);
  EXPECT_TRUE(view2->ConvertPointToView(view1, point));
  ExpectClosePoints(CefPoint(0, kWSize / 2), point, 3);

  // Test view from view coordinate conversions.
  point = CefPoint(0, -kWSize / 2);
  EXPECT_TRUE(view1->ConvertPointFromView(view2, point));
  ExpectClosePoints(CefPoint(0, 0), point, 3);
  point = CefPoint(0, kWSize / 2);
  EXPECT_TRUE(view2->ConvertPointFromView(view1, point));
  ExpectClosePoints(CefPoint(0, 0), point, 3);

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
  ExpectClosePoints(point, dip, 1);
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

constexpr int kChar = 'A';
constexpr int kCloseWindowId = 2;
bool got_accelerator;

void TriggerAccelerator(CefRefPtr<CefWindow> window) {
  window->SendKeyPress(kChar, EVENTFLAG_ALT_DOWN);
}

bool OnAccelerator(CefRefPtr<CefWindow> window, int command_id) {
  EXPECT_FALSE(got_accelerator);

  EXPECT_EQ(kCloseWindowId, command_id);
  got_accelerator = true;

  window->Close();

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
}

void WindowAcceleratorImpl(CefRefPtr<CefWaitableEvent> event) {
  got_accelerator = false;

  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunWindowAccelerator);
  config->on_window_destroyed = base::BindOnce(VerifyWindowAccelerator);
  config->on_accelerator = base::BindRepeating(OnAccelerator);
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

namespace {

enum class OverlayTestMode {
  // Destroy the overlay after the Window is destroyed.
  kDestroyAfterWindowDestroyImplicit,
  kDestroyAfterWindowDestroyExplicit,

  // Destroy the overlay explicitly before the Window is shown.
  kDestroyBeforeWindowShow,
  kDestroyBeforeWindowShowAndAddAgain,

  // Destroy the overlay explicitly after the Window is shown.
  kDestroyAfterWindowShow,
  kDestroyAfterWindowShowAndAddAgain,
};

class OverlayTestWindowDelegate : public TestWindowDelegate {
 public:
  static TestWindowDelegate* Factory(OverlayTestMode test_mode,
                                     CefRefPtr<CefWaitableEvent> event,
                                     std::unique_ptr<Config> config,
                                     const CefSize& window_size) {
    return new OverlayTestWindowDelegate(test_mode, event, std::move(config),
                                         window_size);
  }

 private:
  OverlayTestWindowDelegate(OverlayTestMode test_mode,
                            CefRefPtr<CefWaitableEvent> event,
                            std::unique_ptr<Config> config,
                            const CefSize& window_size)
      : TestWindowDelegate(event, std::move(config), window_size),
        test_mode_(test_mode) {
    this->config()->on_window_created = base::BindOnce(
        &OverlayTestWindowDelegate::RunWindowCreated, base::Unretained(this));
    this->config()->on_window_destroyed = base::BindOnce(
        &OverlayTestWindowDelegate::RunWindowDestroyed, base::Unretained(this));
  }

  bool DestroyBeforeShow() const {
    return test_mode_ == OverlayTestMode::kDestroyBeforeWindowShow ||
           test_mode_ == OverlayTestMode::kDestroyBeforeWindowShowAndAddAgain;
  }

  bool DestroyAfterShow() const {
    return test_mode_ == OverlayTestMode::kDestroyAfterWindowShow ||
           test_mode_ == OverlayTestMode::kDestroyAfterWindowShowAndAddAgain;
  }

  bool AddAgain() const {
    return test_mode_ == OverlayTestMode::kDestroyBeforeWindowShowAndAddAgain ||
           test_mode_ == OverlayTestMode::kDestroyAfterWindowShowAndAddAgain;
  }

  void RunWindowCreated(CefRefPtr<CefWindow> window) {
    CreateOverlay();

    if (DestroyBeforeShow()) {
      DestroyOverlay();
    }

    window->Show();

    if (DestroyAfterShow()) {
      DestroyOverlay();
    }
  }

  void RunWindowDestroyed(CefRefPtr<CefWindow> window) {
    if (test_mode_ == OverlayTestMode::kDestroyAfterWindowDestroyExplicit) {
      DestroyOverlay();
    }
  }

  void CreateOverlay() {
    // |view_| may be reused.
    if (!view_) {
      view_ = CefPanel::CreatePanel(nullptr);
    }

    // View is visible but not drawn.
    EXPECT_EQ(nullptr, view_->GetWindow());
    EXPECT_TRUE(view_->IsVisible());
    EXPECT_FALSE(view_->IsDrawn());

    EXPECT_FALSE(controller_);
    controller_ = window()->AddOverlayView(view_, CEF_DOCKING_MODE_TOP_LEFT,
                                           /*can_activate=*/false);

    // View is visible/drawn (because it belongs to the controller), but the
    // controller itself is not.
    EXPECT_FALSE(controller_->IsVisible());
    EXPECT_FALSE(controller_->IsDrawn());
    EXPECT_TRUE(window()->IsSame(view_->GetWindow()));
    EXPECT_TRUE(view_->IsVisible());
    EXPECT_TRUE(view_->IsDrawn());

    controller_->SetVisible(true);

    EXPECT_TRUE(controller_->IsValid());
    EXPECT_TRUE(controller_->GetContentsView()->IsSame(view_));
    EXPECT_TRUE(controller_->GetWindow()->IsSame(window()));
    EXPECT_EQ(CEF_DOCKING_MODE_TOP_LEFT, controller_->GetDockingMode());

    // Controller is visible/drawn if the host window is drawn.
    if (window()->IsDrawn()) {
      EXPECT_TRUE(controller_->IsVisible());
      EXPECT_TRUE(controller_->IsDrawn());
    } else {
      EXPECT_FALSE(controller_->IsVisible());
      EXPECT_FALSE(controller_->IsDrawn());
    }

    EXPECT_TRUE(view_->IsVisible());
    EXPECT_TRUE(view_->IsDrawn());
  }

  void DestroyOverlay() {
    // Disassociates the controller from the view and host window.
    controller_->Destroy();

    EXPECT_FALSE(controller_->IsValid());
    EXPECT_EQ(nullptr, controller_->GetContentsView());
    EXPECT_EQ(nullptr, controller_->GetWindow());
    EXPECT_FALSE(controller_->IsVisible());
    EXPECT_FALSE(controller_->IsDrawn());

    // View is still visible but no longer drawn (because it no longer belongs
    // to the controller).
    EXPECT_EQ(nullptr, view_->GetWindow());
    EXPECT_TRUE(view_->IsVisible());
    EXPECT_FALSE(view_->IsDrawn());

    controller_ = nullptr;

    if (AddAgain()) {
      CreateOverlay();
    }
  }

  OverlayTestMode const test_mode_;
  CefRefPtr<CefView> view_;
  CefRefPtr<CefOverlayController> controller_;
};

void WindowOverlay(OverlayTestMode test_mode,
                   CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  TestWindowDelegate::RunTest(
      event, std::move(config),
      base::BindOnce(&OverlayTestWindowDelegate::Factory, test_mode));
}

}  // namespace

#define WINDOW_OVERLAY_TEST(name)                                     \
  namespace {                                                         \
  void WindowOverlay##name##Impl(CefRefPtr<CefWaitableEvent> event) { \
    WindowOverlay(OverlayTestMode::k##name, event);                   \
  }                                                                   \
  }                                                                   \
  WINDOW_TEST_ASYNC(WindowOverlay##name)

WINDOW_OVERLAY_TEST(DestroyAfterWindowDestroyImplicit)
WINDOW_OVERLAY_TEST(DestroyAfterWindowDestroyExplicit)
WINDOW_OVERLAY_TEST(DestroyBeforeWindowShow)
WINDOW_OVERLAY_TEST(DestroyBeforeWindowShowAndAddAgain)
WINDOW_OVERLAY_TEST(DestroyAfterWindowShow)
WINDOW_OVERLAY_TEST(DestroyAfterWindowShowAndAddAgain)
