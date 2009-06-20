// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cef.h"
#include "cef_capi.h"
#include "cef_nplugin.h"
#include "cef_nplugin_capi.h"
#include "../cpptoc/handler_cpptoc.h"
#include "../cpptoc/v8handler_cpptoc.h"
#include "../ctocpp/browser_ctocpp.h"
#include "../ctocpp/post_data_ctocpp.h"
#include "../ctocpp/post_data_element_ctocpp.h"
#include "../ctocpp/request_ctocpp.h"
#include "../ctocpp/stream_reader_ctocpp.h"
#include "../ctocpp/stream_writer_ctocpp.h"
#include "../ctocpp/v8value_ctocpp.h"


bool CefInitialize(bool multi_threaded_message_loop,
                   const std::wstring& cache_path)
{
  return (bool)cef_initialize(multi_threaded_message_loop, cache_path.c_str());
}

void CefShutdown()
{
  cef_shutdown();

#ifdef _DEBUG
  // Check that all wrapper objects have been destroyed
  DCHECK(CefHandlerCppToC::DebugObjCt == 0);
  DCHECK(CefV8HandlerCppToC::DebugObjCt == 0);
  DCHECK(CefBrowserCToCpp::DebugObjCt == 0);
  DCHECK(CefRequestCToCpp::DebugObjCt == 0);
  DCHECK(CefPostDataCToCpp::DebugObjCt == 0);
  DCHECK(CefPostDataElementCToCpp::DebugObjCt == 0);
  DCHECK(CefStreamReaderCToCpp::DebugObjCt == 0);
  DCHECK(CefStreamWriterCToCpp::DebugObjCt == 0);
  DCHECK(CefV8ValueCToCpp::DebugObjCt == 0);
#endif // _DEBUG
}

void CefDoMessageLoopWork()
{
  cef_do_message_loop_work();
}

bool CefRegisterExtension(const std::wstring& extension_name,
                          const std::wstring& javascript_code,
                          CefRefPtr<CefV8Handler> handler)
{
  return cef_register_extension(extension_name.c_str(), javascript_code.c_str(),
      CefV8HandlerCppToC::Wrap(handler));
}

bool CefRegisterPlugin(const struct CefPluginInfo& plugin_info)
{
  cef_plugin_info_t pluginInfo;

  pluginInfo.unique_name = plugin_info.unique_name.c_str();
  pluginInfo.display_name = plugin_info.display_name.c_str();
  pluginInfo.version =plugin_info.version.c_str();
  pluginInfo.description = plugin_info.description.c_str();
  
  std::wstring mimeTypes, fileExtensions, typeDescriptions;

  for(size_t i = 0; i < plugin_info.mime_types.size(); ++i) {
    if(i > 0) {
      mimeTypes += L"|";
      fileExtensions += L"|";
      typeDescriptions += L"|";
    }

    mimeTypes += plugin_info.mime_types[i].mime_type;
    typeDescriptions += plugin_info.mime_types[i].description;
    
    for(size_t j = 0;
        j < plugin_info.mime_types[i].file_extensions.size(); ++j) {
      if(j > 0) {
        fileExtensions += L",";
      }
      fileExtensions += plugin_info.mime_types[i].file_extensions[j];
    }
  }

  pluginInfo.mime_types = mimeTypes.c_str();
  pluginInfo.file_extensions = fileExtensions.c_str();
  pluginInfo.type_descriptions = typeDescriptions.c_str();

  pluginInfo.np_getentrypoints = plugin_info.np_getentrypoints;
  pluginInfo.np_initialize = plugin_info.np_initialize;
  pluginInfo.np_shutdown = plugin_info.np_shutdown;

  return (cef_register_plugin(&pluginInfo) ? true : false);
}
