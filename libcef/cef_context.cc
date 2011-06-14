// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef_context.h"
#include "browser_impl.h"
#include "browser_webkit_glue.h"
#include "cef_thread.h"
#include "cef_time_util.h"
#include "cef_process.h"
#include "../include/cef_nplugin.h"

#include "base/file_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "net/base/cookie_monster.h"
#include "webkit/plugins/npapi/plugin_list.h"

#if defined(OS_MACOSX) || defined(OS_WIN)
#include "crypto/nss_util.h"
#endif

// Both the CefContext constuctor and the CefContext::RemoveBrowser method need
// to initialize or reset to the same value.
const int kNextBrowserIdReset = 1;

// Global CefContext pointer
CefRefPtr<CefContext> _Context;

namespace {

void UIT_RegisterPlugin(CefPluginInfo* plugin_info)
{
  REQUIRE_UIT();

  webkit::npapi::WebPluginInfo info;

  FilePath filename = FilePath(CefString(&plugin_info->unique_name));
  std::string name = CefString(&plugin_info->display_name);
  std::string description = CefString(&plugin_info->description);
  std::string mime_type = CefString(&plugin_info->mime_type);

  webkit::npapi::PluginEntryPoints entry_points;
#if !defined(OS_POSIX) || defined(OS_MACOSX)
  entry_points.np_getentrypoints = plugin_info->np_getentrypoints;
#endif
  entry_points.np_initialize = plugin_info->np_initialize;
  entry_points.np_shutdown = plugin_info->np_shutdown;

  webkit::npapi::PluginList::Singleton()->RegisterInternalPlugin(filename,
      name, description, mime_type, entry_points);

  delete plugin_info;
}

int GetThreadId(CefThreadId threadId)
{
  switch(threadId) {
  case TID_UI: return CefThread::UI;
  case TID_IO: return CefThread::IO;
  case TID_FILE: return CefThread::FILE;
  };
  NOTREACHED();
  return -1;
}

void IOT_VisitCookies(net::CookieMonster* cookie_monster,
                      const net::CookieList& list,
                      CefRefPtr<CefCookieVisitor> visitor)
{
  int total = list.size(), count = 0;
  
  net::CookieList::const_iterator it = list.begin();
  for (; it != list.end(); ++it, ++count) {
    CefCookie cookie;
    const net::CookieMonster::CanonicalCookie& cc = *(it);
    
    CefString(&cookie.name).FromString(cc.Name());
    CefString(&cookie.value).FromString(cc.Value());
    CefString(&cookie.domain).FromString(cc.Domain());
    CefString(&cookie.path).FromString(cc.Path());
    cookie.secure = cc.IsSecure();
    cookie.httponly = cc.IsHttpOnly();
    cef_time_from_basetime(cc.CreationDate(), cookie.creation);
    cef_time_from_basetime(cc.LastAccessDate(), cookie.last_access);
    cookie.has_expires = cc.DoesExpire();
    if (cookie.has_expires)
      cef_time_from_basetime(cc.ExpiryDate(), cookie.expires);

    bool deleteCookie = false;
    bool keepLooping = visitor->Visit(cookie, count, total, deleteCookie);
    if (deleteCookie)
      cookie_monster->DeleteCanonicalCookie(cc);
    if (!keepLooping)
      break;
  }
}

void IOT_VisitAllCookies(CefRefPtr<CefCookieVisitor> visitor)
{
  REQUIRE_IOT();

  net::CookieMonster* cookie_monster = static_cast<net::CookieMonster*>(
      _Context->request_context()->cookie_store());
  if (!cookie_monster)
    return;

  net::CookieList list = cookie_monster->GetAllCookies();
  if (!list.empty())
    IOT_VisitCookies(cookie_monster, list, visitor);
}

void IOT_VisitUrlCookies(const GURL& url, bool includeHttpOnly,
                         CefRefPtr<CefCookieVisitor> visitor)
{
  REQUIRE_IOT();

  net::CookieMonster* cookie_monster = static_cast<net::CookieMonster*>(
      _Context->request_context()->cookie_store());
  if (!cookie_monster)
    return;

  net::CookieOptions options;
  if (includeHttpOnly)
    options.set_include_httponly();
  net::CookieList list =
      cookie_monster->GetAllCookiesForURLWithOptions(url, options);
  if (!list.empty())
    IOT_VisitCookies(cookie_monster, list, visitor);
}

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

bool CefInitialize(const CefSettings& settings)
{
  // Return true if the global context already exists.
  if(_Context.get())
    return true;

  if(settings.size != sizeof(cef_settings_t)) {
    NOTREACHED();
    return false;
  }

  // Create the new global context object.
  _Context = new CefContext();

  // Initialize the global context.
  return _Context->Initialize(settings);
}

void CefShutdown()
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED();
    return;
  }

  // Must always be called on the same thread as Initialize.
  if(!_Context->process()->CalledOnValidThread()) {
    NOTREACHED();
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
    NOTREACHED();
    return;
  }

  // Must always be called on the same thread as Initialize.
  if(!_Context->process()->CalledOnValidThread()) {
    NOTREACHED();
    return;
  }

  _Context->process()->DoMessageLoopIteration();
}

