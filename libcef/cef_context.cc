// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef_context.h"
#include "browser_devtools_scheme_handler.h"
#include "browser_impl.h"
#include "browser_webkit_glue.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/synchronization/waitable_event.h"

#if defined(OS_MACOSX) || defined(OS_WIN)
#include "crypto/nss_util.h"
#endif

// Both the CefContext constuctor and the CefContext::RemoveBrowser method need
// to initialize or reset to the same value.
const int kNextBrowserIdReset = 1;

// Global CefContext pointer
CefRefPtr<CefContext> _Context;

namespace {

// Used in multi-threaded message loop mode to observe shutdown of the UI
// thread.
class DestructionObserver : public MessageLoop::DestructionObserver
{
public:
  DestructionObserver(base::WaitableEvent *event) : event_(event) {}
  virtual void WillDestroyCurrentMessageLoop() {
    MessageLoop::current()->RemoveDestructionObserver(this);
    event_->Signal();
    delete this;
  }
private:
  base::WaitableEvent *event_;
};

} // anonymous

bool CefInitialize(const CefSettings& settings, CefRefPtr<CefApp> application)
{
  // Return true if the global context already exists.
  if(_Context.get())
    return true;

  if(settings.size != sizeof(cef_settings_t)) {
    NOTREACHED() << "invalid CefSettings structure size";
    return false;
  }

  // Create the new global context object.
  _Context = new CefContext();

  // Initialize the global context.
  return _Context->Initialize(settings, application);
}

void CefShutdown()
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if(!_Context->process()->CalledOnValidThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  // Shut down the global context. This will block until shutdown is complete.
  _Context->Shutdown();

  // Delete the global context object.
  _Context = NULL;
}

void CefDoMessageLoopWork()
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if(!_Context->process()->CalledOnValidThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  _Context->process()->DoMessageLoopIteration();
}

void CefRunMessageLoop()
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if(!_Context->process()->CalledOnValidThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  _Context->process()->RunMessageLoop();
}

void CefQuitMessageLoop()
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if(!_Context->process()->CalledOnValidThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  _Context->process()->QuitMessageLoop();
}


// CefContext

CefContext::CefContext()
  : initialized_(false),
    shutting_down_(false),
    next_browser_id_(kNextBrowserIdReset),
    current_webviewhost_(NULL)
{
}

CefContext::~CefContext()
{
  if(!shutting_down_)
    Shutdown();
}

bool CefContext::Initialize(const CefSettings& settings,
                            CefRefPtr<CefApp> application)
{
  settings_ = settings;
  application_ = application;

  cache_path_ = FilePath(CefString(&settings.cache_path));

#if defined(OS_MACOSX) || defined(OS_WIN)
  // We want to be sure to init NSPR on the main thread.
  crypto::EnsureNSPRInit();
#endif

  process_.reset(new CefProcess(settings_.multi_threaded_message_loop));
  process_->CreateChildThreads();

  initialized_ = true;

  // Perform DevTools scheme registration when CEF initialization is complete.
  CefThread::PostTask(CefThread::UI, FROM_HERE,
                      base::Bind(&RegisterDevToolsSchemeHandler, true));

  return true;
}

void CefContext::Shutdown()
{
  // Must always be called on the same thread as Initialize.
  DCHECK(process_->CalledOnValidThread());
  
  shutting_down_ = true;

  if(settings_.multi_threaded_message_loop) {
    // Events that will be used to signal when shutdown is complete. Start in
    // non-signaled mode so that the event will block.
    base::WaitableEvent browser_shutdown_event(false, false);
    base::WaitableEvent uithread_shutdown_event(false, false);

    // Finish shutdown on the UI thread.
    CefThread::PostTask(CefThread::UI, FROM_HERE,
        NewRunnableMethod(this, &CefContext::UIT_FinishShutdown,
            &browser_shutdown_event, &uithread_shutdown_event));

    // Block until browser shutdown is complete.
    browser_shutdown_event.Wait();

    // Delete the process to destroy the child threads.
    process_.reset(NULL);

    // Block until UI thread shutdown is complete.
    uithread_shutdown_event.Wait();
  } else {
    // Finish shutdown on the current thread, which should be the UI thread.
    UIT_FinishShutdown(NULL, NULL);

    // Delete the process to destroy the child threads.
    process_.reset(NULL);
  }
}

bool CefContext::AddBrowser(CefRefPtr<CefBrowserImpl> browser)
{
  bool found = false;
  
  AutoLock lock_scope(this);
  
  // check that the browser isn't already in the list before adding
  BrowserList::const_iterator it = browserlist_.begin();
  for(; it != browserlist_.end(); ++it) {
    if(it->get() == browser.get()) {
      found = true;
      break;
    }
  }

  if(!found)
  {
    browser->UIT_SetUniqueID(next_browser_id_++);
    browserlist_.push_back(browser);
  }

  return !found;
}

bool CefContext::RemoveBrowser(CefRefPtr<CefBrowserImpl> browser)
{
  bool deleted = false;
  bool empty = false;

  {
    AutoLock lock_scope(this);

    BrowserList::iterator it = browserlist_.begin();
    for(; it != browserlist_.end(); ++it) {
      if(it->get() == browser.get()) {
        browserlist_.erase(it);
        deleted = true;
        break;
      }
    }

    if (browserlist_.empty()) {
      next_browser_id_ = kNextBrowserIdReset;
      empty = true;
    }
  }

  if (empty) {
    if (CefThread::CurrentlyOn(CefThread::UI)) {
      webkit_glue::ClearCache();
    } else {
      CefThread::PostTask(CefThread::UI, FROM_HERE,
          NewRunnableFunction(webkit_glue::ClearCache));
    }
  }

  return deleted;
}

CefRefPtr<CefBrowserImpl> CefContext::GetBrowserByID(int id)
{
  AutoLock lock_scope(this);

  BrowserList::const_iterator it = browserlist_.begin();
  for(; it != browserlist_.end(); ++it) {
    if(it->get()->UIT_GetUniqueID() == id)
      return it->get();
  }

  return NULL;
}

std::string CefContext::locale() const
{
  std::string localeStr = CefString(&settings_.locale);
  if (!localeStr.empty())
    return localeStr;

  return "en-US";
}

void CefContext::UIT_FinishShutdown(base::WaitableEvent* browser_shutdown_event,
                                   base::WaitableEvent* uithread_shutdown_event)
{
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  BrowserList list;

  {
    AutoLock lock_scope(this);
    if (!browserlist_.empty()) {
      list = browserlist_;
      browserlist_.clear();
    }
  }

  // Destroy any remaining browser windows.
  if (!list.empty()) {
    BrowserList::iterator it = list.begin();
    for(; it != list.end(); ++it)
      (*it)->UIT_DestroyBrowser();
  }

  if (uithread_shutdown_event) {
    // The destruction observer will signal the UI thread shutdown event when
    // the UI thread has been destroyed.
    MessageLoop::current()->AddDestructionObserver(
        new DestructionObserver(uithread_shutdown_event));

    // Signal the browser shutdown event now.
    browser_shutdown_event->Signal();
  }
}
