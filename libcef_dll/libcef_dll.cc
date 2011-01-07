// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "include/cef_capi.h"
#include "include/cef_nplugin.h"
#include "include/cef_nplugin_capi.h"
#include "cef_logging.h"
#include "cpptoc/browser_cpptoc.h"
#include "cpptoc/post_data_cpptoc.h"
#include "cpptoc/post_data_element_cpptoc.h"
#include "cpptoc/request_cpptoc.h"
#include "cpptoc/stream_reader_cpptoc.h"
#include "cpptoc/stream_writer_cpptoc.h"
#include "cpptoc/v8value_cpptoc.h"
#include "cpptoc/xml_reader_cpptoc.h"
#include "cpptoc/zip_reader_cpptoc.h"
#include "ctocpp/download_handler_ctocpp.h"
#include "ctocpp/handler_ctocpp.h"
#include "ctocpp/read_handler_ctocpp.h"
#include "ctocpp/scheme_handler_ctocpp.h"
#include "ctocpp/scheme_handler_factory_ctocpp.h"
#include "ctocpp/task_ctocpp.h"
#include "ctocpp/v8handler_ctocpp.h"
#include "ctocpp/write_handler_ctocpp.h"
#include "base/string_split.h"


CEF_EXPORT int cef_initialize(const struct _cef_settings_t* settings,
    const struct _cef_browser_settings_t* browser_defaults)
{
  CefSettings settingsObj;
  CefBrowserSettings browserDefaultsObj;

  // Take ownership of the pointers instead of copying.
  if (settings)
    settingsObj.Attach(*settings);
  if (browser_defaults)
    browserDefaultsObj.Attach(*browser_defaults);

  int ret = CefInitialize(settingsObj, browserDefaultsObj);

  // Don't free the pointers.
  settingsObj.Detach();
  browserDefaultsObj.Detach();

  return ret;
}

CEF_EXPORT void cef_shutdown()
{
  CefShutdown();

#ifdef _DEBUG
  // Check that all wrapper objects have been destroyed
  DCHECK(CefBrowserCppToC::DebugObjCt == 0);
  DCHECK(CefRequestCppToC::DebugObjCt == 0);
  DCHECK(CefPostDataCppToC::DebugObjCt == 0);
  DCHECK(CefPostDataElementCppToC::DebugObjCt == 0);
  DCHECK(CefStreamReaderCppToC::DebugObjCt == 0);
  DCHECK(CefStreamWriterCppToC::DebugObjCt == 0);
  DCHECK(CefV8ValueCppToC::DebugObjCt == 0);
  DCHECK(CefXmlReaderCppToC::DebugObjCt == 0);
  DCHECK(CefZipReaderCppToC::DebugObjCt == 0);
  DCHECK(CefDownloadHandlerCToCpp::DebugObjCt == 0);
  DCHECK(CefReadHandlerCToCpp::DebugObjCt == 0);
  DCHECK(CefSchemeHandlerCToCpp::DebugObjCt == 0);
  DCHECK(CefSchemeHandlerFactoryCToCpp::DebugObjCt == 0);
  DCHECK(CefV8HandlerCToCpp::DebugObjCt == 0);
  DCHECK(CefWriteHandlerCToCpp::DebugObjCt == 0);

  // TODO: This breakpoint may be hit if content is still loading when CEF
  // exits. Re-enable the breakpoint if/when CEF stops content loading before
  // exit.
  //DCHECK(CefHandlerCToCpp::DebugObjCt == 0);
#endif // _DEBUG
}

CEF_EXPORT void cef_do_message_loop_work()
{
  CefDoMessageLoopWork();
}

CEF_EXPORT int cef_register_extension(const cef_string_t* extension_name,
                                      const cef_string_t* javascript_code,
                                      struct _cef_v8handler_t* handler)
{
  DCHECK(extension_name);
  DCHECK(javascript_code);

  CefRefPtr<CefV8Handler> handlerPtr;
  std::wstring nameStr, codeStr;
  
  if(handler)
    handlerPtr = CefV8HandlerCToCpp::Wrap(handler);
  
  return CefRegisterExtension(CefString(extension_name),
      CefString(javascript_code), handlerPtr);
}

CEF_EXPORT int cef_register_plugin(const cef_plugin_info_t* plugin_info)
{
  CefPluginInfo pluginInfo;

  pluginInfo.unique_name.FromString(plugin_info->unique_name.str,
      plugin_info->unique_name.length, true);
  pluginInfo.display_name.FromString(plugin_info->display_name.str,
      plugin_info->display_name.length, true);
  pluginInfo.version.FromString(plugin_info->version.str,
      plugin_info->version.length, true);
  pluginInfo.description.FromString(plugin_info->description.str,
      plugin_info->description.length, true);

  typedef std::vector<std::string> VectorType;
  VectorType mime_types, file_extensions, descriptions, file_extensions_parts;
  base::SplitString(CefString(&plugin_info->mime_types), '|',
      &mime_types);
  base::SplitString(CefString(&plugin_info->file_extensions), '|',
      &file_extensions);
  base::SplitString(CefString(&plugin_info->type_descriptions), '|',
      &descriptions);

  for (size_t i = 0; i < mime_types.size(); ++i) {
    CefPluginMimeType mimeType;
    
    mimeType.mime_type = mime_types[i];
    
    if (file_extensions.size() > i) {
      base::SplitString(file_extensions[i], ',', &file_extensions_parts);
      VectorType::const_iterator it = file_extensions_parts.begin();
      for(; it != file_extensions_parts.end(); ++it)
        mimeType.file_extensions.push_back(*(it));
      file_extensions_parts.clear();
    }

    if (descriptions.size() > i)
      mimeType.description = descriptions[i];

    pluginInfo.mime_types.push_back(mimeType);
  }
  
#if !defined(OS_POSIX) || defined(OS_MACOSX)
  pluginInfo.np_getentrypoints = plugin_info->np_getentrypoints;
#endif
  pluginInfo.np_initialize = plugin_info->np_initialize;
  pluginInfo.np_shutdown = plugin_info->np_shutdown;
  
  return CefRegisterPlugin(pluginInfo);
}

CEF_EXPORT int cef_register_scheme(const cef_string_t* scheme_name,
    const cef_string_t* host_name,
    struct _cef_scheme_handler_factory_t* factory)
{
  DCHECK(scheme_name);
  DCHECK(factory);
  if(!scheme_name || !factory)
    return 0;

  return CefRegisterScheme(CefString(scheme_name), CefString(host_name),
      CefSchemeHandlerFactoryCToCpp::Wrap(factory));
}

CEF_EXPORT int cef_currently_on(cef_thread_id_t threadId)
{
  return CefCurrentlyOn(threadId);
}

CEF_EXPORT int cef_post_task(cef_thread_id_t threadId,
    struct _cef_task_t* task)
{
  DCHECK(task);
  if(!task)
    return 0;

  return CefPostTask(threadId, CefTaskCToCpp::Wrap(task));
}

CEF_EXPORT int cef_post_delayed_task(cef_thread_id_t threadId,
    struct _cef_task_t* task, long delay_ms)
{
  DCHECK(task);
  if(!task)
    return 0;

  return CefPostDelayedTask(threadId, CefTaskCToCpp::Wrap(task), delay_ms);
}
