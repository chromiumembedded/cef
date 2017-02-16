// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/base/cef_weak_ptr.h"
#include "include/cef_waitable_event.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"

class TestWindowDelegate : public CefWindowDelegate {
 public:
  // Default window size.
  static const int kWSize;

  // Test execution callback.
  typedef base::Callback<void(CefRefPtr<CefWindow>)> OnWindowCreatedCallback;
  typedef base::Callback<void(CefRefPtr<CefWindow>)> OnWindowDestroyedCallback;
  typedef base::Callback<bool(CefRefPtr<CefWindow>,int)> OnAcceleratorCallback;
  typedef base::Callback<bool(CefRefPtr<CefWindow>,const CefKeyEvent&)>
      OnKeyEventCallback;

  struct Config {
    OnWindowCreatedCallback on_window_created;
    OnWindowDestroyedCallback on_window_destroyed;
    OnAcceleratorCallback on_accelerator;
    OnKeyEventCallback on_key_event;
    bool frameless = false;
    bool close_window = true;
    int window_size = kWSize;
  };

  // Creates a Window with a new TestWindowDelegate instance and executes
  // |window_test| after the Window is created. |event| will be signaled once
  // the Window is closed. If |frameless| is true the Window will be created
  // without a frame. If |close_window| is true the Window will be closed
  // immediately after |window_test| returns. Otherwise, the caller is
  // responsible for closing the Window passed to |window_test|.
  static void RunTest(CefRefPtr<CefWaitableEvent> event,
                      const Config& config);

  // CefWindowDelegate methods:
  void OnWindowCreated(CefRefPtr<CefWindow> window) override;
  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
  bool IsFrameless(CefRefPtr<CefWindow> window) override;
  CefSize GetPreferredSize(CefRefPtr<CefView> view) override;
  bool OnAccelerator(CefRefPtr<CefWindow> window, int command_id) override;
  bool OnKeyEvent(CefRefPtr<CefWindow> window,
                  const CefKeyEvent& event) override;

 private:
  TestWindowDelegate(CefRefPtr<CefWaitableEvent> event,
                     const Config& config);
  ~TestWindowDelegate() override;

  void OnCloseWindow();
  void OnTimeoutWindow();

  CefRefPtr<CefWaitableEvent> event_;
  Config config_;

  CefRefPtr<CefWindow> window_;

  bool got_get_preferred_size_ = false;

  // Must be the last member.
  base::WeakPtrFactory<TestWindowDelegate> weak_ptr_factory_;

  IMPLEMENT_REFCOUNTING(TestWindowDelegate);
  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegate);
};
