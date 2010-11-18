// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef_context.h"
#include "browser_impl.h"
#include "browser_webkit_glue.h"
#include "cef_thread.h"
#include "cef_process.h"
#include "../include/cef_nplugin.h"

#include "base/file_util.h"
#if defined(OS_MACOSX) || defined(OS_WIN)
#include "base/nss_util.h"
#endif
#include "base/utf_string_conversions.h"
#include "webkit/glue/plugins/plugin_list.h"

// Global CefContext pointer
CefRefPtr<CefContext> _Context;

bool CefInitialize(const CefSettings& settings,
                   const CefBrowserSettings& browser_defaults)
{
  // Return true if the context is already initialized
  if(_Context.get())
    return true;

  if(settings.size != sizeof(cef_settings_t) ||
    browser_defaults.size != sizeof(cef_browser_settings_t)) {
    NOTREACHED();
    return false;
  }

  // Create the new global context object
  _Context = new CefContext();

  // Initialize the global context
  return _Context->Initialize(settings, browser_defaults);
}

void CefShutdown()
{
  // Verify that the context is already initialized
  if(!_Context.get())
    return;

  // Shut down the global context
  _Context->Shutdown();
  // Delete the global context object
  _Context = NULL;
}

void CefDoMessageLoopWork()
{
  // Verify that the context is already initialized.
  if(!_Context.get() || !_Context->process())
    return;

  if(!_Context->process()->CalledOnValidThread())
    return;
  _Context->process()->DoMessageLoopIteration();
}

static void UIT_RegisterPlugin(struct CefPluginInfo* plugin_info)
{
  REQUIRE_UIT();

  NPAPI::PluginVersionInfo info;

#if defined(OS_WIN)
  info.path = FilePath(plugin_info->unique_name);
#else
  info.path = FilePath(WideToUTF8(plugin_info->unique_name));
#endif
  info.product_name = plugin_info->display_name;
  info.file_description = plugin_info->description;
  info.file_version =plugin_info->version;

  for(size_t i = 0; i < plugin_info->mime_types.size(); ++i) {
    if(i > 0) {
      info.mime_types += L"|";
      info.file_extensions += L"|";
      info.type_descriptions += L"|";
    }

    info.mime_types += plugin_info->mime_types[i].mime_type;
    info.type_descriptions += plugin_info->mime_types[i].description;
    
    for(size_t j = 0;
        j < plugin_info->mime_types[i].file_extensions.size(); ++j) {
      if(j > 0) {
        info.file_extensions += L",";
      }
      info.file_extensions += plugin_info->mime_types[i].file_extensions[j];
    }
  }

  info.entry_points.np_getentrypoints = plugin_info->np_getentrypoints;
  info.entry_points.np_initialize = plugin_info->np_initialize;
  info.entry_points.np_shutdown = plugin_info->np_shutdown;

  NPAPI::PluginList::Singleton()->RegisterInternalPlugin(info);

  delete plugin_info;
}

bool CefRegisterPlugin(const struct CefPluginInfo& plugin_info)
{
  if(!_Context.get())
    return false;

  CefPluginInfo* pPluginInfo = new CefPluginInfo;
  *pPluginInfo = plugin_info;

  CefThread::PostTask(CefThread::UI, FROM_HERE,
      NewRunnableFunction(UIT_RegisterPlugin, pPluginInfo));
  
  return true;
}

static int GetThreadId(CefThreadId threadId)
{
  switch(threadId) {
  case TID_UI: return CefThread::UI;
  case TID_IO: return CefThread::IO;
  case TID_FILE: return CefThread::FILE;
  };
  NOTREACHED();
  return -1;
}

bool CefCurrentlyOn(CefThreadId threadId)
{
  int id = GetThreadId(threadId);
  if(id < 0)
    return false;

  return CefThread::CurrentlyOn(static_cast<CefThread::ID>(id));
}

class CefTaskHelper : public base::RefCountedThreadSafe<CefTaskHelper>
{
public:
  CefTaskHelper(CefRefPtr<CefTask> task) : task_(task) {}
  void Execute(CefThreadId threadId) { task_->Execute(threadId); }
private:
  CefRefPtr<CefTask> task_;
  DISALLOW_COPY_AND_ASSIGN(CefTaskHelper);
};

bool CefPostTask(CefThreadId threadId, CefRefPtr<CefTask> task)
{
  int id = GetThreadId(threadId);
  if(id < 0)
    return false;

  scoped_refptr<CefTaskHelper> helper(new CefTaskHelper(task));
  return CefThread::PostTask(static_cast<CefThread::ID>(id), FROM_HERE,
      NewRunnableMethod(helper.get(), &CefTaskHelper::Execute, threadId));
}

bool CefPostDelayedTask(CefThreadId threadId, CefRefPtr<CefTask> task,
                        long delay_ms)
{
  int id = GetThreadId(threadId);
  if(id < 0)
    return false;

  scoped_refptr<CefTaskHelper> helper(new CefTaskHelper(task));
  return CefThread::PostDelayedTask(static_cast<CefThread::ID>(id), FROM_HERE,
      NewRunnableMethod(helper.get(), &CefTaskHelper::Execute, threadId),
      delay_ms);
}


// CefContext

CefContext::CefContext() : process_(NULL)
{
  
}

CefContext::~CefContext()
{
  // Just in case CefShutdown() isn't called
  Shutdown();
}

bool CefContext::Initialize(const CefSettings& settings,
                            const CefBrowserSettings& browser_defaults)
{
  settings_ = settings;
  browser_defaults_ = browser_defaults;

  std::wstring cachePathStr;
  if(settings.cache_path)
    cachePathStr = settings.cache_path;
#if defined(OS_WIN)
  cache_path_ = FilePath(cachePathStr);
#else
  cache_path_ = FilePath(WideToUTF8(cachePathStr));
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
  // We want to be sure to init NSPR on the main thread.
  base::EnsureNSPRInit();
#endif

  process_ = new CefProcess(settings_.multi_threaded_message_loop);
  process_->CreateChildThreads();

  return true;
}

void CefContext::Shutdown()
{
  // Remove all existing browsers.
  //RemoveAllBrowsers();

  // Deleting the process will destroy the child threads.
  process_ = NULL;
}


bool CefContext::AddBrowser(CefRefPtr<CefBrowserImpl> browser)
{
  bool found = false;
  
  Lock();
  
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
 
  Unlock();
  return !found;
}

bool CefContext::RemoveBrowser(CefRefPtr<CefBrowserImpl> browser)
{
  bool deleted = false;
  bool empty = false;

  Lock();

  BrowserList::iterator it = browserlist_.begin();
  for(; it != browserlist_.end(); ++it) {
    if(it->get() == browser.get()) {
      browserlist_.erase(it);
      deleted = true;
      break;
    }
  }

  if (browserlist_.empty()) {
    next_browser_id_ = 1;
    empty = true;
  }

  Unlock();

  if (empty) {
    CefThread::PostTask(CefThread::UI, FROM_HERE,
        NewRunnableFunction(webkit_glue::ClearCache));
  }

  return deleted;
}

CefRefPtr<CefBrowserImpl> CefContext::GetBrowserByID(int id)
{
  CefRefPtr<CefBrowserImpl> browser;
  Lock();

  BrowserList::const_iterator it = browserlist_.begin();
  for(; it != browserlist_.end(); ++it) {
    if(it->get()->UIT_GetUniqueID() == id) {
      browser = it->get();
      break;
    }
  }

  Unlock();

  return browser;
}
