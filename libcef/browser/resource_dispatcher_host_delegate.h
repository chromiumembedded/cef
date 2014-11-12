// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"

// Implements ResourceDispatcherHostDelegate.
class CefResourceDispatcherHostDelegate
    : public content::ResourceDispatcherHostDelegate {
 public:
  CefResourceDispatcherHostDelegate();
  ~CefResourceDispatcherHostDelegate() override;

  // ResourceDispatcherHostDelegate methods.
  void RequestBeginning(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::AppCacheService* appcache_service,
      content::ResourceType resource_type,
      ScopedVector<content::ResourceThrottle>* throttles) override;
  bool HandleExternalProtocol(const GURL& url,
                              int child_id,
                              int route_id) override;
  void OnRequestRedirected(
      const GURL& redirect_url,
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::ResourceResponse* response) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CefResourceDispatcherHostDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
