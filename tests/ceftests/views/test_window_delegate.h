// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <memory>

#include "include/base/cef_callback.h"
#include "include/base/cef_callback_helpers.h"
#include "include/base/cef_weak_ptr.h"
#include "include/cef_waitable_event.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"

class TestWindowDelegate : public CefWindowDelegate {
 public:
  // Default window size.
  static const int kWSize;

  // Test execution callback.
  using OnWindowCreatedCallback =
      base::OnceCallback<void(CefRefPtr<CefWindow>)>;
  using OnWindowDestroyedCallback =
      base::OnceCallback<void(CefRefPtr<CefWindow>)>;
  using OnWindowFullscreenTransitionCompleteCallback =
      base::RepeatingCallback<void(CefRefPtr<CefWindow>, size_t /*count*/)>;
  using OnAcceleratorCallback =
      base::RepeatingCallback<bool(CefRefPtr<CefWindow>, int)>;
  using OnKeyEventCallback =
      base::RepeatingCallback<bool(CefRefPtr<CefWindow>, const CefKeyEvent&)>;

  struct Config {
    OnWindowCreatedCallback on_window_created;
    OnWindowDestroyedCallback on_window_destroyed;
    OnWindowFullscreenTransitionCompleteCallback
        on_window_fullscreen_transition_complete;
    OnAcceleratorCallback on_accelerator;
    OnKeyEventCallback on_key_event;
    bool frameless = false;
    bool close_window = true;
    int window_size = kWSize;
    CefPoint window_origin = {};
    cef_show_state_t initial_show_state = CEF_SHOW_STATE_NORMAL;
  };

  using TestWindowDelegateFactory =
      base::OnceCallback<TestWindowDelegate*(CefRefPtr<CefWaitableEvent> event,
                                             std::unique_ptr<Config> config,
                                             const CefSize& window_size)>;

  // Creates a Window with a new TestWindowDelegate instance and executes
  // |window_test| after the Window is created. |event| will be signaled once
  // the Window is closed. If |frameless| is true the Window will be created
  // without a frame. If |close_window| is true the Window will be closed
  // immediately after |window_test| returns. Otherwise, the caller is
  // responsible for closing the Window passed to |window_test|.
  static void RunTest(CefRefPtr<CefWaitableEvent> event,
                      std::unique_ptr<Config> config,
                      TestWindowDelegateFactory factory = base::NullCallback());

  // CefWindowDelegate methods:
  void OnWindowCreated(CefRefPtr<CefWindow> window) override;
  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
  void OnWindowFullscreenTransition(CefRefPtr<CefWindow> window,
                                    bool is_completed) override;
  bool IsFrameless(CefRefPtr<CefWindow> window) override;
  CefRect GetInitialBounds(CefRefPtr<CefWindow> window) override;
  cef_show_state_t GetInitialShowState(CefRefPtr<CefWindow> window) override;
  CefSize GetPreferredSize(CefRefPtr<CefView> view) override;
  bool OnAccelerator(CefRefPtr<CefWindow> window, int command_id) override;
  bool OnKeyEvent(CefRefPtr<CefWindow> window,
                  const CefKeyEvent& event) override;
  cef_runtime_style_t GetWindowRuntimeStyle() override;

 protected:
  TestWindowDelegate(CefRefPtr<CefWaitableEvent> event,
                     std::unique_ptr<Config> config,
                     const CefSize& window_size);
  ~TestWindowDelegate() override;

  Config* config() const { return config_.get(); }
  CefRefPtr<CefWindow> window() const { return window_; }

 private:
  void OnCloseWindow();
  void OnTimeoutWindow();

  CefRefPtr<CefWaitableEvent> event_;
  std::unique_ptr<Config> config_;
  const CefSize window_size_;

  CefRefPtr<CefWindow> window_;

  bool got_get_initial_bounds_ = false;
  bool got_get_preferred_size_ = false;

  size_t fullscreen_transition_callback_count_ = 0;
  size_t fullscreen_transition_complete_count_ = 0;

  // Must be the last member.
  base::WeakPtrFactory<TestWindowDelegate> weak_ptr_factory_;

  IMPLEMENT_REFCOUNTING(TestWindowDelegate);
  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegate);
};
