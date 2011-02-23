// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "include/cef_capi.h"
#include "include/cef_nplugin.h"
#include "include/cef_nplugin_capi.h"
#include "cef_logging.h"
#include "cpptoc/browser_cpptoc.h"
#include "cpptoc/domdocument_cpptoc.h"
#include "cpptoc/domevent_cpptoc.h"
#include "cpptoc/domnode_cpptoc.h"
#include "cpptoc/post_data_cpptoc.h"
#include "cpptoc/post_data_element_cpptoc.h"
#include "cpptoc/request_cpptoc.h"
#include "cpptoc/stream_reader_cpptoc.h"
#include "cpptoc/stream_writer_cpptoc.h"
#include "cpptoc/v8value_cpptoc.h"
#include "cpptoc/web_urlrequest_cpptoc.h"
#include "cpptoc/xml_reader_cpptoc.h"
#include "cpptoc/zip_reader_cpptoc.h"
#include "ctocpp/domevent_listener_ctocpp.h"
#include "ctocpp/domvisitor_ctocpp.h"
#include "ctocpp/download_handler_ctocpp.h"
#include "ctocpp/handler_ctocpp.h"
#include "ctocpp/read_handler_ctocpp.h"
#include "ctocpp/scheme_handler_ctocpp.h"
#include "ctocpp/scheme_handler_factory_ctocpp.h"
#include "ctocpp/task_ctocpp.h"
#include "ctocpp/v8handler_ctocpp.h"
#include "ctocpp/web_urlrequest_client_ctocpp.h"
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
  DCHECK(CefDOMDocumentCppToC::DebugObjCt == 0);
  DCHECK(CefDOMEventCppToC::DebugObjCt == 0);
  DCHECK(CefDOMNodeCppToC::DebugObjCt == 0);
  DCHECK(CefRequestCppToC::DebugObjCt == 0);
  DCHECK(CefPostDataCppToC::DebugObjCt == 0);
  DCHECK(CefPostDataElementCppToC::DebugObjCt == 0);
  DCHECK(CefStreamReaderCppToC::DebugObjCt == 0);
  DCHECK(CefStreamWriterCppToC::DebugObjCt == 0);
  DCHECK(CefV8ValueCppToC::DebugObjCt == 0);
  DCHECK(CefWebURLRequestCppToC::DebugObjCt == 0);
  DCHECK(CefXmlReaderCppToC::DebugObjCt == 0);
  DCHECK(CefZipReaderCppToC::DebugObjCt == 0);
  DCHECK(CefDOMEventListenerCToCpp::DebugObjCt == 0);
  DCHECK(CefDOMVisitorCToCpp::DebugObjCt == 0);
  DCHECK(CefDownloadHandlerCToCpp::DebugObjCt == 0);
  DCHECK(CefReadHandlerCToCpp::DebugObjCt == 0);
  DCHECK(CefSchemeHandlerCToCpp::DebugObjCt == 0);
  DCHECK(CefSchemeHandlerFactoryCToCpp::DebugObjCt == 0);
  DCHECK(CefV8HandlerCToCpp::DebugObjCt == 0);
  DCHECK(CefWebURLRequestClientCToCpp::DebugObjCt == 0);
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
  DCHECK(plugin_info);
  return CefRegisterPlugin(*plugin_info);
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

CEF_EXPORT int cef_parse_url(const cef_string_t* url,
    struct _cef_urlparts_t* parts)
{
  DCHECK(url && parts);
  if(!url || !parts)
    return 0;

  CefURLParts urlParts;
  bool ret = CefParseURL(CefString(url), urlParts);

  // Clear the current structure values, if any.
  cef_string_clear(&parts->spec);
  cef_string_clear(&parts->scheme);
  cef_string_clear(&parts->username);
  cef_string_clear(&parts->password);
  cef_string_clear(&parts->host);
  cef_string_clear(&parts->port);
  cef_string_clear(&parts->path);
  cef_string_clear(&parts->query);

  // Transfer ownership of the values from |urlParts| to the structure.
  memcpy(parts, static_cast<cef_urlparts_t*>(&urlParts),
      sizeof(cef_urlparts_t));
  urlParts.Detach();

  return ret;
}

CEF_EXPORT int cef_create_url(const struct _cef_urlparts_t* parts,
    cef_string_t* url)
{
  DCHECK(parts && url);
  if(!parts || !url)
    return 0;

  CefURLParts urlParts;

  // Reference the existing structure values without copying.
  cef_string_set(parts->spec.str, parts->spec.length, &urlParts.spec, false);
  cef_string_set(parts->scheme.str, parts->scheme.length, &urlParts.scheme,
      false);
  cef_string_set(parts->username.str, parts->username.length,
      &urlParts.username, false);
  cef_string_set(parts->password.str, parts->password.length,
      &urlParts.password, false);
  cef_string_set(parts->host.str, parts->host.length, &urlParts.host, false);
  cef_string_set(parts->port.str, parts->port.length, &urlParts.port, false);
  cef_string_set(parts->path.str, parts->path.length, &urlParts.path, false);
  cef_string_set(parts->query.str, parts->query.length, &urlParts.query, false);

  CefString urlStr(url);
  return CefCreateURL(urlParts, urlStr);
}
