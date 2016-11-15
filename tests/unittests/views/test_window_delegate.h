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
  typedef base::Callback<void(CefRefPtr<CefWindow>)> WindowTest;

  // Creates a Window with a new TestWindowDelegate instance and executes
  // |window_test| after the Window is created. |event| will be signaled once
  // the Window is closed. If |frameless| is true the Window will be created
  // without a frame. If |close_window| is true the Window will be closed
  // immediately after |window_test| returns. Otherwise, the caller is
  // responsible for closing the Window passed to |window_test|.
  static void RunTest(CefRefPtr<CefWaitableEvent> event,
                      const WindowTest& window_test,
                      bool frameless = false,
                      bool close_window = true,
                      int window_size = kWSize);

  // CefWindowDelegate methods:
  void OnWindowCreated(CefRefPtr<CefWindow> window) override;
  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
  bool IsFrameless(CefRefPtr<CefWindow> window) override;
  CefSize GetPreferredSize(CefRefPtr<CefView> view) override;

 private:
  TestWindowDelegate(CefRefPtr<CefWaitableEvent> event,
                     const WindowTest& window_test,
                     bool frameless,
                     bool close_window,
                     int window_size);
  ~TestWindowDelegate() override;

  void OnCloseWindow();
  void OnTimeoutWindow();

  CefRefPtr<CefWaitableEvent> event_;
  WindowTest window_test_;
  bool frameless_;
  bool close_window_;
  int window_size_;

  CefRefPtr<CefWindow> window_;

  bool got_get_preferred_size_ = false;

  // Must be the last member.
  base::WeakPtrFactory<TestWindowDelegate> weak_ptr_factory_;

  IMPLEMENT_REFCOUNTING(TestWindowDelegate);
  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegate);
};
