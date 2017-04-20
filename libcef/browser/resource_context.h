// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_RESOURCE_CONTEXT_H_
#define CEF_LIBCEF_BROWSER_RESOURCE_CONTEXT_H_
#pragma once

#include "include/cef_request_context_handler.h"

#include "base/files/file_path.h"
#include "content/public/browser/resource_context.h"
#include "extensions/browser/info_map.h"
#include "net/ssl/client_cert_store.h"
#include "url/origin.h"

class CefURLRequestContextGetter;
enum class CefViewHostMsg_GetPluginInfo_Status;

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
  CefResourceContext(bool is_off_the_record,
                     CefRefPtr<CefRequestContextHandler> handler);
  ~CefResourceContext() override;

  // SupportsUserData implementation.
  Data* GetUserData(const void* key) const override;
  void SetUserData(const void* key, Data* data) override;
  void SetUserData(const void* key, std::unique_ptr<Data> data) override;
  void RemoveUserData(const void* key) override;

  // ResourceContext implementation.
  net::HostResolver* GetHostResolver() override;
  net::URLRequestContext* GetRequestContext() override;

  std::unique_ptr<net::ClientCertStore> CreateClientCertStore();

  void set_extensions_info_map(extensions::InfoMap* extensions_info_map);
  void set_url_request_context_getter(CefURLRequestContextGetter* getter);
  void set_parent(CefResourceContext* parent);

  // Remember the plugin load decision for plugin status requests that arrive
  // via CefPluginServiceFilter::IsPluginAvailable.
  void AddPluginLoadDecision(int render_process_id,
                             const base::FilePath& plugin_path,
                             bool is_main_frame,
                             const url::Origin& main_frame_origin,
                             CefViewHostMsg_GetPluginInfo_Status status);
  bool HasPluginLoadDecision(int render_process_id,
                             const base::FilePath& plugin_path,
                             bool is_main_frame,
                             const url::Origin& main_frame_origin,
                             CefViewHostMsg_GetPluginInfo_Status* status) const;

  // Clear the plugin load decisions associated with |render_process_id|, or all
  // plugin load decisions if |render_process_id| is -1.
  void ClearPluginLoadDecision(int render_process_id);

  // State transferred from the BrowserContext for use on the IO thread.
  bool IsOffTheRecord() const { return is_off_the_record_; }
  const extensions::InfoMap* GetExtensionInfoMap() const {
    return extension_info_map_.get();
  }
  CefRefPtr<CefRequestContextHandler> GetHandler() const { return handler_; }

 private:
  scoped_refptr<CefURLRequestContextGetter> getter_;

  // Non-NULL when this object is owned by a CefBrowserContextProxy. |parent_|
  // is guaranteed to outlive this object because CefBrowserContextProxy has a
  // refptr to the CefBrowserContextImpl that owns |parent_|.
  CefResourceContext* parent_;

  // Only accessed on the IO thread.
  bool is_off_the_record_;
  scoped_refptr<extensions::InfoMap> extension_info_map_;
  CefRefPtr<CefRequestContextHandler> handler_;

  // Map (render_process_id, plugin_path, is_main_frame, main_frame_origin) to
  // plugin load decision.
  typedef std::map<std::pair<std::pair<int, base::FilePath>,
                             std::pair<bool, url::Origin>>,
                   CefViewHostMsg_GetPluginInfo_Status>
      PluginLoadDecisionMap;
  PluginLoadDecisionMap plugin_load_decision_map_;

  DISALLOW_COPY_AND_ASSIGN(CefResourceContext);
};

#endif  // CEF_LIBCEF_BROWSER_RESOURCE_CONTEXT_H_
