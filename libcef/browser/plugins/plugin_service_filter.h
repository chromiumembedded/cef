// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PLUGINS_PLUGIN_SERVICE_FILTER_H_
#define CEF_LIBCEF_BROWSER_PLUGINS_PLUGIN_SERVICE_FILTER_H_

#include "content/public/browser/plugin_service_filter.h"

#include "include/internal/cef_types.h"

#include "base/macros.h"

class CefResourceContext;
enum class CefViewHostMsg_GetPluginInfo_Status;

class CefPluginServiceFilter : public content::PluginServiceFilter {
 public:
  CefPluginServiceFilter();

  // Called whenever the plugin list is queried. For example, when choosing the
  // plugin to handle a mime type or when determining the plugins that will be
  // exposed to JavaScript via 'navigator.plugins'.
  bool IsPluginAvailable(int render_process_id,
                         int render_frame_id,
                         const void* context,
                         const GURL& url,
                         bool is_main_frame,
                         const url::Origin& main_frame_origin,
                         content::WebPluginInfo* plugin) override;

  bool CanLoadPlugin(int render_process_id,
                     const base::FilePath& path) override;

  // Returns false if the plugin is not found or disabled. May call
  // CefRequestContextHandler::OnBeforePluginLoad if possible/necessary.
  // See related discussion in issue #2015.
  bool IsPluginAvailable(int render_process_id,
                         CefResourceContext* resource_context,
                         const GURL& url,
                         bool is_main_frame,
                         const url::Origin& main_frame_origin,
                         content::WebPluginInfo* plugin,
                         CefViewHostMsg_GetPluginInfo_Status* status);

 private:
  DISALLOW_COPY_AND_ASSIGN(CefPluginServiceFilter);
};

#endif  // CEF_LIBCEF_BROWSER_PLUGINS_PLUGIN_SERVICE_FILTER_H_
