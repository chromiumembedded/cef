// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_RESOURCE_CONTEXT_H_
#define CEF_LIBCEF_BROWSER_RESOURCE_CONTEXT_H_
#pragma once

#include "include/cef_request_context.h"
#include "include/cef_request_context_handler.h"

#include "base/files/file_path.h"
#include "chrome/common/plugin.mojom.h"
#include "content/public/browser/resource_context.h"
#include "extensions/browser/info_map.h"
#include "net/ssl/client_cert_store.h"
#include "url/origin.h"

class CefURLRequestContextGetter;

// Acts as a bridge for resource loading. Life span is controlled by
// CefBrowserContext. Created on the UI thread but accessed and destroyed on the
// IO thread. URLRequest objects are associated with the ResourceContext via
// ResourceDispatcherHost. When the ResourceContext is destroyed all outstanding
// URLRequest objects will be deleted via the ResourceLoader that owns them and
// removed from the associated URLRequestContext. Other URLRequest objects may
// be created via URLFetcher that are not associated with a RequestContext.
// See browser_context.h for an object relationship diagram.
class CefResourceContext : public content::ResourceContext {
 public:
  explicit CefResourceContext(bool is_off_the_record);
  ~CefResourceContext() override;

  std::unique_ptr<net::ClientCertStore> CreateClientCertStore();

  void set_extensions_info_map(extensions::InfoMap* extensions_info_map);

  // Keep track of handlers associated with specific frames. This information
  // originates from frame create/delete notifications in CefBrowserHostImpl or
  // CefMimeHandlerViewGuestDelegate which are forwarded via
  // CefRequestContextImpl and CefBrowserContext.
  void AddHandler(int render_process_id,
                  int render_frame_id,
                  CefRefPtr<CefRequestContextHandler> handler);
  void RemoveHandler(int render_process_id, int render_frame_id);
  CefRefPtr<CefRequestContextHandler> GetHandler(int render_process_id,
                                                 int render_frame_id,
                                                 bool require_frame_match);

  // Remember the plugin load decision for plugin status requests that arrive
  // via CefPluginServiceFilter::IsPluginAvailable.
  void AddPluginLoadDecision(int render_process_id,
                             const base::FilePath& plugin_path,
                             bool is_main_frame,
                             const url::Origin& main_frame_origin,
                             chrome::mojom::PluginStatus status);
  bool HasPluginLoadDecision(int render_process_id,
                             const base::FilePath& plugin_path,
                             bool is_main_frame,
                             const url::Origin& main_frame_origin,
                             chrome::mojom::PluginStatus* status) const;

  // Clear the plugin load decisions associated with |render_process_id|, or all
  // plugin load decisions if |render_process_id| is -1.
  void ClearPluginLoadDecision(int render_process_id);

  // State transferred from the BrowserContext for use on the IO thread.
  bool IsOffTheRecord() const { return is_off_the_record_; }
  const extensions::InfoMap* GetExtensionInfoMap() const {
    return extension_info_map_.get();
  }

 private:
  // Only accessed on the IO thread.
  bool is_off_the_record_;
  scoped_refptr<extensions::InfoMap> extension_info_map_;

  // Map of (render_process_id, render_frame_id) to handler.
  typedef std::map<std::pair<int, int>, CefRefPtr<CefRequestContextHandler>>
      HandlerMap;
  HandlerMap handler_map_;

  // Map (render_process_id, plugin_path, is_main_frame, main_frame_origin) to
  // plugin load decision.
  typedef std::map<
      std::pair<std::pair<int, base::FilePath>, std::pair<bool, url::Origin>>,
      chrome::mojom::PluginStatus>
      PluginLoadDecisionMap;
  PluginLoadDecisionMap plugin_load_decision_map_;

  DISALLOW_COPY_AND_ASSIGN(CefResourceContext);
};

#endif  // CEF_LIBCEF_BROWSER_RESOURCE_CONTEXT_H_
