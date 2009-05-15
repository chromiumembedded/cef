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
#include "cpptoc/variant_cpptoc.h"
#include "cpptoc/stream_cpptoc.h"
#include "ctocpp/handler_ctocpp.h"
#include "ctocpp/jshandler_ctocpp.h"
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
  DCHECK(CefVariantCppToC::DebugObjCt == 0);
  DCHECK(CefHandlerCToCpp::DebugObjCt == 0);
  DCHECK(CefJSHandlerCToCpp::DebugObjCt == 0);
#endif // _DEBUG
}

CEF_EXPORT void cef_do_message_loop_work()
{
  CefDoMessageLoopWork();
}

CEF_EXPORT int cef_create_browser(cef_window_info_t* windowInfo, int popup,
                                  cef_handler_t* handler, const wchar_t* url)
{
  DCHECK(windowInfo);

  CefRefPtr<CefHandler> handlerPtr;
  std::wstring urlStr;
  CefWindowInfo wi = *windowInfo;
  
  if(handler) {
    CefHandlerCToCpp* hp = new CefHandlerCToCpp(handler);
    handlerPtr = hp;
    hp->UnderlyingRelease();
  }
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
  
  if(handler) {
    CefHandlerCToCpp* hp = new CefHandlerCToCpp(handler);
    handlerPtr = hp;
    hp->UnderlyingRelease();
  }
  if(url)
    urlStr = url;
  
  cef_browser_t* browserStruct = NULL;
  CefRefPtr<CefBrowser> browserPtr(
      CefBrowser::CreateBrowserSync(wi, popup, handlerPtr, urlStr));
  if(!browserPtr.get())
    return NULL;

  CefBrowserCppToC* bp = new CefBrowserCppToC(browserPtr);
  bp->AddRef();
  return bp->GetStruct();
}

CEF_EXPORT cef_request_t* cef_create_request()
{
  CefRefPtr<CefRequest> impl = CefRequest::CreateRequest();
  if(!impl.get())
    return NULL;
  CefRequestCppToC* rp = new CefRequestCppToC(impl);
  rp->AddRef();
  return rp->GetStruct();
}

CEF_EXPORT cef_post_data_t* cef_create_post_data()
{
  CefRefPtr<CefPostData> impl = CefPostData::CreatePostData();
  if(!impl.get())
    return NULL;
  CefPostDataCppToC* rp = new CefPostDataCppToC(impl);
  rp->AddRef();
  return rp->GetStruct();
}

CEF_EXPORT cef_post_data_element_t* cef_create_post_data_element()
{
  CefRefPtr<CefPostDataElement> impl =
      CefPostDataElement::CreatePostDataElement();
  if(!impl.get())
    return NULL;
  CefPostDataElementCppToC* rp = new CefPostDataElementCppToC(impl);
  rp->AddRef();
  return rp->GetStruct();
}

CEF_EXPORT cef_stream_reader_t* cef_create_stream_reader_for_file(
    const wchar_t* fileName)
{
  std::wstring filenamestr;
  if(fileName)
    filenamestr = fileName;
  CefRefPtr<CefStreamReader> impl = CefStreamReader::CreateForFile(filenamestr);
  if(!impl.get())
    return NULL;
  CefStreamReaderCppToC* rp = new CefStreamReaderCppToC(impl);
  rp->AddRef();
  return rp->GetStruct();
}

CEF_EXPORT cef_stream_reader_t* cef_create_stream_reader_for_data(void *data,
    size_t size)
{
  CefRefPtr<CefStreamReader> impl = CefStreamReader::CreateForData(data, size);
  if(!impl.get())
    return NULL;
  CefStreamReaderCppToC* rp = new CefStreamReaderCppToC(impl);
  rp->AddRef();
  return rp->GetStruct();
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
