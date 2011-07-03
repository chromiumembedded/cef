// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_REQUEST_CONTEXT_H
#define _BROWSER_REQUEST_CONTEXT_H

#include "net/http/http_cache.h"
#include "net/http/url_security_manager.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"

class FilePath;

namespace fileapi {
class FileSystemContext;
}

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

  virtual const std::string& GetUserAgent(const GURL& url) const;

  void SetAcceptAllCookies(bool accept_all_cookies);
  bool AcceptAllCookies();

  webkit_blob::BlobStorageController* blob_storage_controller() const {
    return blob_storage_controller_.get();
  }

  fileapi::FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

 private:
  void Init(const FilePath& cache_path, net::HttpCache::Mode cache_mode,
            bool no_proxy);

  net::URLRequestContextStorage storage_;
  scoped_ptr<webkit_blob::BlobStorageController> blob_storage_controller_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  scoped_ptr<net::URLSecurityManager> url_security_manager_;
  bool accept_all_cookies_;
};

#endif  // _BROWSER_REQUEST_CONTEXT_H

