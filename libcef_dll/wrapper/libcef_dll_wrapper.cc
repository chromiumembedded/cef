// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "include/cef_capi.h"
#include "include/cef_nplugin.h"
#include "include/cef_nplugin_capi.h"
#include "libcef_dll/cpptoc/content_filter_cpptoc.h"
#include "libcef_dll/cpptoc/cookie_visitor_cpptoc.h"
#include "libcef_dll/cpptoc/domevent_listener_cpptoc.h"
#include "libcef_dll/cpptoc/domvisitor_cpptoc.h"
#include "libcef_dll/cpptoc/download_handler_cpptoc.h"
#include "libcef_dll/cpptoc/read_handler_cpptoc.h"
#include "libcef_dll/cpptoc/scheme_handler_cpptoc.h"
#include "libcef_dll/cpptoc/scheme_handler_factory_cpptoc.h"
#include "libcef_dll/cpptoc/task_cpptoc.h"
#include "libcef_dll/cpptoc/v8accessor_cpptoc.h"
#include "libcef_dll/cpptoc/v8handler_cpptoc.h"
#include "libcef_dll/cpptoc/web_urlrequest_client_cpptoc.h"
#include "libcef_dll/cpptoc/write_handler_cpptoc.h"
#include "libcef_dll/ctocpp/browser_ctocpp.h"
#include "libcef_dll/ctocpp/domdocument_ctocpp.h"
#include "libcef_dll/ctocpp/domevent_ctocpp.h"
#include "libcef_dll/ctocpp/domnode_ctocpp.h"
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


bool CefInitialize(const CefSettings& settings)
{
  return cef_initialize(&settings)?true:false;
}

void CefShutdown()
{
  cef_shutdown();

#ifndef NDEBUG
  // Check that all wrapper objects have been destroyed
  DCHECK(CefContentFilterCppToC::DebugObjCt == 0);
  DCHECK(CefCookieVisitorCppToC::DebugObjCt == 0);
  DCHECK(CefDOMEventListenerCppToC::DebugObjCt == 0);
  DCHECK(CefDOMVisitorCppToC::DebugObjCt == 0);
  DCHECK(CefDownloadHandlerCppToC::DebugObjCt == 0);
  DCHECK(CefReadHandlerCppToC::DebugObjCt == 0);
  DCHECK(CefSchemeHandlerCppToC::DebugObjCt == 0);
  DCHECK(CefSchemeHandlerFactoryCppToC::DebugObjCt == 0);
  DCHECK(CefV8AccessorCppToC::DebugObjCt == 0);
  DCHECK(CefV8HandlerCppToC::DebugObjCt == 0);
  DCHECK(CefWebURLRequestClientCppToC::DebugObjCt == 0);
  DCHECK(CefWriteHandlerCppToC::DebugObjCt == 0);
  DCHECK(CefBrowserCToCpp::DebugObjCt == 0);
  DCHECK(CefDOMDocumentCToCpp::DebugObjCt == 0);
  DCHECK(CefDOMEventCToCpp::DebugObjCt == 0);
  DCHECK(CefDOMNodeCToCpp::DebugObjCt == 0);
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
#endif // !NDEBUG
}

void CefDoMessageLoopWork()
{
  cef_do_message_loop_work();
}

void CefRunMessageLoop()
{
  cef_run_message_loop();
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

bool CefRegisterCustomScheme(const CefString& scheme_name,
                             bool is_standard,
                             bool is_local,
                             bool is_display_isolated)
{
  return cef_register_custom_scheme(scheme_name.GetStruct(), is_standard,
      is_local, is_display_isolated)?true:false;
}

bool CefRegisterSchemeHandlerFactory(const CefString& scheme_name,
                                     const CefString& domain_name,
                                     CefRefPtr<CefSchemeHandlerFactory> factory)
{
  cef_scheme_handler_factory_t* factory_struct = NULL;
  if (factory.get())
    factory_struct = CefSchemeHandlerFactoryCppToC::Wrap(factory);
  
  return cef_register_scheme_handler_factory(scheme_name.GetStruct(),
      domain_name.GetStruct(), factory_struct)?true:false;
}

bool CefClearSchemeHandlerFactories()
{
  return cef_clear_scheme_handler_factories()?true:false;
}

bool CefAddCrossOriginWhitelistEntry(const CefString& source_origin,
                                     const CefString& target_protocol,
                                     const CefString& target_domain,
                                     bool allow_target_subdomains)
{
  return cef_add_cross_origin_whitelist_entry(source_origin.GetStruct(),
      target_protocol.GetStruct(), target_domain.GetStruct(),
      allow_target_subdomains)?true:false;
}

bool CefRemoveCrossOriginWhitelistEntry(const CefString& source_origin,
                                        const CefString& target_protocol,
                                        const CefString& target_domain,
                                        bool allow_target_subdomains)
{
  return cef_remove_cross_origin_whitelist_entry(source_origin.GetStruct(),
      target_protocol.GetStruct(), target_domain.GetStruct(),
      allow_target_subdomains)?true:false;
}

bool CefClearCrossOriginWhitelist()
{
  return cef_clear_cross_origin_whitelist()?true:false;
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

bool CefVisitAllCookies(CefRefPtr<CefCookieVisitor> visitor)
{
  return cef_visit_all_cookies(CefCookieVisitorCppToC::Wrap(visitor)) ?
      true : false;
}

bool CefVisitUrlCookies(const CefString& url, bool includeHttpOnly,
                        CefRefPtr<CefCookieVisitor> visitor)
{
  return cef_visit_url_cookies(url.GetStruct(), includeHttpOnly,
      CefCookieVisitorCppToC::Wrap(visitor)) ? true : false;
}

bool CefSetCookie(const CefString& url, const CefCookie& cookie)
{
  return cef_set_cookie(url.GetStruct(), &cookie) ? true : false;
}

bool CefDeleteCookies(const CefString& url, const CefString& cookie_name)
{
  return cef_delete_cookies(url.GetStruct(), cookie_name.GetStruct()) ?
      true : false;
}