void CefRunMessageLoop()
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED();
    return;
  }

  // Must always be called on the same thread as Initialize.
  if(!_Context->process()->CalledOnValidThread()) {
    NOTREACHED();
    return;
  }

  _Context->process()->RunMessageLoop();
}

bool CefRegisterPlugin(const CefPluginInfo& plugin_info)
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED();
    return false;
  }

  CefThread::PostTask(CefThread::UI, FROM_HERE,
      NewRunnableFunction(UIT_RegisterPlugin, new CefPluginInfo(plugin_info)));
  
  return true;
}

bool CefCurrentlyOn(CefThreadId threadId)
{
  int id = GetThreadId(threadId);
  if(id < 0)
    return false;

  return CefThread::CurrentlyOn(static_cast<CefThread::ID>(id));
}

class CefTaskHelper : public Task
{
public:
  CefTaskHelper(CefRefPtr<CefTask> task, CefThreadId threadId)
    : task_(task), thread_id_(threadId) {}
  virtual void Run() { task_->Execute(thread_id_); }
private:
  CefRefPtr<CefTask> task_;
  CefThreadId thread_id_;
  DISALLOW_COPY_AND_ASSIGN(CefTaskHelper);
};

bool CefPostTask(CefThreadId threadId, CefRefPtr<CefTask> task)
{
  int id = GetThreadId(threadId);
  if(id < 0)
    return false;

  return CefThread::PostTask(static_cast<CefThread::ID>(id), FROM_HERE,
      new CefTaskHelper(task, threadId));
}

bool CefPostDelayedTask(CefThreadId threadId, CefRefPtr<CefTask> task,
                        long delay_ms)
{
  int id = GetThreadId(threadId);
  if(id < 0)
    return false;

  return CefThread::PostDelayedTask(static_cast<CefThread::ID>(id), FROM_HERE,
      new CefTaskHelper(task, threadId), delay_ms);
}

bool CefParseURL(const CefString& url,
                 CefURLParts& parts)
{
  GURL gurl(url.ToString());
  if (!gurl.is_valid())
    return false;

  CefString(&parts.spec).FromString(gurl.spec());
  CefString(&parts.scheme).FromString(gurl.scheme());
  CefString(&parts.username).FromString(gurl.username());
  CefString(&parts.password).FromString(gurl.password());
  CefString(&parts.host).FromString(gurl.host());
  CefString(&parts.port).FromString(gurl.port());
  CefString(&parts.path).FromString(gurl.path());
  CefString(&parts.query).FromString(gurl.query());

  return true;
}

bool CefCreateURL(const CefURLParts& parts,
                  CefString& url)
{
  std::string spec = CefString(parts.spec.str, parts.spec.length, false);
  std::string scheme = CefString(parts.scheme.str, parts.scheme.length, false);
  std::string username =
      CefString(parts.username.str, parts.username.length, false);
  std::string password =
      CefString(parts.password.str, parts.password.length, false);
  std::string host = CefString(parts.host.str, parts.host.length, false);
  std::string port = CefString(parts.port.str, parts.port.length, false);
  std::string path = CefString(parts.path.str, parts.path.length, false);
  std::string query = CefString(parts.query.str, parts.query.length, false);

  GURL gurl;
  if (!spec.empty()) {
    gurl = GURL(spec);
  } else if (!scheme.empty() && !host.empty()) {
    std::stringstream ss;
    ss << scheme << "://";
    if (!username.empty()) {
      ss << username;
      if (!password.empty())
        ss << ":" << password;
      ss << "@";
    }
    ss << host;
    if (!port.empty())
      ss << ":" << port;
    if (!path.empty())
      ss << path;
    if (!query.empty())
      ss << "?" << query;
    gurl = GURL(ss.str());
  }

  if (gurl.is_valid()) {
    url = gurl.spec();
    return true;
  }

  return false;
}

