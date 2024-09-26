// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/views/test_window_delegate.h"

#include "include/cef_command_line.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_util.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>

#include "tests/shared/browser/geometry_util.h"
#include "tests/shared/browser/util_win.h"
#endif

namespace {

// Test timeout in MS.
const int kTestTimeout = 5000;

#if defined(OS_MAC)
// Match the value in view_util_mac.mm.
constexpr float kDefaultTitleBarHeight = 30;
#endif

}  // namespace

// static
const int TestWindowDelegate::kWSize = 400;

// static
void TestWindowDelegate::RunTest(CefRefPtr<CefWaitableEvent> event,
                                 std::unique_ptr<Config> config,
                                 TestWindowDelegateFactory factory) {
  CefSize window_size{config->window_size, config->window_size};

  if (!config->frameless) {
#if defined(OS_WIN)
    // Expand the client area size to full window size based on the default
    // frame window style. AdjustWindowRect expects pixel coordinates, so
    // perform the necessary conversions.
    auto scale_factor = client::GetDeviceScaleFactor();
    auto scaled_size =
        client::LogicalToDevice(config->window_size, scale_factor);

    // Convert from DIP to pixel coords.
    RECT rect = {0, 0, scaled_size, scaled_size};

    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                     false /* has_menu */);

    // Convert from pixel to DIP coords.
    auto scaled_rect = client::DeviceToLogical(
        {rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top},
        scale_factor);
    window_size = {scaled_rect.width, scaled_rect.height};
#elif defined(OS_MAC)
    // Expand client area size to include the default titlebar height.
    window_size.height += kDefaultTitleBarHeight;
#endif
  }

  TestWindowDelegate* delegate;
  if (!factory.is_null()) {
    delegate = std::move(factory).Run(event, std::move(config), window_size);
    CHECK(delegate);
  } else {
    delegate = new TestWindowDelegate(event, std::move(config), window_size);
  }

  auto window = CefWindow::CreateTopLevelWindow(delegate);
  EXPECT_EQ(delegate->GetWindowRuntimeStyle(), window->GetRuntimeStyle());
}

void TestWindowDelegate::OnWindowCreated(CefRefPtr<CefWindow> window) {
  EXPECT_FALSE(window_);
  window_ = window;

  EXPECT_TRUE(window->IsValid());
  EXPECT_FALSE(window->IsClosed());

  EXPECT_FALSE(window->IsActive());
  EXPECT_FALSE(window->IsAlwaysOnTop());

  const std::string& title = ComputeViewsWindowTitle(window, nullptr);
  window->SetTitle(title);
  EXPECT_STREQ(title.c_str(), window->GetTitle().ToString().c_str());

  EXPECT_FALSE(window->GetWindowIcon().get());
  EXPECT_FALSE(window->GetWindowAppIcon().get());

  auto display = window->GetDisplay();
  EXPECT_TRUE(display.get());

  // Size will come from GetGetInitialBounds() or GetPreferredSize() on
  // initial Window creation.
  EXPECT_TRUE(got_get_initial_bounds_);
  if (config_->window_origin.IsEmpty()) {
    EXPECT_TRUE(got_get_preferred_size_);
  } else {
    EXPECT_FALSE(got_get_preferred_size_);
  }

  // Expectations for the default |initial_show_state| value.
  if (config_->initial_show_state == CEF_SHOW_STATE_NORMAL) {
    EXPECT_FALSE(window->IsVisible());
    EXPECT_FALSE(window->IsDrawn());

    EXPECT_FALSE(window->IsMaximized());
    EXPECT_FALSE(window->IsMinimized());
    EXPECT_FALSE(window->IsFullscreen());

    CefRect client_bounds = window->GetBounds();
    if (!config_->window_origin.IsEmpty()) {
      EXPECT_EQ(config_->window_origin.x, client_bounds.x);
      EXPECT_EQ(config_->window_origin.y, client_bounds.y);
    } else {
      // Default origin is the upper-left corner of the display's work area.
      auto work_area = display->GetWorkArea();
      EXPECT_NEAR(work_area.x, client_bounds.x, 1);
      EXPECT_NEAR(work_area.y, client_bounds.y, 1);
    }

    if (config_->frameless) {
      EXPECT_NEAR(config_->window_size, client_bounds.width, 2);
      EXPECT_NEAR(config_->window_size, client_bounds.height, 2);
    } else {
      // Client area bounds calculation might have off-by-one errors on Windows
      // due to non-client frame size being calculated internally in pixels and
      // then converted to DIPs. See https://crbug.com/602692.
      EXPECT_NEAR(client_bounds.width, window_size_.width, 2);
      EXPECT_NEAR(client_bounds.height, window_size_.height, 2);
    }
  }

  // Run the callback.
  if (!config_->on_window_created.is_null()) {
    std::move(config_->on_window_created).Run(window);
  }

  if (config_->close_window) {
    // Close the window asynchronously.
    CefPostTask(TID_UI,
                base::BindOnce(&TestWindowDelegate::OnCloseWindow, this));
  } else {
    const auto timeout = GetConfiguredTestTimeout(kTestTimeout);
    if (timeout) {
      // Timeout the test after a reasonable delay. Use a WeakPtr so that the
      // delayed task doesn't keep this object alive.
      CefPostDelayedTask(TID_UI,
                         base::BindOnce(&TestWindowDelegate::OnTimeoutWindow,
                                        weak_ptr_factory_.GetWeakPtr()),
                         *timeout);
    }
  }
}

