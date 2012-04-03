// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_webstoragenamespace_impl.h"
#include "libcef/browser_webstoragearea_impl.h"
#include "libcef/cef_context.h"

#include "base/logging.h"

using WebKit::WebStorageArea;
using WebKit::WebStorageNamespace;
using WebKit::WebString;

BrowserWebStorageNamespaceImpl::BrowserWebStorageNamespaceImpl(
    DOMStorageType storage_type)
    : storage_type_(storage_type),
      namespace_id_(kLocalStorageNamespaceId) {
}

BrowserWebStorageNamespaceImpl::BrowserWebStorageNamespaceImpl(
    DOMStorageType storage_type, int64 namespace_id)
    : storage_type_(storage_type),
      namespace_id_(namespace_id) {
}

BrowserWebStorageNamespaceImpl::~BrowserWebStorageNamespaceImpl() {
}

WebStorageArea* BrowserWebStorageNamespaceImpl::createStorageArea(
    const WebString& origin) {
  // Ideally, we'd keep a hash map of origin to these objects.  Unfortunately
  // this doesn't seem practical because there's no good way to ref-count these
  // objects, and it'd be unclear who owned them.  So, instead, we'll pay the
  // price in terms of wasted memory.
  return new BrowserWebStorageAreaImpl(namespace_id_, origin);
}

WebStorageNamespace* BrowserWebStorageNamespaceImpl::copy() {
  // By returning NULL, we're telling WebKit to lazily fetch it the next time
  // session storage is used.  In the WebViewClient::createView, we do the
  // book-keeping necessary to make it a true copy-on-write despite not doing
  // anything here, now.
  return NULL;
}

void BrowserWebStorageNamespaceImpl::close() {
  // This is called only on LocalStorage namespaces when WebKit thinks its
  // shutting down.  This has no impact on Chromium.
}
