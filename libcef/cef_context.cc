// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_nplugin.h"
#include "browser_devtools_scheme_handler.h"
#include "browser_impl.h"
#include "browser_webkit_glue.h"
#include "cef_context.h"
#include "cef_time_util.h"
#include "dom_storage_common.h"
#include "dom_storage_namespace.h"
#include "dom_storage_area.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/stringprintf.h"
#include "base/string_split.h"
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
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

  webkit::WebPluginInfo info;

  info.path = FilePath(CefString(&plugin_info->unique_name));
  info.name = CefString(&plugin_info->display_name);
  info.version = CefString(&plugin_info->version);
  info.desc = CefString(&plugin_info->description);

  typedef std::vector<std::string> VectorType;
  VectorType mime_types, file_extensions, descriptions, file_extensions_parts;
  base::SplitString(CefString(&plugin_info->mime_types), '|', &mime_types);
  base::SplitString(CefString(&plugin_info->file_extensions), '|',
      &file_extensions);
  base::SplitString(CefString(&plugin_info->type_descriptions), '|',
      &descriptions);

  for (size_t i = 0; i < mime_types.size(); ++i) {
    webkit::WebPluginMimeType mimeType;
    
    mimeType.mime_type = mime_types[i];
    
    if (file_extensions.size() > i) {
      base::SplitString(file_extensions[i], ',', &file_extensions_parts);
      VectorType::const_iterator it = file_extensions_parts.begin();
      for(; it != file_extensions_parts.end(); ++it)
        mimeType.file_extensions.push_back(*(it));
      file_extensions_parts.clear();
    }

    if (descriptions.size() > i)
      mimeType.description = UTF8ToUTF16(descriptions[i]);

    info.mime_types.push_back(mimeType);
  }

  webkit::npapi::PluginEntryPoints entry_points;
#if !defined(OS_POSIX) || defined(OS_MACOSX)
  entry_points.np_getentrypoints = plugin_info->np_getentrypoints;
#endif
  entry_points.np_initialize = plugin_info->np_initialize;
  entry_points.np_shutdown = plugin_info->np_shutdown;

  webkit::npapi::PluginList::Singleton()->RegisterInternalPlugin(
      info, entry_points, true);

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

// Callback class for visiting cookies.
class VisitCookiesCallback : public base::RefCounted<VisitCookiesCallback> {
public:
  VisitCookiesCallback(CefRefPtr<CefCookieVisitor> visitor)
    : visitor_(visitor)
  {
  }

  void Run(const net::CookieList& list)
  {
    REQUIRE_IOT();

    net::CookieMonster* cookie_monster = static_cast<net::CookieMonster*>(
        _Context->request_context()->cookie_store());
    if (!cookie_monster)
      return;

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
      bool keepLooping = visitor_->Visit(cookie, count, total, deleteCookie);
      if (deleteCookie) {
        cookie_monster->DeleteCanonicalCookieAsync(cc,
            net::CookieMonster::DeleteCookieCallback());
      }
      if (!keepLooping)
        break;
    }
  }

private:
  CefRefPtr<CefCookieVisitor> visitor_;
};

void IOT_VisitAllCookies(CefRefPtr<CefCookieVisitor> visitor)
{
  REQUIRE_IOT();

  net::CookieMonster* cookie_monster = static_cast<net::CookieMonster*>(
      _Context->request_context()->cookie_store());
  if (!cookie_monster)
    return;

  scoped_refptr<VisitCookiesCallback> callback(
      new VisitCookiesCallback(visitor));

  cookie_monster->GetAllCookiesAsync(
      base::Bind(&VisitCookiesCallback::Run, callback.get()));
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
  
  scoped_refptr<VisitCookiesCallback> callback(
      new VisitCookiesCallback(visitor));

  cookie_monster->GetAllCookiesForURLWithOptionsAsync(url, options,
      base::Bind(&VisitCookiesCallback::Run, callback.get()));
}