bool CefVisitAllCookies(CefRefPtr<CefCookieVisitor> visitor)
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED();
    return false;
  }

  return CefThread::PostTask(CefThread::IO, FROM_HERE,
      NewRunnableFunction(IOT_VisitAllCookies, visitor));
}

bool CefVisitUrlCookies(const CefString& url, bool includeHttpOnly,
                        CefRefPtr<CefCookieVisitor> visitor)
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED();
    return false;
  }

  std::string urlStr = url;
  GURL gurl = GURL(urlStr);
  if (!gurl.is_valid())
    return false;

  return CefThread::PostTask(CefThread::IO, FROM_HERE,
      NewRunnableFunction(IOT_VisitUrlCookies, gurl, includeHttpOnly, visitor));
}

bool CefSetCookie(const CefString& url, const CefCookie& cookie)
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED();
    return false;
  }

  // Verify that this function is being called on the IO thread.
  if (!CefThread::CurrentlyOn(CefThread::IO)) {
    NOTREACHED();
    return false;
  }

  net::CookieMonster* cookie_monster = static_cast<net::CookieMonster*>(
      _Context->request_context()->cookie_store());
  if (!cookie_monster)
    return false;

  std::string urlStr = url;
  GURL gurl = GURL(urlStr);
  if (!gurl.is_valid())
    return false;

  std::string name = CefString(&cookie.name).ToString();
  std::string value = CefString(&cookie.value).ToString();
  std::string domain = CefString(&cookie.domain).ToString();
  std::string path = CefString(&cookie.path).ToString();
  
  base::Time expiration_time;
  if (cookie.has_expires)
    cef_time_to_basetime(cookie.expires, expiration_time);

  return cookie_monster->SetCookieWithDetails(gurl, name, value, domain, path,
                                              expiration_time, cookie.secure,
                                              cookie.httponly);
}

bool CefDeleteCookies(const CefString& url, const CefString& cookie_name)
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED();
    return false;
  }

  // Verify that this function is being called on the IO thread.
  if (!CefThread::CurrentlyOn(CefThread::IO)) {
    NOTREACHED();
    return false;
  }

  net::CookieMonster* cookie_monster = static_cast<net::CookieMonster*>(
      _Context->request_context()->cookie_store());
  if (!cookie_monster)
    return false;

  if (url.empty()) {
    // Delete all cookies.
    cookie_monster->DeleteAll(true);
    return true;
  }

  std::string urlStr = url;
  GURL gurl = GURL(urlStr);
  if (!gurl.is_valid())
    return false;

  if (cookie_name.empty()) {
    // Delete all matching host cookies.
    cookie_monster->DeleteAllForHost(gurl);
  } else {
    // Delete all matching host and domain cookies.
    cookie_monster->DeleteCookie(gurl, cookie_name);
  }
  return true;
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

bool CefContext::Initialize(const CefSettings& settings)
{
  settings_ = settings;

  cache_path_ = FilePath(CefString(&settings.cache_path));

#if defined(OS_MACOSX) || defined(OS_WIN)
  // We want to be sure to init NSPR on the main thread.
  crypto::EnsureNSPRInit();
#endif

  process_ = new CefProcess(settings_.multi_threaded_message_loop);
  process_->CreateChildThreads();

  initialized_ = true;

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
    process_ = NULL;

    // Block until UI thread shutdown is complete.
    uithread_shutdown_event.Wait();
  } else {
    // Finish shutdown on the current thread, which should be the UI thread.
    UIT_FinishShutdown(NULL, NULL);

    // Delete the process to destroy the child threads.
    process_ = NULL;
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
    CefThread::PostTask(CefThread::UI, FROM_HERE,
        NewRunnableFunction(webkit_glue::ClearCache));
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
