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


CEF_EXPORT int cef_initialize(int multi_threaded_message_loop,
                              const wchar_t* cache_path)
{
  std::wstring cachePath;
  if(cache_path)
    cachePath = cache_path;
  return CefInitialize(multi_threaded_message_loop?true:false, cachePath);
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
  DCHECK(CefHandlerCToCpp::DebugObjCt == 0);
  DCHECK(CefReadHandlerCToCpp::DebugObjCt == 0);
  DCHECK(CefSchemeHandlerCToCpp::DebugObjCt == 0);
  DCHECK(CefSchemeHandlerFactoryCToCpp::DebugObjCt == 0);
  DCHECK(CefV8HandlerCToCpp::DebugObjCt == 0);
  DCHECK(CefWriteHandlerCToCpp::DebugObjCt == 0);
#endif // _DEBUG
}

CEF_EXPORT void cef_do_message_loop_work()
{
  CefDoMessageLoopWork();
}

CEF_EXPORT int cef_register_extension(const wchar_t* extension_name,
                                      const wchar_t* javascript_code,
                                      struct _cef_v8handler_t* handler)
{
  DCHECK(extension_name);
  DCHECK(javascript_code);

  CefRefPtr<CefV8Handler> handlerPtr;
  std::wstring nameStr, codeStr;
  
  if(handler)
    handlerPtr = CefV8HandlerCToCpp::Wrap(handler);
  if(extension_name)
    nameStr = extension_name;
  if(javascript_code)
    codeStr = javascript_code;
  
  return CefRegisterExtension(nameStr, codeStr, handlerPtr);
}

CEF_EXPORT int cef_register_plugin(const cef_plugin_info_t* plugin_info)
{
  CefPluginInfo pluginInfo;

  pluginInfo.unique_name = plugin_info->unique_name;
  pluginInfo.display_name = plugin_info->display_name;
  pluginInfo.version = plugin_info->version;
  pluginInfo.description = plugin_info->description;

  std::vector<std::wstring> mime_types, file_extensions;
  std::vector<std::wstring> descriptions;
  base::SplitString(plugin_info->mime_types, '|', &mime_types);
  base::SplitString(plugin_info->file_extensions, '|', &file_extensions);
  base::SplitString(plugin_info->type_descriptions, '|', &descriptions);

  for (size_t i = 0; i < mime_types.size(); ++i) {
    CefPluginMimeType mimeType;
    
    mimeType.mime_type = mime_types[i];
    
    if (file_extensions.size() > i)
      base::SplitString(file_extensions[i], ',', &mimeType.file_extensions);

    if (descriptions.size() > i)
      mimeType.description = descriptions[i];

    pluginInfo.mime_types.push_back(mimeType);
  }
  
  pluginInfo.np_getentrypoints = plugin_info->np_getentrypoints;
  pluginInfo.np_initialize = plugin_info->np_initialize;
  pluginInfo.np_shutdown = plugin_info->np_shutdown;
  
  return CefRegisterPlugin(pluginInfo);
}

CEF_EXPORT int cef_register_scheme(const wchar_t* scheme_name,
    const wchar_t* host_name, struct _cef_scheme_handler_factory_t* factory)
{
  DCHECK(scheme_name);
  DCHECK(factory);
  if(!scheme_name || !factory)
    return 0;

  std::wstring nameStr, codeStr;

  if(scheme_name)
    nameStr = scheme_name;
  if(host_name)
    codeStr = host_name;

  return CefRegisterScheme(nameStr, codeStr,
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
