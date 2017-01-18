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

#include "extensions/common/constants.h"

CefPluginServiceFilter::CefPluginServiceFilter() {
}

bool CefPluginServiceFilter::IsPluginAvailable(
    int render_process_id,
    int render_frame_id,
    const void* context,
    const GURL& url,
    bool is_main_frame,
    const url::Origin& main_frame_origin,
    content::WebPluginInfo* plugin) {
  CefResourceContext* resource_context = const_cast<CefResourceContext*>(
      reinterpret_cast<const CefResourceContext*>(context));
  CefViewHostMsg_GetPluginInfo_Status status =
      CefViewHostMsg_GetPluginInfo_Status::kAllowed;

  // Perform origin check here because we're passing an empty origin value to
  // IsPluginAvailable() below.
  const GURL& policy_url = main_frame_origin.GetURL();
  if (!policy_url.is_empty() &&
      policy_url.scheme() == extensions::kExtensionScheme) {
    // Always allow extension origins to load plugins.
    // TODO(extensions): Revisit this decision once CEF supports more than just
    // the PDF extension.
    return true;
  }

  // Blink requires this method to return a consistent value during renderer
  // process initialization and page load, so we always call IsPluginAvailable()
  // with an empty origin. If we return false then the plugin will not be listed
  // in navigator.plugins and navigating to the plugin mime type will trigger
  // the download code path. If we return true then individual plugin instance
  // loads will be evaluated in CefContentRendererClient::OverrideCreatePlugin,
  // which will result in a call to CefPluginInfoMessageFilter::PluginsLoaded to
  // retrieve the actual load decision with a non-empty origin. That will
  // determine whether the plugin load is allowed or the plugin placeholder is
  // displayed.
  return IsPluginAvailable(render_process_id, resource_context, url,
                           is_main_frame, url::Origin(), plugin, &status);
}

bool CefPluginServiceFilter::CanLoadPlugin(int render_process_id,
                                           const base::FilePath& path) {
  return true;
}

bool CefPluginServiceFilter::IsPluginAvailable(
    int render_process_id,
    CefResourceContext* resource_context,
    const GURL& url,
    bool is_main_frame,
    const url::Origin& main_frame_origin,
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

  const GURL& policy_url = main_frame_origin.GetURL();
  if (!policy_url.is_empty() &&
      policy_url.scheme() == extensions::kExtensionScheme) {
    // Always allow extension origins to load plugins.
    // TODO(extensions): Revisit this decision once CEF supports more than just
    // the PDF extension.
    *status = CefViewHostMsg_GetPluginInfo_Status::kAllowed;
    return true;
  }

  CefRefPtr<CefRequestContextHandler> handler = resource_context->GetHandler();
  if (!handler) {
    // No handler so go with the default plugin load decision.
    return *status != CefViewHostMsg_GetPluginInfo_Status::kDisabled;
  }

  // Check for a cached plugin load decision.
  if (resource_context->HasPluginLoadDecision(
          render_process_id, plugin->path,
          is_main_frame, main_frame_origin, status)) {
    return *status != CefViewHostMsg_GetPluginInfo_Status::kDisabled;
  }

  CefRefPtr<CefWebPluginInfoImpl> pluginInfo(new CefWebPluginInfoImpl(*plugin));

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
                                  is_main_frame,
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

  // Cache the plugin load decision.
  resource_context->AddPluginLoadDecision(
      render_process_id, plugin->path,
      is_main_frame, main_frame_origin, *status);

  return *status != CefViewHostMsg_GetPluginInfo_Status::kDisabled;
}
