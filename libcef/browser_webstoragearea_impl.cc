// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser_webstoragearea_impl.h"
#include "libcef/cef_context.h"
#include "libcef/dom_storage_area.h"
#include "libcef/dom_storage_namespace.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebView;

BrowserWebStorageAreaImpl::BrowserWebStorageAreaImpl(
    int64 namespace_id, const WebString& origin) {
  area_ =
      _Context->storage_context()->GetStorageArea(namespace_id, origin, true);
  DCHECK(area_ != NULL);
}

BrowserWebStorageAreaImpl::~BrowserWebStorageAreaImpl() {
}

unsigned BrowserWebStorageAreaImpl::length() {
  return area_->Length();
}

WebString BrowserWebStorageAreaImpl::key(unsigned index) {
  return area_->Key(index);
}

WebString BrowserWebStorageAreaImpl::getItem(const WebString& key) {
  return area_->GetItem(key);
}

void BrowserWebStorageAreaImpl::setItem(
    const WebString& key, const WebString& value, const WebURL& url,
    WebStorageArea::Result& result, WebString& old_value_webkit) {
  old_value_webkit = area_->SetItem(key, value, &result);
}

void BrowserWebStorageAreaImpl::removeItem(
    const WebString& key, const WebURL& url, WebString& old_value_webkit) {
  old_value_webkit = area_->RemoveItem(key);
}

void BrowserWebStorageAreaImpl::clear(
    const WebURL& url, bool& cleared_something) {
  cleared_something = area_->Clear();
}
