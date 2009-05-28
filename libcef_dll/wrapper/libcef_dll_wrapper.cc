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
#include "../ctocpp/request_ctocpp.h"
#include "../ctocpp/stream_ctocpp.h"
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

bool CefBrowser::CreateBrowser(CefWindowInfo& windowInfo, bool popup,
                               CefRefPtr<CefHandler> handler,
                               const std::wstring& url)
{
  return cef_create_browser(&windowInfo, popup, CefHandlerCppToC::Wrap(handler),
      url.c_str());
}

CefRefPtr<CefBrowser> CefBrowser::CreateBrowserSync(CefWindowInfo& windowInfo,
                                                    bool popup,
                                                    CefRefPtr<CefHandler> handler,
                                                    const std::wstring& url)
{
  cef_browser_t* impl = cef_create_browser_sync(&windowInfo, popup,
      CefHandlerCppToC::Wrap(handler), url.c_str());
  if(impl)
    return CefBrowserCToCpp::Wrap(impl);
  return NULL;
}

CefRefPtr<CefRequest> CreateRequest()
{
  cef_request_t* impl = cef_create_request();
  if(impl)
    return CefRequestCToCpp::Wrap(impl);
  return NULL;
}

CefRefPtr<CefPostData> CreatePostData()
{
  cef_post_data_t* impl = cef_create_post_data();
  if(impl)
    return CefPostDataCToCpp::Wrap(impl);
  return NULL;
}

CefRefPtr<CefPostDataElement> CreatePostDataElement()
{
  cef_post_data_element_t* impl = cef_create_post_data_element();
  if(impl)
    return CefPostDataElementCToCpp::Wrap(impl);
  return NULL;
}

CefRefPtr<CefStreamReader> CefStreamReader::CreateForFile(const std::wstring& fileName)
{
  cef_stream_reader_t* impl =
      cef_create_stream_reader_for_file(fileName.c_str());
  if(impl)
    return CefStreamReaderCToCpp::Wrap(impl);
  return NULL;
}

CefRefPtr<CefStreamReader> CefStreamReader::CreateForData(void *data, size_t size)
{
  cef_stream_reader_t* impl = cef_create_stream_reader_for_data(data, size);
  if(impl)
    return CefStreamReaderCToCpp::Wrap(impl);
  return NULL;
}

CefRefPtr<CefV8Value> CefV8Value::CreateUndefined()
{
  cef_v8value_t* impl = cef_create_v8value_undefined();
  if(impl)
    return CefV8ValueCToCpp::Wrap(impl);
  return NULL;
}

CefRefPtr<CefV8Value> CefV8Value::CreateNull()
{
  cef_v8value_t* impl = cef_create_v8value_null();
  if(impl)
    return CefV8ValueCToCpp::Wrap(impl);
  return NULL;
}

CefRefPtr<CefV8Value> CefV8Value::CreateBool(bool value)
{
  cef_v8value_t* impl = cef_create_v8value_bool(value);
  if(impl)
    return CefV8ValueCToCpp::Wrap(impl);
  return NULL;
}

CefRefPtr<CefV8Value> CefV8Value::CreateInt(int value)
{
  cef_v8value_t* impl = cef_create_v8value_int(value);
  if(impl)
    return CefV8ValueCToCpp::Wrap(impl);
  return NULL;
}

CefRefPtr<CefV8Value> CefV8Value::CreateDouble(double value)
{
  cef_v8value_t* impl = cef_create_v8value_double(value);
  if(impl)
    return CefV8ValueCToCpp::Wrap(impl);
  return NULL;
}

CefRefPtr<CefV8Value> CefV8Value::CreateString(const std::wstring& value)
{
  cef_v8value_t* impl = cef_create_v8value_string(value.c_str());
  if(impl)
    return CefV8ValueCToCpp::Wrap(impl);
  return NULL;
}

CefRefPtr<CefV8Value> CefV8Value::CreateObject(CefRefPtr<CefBase> user_data)
{
  cef_base_t* baseStruct = NULL;
  if(user_data)
    baseStruct = CefBaseCppToC::Wrap(user_data);

  cef_v8value_t* impl = cef_create_v8value_object(baseStruct);
  if(impl)
    return CefV8ValueCToCpp::Wrap(impl);
  return NULL;
}

CefRefPtr<CefV8Value> CefV8Value::CreateArray()
{
  cef_v8value_t* impl = cef_create_v8value_array();
  if(impl)
    return CefV8ValueCToCpp::Wrap(impl);
  return NULL;
}

CefRefPtr<CefV8Value> CefV8Value::CreateFunction(const std::wstring& name,
                                                CefRefPtr<CefV8Handler> handler)
{
  cef_v8handler_t* handlerStruct = NULL;
  if(handler.get())
    handlerStruct = CefV8HandlerCppToC::Wrap(handler);

  cef_v8value_t* impl = cef_create_v8value_function(name.c_str(),
      handlerStruct);
  if(impl)
    return CefV8ValueCToCpp::Wrap(impl);
  return NULL;
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
