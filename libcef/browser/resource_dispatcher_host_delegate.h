// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"

// Implements ResourceDispatcherHostDelegate.
class CefResourceDispatcherHostDelegate
    : public content::ResourceDispatcherHostDelegate {
 public:
  CefResourceDispatcherHostDelegate();
  ~CefResourceDispatcherHostDelegate() override;

  // ResourceDispatcherHostDelegate methods.
  bool HandleExternalProtocol(
      const GURL& url,
      int child_id,
      const content::ResourceRequestInfo::WebContentsGetter&
          web_contents_getter,
      bool is_main_frame,
      ui::PageTransition page_transition,
      bool has_user_gesture) override;
  bool ShouldInterceptResourceAsStream(net::URLRequest* request,
                                       const base::FilePath& plugin_path,
                                       const std::string& mime_type,
                                       GURL* origin,
                                       std::string* payload) override;
  void OnStreamCreated(net::URLRequest* request,
                       scoped_ptr<content::StreamInfo> stream) override;
  void OnRequestRedirected(
      const GURL& redirect_url,
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::ResourceResponse* response) override;

 private:
  struct StreamTargetInfo {
    std::string extension_id;
    std::string view_id;
  };
  std::map<net::URLRequest*, StreamTargetInfo> stream_target_info_;

  DISALLOW_COPY_AND_ASSIGN(CefResourceDispatcherHostDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
