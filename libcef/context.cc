// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "context.h"
#include "browser_impl.h"
#include "browser_resource_loader_bridge.h"
#include "browser_request_context.h"
#include "browser_webkit_glue.h"
#include "browser_webkit_init.h"
#include "../include/cef_nplugin.h"

#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/resource_util.h"
#include "base/stats_table.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_module.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptController.h"
#include "webkit/extensions/v8/gc_extension.h"
#include "webkit/glue/plugins/webplugin.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"

#include <commctrl.h>


// Global CefContext pointer
CefRefPtr<CefContext> _Context;

static const char* kStatsFilePrefix = "libcef_";
static int kStatsFileThreads = 20;
static int kStatsFileCounters = 200;


// Class used to process events on the current message loop.
class CefMessageLoopForUI : public MessageLoopForUI
{
  typedef MessageLoopForUI inherited;

public:
  CefMessageLoopForUI()
  {
  }

  virtual bool DoIdleWork() {
    bool valueToRet = inherited::DoIdleWork();
    pump_->Quit();
    return valueToRet;
  }

  void DoMessageLoopIteration() {
    Run(NULL);
  }
};


bool CefInitialize(bool multi_threaded_message_loop,
                   const std::wstring& cache_path)
{
  // Return true if the context is already initialized
  if(_Context.get())
    return true;

  // Create the new global context object
  _Context = new CefContext();
  // Initialize the glboal context
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
  if(!_Context->RunningOnUIThread())
    return;
  ((CefMessageLoopForUI*)CefMessageLoopForUI::current())->DoMessageLoopIteration();
}

bool CefRegisterPlugin(const struct CefPluginInfo& plugin_info)
{
  if(!_Context.get())
    return false;

  CefPluginInfo* pPluginInfo = new CefPluginInfo;
  *pPluginInfo = plugin_info;

   PostTask(FROM_HERE, NewRunnableMethod(_Context.get(),
        &CefContext::UIT_RegisterPlugin, pPluginInfo));
  
  return true;
}

void CefContext::UIT_RegisterPlugin(struct CefPluginInfo* plugin_info)
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

bool CefContext::DoInitialize()
{
  HRESULT res;

  // Initialize common controls
  res = CoInitialize(NULL);
  DCHECK(SUCCEEDED(res));
  INITCOMMONCONTROLSEX InitCtrlEx;
  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC  = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&InitCtrlEx);

  // Start COM stuff
  res = OleInitialize(NULL);
  DCHECK(SUCCEEDED(res));

  // Initialize the global CommandLine object.
  CommandLine::Init(0, NULL);

  // Initialize WebKit.
  webkit_init_ = new BrowserWebKitInit();

  // Initialize WebKit encodings
  webkit_glue::InitializeTextEncoding();

  // Initializing with a default context, which means no on-disk cookie DB,
  // and no support for directory listings.
  //PathService::Get(base::DIR_EXE, &cache_path);
  BrowserResourceLoaderBridge::Init(FilePath(cache_path_), net::HttpCache::NORMAL,
      false);

  // Load ICU data tables.
  bool ret = icu_util::Initialize();
  if(!ret) {
    MessageBox(NULL, L"Failed to load the required icudt38 library",
      L"CEF Initialization Error", MB_ICONERROR | MB_OK);
    return false;
  }

  // Config the network module so it has access to a limited set of resources.
  net::NetModule::SetResourceProvider(webkit_glue::NetResourceProvider);

  // Load and initialize the stats table.  Attempt to construct a somewhat
  // unique name to isolate separate instances from each other.
  statstable_ = new StatsTable(
      kStatsFilePrefix + Uint64ToString(base::RandUint64()),
      kStatsFileThreads,
      kStatsFileCounters);
  StatsTable::set_current(statstable_);

  // CEF always exposes the GC.
  webkit_glue::SetJavaScriptFlags(L"--expose-gc");
  // Expose GCController to JavaScript.
  WebKit::WebScriptController::registerExtension(
      extensions_v8::GCExtension::Get());

  return true;
}

void CefContext::DoUninitialize()
{
  // Flush any remaining messages.  This ensures that any accumulated
  // Task objects get destroyed before we exit, which avoids noise in
  // purify leak-test results.
  MessageLoop::current()->RunAllPending();

  BrowserResourceLoaderBridge::Shutdown();
    
  // Tear down the shared StatsTable.
  StatsTable::set_current(NULL);
  delete statstable_;
  statstable_ = NULL;

  // Shut down WebKit.
  delete webkit_init_;
  webkit_init_ = NULL;

  // Uninitialize COM stuff
  OleUninitialize();

  // Uninitialize common controls
  CoUninitialize();
}

DWORD WINAPI ThreadHandlerUI(LPVOID lpParam)
{
  CefContext *pContext = static_cast<CefContext*>(lpParam);

  if (!pContext->DoInitialize()) 
    return 1;

  // Instantiate the message loop for this thread.
  MessageLoopForUI main_message_loop;
  pContext->SetMessageLoopForUI(&main_message_loop);

  // Notify the context that initialization is complete so that the
  // Initialize() function can return.
  pContext->NotifyEvent();

  // Execute the message loop that will run until a quit task is received.
  MessageLoop::current()->Run();

  pContext->DoUninitialize();

  return 0;
}

CefContext::CefContext()
{
  hthreadui_ = NULL;
  idthreadui_ = 0;
  heventui_ = NULL;
  messageloopui_ = NULL;
  in_transition_ = false;
  webprefs_ = NULL;
  hinstance_ = ::GetModuleHandle(NULL);
  next_browser_id_ = 1;
  webkit_init_ = NULL;
}

