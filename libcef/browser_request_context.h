// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_REQUEST_CONTEXT_H
#define _BROWSER_REQUEST_CONTEXT_H

#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"

class FilePath;
namespace webkit_blob {
class BlobStorageController;
}

// A basic URLRequestContext that only provides an in-memory cookie store.
class BrowserRequestContext : public URLRequestContext {
 public:
  // Use an in-memory cache
  BrowserRequestContext();
  ~BrowserRequestContext();

  // Use an on-disk cache at the specified location.  Optionally, use the cache
  // in playback or record mode.
  BrowserRequestContext(const FilePath& cache_path,
                        net::HttpCache::Mode cache_mode,
                        bool no_proxy);

  virtual const std::string& GetUserAgent(const GURL& url) const;

  void SetAcceptAllCookies(bool accept_all_cookies);

  webkit_blob::BlobStorageController* blob_storage_controller() const {
    return blob_storage_controller_.get();
  }

 private:
  void Init(const FilePath& cache_path, net::HttpCache::Mode cache_mode,
            bool no_proxy);

  scoped_ptr<webkit_blob::BlobStorageController> blob_storage_controller_;
};

#endif  // _BROWSER_REQUEST_CONTEXT_H