void IOT_SetCookiePath(const CefString& path)
{
  REQUIRE_IOT();

  FilePath cookie_path;
  if (!path.empty())
    cookie_path = FilePath(path);

  _Context->request_context()->SetCookieStoragePath(cookie_path);
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

void UIT_VisitStorage(int64 namespace_id, const CefString& origin,
                      const CefString& key,
                      CefRefPtr<CefStorageVisitor> visitor)
{
  REQUIRE_UIT();

  DOMStorageContext* context = _Context->storage_context();

  // Allow storage to be allocated for localStorage so that on-disk data, if
  // any, will be available.
  bool allocation_allowed = (namespace_id == kLocalStorageNamespaceId);

  DOMStorageNamespace* ns =
      context->GetStorageNamespace(namespace_id, allocation_allowed);
  if (!ns)
    return;

  typedef std::vector<DOMStorageArea*> AreaList;
  AreaList areas;

  if (!origin.empty()) {
    // Visit only the area with the specified origin.
    DOMStorageArea* area = ns->GetStorageArea(origin, allocation_allowed);
    if (area)
      areas.push_back(area);
  } else {
    // Visit all areas.
    ns->GetStorageAreas(areas, true);
  }

  if (areas.empty())
    return;

  // Count the total number of matching keys.
  unsigned int total = 0;
  {
    NullableString16 value;
    AreaList::iterator it = areas.begin();
    for (; it != areas.end(); ) {
      DOMStorageArea* area = (*it);
      if (!key.empty()) {
        value = area->GetItem(key);
        if (value.is_null()) {
          it = areas.erase(it);
          // Don't increment the iterator.
          continue;
        } else {
          total++;
        }
      } else {
        total += area->Length();
      }
      ++it;
    }
  }

  if (total == 0)
    return;

  DOMStorageArea* area;
  bool stop = false, deleteData;
  unsigned int count = 0, i, len;
  NullableString16 keyVal, valueVal;
  string16 keyStr, valueStr;
  typedef std::vector<string16> String16List;
  String16List delete_keys;

  // Visit all matching pairs.
  AreaList::iterator it = areas.begin();
  for (; it != areas.end() && !stop; ++it) {
    // Each area.
    area = *(it);
    if (!key.empty()) {
      // Visit only the matching key.
      valueVal = area->GetItem(key);
      if (valueVal.is_null())
        valueStr.clear();
      else
        valueStr = valueVal.string();

      deleteData = false;
      stop = !visitor->Visit(static_cast<CefStorageType>(namespace_id),
          area->origin(), key, valueStr, count, total, deleteData);
      if (deleteData)
        area->RemoveItem(key);
      count++;
    } else {
      // Visit all keys.
      len = area->Length();
      for(i = 0; i < len && !stop; ++i) {
        keyVal = area->Key(i);
        if (keyVal.is_null()) {
          keyStr.clear();
          valueStr.clear();
        } else {
          keyStr = keyVal.string();
          valueVal = area->GetItem(keyStr);
          if (valueVal.is_null())
            valueStr.clear();
          else
            valueStr = valueVal.string();
        }

        deleteData = false;
        stop = !visitor->Visit(static_cast<CefStorageType>(namespace_id),
            area->origin(), keyStr, valueStr, count, total, deleteData);
        if (deleteData)
          delete_keys.push_back(keyStr);
        count++;
      }

      // Delete the requested keys.
      if (!delete_keys.empty()) {
        String16List::const_iterator it = delete_keys.begin();
        for (; it != delete_keys.end(); ++it)
          area->RemoveItem(*it);
        delete_keys.clear();
      }
    }
  }
}

void UIT_SetStoragePath(int64 namespace_id, const CefString& path)
{
  REQUIRE_UIT();

  if (namespace_id != kLocalStorageNamespaceId)
    return;

  FilePath file_path;
  if (!path.empty())
    file_path = FilePath(path);

  DOMStorageContext* context = _Context->storage_context();
  DCHECK(context);
  if (context)
    context->SetLocalStoragePath(file_path);
}

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

bool CefRegisterPlugin(const CefPluginInfo& plugin_info)
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
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
    NOTREACHED() << "context not valid";
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
    NOTREACHED() << "context not valid";
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
    NOTREACHED() << "context not valid";
    return false;
  }

  // Verify that this function is being called on the IO thread.
  if (!CefThread::CurrentlyOn(CefThread::IO)) {
    NOTREACHED() << "called on invalid thread";
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

  cookie_monster->SetCookieWithDetailsAsync(gurl, name, value, domain, path,
      expiration_time, cookie.secure, cookie.httponly,
      net::CookieStore::SetCookiesCallback());
  return true;
}

bool CefDeleteCookies(const CefString& url, const CefString& cookie_name)
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  // Verify that this function is being called on the IO thread.
  if (!CefThread::CurrentlyOn(CefThread::IO)) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  net::CookieMonster* cookie_monster = static_cast<net::CookieMonster*>(
      _Context->request_context()->cookie_store());
  if (!cookie_monster)
    return false;

  if (url.empty()) {
    // Delete all cookies.
    cookie_monster->DeleteAllAsync(net::CookieMonster::DeleteCallback());
    return true;
  }

  std::string urlStr = url;
  GURL gurl = GURL(urlStr);
  if (!gurl.is_valid())
    return false;

  if (cookie_name.empty()) {
    // Delete all matching host cookies.
    cookie_monster->DeleteAllForHostAsync(gurl,
        net::CookieMonster::DeleteCallback());
  } else {
    // Delete all matching host and domain cookies.
    cookie_monster->DeleteCookieAsync(gurl, cookie_name, base::Closure());
  }
  return true;
}

