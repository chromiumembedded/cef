// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "include/cef_capi.h"
#include "include/cef_nplugin.h"
#include "include/cef_nplugin_capi.h"
#include "libcef_dll/cpptoc/download_handler_cpptoc.h"
#include "libcef_dll/cpptoc/handler_cpptoc.h"
#include "libcef_dll/cpptoc/read_handler_cpptoc.h"
#include "libcef_dll/cpptoc/scheme_handler_cpptoc.h"
#include "libcef_dll/cpptoc/scheme_handler_factory_cpptoc.h"
#include "libcef_dll/cpptoc/task_cpptoc.h"
#include "libcef_dll/cpptoc/v8handler_cpptoc.h"
#include "libcef_dll/cpptoc/write_handler_cpptoc.h"
#include "libcef_dll/ctocpp/browser_ctocpp.h"
#include "libcef_dll/ctocpp/post_data_ctocpp.h"
#include "libcef_dll/ctocpp/post_data_element_ctocpp.h"
#include "libcef_dll/ctocpp/request_ctocpp.h"
#include "libcef_dll/ctocpp/stream_reader_ctocpp.h"
#include "libcef_dll/ctocpp/stream_writer_ctocpp.h"
#include "libcef_dll/ctocpp/v8value_ctocpp.h"
#include "libcef_dll/ctocpp/xml_reader_ctocpp.h"
#include "libcef_dll/ctocpp/zip_reader_ctocpp.h"


bool CefInitialize(const CefSettings& settings,
                   const CefBrowserSettings& browser_defaults)
{
  return cef_initialize(&settings, &browser_defaults)?true:false;
}

void CefShutdown()
{
  cef_shutdown();

#ifdef _DEBUG
  // Check that all wrapper objects have been destroyed
  DCHECK(CefDownloadHandlerCppToC::DebugObjCt == 0);
  DCHECK(CefReadHandlerCppToC::DebugObjCt == 0);
  DCHECK(CefSchemeHandlerCppToC::DebugObjCt == 0);
  DCHECK(CefSchemeHandlerFactoryCppToC::DebugObjCt == 0);
  DCHECK(CefV8HandlerCppToC::DebugObjCt == 0);
  DCHECK(CefWriteHandlerCppToC::DebugObjCt == 0);
  DCHECK(CefBrowserCToCpp::DebugObjCt == 0);
  DCHECK(CefRequestCToCpp::DebugObjCt == 0);
  DCHECK(CefPostDataCToCpp::DebugObjCt == 0);
  DCHECK(CefPostDataElementCToCpp::DebugObjCt == 0);
  DCHECK(CefStreamReaderCToCpp::DebugObjCt == 0);
  DCHECK(CefStreamWriterCToCpp::DebugObjCt == 0);
  DCHECK(CefV8ValueCToCpp::DebugObjCt == 0);
  DCHECK(CefXmlReaderCToCpp::DebugObjCt == 0);
  DCHECK(CefZipReaderCToCpp::DebugObjCt == 0);

  // TODO: This breakpoint may be hit if content is still loading when CEF
  // exits. Re-enable the breakpoint if/when CEF stops content loading before
  // exit.
  //DCHECK(CefHandlerCppToC::DebugObjCt == 0);
#endif // _DEBUG
}

void CefDoMessageLoopWork()
{
  cef_do_message_loop_work();
}

bool CefRegisterExtension(const CefString& extension_name,
                          const CefString& javascript_code,
                          CefRefPtr<CefV8Handler> handler)
{
  return cef_register_extension(extension_name.GetStruct(),
      javascript_code.GetStruct(), CefV8HandlerCppToC::Wrap(handler))?
      true:false;
}

bool CefRegisterPlugin(const struct CefPluginInfo& plugin_info)
{
  cef_plugin_info_t pluginInfo;
  memset(&pluginInfo, 0, sizeof(pluginInfo));

  cef_string_set(plugin_info.unique_name.c_str(),
      plugin_info.unique_name.length(),
      &pluginInfo.unique_name, false);
  cef_string_set(plugin_info.display_name.c_str(),
      plugin_info.display_name.length(),
      &pluginInfo.display_name, false);
  cef_string_set(plugin_info.version.c_str(),
      plugin_info.version.length(),
      &pluginInfo.version, false);
  cef_string_set(plugin_info.description.c_str(),
      plugin_info.description.length(),
      &pluginInfo.description, false);
  
  std::string mimeTypes, fileExtensions, typeDescriptions;

  for(size_t i = 0; i < plugin_info.mime_types.size(); ++i) {
    if(i > 0) {
      mimeTypes += "|";
      fileExtensions += "|";
      typeDescriptions += "|";
    }

    mimeTypes += plugin_info.mime_types[i].mime_type;
    typeDescriptions += plugin_info.mime_types[i].description;
    
    for(size_t j = 0;
        j < plugin_info.mime_types[i].file_extensions.size(); ++j) {
      if(j > 0) {
        fileExtensions += ",";
      }
      fileExtensions += plugin_info.mime_types[i].file_extensions[j];
    }
  }

  cef_string_from_utf8(mimeTypes.c_str(), mimeTypes.length(),
      &pluginInfo.mime_types);
  cef_string_from_utf8(fileExtensions.c_str(), fileExtensions.length(),
      &pluginInfo.file_extensions);
  cef_string_from_utf8(typeDescriptions.c_str(), typeDescriptions.length(),
      &pluginInfo.type_descriptions);

#if !defined(OS_POSIX) || defined(OS_MACOSX)
  pluginInfo.np_getentrypoints = plugin_info.np_getentrypoints;
#endif
  pluginInfo.np_initialize = plugin_info.np_initialize;
  pluginInfo.np_shutdown = plugin_info.np_shutdown;

  bool ret = cef_register_plugin(&pluginInfo) ? true : false;

  cef_string_clear(&pluginInfo.mime_types);
  cef_string_clear(&pluginInfo.file_extensions);
  cef_string_clear(&pluginInfo.type_descriptions);

  return ret;
}

bool CefRegisterScheme(const CefString& scheme_name,
                       const CefString& host_name,
                       CefRefPtr<CefSchemeHandlerFactory> factory)
{
  return cef_register_scheme(scheme_name.GetStruct(), host_name.GetStruct(),
      CefSchemeHandlerFactoryCppToC::Wrap(factory))?true:false;
}

bool CefCurrentlyOn(CefThreadId threadId)
{
  return cef_currently_on(threadId)?true:false;
}

bool CefPostTask(CefThreadId threadId, CefRefPtr<CefTask> task)
{
  return cef_post_task(threadId, CefTaskCppToC::Wrap(task))?true:false;
}

bool CefPostDelayedTask(CefThreadId threadId, CefRefPtr<CefTask> task,
                        long delay_ms)
{
  return cef_post_delayed_task(threadId, CefTaskCppToC::Wrap(task), delay_ms)?
      true:false;
}

bool CefParseURL(const CefString& url,
                 CefURLParts& parts)
{
  return cef_parse_url(url.GetStruct(), &parts) ? true : false;
}

bool CefCreateURL(const CefURLParts& parts,
                  CefString& url)
{
  return cef_create_url(&parts, url.GetWritableStruct()) ? true : false;
}
