// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/resource_dispatcher_host_delegate.h"

#include <stdint.h>

#include <utility>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/extensions/api/streams_private/streams_private_api.h"
#include "libcef/browser/origin_whitelist_impl.h"
#include "libcef/browser/resource_context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/extensions/extensions_util.h"

#include "base/guid.h"
#include "base/memory/scoped_vector.h"
#include "build/build_config.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/stream_info.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

namespace {

void SendExecuteMimeTypeHandlerEvent(scoped_ptr<content::StreamInfo> stream,
                                     int64_t expected_content_size,
                                     int render_process_id,
                                     int render_frame_id,
                                     const std::string& extension_id,
                                     const std::string& view_id,
                                     bool embedded) {
  CEF_REQUIRE_UIT();

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForFrame(render_process_id,
                                             render_frame_id);
  if (!browser.get())
    return;

  content::WebContents* web_contents = browser->web_contents();
  if (!web_contents)
    return;

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();

  extensions::StreamsPrivateAPI* streams_private =
      extensions::StreamsPrivateAPI::Get(browser_context);
  if (!streams_private)
    return;

  // A |tab_id| value of -1 disables zoom management in the PDF extension.
  // Otherwise we need to implement chrome.tabs zoom handling. See
  // chrome/browser/resources/pdf/browser_api.js.
  int tab_id = -1;

  streams_private->ExecuteMimeTypeHandler(
      extension_id, tab_id, std::move(stream), view_id, expected_content_size,
      embedded, render_process_id, render_frame_id);
}

}  // namespace

CefResourceDispatcherHostDelegate::CefResourceDispatcherHostDelegate() {
}

CefResourceDispatcherHostDelegate::~CefResourceDispatcherHostDelegate() {
}

bool CefResourceDispatcherHostDelegate::HandleExternalProtocol(
    const GURL& url,
    int child_id,
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    bool is_main_frame,
    ui::PageTransition page_transition,
    bool has_user_gesture) {
  if (CEF_CURRENTLY_ON_UIT()) {
    content::WebContents* web_contents = web_contents_getter.Run();
    CefRefPtr<CefBrowserHostImpl> browser =
        CefBrowserHostImpl::GetBrowserForContents(web_contents);
    if (browser.get())
      browser->HandleExternalProtocol(url);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(base::IgnoreResult(&CefResourceDispatcherHostDelegate::
                       HandleExternalProtocol),
                   base::Unretained(this), url, child_id, web_contents_getter,
                   is_main_frame, page_transition, has_user_gesture));
  }
  return false;
}

// Implementation based on
// ChromeResourceDispatcherHostDelegate::ShouldInterceptResourceAsStream.
bool CefResourceDispatcherHostDelegate::ShouldInterceptResourceAsStream(
    net::URLRequest* request,
    const base::FilePath& plugin_path,
    const std::string& mime_type,
    GURL* origin,
    std::string* payload) {
  if (!extensions::ExtensionsEnabled())
    return false;

  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  CefResourceContext* context =
      static_cast<CefResourceContext*>(info->GetContext());
  bool profile_is_off_the_record = context->IsOffTheRecord();
  const scoped_refptr<const extensions::InfoMap> extension_info_map(
      context->GetExtensionInfoMap());

  std::vector<std::string> whitelist = MimeTypesHandler::GetMIMETypeWhitelist();
  // Go through the white-listed extensions and try to use them to intercept
  // the URL request.
  for (const std::string& extension_id : whitelist) {
    const extensions::Extension* extension =
        extension_info_map->extensions().GetByID(extension_id);
    // The white-listed extension may not be installed, so we have to NULL check
    // |extension|.
    if (!extension ||
        (profile_is_off_the_record &&
         !extension_info_map->IsIncognitoEnabled(extension_id))) {
      continue;
    }

    MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
    if (!handler)
      continue;

    // If a plugin path is provided then a stream is being intercepted for the
    // mimeHandlerPrivate API. Otherwise a stream is being intercepted for the
    // streamsPrivate API.
    if (!plugin_path.empty()) {
      if (handler->HasPlugin() && plugin_path == handler->GetPluginPath()) {
        StreamTargetInfo target_info;
        *origin =
            extensions::Extension::GetBaseURLFromExtensionId(extension_id);
        target_info.extension_id = extension_id;
        target_info.view_id = base::GenerateGUID();
        *payload = target_info.view_id;
        stream_target_info_[request] = target_info;
        return true;
      }
    } else {
      if (!handler->HasPlugin() && handler->CanHandleMIMEType(mime_type)) {
        StreamTargetInfo target_info;
        *origin =
            extensions::Extension::GetBaseURLFromExtensionId(extension_id);
        target_info.extension_id = extension_id;
        stream_target_info_[request] = target_info;
        return true;
      }
    }
  }

  return false;
}

// Implementation based on
// ChromeResourceDispatcherHostDelegate::OnStreamCreated.
void CefResourceDispatcherHostDelegate::OnStreamCreated(
    net::URLRequest* request,
    scoped_ptr<content::StreamInfo> stream) {
  DCHECK(extensions::ExtensionsEnabled());
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  std::map<net::URLRequest*, StreamTargetInfo>::iterator ix =
      stream_target_info_.find(request);
  CHECK(ix != stream_target_info_.end());
  bool embedded = info->GetResourceType() != content::RESOURCE_TYPE_MAIN_FRAME;
  CEF_POST_TASK(CEF_UIT,
      base::Bind(&SendExecuteMimeTypeHandlerEvent, base::Passed(&stream),
                 request->GetExpectedContentSize(), info->GetChildID(),
                 info->GetRenderFrameID(), ix->second.extension_id,
                 ix->second.view_id, embedded));
  stream_target_info_.erase(request);
}

void CefResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceResponse* response) {
  const GURL& active_url = request->url();
  if (active_url.is_valid() && redirect_url.is_valid() &&
      active_url.GetOrigin() != redirect_url.GetOrigin() &&
      HasCrossOriginWhitelistEntry(active_url, redirect_url)) {
    if (!response->head.headers.get())
      response->head.headers = new net::HttpResponseHeaders(std::string());

    // Add CORS headers to support XMLHttpRequest redirects.
    response->head.headers->AddHeader("Access-Control-Allow-Origin: " +
        active_url.scheme() + "://" + active_url.host());
    response->head.headers->AddHeader("Access-Control-Allow-Credentials: true");
  }
}
