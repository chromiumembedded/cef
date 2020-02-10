// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_RESOURCE_CONTEXT_H_
#define CEF_LIBCEF_BROWSER_RESOURCE_CONTEXT_H_
#pragma once

#include "include/cef_request_context.h"
#include "include/cef_request_context_handler.h"
#include "include/cef_scheme.h"

#include "libcef/browser/request_context_handler_map.h"

#include "content/public/browser/resource_context.h"

class GURL;

// Acts as a bridge for resource loading. Life span is controlled by
// CefBrowserContext. Created on the UI thread but accessed and destroyed on the
// IO thread. Network request objects are associated with the ResourceContext
// via ProxyURLLoaderFactory. When the ResourceContext is destroyed all
// outstanding network request objects will be canceled.
// See browser_context.h for an object relationship diagram.
class CefResourceContext : public content::ResourceContext {
 public:
  explicit CefResourceContext(bool is_off_the_record);
  ~CefResourceContext() override;

  // See comments in CefRequestContextHandlerMap.
  void AddHandler(int render_process_id,
                  int render_frame_id,
                  int frame_tree_node_id,
                  CefRefPtr<CefRequestContextHandler> handler);
  void RemoveHandler(int render_process_id,
                     int render_frame_id,
                     int frame_tree_node_id);
  CefRefPtr<CefRequestContextHandler> GetHandler(
      int render_process_id,
      int render_frame_id,
      int frame_tree_node_id,
      bool require_frame_match) const;

  // Manage scheme handler factories associated with this context.
  void RegisterSchemeHandlerFactory(const std::string& scheme_name,
                                    const std::string& domain_name,
                                    CefRefPtr<CefSchemeHandlerFactory> factory);
  void ClearSchemeHandlerFactories();
  CefRefPtr<CefSchemeHandlerFactory> GetSchemeHandlerFactory(const GURL& url);

  // State transferred from the BrowserContext for use on the IO thread.
  bool IsOffTheRecord() const { return is_off_the_record_; }

 private:
  void InitOnIOThread();

  // Only accessed on the IO thread.
  const bool is_off_the_record_;

  // Map IDs to CefRequestContextHandler objects.
  CefRequestContextHandlerMap handler_map_;

  // Map (scheme, domain) to factories.
  typedef std::map<std::pair<std::string, std::string>,
                   CefRefPtr<CefSchemeHandlerFactory>>
      SchemeHandlerFactoryMap;
  SchemeHandlerFactoryMap scheme_handler_factory_map_;

  DISALLOW_COPY_AND_ASSIGN(CefResourceContext);
};

#endif  // CEF_LIBCEF_BROWSER_RESOURCE_CONTEXT_H_
