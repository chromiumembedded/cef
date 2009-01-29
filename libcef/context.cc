// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "context.h"
#include "browser_impl.h"
#include "browser_resource_loader_bridge.h"
#include "browser_request_context.h"
#include "../include/cef_nplugin.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/icu_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/resource_util.h"
#include "base/stats_table.h"
#include "base/string_util.h"
#include "net/base/net_module.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"

#include <commctrl.h>


// Global CefContext pointer
CefRefPtr<CefContext> _Context;

static const char* kStatsFilePrefix = "libcef_";
static int kStatsFileThreads = 20;
static int kStatsFileCounters = 200;

bool CefInitialize()
{
  // Return true if the context is already initialized
  if(_Context.get())
    return true;

  // Create the new global context object
  _Context = new CefContext();
  // Initialize the glboal context
  return _Context->Initialize();
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

  info.np_getentrypoints = plugin_info->np_getentrypoints;
  info.np_initialize = plugin_info->np_initialize;
  info.np_shutdown = plugin_info->np_shutdown;

  NPAPI::PluginList::RegisterInternalPlugin(info);
  NPAPI::PluginList::Singleton()->LoadPlugin(FilePath(info.path));

  delete plugin_info;
}

StringPiece GetRawDataResource(HMODULE module, int resource_id) {
  void* data_ptr;
  size_t data_size;
  return base::GetDataResourceFromModule(module, resource_id, &data_ptr,
                                         &data_size) ?
      StringPiece(static_cast<char*>(data_ptr), data_size) : StringPiece();
}

// This is called indirectly by the network layer to access resources.
StringPiece NetResourceProvider(int key) {
  return GetRawDataResource(::GetModuleHandle(NULL), key);
}

DWORD WINAPI ThreadHandlerUI(LPVOID lpParam)
{
  CefContext *pContext = static_cast<CefContext*>(lpParam);

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

  // Instantiate the AtExitManager to avoid asserts and possible memory leaks.
  base::AtExitManager at_exit_manager;

  // Initialize the global CommandLine object.
  CommandLine::Init(0, NULL);

  // Instantiate the message loop for this thread.
  MessageLoopForUI main_message_loop;
  pContext->SetMessageLoopForUI(&main_message_loop);

  // Initializing with a default context, which means no on-disk cookie DB,
  // and no support for directory listings.
  // TODO(cef): Either disable caching or send the cache files to a reasonable
  // temporary directory
  std::wstring cache_path;
  PathService::Get(base::DIR_EXE, &cache_path);
  BrowserResourceLoaderBridge::Init(
    new BrowserRequestContext(cache_path, net::HttpCache::NORMAL, false));

  // Load ICU data tables.
  bool ret = icu_util::Initialize();
  if(!ret) {
    MessageBox(NULL, L"Failed to load the required icudt38 library",
      L"CEF Initialization Error", MB_ICONERROR | MB_OK);
    return 1;
  }

  // Config the network module so it has access to a limited set of resources.
  net::NetModule::SetResourceProvider(NetResourceProvider);

  // Load and initialize the stats table.  Attempt to construct a somewhat
  // unique name to isolate separate instances from each other.
  StatsTable *table = new StatsTable(
      kStatsFilePrefix + Uint64ToString(base::RandUint64()),
      kStatsFileThreads,
      kStatsFileCounters);
  StatsTable::set_current(table);

  // Notify the context that initialization is complete so that the
  // Initialize() function can return.
  pContext->NotifyEvent();

  // Execute the message loop that will run until a quit task is received.
  MessageLoop::current()->Run();

  // Flush any remaining messages.  This ensures that any accumulated
  // Task objects get destroyed before we exit, which avoids noise in
  // purify leak-test results.
  MessageLoop::current()->RunAllPending();
 
  BrowserResourceLoaderBridge::Shutdown();
    
  // Tear down shared StatsTable; prevents unit_tests from leaking it.
  StatsTable::set_current(NULL);
  delete table;

  // Uninitialize COM stuff
  OleUninitialize();

  // Uninitialize common controls
  CoUninitialize();

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
}

CefContext::~CefContext()
{
  // Just in case CefShutdown() isn't called
  Shutdown();
}

bool CefContext::Initialize()
{
  bool initialized = false, intransition = false;
  
  Lock();
  
  // We only need to initialize if the UI thread is not currently running
  if(!hthreadui_) {
    // Only start the initialization if we're not currently in a transitional
    // state
    intransition = in_transition_;
    if(!intransition) {
      // We are now in a transitional state
      in_transition_ = true;

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
      webprefs_->default_encoding = L"ISO-8859-1";
      webprefs_->default_font_size = 16;
      webprefs_->default_fixed_font_size = 13;
      webprefs_->minimum_font_size = 1;
      webprefs_->minimum_logical_font_size = 9;
      webprefs_->javascript_can_open_windows_automatically = true;
      webprefs_->dom_paste_enabled = true;
      webprefs_->developer_extras_enabled = true;
      webprefs_->shrinks_standalone_images_to_fit = false;
      webprefs_->uses_universal_detector = false;
      webprefs_->text_areas_are_resizable = false;
      webprefs_->java_enabled = true;
      webprefs_->allow_scripts_to_close_windows = false;

      // Event that will be used to signal thread setup completion. Start
      // in non-signaled mode so that the event will block.
      heventui_ = CreateEvent(NULL, TRUE, FALSE, NULL);
      DCHECK(heventui_ != NULL);
      
      // Thread that hosts the UI loop
      hthreadui_ = CreateThread(
          NULL, 0, ThreadHandlerUI, this, 0, &idthreadui_);
      DCHECK(hthreadui_ != NULL);
      
      initialized = true;
    }
  }

  Unlock();
  
  if(initialized) {
    // Wait for initial UI thread setup to complete
    WaitForSingleObject(heventui_, INFINITE);

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
  
  Lock();

  // We only need to shut down if the UI thread is currently running
  if(hthreadui_) {
    // Only start the shutdown if we're not currently in a transitional state
    intransition = in_transition_;
    if(!intransition) {
      DCHECK(messageloopui_ != NULL);

      // We are now in a transitional state
      in_transition_ = true;

      if(browserlist_.size() > 0)
        browserlist_.clear();
      
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
    // Wait for the UI thread to exit
    WaitForSingleObject(hthreadui_, INFINITE);

    Lock();

    // Unregister the window class
    UnregisterClass(CefBrowserImpl::GetWndClass(), hinstance_);

    // Clean up thread and event handles
    CloseHandle(hthreadui_);
    CloseHandle(heventui_);

    hthreadui_ = NULL;
    idthreadui_ = 0;
    heventui_ = NULL;
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
    browserlist_.push_back(browser);
 
  Unlock();
  return !found;
}

bool CefContext::RemoveBrowser(CefRefPtr<CefBrowserImpl> browser)
{
  bool deleted = false;

  Lock();

  BrowserList::iterator it = browserlist_.begin();
  for(; it != browserlist_.end(); ++it) {
    if(it->get() == browser.get()) {
      browserlist_.erase(it);
      deleted = true;
      break;
    }
  }

  Unlock();
  
  return deleted;
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
