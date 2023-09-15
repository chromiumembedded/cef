// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_message_loop.h"
#include "libcef/common/app_manager.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_for_ui.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/message_loop/message_pump_apple.h"
#endif

#include "content/public/browser/browser_thread.h"

namespace {

// MessagePump implementation that delegates to OnScheduleMessagePumpWork() for
// scheduling.
class MessagePumpExternal : public base::MessagePumpForUI {
 public:
  MessagePumpExternal(float max_time_slice,
                      CefRefPtr<CefBrowserProcessHandler> handler)
      : max_time_slice_(max_time_slice), handler_(handler) {}

  void Run(Delegate* delegate) override {
    base::TimeTicks start = base::TimeTicks::Now();
    while (true) {
#if BUILDFLAG(IS_MAC)
      base::apple::ScopedNSAutoreleasePool autorelease_pool;
#endif

      base::TimeTicks next_run_time;  // is_null()
      const bool has_more_work = DirectRunWork(delegate, &next_run_time);
      if (!has_more_work) {
        break;
      }

      if (next_run_time.is_null()) {
        // We have more work that should run immediately.
        next_run_time = base::TimeTicks::Now();
      }

      const base::TimeDelta& delta = next_run_time - start;
      if (delta.InSecondsF() > max_time_slice_) {
        break;
      }
    }
  }

  void Quit() override {}

  void ScheduleWork() override { handler_->OnScheduleMessagePumpWork(0); }

  void ScheduleDelayedWork(
      const Delegate::NextWorkInfo& next_work_info) override {
    const base::TimeDelta& delta =
        next_work_info.delayed_run_time - next_work_info.recent_now;
    handler_->OnScheduleMessagePumpWork(delta.InMilliseconds());
  }

 private:
  static bool DirectRunWork(Delegate* delegate,
                            base::TimeTicks* next_run_time) {
    bool more_immediate_work = false;
    bool more_idle_work = false;
    bool more_delayed_work = false;

    Delegate::NextWorkInfo next_work_info = delegate->DoWork();

    // is_immediate() returns true if the next task is ready right away.
    more_immediate_work = next_work_info.is_immediate();
    if (!more_immediate_work) {
      // Check the next PendingTask's |delayed_run_time|.
      // is_max() returns true if there are no more immediate nor delayed tasks.
      more_delayed_work = !next_work_info.delayed_run_time.is_max();
      if (more_delayed_work) {
        // The only remaining work that we know about is the PendingTask.
        // Consider the run time for that task in the time slice calculation.
        *next_run_time = next_work_info.delayed_run_time;
      }
    }

    if (!more_immediate_work && !more_delayed_work) {
      // DoIdleWork() returns true if idle work was all done.
      more_idle_work = !delegate->DoIdleWork();
    }

    return more_immediate_work || more_idle_work || more_delayed_work;
  }

  const float max_time_slice_;
  CefRefPtr<CefBrowserProcessHandler> handler_;
};

CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() {
  CefRefPtr<CefApp> app = CefAppManager::Get()->GetApplication();
  if (app) {
    return app->GetBrowserProcessHandler();
  }
  return nullptr;
}

std::unique_ptr<base::MessagePump> MessagePumpFactoryForUI() {
  if (!content::BrowserThread::IsThreadInitialized(
          content::BrowserThread::UI) ||
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    CefRefPtr<CefBrowserProcessHandler> handler = GetBrowserProcessHandler();
    if (handler) {
      return std::make_unique<MessagePumpExternal>(0.01f, handler);
    }
  }

#if BUILDFLAG(IS_MAC)
  return base::message_pump_apple::Create();
#else
  return std::make_unique<base::MessagePumpForUI>();
#endif
}

}  // namespace

void InitExternalMessagePumpFactoryForUI() {
  base::MessagePump::OverrideMessagePumpForUIFactory(MessagePumpFactoryForUI);
}
