// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/resource_dispatcher_host_delegate.h"

#include <stdint.h>

#include <utility>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/origin_whitelist_impl.h"
#include "libcef/browser/resource_context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/extensions/extensions_util.h"

#include "base/guid.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/stream_info.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

CefResourceDispatcherHostDelegate::CefResourceDispatcherHostDelegate() {}

CefResourceDispatcherHostDelegate::~CefResourceDispatcherHostDelegate() {}

// Implementation based on
// ChromeResourceDispatcherHostDelegate::ShouldInterceptResourceAsStream.
bool CefResourceDispatcherHostDelegate::ShouldInterceptResourceAsStream(
    net::URLRequest* request,
    const std::string& mime_type,
    GURL* origin,
    std::string* payload) {
  if (!extensions::ExtensionsEnabled())
    return false;

  content::ResourceRequestInfo* info =
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
    if (!extension || (profile_is_off_the_record &&
                       !extension_info_map->IsIncognitoEnabled(extension_id))) {
      continue;
    }

    MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
    if (!handler)
      continue;

    if (handler->CanHandleMIMEType(mime_type)) {
      StreamTargetInfo target_info;
      *origin = extensions::Extension::GetBaseURLFromExtensionId(extension_id);
      target_info.extension_id = extension_id;
      target_info.view_id = base::GenerateGUID();
      *payload = target_info.view_id;
      stream_target_info_[request] = target_info;
      return true;
    }
  }

  return false;
}

// Implementation based on
// ChromeResourceDispatcherHostDelegate::OnStreamCreated.
void CefResourceDispatcherHostDelegate::OnStreamCreated(
    net::URLRequest* request,
    std::unique_ptr<content::StreamInfo> stream) {
  DCHECK(extensions::ExtensionsEnabled());
  content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  std::map<net::URLRequest*, StreamTargetInfo>::iterator ix =
      stream_target_info_.find(request);
  CHECK(ix != stream_target_info_.end());
  bool embedded = info->GetResourceType() != content::ResourceType::kMainFrame;
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &extensions::StreamsPrivateAPI::SendExecuteMimeTypeHandlerEvent,
          ix->second.extension_id, ix->second.view_id, embedded,
          info->GetFrameTreeNodeId(), info->GetChildID(),
          info->GetRenderFrameID(), std::move(stream),
          nullptr /* transferrable_loader */, GURL()));
  stream_target_info_.erase(request);
}

void CefResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    network::ResourceResponse* response) {
  const GURL& active_url = request->url();
  if (active_url.is_valid() && redirect_url.is_valid() &&
      active_url.GetOrigin() != redirect_url.GetOrigin() &&
      HasCrossOriginWhitelistEntry(active_url, redirect_url)) {
    if (!response->head.headers.get())
      response->head.headers = new net::HttpResponseHeaders(std::string());

    // Add CORS headers to support XMLHttpRequest redirects.
    response->head.headers->AddHeader(
        "Access-Control-Allow-Origin: " + active_url.scheme() + "://" +
        active_url.host());
    response->head.headers->AddHeader("Access-Control-Allow-Credentials: true");
  }
}