CefContext::~CefContext()
{
  // Just in case CefShutdown() isn't called
  Shutdown();
}

bool CefContext::Initialize(bool multi_threaded_message_loop,
                            const std::wstring& cache_path)
{
  bool initialized = false, intransition = false;
  
  Lock();
  
  // We only need to initialize if the UI thread is not currently running
  if(!idthreadui_) {
    // Only start the initialization if we're not currently in a transitional
    // state
    intransition = in_transition_;
    if(!intransition) {
      // We are now in a transitional state
      in_transition_ = true;

      cache_path_ = cache_path;

      // Register the window class
      WNDCLASSEX wcex = {
        /* cbSize = */ sizeof(WNDCLASSEX),
        /* style = */ CS_HREDRAW | CS_VREDRAW,
        /* lpfnWndProc = */ CefBrowserImpl::WndProc,
        /* cbClsExtra = */ 0,
        /* cbWndExtra = */ 0,
        /* hInstance = */ hinstance_,
        /* hIcon = */ NULL,
        /* hCursor = */ LoadCursor(NULL, IDC_ARROW),
        /* hbrBackground = */ 0,
        /* lpszMenuName = */ NULL,
        /* lpszClassName = */ CefBrowserImpl::GetWndClass(),
        /* hIconSm = */ NULL,
      };
      RegisterClassEx(&wcex);

#ifndef _DEBUG
      // Only log error messages and above in release build.
      logging::SetMinLogLevel(logging::LOG_ERROR);
#endif

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

      if (multi_threaded_message_loop) {
        // Event that will be used to signal thread setup completion. Start
        // in non-signaled mode so that the event will block.
        heventui_ = CreateEvent(NULL, TRUE, FALSE, NULL);
        DCHECK(heventui_ != NULL);
        
        // Thread that hosts the UI loop
        hthreadui_ = CreateThread(
            NULL, 0, ThreadHandlerUI, this, 0, &idthreadui_);
        DCHECK(hthreadui_ != NULL);
        DCHECK(idthreadui_ != 0);
      } else {
        if (!DoInitialize()) {
          // TODO: Process initialization errors
        }
        // Create our own message loop there
        SetMessageLoopForUI(new CefMessageLoopForUI());
        idthreadui_ = GetCurrentThreadId();
        DCHECK(idthreadui_ != 0);
      }
      
      initialized = true;
    }
  }

  Unlock();
  
  if(initialized) {
    if (multi_threaded_message_loop) {
      // Wait for initial UI thread setup to complete
      WaitForSingleObject(heventui_, INFINITE);
    }

    Lock();

    // We have exited the transitional state
    in_transition_ = false;

    Unlock();
  }
  
  return intransition ? false : true;
}

void CefContext::Shutdown()
{
  bool shutdown = false, intransition = false;
  BrowserList browserlist;
  
  Lock();

  // We only need to shut down if the UI thread is currently running
  if(idthreadui_) {
    // Only start the shutdown if we're not currently in a transitional state
    intransition = in_transition_;
    if(!intransition) {
      DCHECK(messageloopui_ != NULL);

      // We are now in a transitional state
      in_transition_ = true;

      browserlist = browserlist_;
      browserlist_.empty();
      
      if(webprefs_) {
        delete webprefs_;
        webprefs_ = NULL;
      }

      // Post the quit message to the UI message loop
      messageloopui_->PostTask(FROM_HERE, new MessageLoop::QuitTask());

      shutdown = true;
    }
  }

  Unlock();
  
  if(shutdown) {
    if (browserlist.size() > 0) {
      // Close any remaining browser windows
      BrowserList::const_iterator it = browserlist.begin();
      for (; it != browserlist.end(); ++it)
        PostMessage((*it)->GetWindowHandle(), WM_CLOSE, 0, 0);
      browserlist.empty();
    }

    if (hthreadui_) {
      // Wait for the UI thread to exit
      WaitForSingleObject(hthreadui_, INFINITE);

      // Clean up thread and event handles
      CloseHandle(hthreadui_);
      CloseHandle(heventui_);

      hthreadui_ = NULL;
      heventui_ = NULL;
    } else {
      DoUninitialize();
    }

    Lock();

    // Unregister the window class
    UnregisterClass(CefBrowserImpl::GetWndClass(), hinstance_);

    idthreadui_ = 0;
    messageloopui_ = NULL;

    // We have exited the transitional state
    in_transition_ = false;
    
    Unlock();
  }
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

void CefContext::UIT_ClearCache()
{
  webkit_glue::ClearCache();
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
    PostTask(FROM_HERE, NewRunnableMethod(_Context.get(),
        &CefContext::UIT_ClearCache));
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

void CefContext::SetMessageLoopForUI(MessageLoopForUI* loop)
{
  Lock();
  messageloopui_ = loop;
  Unlock();
}

void CefContext::NotifyEvent()
{
  Lock();
  // Set the event state to signaled so that the waiting thread will be
  // released.
  if(heventui_)
    SetEvent(heventui_);
  Unlock();
}

void PostTask(const tracked_objects::Location& from_here, Task* task)
{
  if(_Context.get()) {
    _Context->Lock();
    MessageLoopForUI *pMsgLoop = _Context->GetMessageLoopForUI();
    if(pMsgLoop)
      pMsgLoop->PostTask(from_here, task);
    _Context->Unlock();
  }
}