bool CefSetCookiePath(const CefString& path)
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  if (CefThread::CurrentlyOn(CefThread::IO)) {
    IOT_SetCookiePath(path);
  } else {
    CefThread::PostTask(CefThread::IO, FROM_HERE,
        base::Bind(&IOT_SetCookiePath, path));
  }

  return true;
}

bool CefVisitStorage(CefStorageType type, const CefString& origin,
                     const CefString& key,
                     CefRefPtr<CefStorageVisitor> visitor)
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  int64 namespace_id;
  if (type == ST_LOCALSTORAGE) {
    namespace_id = kLocalStorageNamespaceId;
  } else if(type == ST_SESSIONSTORAGE) {
    namespace_id = kLocalStorageNamespaceId + 1;
  } else {
    NOTREACHED() << "invalid type";
    return false;
  }

  if (CefThread::CurrentlyOn(CefThread::UI)) {
    UIT_VisitStorage(namespace_id, origin, key, visitor);
  } else {
    CefThread::PostTask(CefThread::UI, FROM_HERE,
        base::Bind(&UIT_VisitStorage, namespace_id, origin, key, visitor));
  }

  return true;
}

bool CefSetStorage(CefStorageType type, const CefString& origin,
                   const CefString& key, const CefString& value)
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  // Verify that this function is being called on the UI thread.
  if (!CefThread::CurrentlyOn(CefThread::UI)) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  int64 namespace_id;
  if (type == ST_LOCALSTORAGE) {
    namespace_id = kLocalStorageNamespaceId;
  } else if(type == ST_SESSIONSTORAGE) {
    namespace_id = kLocalStorageNamespaceId + 1;
  } else {
    NOTREACHED() << "invalid type";
    return false;
  }

  if (origin.empty()) {
    NOTREACHED() << "invalid origin";
    return false;
  }

  DOMStorageArea* area =
      _Context->storage_context()->GetStorageArea(namespace_id, origin, true);
  if (!area)
    return false;

  WebKit::WebStorageArea::Result result;
  area->SetItem(key, value, &result);
  return (result ==  WebKit::WebStorageArea::ResultOK);
}

bool CefDeleteStorage(CefStorageType type, const CefString& origin,
                      const CefString& key)
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  // Verify that this function is being called on the UI thread.
  if (!CefThread::CurrentlyOn(CefThread::UI)) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  int64 namespace_id;
  if (type == ST_LOCALSTORAGE) {
    namespace_id = kLocalStorageNamespaceId;
  } else if(type == ST_SESSIONSTORAGE) {
    namespace_id = kLocalStorageNamespaceId + 1;
  } else {
    NOTREACHED() << "invalid type";
    return false;
  }

  DOMStorageContext* context = _Context->storage_context();

  // Allow storage to be allocated for localStorage so that on-disk data, if
  // any, will be available.
  bool allocation_allowed = (namespace_id == kLocalStorageNamespaceId);

  if (origin.empty()) {
    // Delete all storage for the namespace.
    if (namespace_id == kLocalStorageNamespaceId)
      context->DeleteAllLocalStorageFiles();
    else
      context->PurgeMemory(namespace_id);
  } else if(key.empty()) {
    // Clear the storage area for the specified origin.
    if (namespace_id == kLocalStorageNamespaceId) {
      context->DeleteLocalStorageForOrigin(origin);
    } else {
      DOMStorageArea* area =
          context->GetStorageArea(namespace_id, origin, allocation_allowed);
      if (area) {
        // Calling Clear() is necessary to remove the data from the namespace.
        area->Clear();
        area->PurgeMemory();
      }
    }
  } else {
    // Delete the specified key.
    DOMStorageArea* area =
        context->GetStorageArea(namespace_id, origin, allocation_allowed);
    if (area)
      area->RemoveItem(key);
  }

  return true;
}

bool CefSetStoragePath(CefStorageType type, const CefString& path)
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  int64 namespace_id;
  if (type == ST_LOCALSTORAGE) {
    namespace_id = kLocalStorageNamespaceId;
  } else {
    NOTREACHED() << "invalid type";
    return false;
  }

  if (CefThread::CurrentlyOn(CefThread::UI)) {
    UIT_SetStoragePath(namespace_id, path);
  } else {
    CefThread::PostTask(CefThread::UI, FROM_HERE,
        base::Bind(&UIT_SetStoragePath, namespace_id, path));
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
