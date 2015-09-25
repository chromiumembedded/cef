// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/plugins/plugin_service_filter.h"

#include "include/cef_request_context_handler.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/resource_context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/web_plugin_impl.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/content_client.h"

CefPluginServiceFilter::CefPluginServiceFilter() {
}

bool CefPluginServiceFilter::IsPluginAvailable(
    int render_process_id,
    int render_frame_id,
    const void* context,
    const GURL& url,
    const GURL& policy_url,
    content::WebPluginInfo* plugin) {
  CefRefPtr<CefRequestContextHandler> handler =
      reinterpret_cast<const CefResourceContext*>(context)->GetHandler();

  CefViewHostMsg_GetPluginInfo_Status status =
      CefViewHostMsg_GetPluginInfo_Status::kAllowed;
  return IsPluginAvailable(handler.get(), url, policy_url, plugin, &status);
}

bool CefPluginServiceFilter::CanLoadPlugin(int render_process_id,
                                           const base::FilePath& path) {
  return true;
}

bool CefPluginServiceFilter::IsPluginAvailable(
    CefRequestContextHandler* handler,
    const GURL& url,
    const GURL& policy_url,
    content::WebPluginInfo* plugin,
    CefViewHostMsg_GetPluginInfo_Status* status) {
  if (*status == CefViewHostMsg_GetPluginInfo_Status::kNotFound ||
      *status == CefViewHostMsg_GetPluginInfo_Status::kNPAPINotSupported) {
    // The plugin does not exist so no need to query the handler.
    return false;
  }

  if (plugin->path == CefString(CefContentClient::kPDFPluginPath)) {
    // Always allow the internal PDF plugin to load.
    *status = CefViewHostMsg_GetPluginInfo_Status::kAllowed;
    return true;
  }

  if (handler) {
    CefRefPtr<CefWebPluginInfoImpl> pluginInfo(
        new CefWebPluginInfoImpl(*plugin));

    cef_plugin_policy_t plugin_policy = PLUGIN_POLICY_ALLOW;
    switch (*status) {
      case CefViewHostMsg_GetPluginInfo_Status::kAllowed:
        plugin_policy = PLUGIN_POLICY_ALLOW;
        break;
      case CefViewHostMsg_GetPluginInfo_Status::kBlocked:
      case CefViewHostMsg_GetPluginInfo_Status::kBlockedByPolicy:
      case CefViewHostMsg_GetPluginInfo_Status::kOutdatedBlocked:
      case CefViewHostMsg_GetPluginInfo_Status::kOutdatedDisallowed:
      case CefViewHostMsg_GetPluginInfo_Status::kUnauthorized:
        plugin_policy = PLUGIN_POLICY_BLOCK;
        break;
      case CefViewHostMsg_GetPluginInfo_Status::kDisabled:
        plugin_policy = PLUGIN_POLICY_DISABLE;
        break;
      case CefViewHostMsg_GetPluginInfo_Status::kPlayImportantContent:
        plugin_policy = PLUGIN_POLICY_DETECT_IMPORTANT;
        break;
      default:
        NOTREACHED();
        break;
    }

    if (handler->OnBeforePluginLoad(plugin->mime_types[0].mime_type,
                                    url.possibly_invalid_spec(),
                                    policy_url.possibly_invalid_spec(),
                                    pluginInfo.get(),
                                    &plugin_policy)) {
      switch (plugin_policy) {
        case PLUGIN_POLICY_ALLOW:
          *status = CefViewHostMsg_GetPluginInfo_Status::kAllowed;
          break;
        case PLUGIN_POLICY_DETECT_IMPORTANT:
          *status = CefViewHostMsg_GetPluginInfo_Status::kPlayImportantContent;
          break;
        case PLUGIN_POLICY_BLOCK:
          *status = CefViewHostMsg_GetPluginInfo_Status::kBlocked;
          break;
        case PLUGIN_POLICY_DISABLE:
          *status = CefViewHostMsg_GetPluginInfo_Status::kDisabled;
          break;
      }
    }
  }

  return (*status != CefViewHostMsg_GetPluginInfo_Status::kDisabled);
}
