// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "precompiled_libcef.h"
#include "cef.h"
#include "cef_capi.h"
#include "cef_logging.h"
#include "cef_nplugin.h"
#include "cef_nplugin_capi.h"
#include "cpptoc/browser_cpptoc.h"
#include "cpptoc/request_cpptoc.h"
#include "cpptoc/stream_cpptoc.h"
#include "cpptoc/v8value_cpptoc.h"
#include "ctocpp/handler_ctocpp.h"
#include "ctocpp/v8handler_ctocpp.h"
#include "base/string_util.h"


CEF_EXPORT int cef_initialize(int multi_threaded_message_loop,
                              const wchar_t* cache_path)
{
  std::wstring cachePath;
  if(cache_path)
    cachePath = cache_path;
  return CefInitialize(multi_threaded_message_loop, cachePath);
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
  DCHECK(CefHandlerCToCpp::DebugObjCt == 0);
  DCHECK(CefV8HandlerCToCpp::DebugObjCt == 0);
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

CEF_EXPORT int cef_create_browser(cef_window_info_t* windowInfo, int popup,
                                  cef_handler_t* handler, const wchar_t* url)
{
  DCHECK(windowInfo);

  CefRefPtr<CefHandler> handlerPtr;
  std::wstring urlStr;
  CefWindowInfo wi = *windowInfo;
  
  if(handler)
    handlerPtr = CefHandlerCToCpp::Wrap(handler);
  if(url)
    urlStr = url;
  
  return CefBrowser::CreateBrowser(wi, popup, handlerPtr, urlStr);
}

CEF_EXPORT cef_browser_t* cef_create_browser_sync(cef_window_info_t* windowInfo,
                                                  int popup,
                                                  cef_handler_t* handler,
                                                  const wchar_t* url)
{
  DCHECK(windowInfo);

  CefRefPtr<CefHandler> handlerPtr;
  std::wstring urlStr;
  CefWindowInfo wi = *windowInfo;
  
  if(handler)
    handlerPtr = CefHandlerCToCpp::Wrap(handler);
  if(url)
    urlStr = url;
  
  CefRefPtr<CefBrowser> browserPtr(
      CefBrowser::CreateBrowserSync(wi, popup, handlerPtr, urlStr));
  if(browserPtr.get())
    return CefBrowserCppToC::Wrap(browserPtr);
  return NULL;
}

CEF_EXPORT cef_request_t* cef_create_request()
{
  CefRefPtr<CefRequest> impl = CefRequest::CreateRequest();
  if(impl.get())
    return CefRequestCppToC::Wrap(impl);
  return NULL;
}

CEF_EXPORT cef_post_data_t* cef_create_post_data()
{
  CefRefPtr<CefPostData> impl = CefPostData::CreatePostData();
  if(impl.get())
    return CefPostDataCppToC::Wrap(impl);
  return NULL;
}

CEF_EXPORT cef_post_data_element_t* cef_create_post_data_element()
{
  CefRefPtr<CefPostDataElement> impl =
      CefPostDataElement::CreatePostDataElement();
  if(impl.get())
    return CefPostDataElementCppToC::Wrap(impl);
  return NULL;
}

CEF_EXPORT cef_stream_reader_t* cef_create_stream_reader_for_file(
    const wchar_t* fileName)
{
  std::wstring filenamestr;
  if(fileName)
    filenamestr = fileName;
  CefRefPtr<CefStreamReader> impl = CefStreamReader::CreateForFile(filenamestr);
  if(impl.get())
    return CefStreamReaderCppToC::Wrap(impl);
  return NULL;
}

CEF_EXPORT cef_stream_reader_t* cef_create_stream_reader_for_data(void *data,
    size_t size)
{
  CefRefPtr<CefStreamReader> impl = CefStreamReader::CreateForData(data, size);
  if(impl.get())
    return CefStreamReaderCppToC::Wrap(impl);
  return NULL;
}

CEF_EXPORT cef_v8value_t* cef_create_v8value_undefined()
{
  CefRefPtr<CefV8Value> impl = CefV8Value::CreateUndefined();
  if(impl.get())
    return CefV8ValueCppToC::Wrap(impl);
  return NULL;
}

CEF_EXPORT cef_v8value_t* cef_create_v8value_null()
{
  CefRefPtr<CefV8Value> impl = CefV8Value::CreateNull();
  if(impl.get())
    return CefV8ValueCppToC::Wrap(impl);
  return NULL;
}

CEF_EXPORT cef_v8value_t* cef_create_v8value_bool(int value)
{
  CefRefPtr<CefV8Value> impl = CefV8Value::CreateBool(value?true:false);
  if(impl.get())
    return CefV8ValueCppToC::Wrap(impl);
  return NULL;
}

CEF_EXPORT cef_v8value_t* cef_create_v8value_int(int value)
{
  CefRefPtr<CefV8Value> impl = CefV8Value::CreateInt(value);
  if(impl.get())
    return CefV8ValueCppToC::Wrap(impl);
  return NULL;
}

CEF_EXPORT cef_v8value_t* cef_create_v8value_double(double value)
{
  CefRefPtr<CefV8Value> impl = CefV8Value::CreateDouble(value);
  if(impl.get())
    return CefV8ValueCppToC::Wrap(impl);
  return NULL;
}

CEF_EXPORT cef_v8value_t* cef_create_v8value_string(const wchar_t* value)
{
  std::wstring valueStr;
  if(value)
    valueStr = value;

  CefRefPtr<CefV8Value> impl = CefV8Value::CreateString(valueStr);
  if(impl.get())
    return CefV8ValueCppToC::Wrap(impl);
  return NULL;
}

CEF_EXPORT cef_v8value_t* cef_create_v8value_object(cef_base_t* user_data)
{
  CefRefPtr<CefBase> basePtr;
  if(user_data)
    basePtr = CefBaseCToCpp::Wrap(user_data);

  CefRefPtr<CefV8Value> impl = CefV8Value::CreateObject(basePtr);
  if(impl.get())
    return CefV8ValueCppToC::Wrap(impl);
  return NULL;
}

CEF_EXPORT cef_v8value_t* cef_create_v8value_array()
{
  CefRefPtr<CefV8Value> impl = CefV8Value::CreateArray();
  if(impl.get())
    return CefV8ValueCppToC::Wrap(impl);
  return NULL;
}

CEF_EXPORT cef_v8value_t* cef_create_v8value_function(const wchar_t* name,
                                                      cef_v8handler_t* handler)
{
  std::wstring nameStr;
  if(name)
    nameStr = name;
  CefRefPtr<CefV8Handler> handlerPtr;
  if(handler)
    handlerPtr = CefV8HandlerCToCpp::Wrap(handler);

  CefRefPtr<CefV8Value> impl = CefV8Value::CreateFunction(nameStr, handlerPtr);
  if(impl.get())
    return CefV8ValueCppToC::Wrap(impl);
  return NULL;
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
  SplitString(plugin_info->mime_types, '|', &mime_types);
  SplitString(plugin_info->file_extensions, '|', &file_extensions);
  SplitString(plugin_info->type_descriptions, '|', &descriptions);

  for (size_t i = 0; i < mime_types.size(); ++i) {
    CefPluginMimeType mimeType;
    
    mimeType.mime_type = mime_types[i];
    
    if (file_extensions.size() > i)
      SplitString(file_extensions[i], ',', &mimeType.file_extensions);

    if (descriptions.size() > i)
      mimeType.description = descriptions[i];

    pluginInfo.mime_types.push_back(mimeType);
  }
  
  pluginInfo.np_getentrypoints = plugin_info->np_getentrypoints;
  pluginInfo.np_initialize = plugin_info->np_initialize;
  pluginInfo.np_shutdown = plugin_info->np_shutdown;
  
  return CefRegisterPlugin(pluginInfo);
}
