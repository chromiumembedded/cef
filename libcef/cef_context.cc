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
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/webpreferences.h"

// Global CefContext pointer
CefRefPtr<CefContext> _Context;

bool CefInitialize(bool multi_threaded_message_loop,
                   const std::wstring& cache_path)
{
  // Return true if the context is already initialized
  if(_Context.get())
    return true;

  // Create the new global context object
  _Context = new CefContext();
  // Initialize the global context
  return _Context->Initialize(multi_threaded_message_loop, cache_path);
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

  info.path = FilePath(plugin_info->unique_name);
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

CefContext::CefContext() : process_(NULL), webprefs_(NULL)
{
  
}

CefContext::~CefContext()
{
  // Just in case CefShutdown() isn't called
  Shutdown();
}

bool CefContext::Initialize(bool multi_threaded_message_loop,
                            const std::wstring& cache_path)
{
  cache_path_ = cache_path;

  // Initialize web preferences
  webprefs_ = new WebPreferences;
  *webprefs_ = WebPreferences();
  webprefs_->standard_font_family = L"Times";
  webprefs_->fixed_font_family = L"Courier";
  webprefs_->serif_font_family = L"Times";
  webprefs_->sans_serif_font_family = L"Helvetica";
  // These two fonts are picked from the intersection of
  // Win XP font list and Vista font list :
  //   http://www.microsoft.com/typography/fonts/winxp.htm 
  //   http://blogs.msdn.com/michkap/archive/2006/04/04/567881.aspx
  // Some of them are installed only with CJK and complex script
  // support enabled on Windows XP and are out of consideration here. 
  // (although we enabled both on our buildbots.)
  // They (especially Impact for fantasy) are not typical cursive
  // and fantasy fonts, but it should not matter for layout tests
  // as long as they're available.
#if defined(OS_MACOSX)
  webprefs_->cursive_font_family = L"Apple Chancery";
  webprefs_->fantasy_font_family = L"Papyrus";
#else
  webprefs_->cursive_font_family = L"Comic Sans MS";
  webprefs_->fantasy_font_family = L"Impact";
#endif
  webprefs_->default_encoding = "ISO-8859-1";
  webprefs_->default_font_size = 16;
  webprefs_->default_fixed_font_size = 13;
  webprefs_->minimum_font_size = 1;
  webprefs_->minimum_logical_font_size = 9;
  webprefs_->javascript_can_open_windows_automatically = true;
  webprefs_->dom_paste_enabled = true;
  webprefs_->developer_extras_enabled = true;
  webprefs_->site_specific_quirks_enabled = true;
  webprefs_->shrinks_standalone_images_to_fit = false;
  webprefs_->uses_universal_detector = false;
  webprefs_->text_areas_are_resizable = true;
  webprefs_->java_enabled = true;
  webprefs_->allow_scripts_to_close_windows = false;
  webprefs_->xss_auditor_enabled = false;
  webprefs_->remote_fonts_enabled = true;
  webprefs_->local_storage_enabled = true;
  webprefs_->application_cache_enabled = true;
  webprefs_->databases_enabled = true;
  webprefs_->allow_file_access_from_file_urls = true;
  webprefs_->accelerated_2d_canvas_enabled = true;
  webprefs_->accelerated_compositing_enabled = true;

#if defined(OS_MACOSX) || defined(OS_WIN)
  // We want to be sure to init NSPR on the main thread.
  base::EnsureNSPRInit();
#endif

  process_ = new CefProcess(multi_threaded_message_loop);
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
