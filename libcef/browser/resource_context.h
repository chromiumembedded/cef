// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_RESOURCE_CONTEXT_H_
#define CEF_LIBCEF_BROWSER_RESOURCE_CONTEXT_H_
#pragma once

#include "include/cef_request_context_handler.h"

#include "content/public/browser/resource_context.h"
#include "extensions/browser/info_map.h"

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
  CefResourceContext(bool is_off_the_record,
                     extensions::InfoMap* extension_info_map,
                     CefRefPtr<CefRequestContextHandler> handler);
  ~CefResourceContext() override;

  // ResourceContext implementation.
  net::HostResolver* GetHostResolver() override;
  net::URLRequestContext* GetRequestContext() override;
  scoped_ptr<net::ClientCertStore> CreateClientCertStore() override;

  void set_url_request_context_getter(
      scoped_refptr<CefURLRequestContextGetter> getter);

  // State transferred from the BrowserContext for use on the IO thread.
  bool IsOffTheRecord() const { return is_off_the_record_; }
  const extensions::InfoMap* GetExtensionInfoMap() const {
    return extension_info_map_.get();
  }
  CefRefPtr<CefRequestContextHandler> GetHandler() const { return handler_; }

 private:
  scoped_refptr<CefURLRequestContextGetter> getter_;

  bool is_off_the_record_;
  scoped_refptr<extensions::InfoMap> extension_info_map_;
  CefRefPtr<CefRequestContextHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(CefResourceContext);
};

#endif  // CEF_LIBCEF_BROWSER_RESOURCE_CONTEXT_H_
