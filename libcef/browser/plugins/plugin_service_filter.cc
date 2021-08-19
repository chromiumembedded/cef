// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/plugins/plugin_service_filter.h"

#include "include/cef_request_context_handler.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/web_plugin_impl.h"
#include "libcef/common/alloy/alloy_content_client.h"
#include "libcef/common/frame_util.h"

#include "extensions/common/constants.h"

CefPluginServiceFilter::CefPluginServiceFilter() {}

bool CefPluginServiceFilter::IsPluginAvailable(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    bool is_main_frame,
    const url::Origin& main_frame_origin,
    content::WebPluginInfo* plugin) {
  CEF_REQUIRE_UIT();
  DCHECK_GT(render_process_id, 0);

  chrome::mojom::PluginStatus status = chrome::mojom::PluginStatus::kAllowed;

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
  // loads will be evaluated in
  // AlloyContentRendererClient::OverrideCreatePlugin, which will result in a
  // call to CefPluginInfoMessageFilter::PluginsLoaded to retrieve the actual
  // load decision with a non-empty origin. That will determine whether the
  // plugin load is allowed or the plugin placeholder is displayed.
  return IsPluginAvailable(render_process_id, render_frame_id, url,
                           is_main_frame, url::Origin(), plugin, &status);
}

bool CefPluginServiceFilter::CanLoadPlugin(int render_process_id,
                                           const base::FilePath& path) {
  return true;
}

bool CefPluginServiceFilter::IsPluginAvailable(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    bool is_main_frame,
    const url::Origin& main_frame_origin,
    content::WebPluginInfo* plugin,
    chrome::mojom::PluginStatus* status) {
  CEF_REQUIRE_UIT();
  DCHECK_GT(render_process_id, 0);

  if (*status == chrome::mojom::PluginStatus::kNotFound) {
    // The plugin does not exist so no need to query the handler.
    return false;
  }

  if (plugin->path == CefString(AlloyContentClient::kPDFPluginPath)) {
    // Always allow the internal PDF plugin to load.
    *status = chrome::mojom::PluginStatus::kAllowed;
    return true;
  }

  const GURL& policy_url = main_frame_origin.GetURL();
  if (!policy_url.is_empty() &&
      policy_url.scheme() == extensions::kExtensionScheme) {
    // Always allow extension origins to load plugins.
    // TODO(extensions): Revisit this decision once CEF supports more than just
    // the PDF extension.
    *status = chrome::mojom::PluginStatus::kAllowed;
    return true;
  }

  const auto global_id = frame_util::MakeGlobalId(
      render_process_id, render_frame_id, /*allow_invalid_frame_id=*/true);
  auto browser_context = CefBrowserContext::FromGlobalId(global_id, false);
  CefRefPtr<CefRequestContextHandler> handler;
  if (browser_context) {
    handler = browser_context->GetHandler(global_id, false);
  }

  if (!handler) {
    // No handler so go with the default plugin load decision.
    return *status != chrome::mojom::PluginStatus::kDisabled;
  }

  // Check for a cached plugin load decision.
  if (browser_context->HasPluginLoadDecision(render_process_id, plugin->path,
                                             is_main_frame, main_frame_origin,
                                             status)) {
    return *status != chrome::mojom::PluginStatus::kDisabled;
  }

  CefRefPtr<CefWebPluginInfoImpl> pluginInfo(new CefWebPluginInfoImpl(*plugin));

  cef_plugin_policy_t plugin_policy = PLUGIN_POLICY_ALLOW;
  switch (*status) {
    case chrome::mojom::PluginStatus::kAllowed:
      plugin_policy = PLUGIN_POLICY_ALLOW;
      break;
    case chrome::mojom::PluginStatus::kBlocked:
    case chrome::mojom::PluginStatus::kBlockedByPolicy:
    case chrome::mojom::PluginStatus::kOutdatedBlocked:
    case chrome::mojom::PluginStatus::kOutdatedDisallowed:
    case chrome::mojom::PluginStatus::kUnauthorized:
      plugin_policy = PLUGIN_POLICY_BLOCK;
      break;
    case chrome::mojom::PluginStatus::kDisabled:
      plugin_policy = PLUGIN_POLICY_DISABLE;
      break;
    case chrome::mojom::PluginStatus::kPlayImportantContent:
      plugin_policy = PLUGIN_POLICY_DETECT_IMPORTANT;
      break;
    default:
      NOTREACHED();
      break;
  }

  if (handler->OnBeforePluginLoad(plugin->mime_types[0].mime_type,
                                  url.possibly_invalid_spec(), is_main_frame,
                                  policy_url.possibly_invalid_spec(),
                                  pluginInfo.get(), &plugin_policy)) {
    switch (plugin_policy) {
      case PLUGIN_POLICY_ALLOW:
        *status = chrome::mojom::PluginStatus::kAllowed;
        break;
      case PLUGIN_POLICY_DETECT_IMPORTANT:
        *status = chrome::mojom::PluginStatus::kPlayImportantContent;
        break;
      case PLUGIN_POLICY_BLOCK:
        *status = chrome::mojom::PluginStatus::kBlocked;
        break;
      case PLUGIN_POLICY_DISABLE:
        *status = chrome::mojom::PluginStatus::kDisabled;
        break;
    }
  }

  // Cache the plugin load decision.
  browser_context->AddPluginLoadDecision(render_process_id, plugin->path,
                                         is_main_frame, main_frame_origin,
                                         *status);

  return *status != chrome::mojom::PluginStatus::kDisabled;
}
