// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_WEBSTORAGENAMESPACE_IMPL_H
#define _BROWSER_WEBSTORAGENAMESPACE_IMPL_H

#include "dom_storage_common.h"

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageNamespace.h"

class BrowserWebStorageNamespaceImpl : public WebKit::WebStorageNamespace {
 public:
  explicit BrowserWebStorageNamespaceImpl(DOMStorageType storage_type);
  BrowserWebStorageNamespaceImpl(DOMStorageType storage_type,
                                 int64 namespace_id);

  // See WebStorageNamespace.h for documentation on these functions.
  virtual ~BrowserWebStorageNamespaceImpl();
  virtual WebKit::WebStorageArea* createStorageArea(
      const WebKit::WebString& origin);
  virtual WebKit::WebStorageNamespace* copy();
  virtual void close();

  // Returns true if storage data is being cached to disk.
  static bool IsStorageActive();

 private:
  // Used during lazy initialization of namespace_id_.
  const DOMStorageType storage_type_;

  // Our namespace ID.
  int64 namespace_id_;
};

#endif  // _BROWSER_WEBSTORAGENAMESPACE_IMPL_H
