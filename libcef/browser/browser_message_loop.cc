// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_message_loop.h"
#include "libcef/browser/context.h"
#include "libcef/common/content_client.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace {

// MessagePump implementation that delegates to OnScheduleMessagePumpWork() for
// scheduling.
class MessagePumpExternal : public base::MessagePump {
 public:
  MessagePumpExternal(float max_time_slice,
                      CefRefPtr<CefBrowserProcessHandler> handler)
      : max_time_slice_(max_time_slice),
        handler_(handler) {
  }

  void Run(Delegate* delegate) override {
    base::TimeTicks start = base::TimeTicks::Now();
    while (true) {
#if defined(OS_MACOSX)
      base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

      const bool has_more_work = DirectRunWork(delegate);
      if (!has_more_work)
        break;

      const base::TimeDelta& delta = base::TimeTicks::Now() - start;
      if (delta.InSecondsF() > max_time_slice_)
        break;
    }
  }

  void Quit() override {
  }

  void ScheduleWork() override {
    handler_->OnScheduleMessagePumpWork(0);
  }

  void ScheduleDelayedWork(const base::TimeTicks& delayed_work_time) override {
    const base::TimeDelta& delta = delayed_work_time - base::TimeTicks::Now();
    handler_->OnScheduleMessagePumpWork(delta.InMilliseconds());
  }

 private:
  bool DirectRunWork(Delegate* delegate) {
    bool did_work = false;
    bool did_delayed_work = false;
    bool did_idle_work = false;

    // Perform work & delayed work.
    // If no work was found, then perform idle work.

    did_work = delegate->DoWork();

    // We are using an external timer, so we don't have any action based on the
    // returned next delayed work time.
    base::TimeTicks next_time;
    did_delayed_work =  delegate->DoDelayedWork(&next_time);

    if (!did_work && !did_delayed_work) {
      did_idle_work = delegate->DoIdleWork();
    }

    return did_work || did_delayed_work || did_idle_work;
  }

  const float max_time_slice_;
  CefRefPtr<CefBrowserProcessHandler> handler_;
};

CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() {
  CefRefPtr<CefApp> app = CefContentClient::Get()->application();
  if (app)
    return app->GetBrowserProcessHandler();
  return nullptr;
}

std::unique_ptr<base::MessagePump> CreatePump() {
  const CefSettings& settings = CefContext::Get()->settings();
  if (settings.external_message_pump) {
    CefRefPtr<CefBrowserProcessHandler> handler = GetBrowserProcessHandler();
    if (handler)
      return base::WrapUnique(new MessagePumpExternal(0.01f, handler));
  }

  return base::MessageLoop::CreateMessagePumpForType(
      base::MessageLoop::TYPE_UI);
}

}  // namespace

CefBrowserMessageLoop::CefBrowserMessageLoop()
  : base::MessageLoopForUI(CreatePump()) {
  BindToCurrentThread();
}

CefBrowserMessageLoop::~CefBrowserMessageLoop() {
}

// static
CefBrowserMessageLoop* CefBrowserMessageLoop::current() {
  base::MessageLoop* loop = base::MessageLoop::current();
  DCHECK(loop->IsType(base::MessageLoop::TYPE_UI));
  return static_cast<CefBrowserMessageLoop*>(loop);
}

void CefBrowserMessageLoop::DoMessageLoopIteration() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

void CefBrowserMessageLoop::RunMessageLoop() {
  base::RunLoop run_loop;
  run_loop.Run();
}
