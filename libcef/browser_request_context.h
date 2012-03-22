// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_REQUEST_CONTEXT_H
#define _BROWSER_REQUEST_CONTEXT_H

#include "net/http/http_cache.h"
#include "net/http/url_security_manager.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"

class FilePath;

namespace webkit_blob {
class BlobStorageController;
}

// A basic URLRequestContext that only provides an in-memory cookie store.
class BrowserRequestContext : public net::URLRequestContext {
 public:
  // Use an in-memory cache
  BrowserRequestContext();
  virtual ~BrowserRequestContext();

  // Use an on-disk cache at the specified location.  Optionally, use the cache
  // in playback or record mode.
  BrowserRequestContext(const FilePath& cache_path,
                        net::HttpCache::Mode cache_mode,
                        bool no_proxy);

  virtual const std::string& GetUserAgent(const GURL& url) const OVERRIDE;

  void SetAcceptAllCookies(bool accept_all_cookies);
  bool AcceptAllCookies();

  // Set the path used for cookie storage. If |path| is empty memory only
  // storage will be used. If the old cookie data is being stored on disk it
  // will be flushed and closed.
  void SetCookieStoragePath(const FilePath& path);

  webkit_blob::BlobStorageController* blob_storage_controller() const {
    return blob_storage_controller_.get();
  }

 private:
  void Init(const FilePath& cache_path, net::HttpCache::Mode cache_mode,
            bool no_proxy);

  net::URLRequestContextStorage storage_;
  scoped_ptr<webkit_blob::BlobStorageController> blob_storage_controller_;
  scoped_ptr<net::URLSecurityManager> url_security_manager_;
  scoped_ptr<net::URLRequest::Interceptor> url_request_interceptor_;
  FilePath cookie_store_path_;
  bool accept_all_cookies_;
};

#endif  // _BROWSER_REQUEST_CONTEXT_H

