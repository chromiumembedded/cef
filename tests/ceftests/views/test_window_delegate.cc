// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/views/test_window_delegate.h"

#include "include/cef_command_line.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

// Test timeout in MS.
const int kTestTimeout = 5000;

}  // namespace

// static
const int TestWindowDelegate::kWSize = 400;

// static
void TestWindowDelegate::RunTest(CefRefPtr<CefWaitableEvent> event,
                                 const Config& config) {
  CefWindow::CreateTopLevelWindow(
      new TestWindowDelegate(event, config));
}

void TestWindowDelegate::OnWindowCreated(CefRefPtr<CefWindow> window) {
  EXPECT_FALSE(window_);
  window_ = window;

  EXPECT_TRUE(window->IsValid());
  EXPECT_FALSE(window->IsClosed());

  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->IsDrawn());

  EXPECT_FALSE(window->IsActive());
  EXPECT_FALSE(window->IsAlwaysOnTop());
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_FALSE(window->IsFullscreen());

  const char* title = "ViewsTest";
  window->SetTitle(title);
  EXPECT_STREQ(title, window->GetTitle().ToString().c_str());

  EXPECT_FALSE(window->GetWindowIcon().get());
  EXPECT_FALSE(window->GetWindowAppIcon().get());

  EXPECT_TRUE(window->GetDisplay().get());

  // Size will come from GetPreferredSize() on initial Window creation.
  EXPECT_TRUE(got_get_preferred_size_);
  CefRect client_bounds = window->GetClientAreaBoundsInScreen();
  if (config_.frameless) {
    EXPECT_EQ(config_.window_size, client_bounds.width);
    EXPECT_EQ(config_.window_size, client_bounds.height);
  } else {
    // Client area bounds calculation might have off-by-one errors on Windows
    // due to non-client frame size being calculated internally in pixels and
    // then converted to DIPs. See http://crbug.com/602692.
    EXPECT_TRUE(abs(client_bounds.width - config_.window_size) <= 1);
    EXPECT_TRUE(abs(client_bounds.height - config_.window_size) <= 1);
  }

  // Run the callback.
  if (!config_.on_window_created.is_null())
    config_.on_window_created.Run(window);

  if (config_.close_window) {
    // Close the window asynchronously.
    CefPostTask(TID_UI,
        base::Bind(&TestWindowDelegate::OnCloseWindow, this));
  } else if (!CefCommandLine::GetGlobalCommandLine()->HasSwitch(
                 "disable-test-timeout")) {
    // Timeout the test after a reasonable delay. Use a WeakPtr so that the
    // delayed task doesn't keep this object alive.
    CefPostDelayedTask(TID_UI,
        base::Bind(&TestWindowDelegate::OnTimeoutWindow,
                   weak_ptr_factory_.GetWeakPtr()), kTestTimeout);
  }
}

void TestWindowDelegate::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
  EXPECT_TRUE(window->IsSame(window_));

  EXPECT_TRUE(window->IsValid());
  EXPECT_TRUE(window->IsClosed());
  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->IsDrawn());

  // Run the callback.
  if (!config_.on_window_destroyed.is_null())
    config_.on_window_destroyed.Run(window);

  window_ = nullptr;

  // Don't execute the timeout callback.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool TestWindowDelegate::IsFrameless(CefRefPtr<CefWindow> window) {
  return config_.frameless;
}

CefSize TestWindowDelegate::GetPreferredSize(CefRefPtr<CefView> view) {
  got_get_preferred_size_ = true;
  return CefSize(config_.window_size, config_.window_size);
}

bool TestWindowDelegate::OnAccelerator(CefRefPtr<CefWindow> window,
                                       int command_id) {
  if (!config_.on_accelerator.is_null())
    return config_.on_accelerator.Run(window_, command_id);
  return false;
}

bool TestWindowDelegate::OnKeyEvent(CefRefPtr<CefWindow> window,
                                    const CefKeyEvent& event) {
  if (!config_.on_key_event.is_null())
    return config_.on_key_event.Run(window_, event);
  return false;
}

TestWindowDelegate::TestWindowDelegate(CefRefPtr<CefWaitableEvent> event,
                                       const Config& config)
    : event_(event),
      config_(config),
      weak_ptr_factory_(this) {
}

TestWindowDelegate::~TestWindowDelegate() {
  // Complete the test (signal the event) asynchronously so objects on the call
  // stack have a chance to unwind.
  CefPostTask(TID_UI, base::Bind(SignalEvent, event_));
}

void TestWindowDelegate::OnCloseWindow() {
  if (!window_)
    return;

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
