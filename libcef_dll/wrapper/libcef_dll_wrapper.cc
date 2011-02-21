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
#include "libcef_dll/cpptoc/web_urlrequest_client_cpptoc.h"
#include "libcef_dll/cpptoc/write_handler_cpptoc.h"
#include "libcef_dll/ctocpp/browser_ctocpp.h"
#include "libcef_dll/ctocpp/post_data_ctocpp.h"
#include "libcef_dll/ctocpp/post_data_element_ctocpp.h"
#include "libcef_dll/ctocpp/request_ctocpp.h"
#include "libcef_dll/ctocpp/stream_reader_ctocpp.h"
#include "libcef_dll/ctocpp/stream_writer_ctocpp.h"
#include "libcef_dll/ctocpp/v8value_ctocpp.h"
#include "libcef_dll/ctocpp/v8context_ctocpp.h"
#include "libcef_dll/ctocpp/web_urlrequest_ctocpp.h"
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
  DCHECK(CefWebURLRequestClientCppToC::DebugObjCt == 0);
  DCHECK(CefWriteHandlerCppToC::DebugObjCt == 0);
  DCHECK(CefBrowserCToCpp::DebugObjCt == 0);
  DCHECK(CefRequestCToCpp::DebugObjCt == 0);
  DCHECK(CefPostDataCToCpp::DebugObjCt == 0);
  DCHECK(CefPostDataElementCToCpp::DebugObjCt == 0);
  DCHECK(CefStreamReaderCToCpp::DebugObjCt == 0);
  DCHECK(CefStreamWriterCToCpp::DebugObjCt == 0);
  DCHECK(CefV8ContextCToCpp::DebugObjCt == 0);
  DCHECK(CefV8ValueCToCpp::DebugObjCt == 0);
  DCHECK(CefWebURLRequestCToCpp::DebugObjCt == 0);
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

bool CefRegisterPlugin(const CefPluginInfo& plugin_info)
{
  return cef_register_plugin(&plugin_info)?true:false;
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
