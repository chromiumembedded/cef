// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_WEBBLOBREGISTRY_IMPL_H_
#define CEF_LIBCEF_BROWSER_WEBBLOBREGISTRY_IMPL_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebBlobRegistry.h"

class GURL;

namespace webkit_blob {
class BlobData;
class BlobStorageController;
}

class BrowserWebBlobRegistryImpl
    : public WebKit::WebBlobRegistry,
      public base::RefCountedThreadSafe<BrowserWebBlobRegistryImpl> {
 public:
  static void InitializeOnIOThread(
      webkit_blob::BlobStorageController* blob_storage_controller);
  static void Cleanup();

  BrowserWebBlobRegistryImpl();

  // See WebBlobRegistry.h for documentation on these functions.
  virtual void registerBlobURL(const WebKit::WebURL& url,
                               WebKit::WebBlobData& data);
  virtual void registerBlobURL(const WebKit::WebURL& url,
                               const WebKit::WebURL& src_url);
  virtual void unregisterBlobURL(const WebKit::WebURL& url);

 private:
  friend class base::RefCountedThreadSafe<BrowserWebBlobRegistryImpl>;

  // Run on I/O thread.
  void AddFinishedBlob(const GURL& url, webkit_blob::BlobData* blob_data);
  void CloneBlob(const GURL& url, const GURL& src_url);
  void RemoveBlob(const GURL& url);

  DISALLOW_COPY_AND_ASSIGN(BrowserWebBlobRegistryImpl);
};

#endif  // CEF_LIBCEF_BROWSER_WEBBLOBREGISTRY_IMPL_H_