void TestWindowDelegate::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
  EXPECT_TRUE(window->IsSame(window_));

  EXPECT_TRUE(window->IsValid());
  EXPECT_TRUE(window->IsClosed());
  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->IsDrawn());

  // Run the callback.
  if (!config_->on_window_destroyed.is_null()) {
    std::move(config_->on_window_destroyed).Run(window);
  }

  window_ = nullptr;

  // Don't execute the timeout callback.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void TestWindowDelegate::OnWindowFullscreenTransition(
    CefRefPtr<CefWindow> window,
    bool is_completed) {
  EXPECT_TRUE(window->IsSame(window_));

  EXPECT_TRUE(window->IsValid());
  EXPECT_FALSE(window->IsClosed());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->IsDrawn());

  fullscreen_transition_callback_count_++;

#if defined(OS_MAC)
  // Only one callback when window is initially shown fullscreen on MacOS.
  if (config_->initial_show_state == CEF_SHOW_STATE_FULLSCREEN &&
      fullscreen_transition_complete_count_ == 0) {
    EXPECT_TRUE(is_completed);
    EXPECT_EQ(1U, fullscreen_transition_callback_count_);
  } else {
    // Two callbacks otherwise.
    EXPECT_EQ(is_completed ? 2U : 1U, fullscreen_transition_callback_count_);
  }
#else
  // Single callback on other platforms.
  EXPECT_TRUE(is_completed);
  EXPECT_EQ(1U, fullscreen_transition_callback_count_);
#endif

  if (is_completed) {
    fullscreen_transition_complete_count_++;

    // Reset intermediate state.
    fullscreen_transition_callback_count_ = 0;

    // Run the callback.
    if (!config_->on_window_fullscreen_transition_complete.is_null()) {
      config_->on_window_fullscreen_transition_complete.Run(
          window, fullscreen_transition_complete_count_);
    }
  }
}

bool TestWindowDelegate::IsFrameless(CefRefPtr<CefWindow> window) {
  return config_->frameless;
}

CefRect TestWindowDelegate::GetInitialBounds(CefRefPtr<CefWindow> window) {
  got_get_initial_bounds_ = true;
  if (!config_->window_origin.IsEmpty()) {
    return CefRect(config_->window_origin.x, config_->window_origin.y,
                   window_size_.width, window_size_.height);
  }

  // Call GetPreferredSize().
  return CefRect();
}

cef_show_state_t TestWindowDelegate::GetInitialShowState(
    CefRefPtr<CefWindow> window) {
  return config_->initial_show_state;
}

CefSize TestWindowDelegate::GetPreferredSize(CefRefPtr<CefView> view) {
  got_get_preferred_size_ = true;
  return window_size_;
}

bool TestWindowDelegate::OnAccelerator(CefRefPtr<CefWindow> window,
                                       int command_id) {
  if (!config_->on_accelerator.is_null()) {
    return config_->on_accelerator.Run(window_, command_id);
  }
  return false;
}

bool TestWindowDelegate::OnKeyEvent(CefRefPtr<CefWindow> window,
                                    const CefKeyEvent& event) {
  if (!config_->on_key_event.is_null()) {
    return config_->on_key_event.Run(window_, event);
  }
  return false;
}

cef_runtime_style_t TestWindowDelegate::GetWindowRuntimeStyle() {
  return UseAlloyStyleWindowGlobal() ? CEF_RUNTIME_STYLE_ALLOY
                                     : CEF_RUNTIME_STYLE_CHROME;
}

TestWindowDelegate::TestWindowDelegate(CefRefPtr<CefWaitableEvent> event,
                                       std::unique_ptr<Config> config,
                                       const CefSize& window_size)
    : event_(event),
      config_(std::move(config)),
      window_size_(window_size),
      weak_ptr_factory_(this) {}

TestWindowDelegate::~TestWindowDelegate() {
  // Complete the test (signal the event) asynchronously so objects on the call
  // stack have a chance to unwind.
  CefPostTask(TID_UI, base::BindOnce(SignalEvent, event_));
}

void TestWindowDelegate::OnCloseWindow() {
  if (!window_) {
    return;
  }

  EXPECT_TRUE(window_->IsValid());
  EXPECT_FALSE(window_->IsClosed());

  // Close() may clear |window_| so keep a reference.
  CefRefPtr<CefWindow> window = window_;
  window->Close();

  EXPECT_TRUE(window->IsValid());
  EXPECT_TRUE(window->IsClosed());
}

void TestWindowDelegate::OnTimeoutWindow() {
  EXPECT_TRUE(false) << "Test timed out after " << kTestTimeout << "ms";
  OnCloseWindow();
}
