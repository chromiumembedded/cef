// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cef.h"
#include "cef_capi.h"
#include "cef_nplugin.h"
#include "cef_nplugin_capi.h"
#include "../cpptoc/handler_cpptoc.h"
#include "../cpptoc/jshandler_cpptoc.h"
#include "../ctocpp/browser_ctocpp.h"
#include "../ctocpp/request_ctocpp.h"
#include "../ctocpp/variant_ctocpp.h"
#include "../ctocpp/stream_ctocpp.h"


bool CefInitialize(bool multi_threaded_message_loop,
                   const std::wstring& cache_path)
{
  return (bool)cef_initialize(multi_threaded_message_loop, cache_path.c_str());
}

void CefShutdown()
{
  cef_shutdown();

  // Check that all wrapper objects have been destroyed
  DCHECK(CefHandlerCppToC::DebugObjCt == 0);
  DCHECK(CefJSHandlerCppToC::DebugObjCt == 0);
  DCHECK(CefBrowserCToCpp::DebugObjCt == 0);
  DCHECK(CefRequestCToCpp::DebugObjCt == 0);
  DCHECK(CefPostDataCToCpp::DebugObjCt == 0);
  DCHECK(CefPostDataElementCToCpp::DebugObjCt == 0);
  DCHECK(CefStreamReaderCToCpp::DebugObjCt == 0);
  DCHECK(CefStreamWriterCToCpp::DebugObjCt == 0);
  DCHECK(CefVariantCToCpp::DebugObjCt == 0);
}

void CefDoMessageLoopWork()
{
  cef_do_message_loop_work();
}

bool CefBrowser::CreateBrowser(CefWindowInfo& windowInfo, bool popup,
                               CefRefPtr<CefHandler> handler,
                               const std::wstring& url)
{
  CefHandlerCppToC* hp = new CefHandlerCppToC(handler);
  hp->AddRef();
  return cef_create_browser(&windowInfo, popup, hp->GetStruct(), url.c_str());
}

CefRefPtr<CefBrowser> CefBrowser::CreateBrowserSync(CefWindowInfo& windowInfo,
                                                    bool popup,
                                                    CefRefPtr<CefHandler> handler,
                                                    const std::wstring& url)
{
  CefHandlerCppToC* hp = new CefHandlerCppToC(handler);
  hp->AddRef();

  cef_browser_t* browserStruct = cef_create_browser_sync(&windowInfo, popup,
      hp->GetStruct(), url.c_str());
  if(!browserStruct)
    return NULL;

  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browserStruct);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  return browserPtr;
}

CefRefPtr<CefRequest> CreateRequest()
{
  cef_request_t* impl = cef_create_request();
  if(!impl)
    return NULL;
  CefRequestCToCpp* ptr = new CefRequestCToCpp(impl);
  CefRefPtr<CefRequest> rp(ptr);
  ptr->UnderlyingRelease();
  return rp;
}

CefRefPtr<CefPostData> CreatePostData()
{
  cef_post_data_t* impl = cef_create_post_data();
  if(!impl)
    return NULL;
  CefPostDataCToCpp* ptr = new CefPostDataCToCpp(impl);
  CefRefPtr<CefPostData> rp(ptr);
  ptr->UnderlyingRelease();
  return rp;
}

CefRefPtr<CefPostDataElement> CreatePostDataElement()
{
  cef_post_data_element_t* impl = cef_create_post_data_element();
  if(!impl)
    return NULL;
  CefPostDataElementCToCpp* ptr = new CefPostDataElementCToCpp(impl);
  CefRefPtr<CefPostDataElement> rp(ptr);
  ptr->UnderlyingRelease();
  return rp;
}

CefRefPtr<CefStreamReader> CefStreamReader::CreateForFile(const std::wstring& fileName)
{
  cef_stream_reader_t* impl =
      cef_create_stream_reader_for_file(fileName.c_str());
  if(!impl)
    return NULL;
  CefStreamReaderCToCpp* ptr = new CefStreamReaderCToCpp(impl);
  CefRefPtr<CefStreamReader> rp(ptr);
  ptr->UnderlyingRelease();
  return rp;
}

CefRefPtr<CefStreamReader> CefStreamReader::CreateForData(void *data, size_t size)
{
  cef_stream_reader_t* impl = cef_create_stream_reader_for_data(data, size);
  if(!impl)
    return NULL;
  CefStreamReaderCToCpp* ptr = new CefStreamReaderCToCpp(impl);
  CefRefPtr<CefStreamReader> rp(ptr);
  ptr->UnderlyingRelease();
  return rp;
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
