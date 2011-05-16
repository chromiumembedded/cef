// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dom_storage_area.h"
#include "dom_storage_context.h"
#include "dom_storage_namespace.h"

#include "base/task.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageArea.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebSecurityOrigin;
using WebKit::WebStorageArea;
using WebKit::WebString;
using WebKit::WebURL;

DOMStorageArea::DOMStorageArea(
    const string16& origin,
    int64 id,
    DOMStorageNamespace* owner)
    : origin_(origin),
      origin_url_(origin),
      id_(id),
      owner_(owner) {
  DCHECK(owner_);
}

DOMStorageArea::~DOMStorageArea() {
}

unsigned DOMStorageArea::Length() {
  CreateWebStorageAreaIfNecessary();
  return storage_area_->length();
}

NullableString16 DOMStorageArea::Key(unsigned index) {
  CreateWebStorageAreaIfNecessary();
  return storage_area_->key(index);
}

NullableString16 DOMStorageArea::GetItem(const string16& key) {
  CreateWebStorageAreaIfNecessary();
  return storage_area_->getItem(key);
}

NullableString16 DOMStorageArea::SetItem(
    const string16& key, const string16& value,
    WebStorageArea::Result* result) {
  CreateWebStorageAreaIfNecessary();
  WebString old_value;
  storage_area_->setItem(key, value, WebURL(), *result, old_value);
  return old_value;
}

NullableString16 DOMStorageArea::RemoveItem(const string16& key) {
  CreateWebStorageAreaIfNecessary();
  WebString old_value;
  storage_area_->removeItem(key, WebURL(), old_value);
  return old_value;
}

bool DOMStorageArea::Clear() {
  CreateWebStorageAreaIfNecessary();
  bool somethingCleared;
  storage_area_->clear(WebURL(), somethingCleared);
  return somethingCleared;
}

void DOMStorageArea::PurgeMemory() {
  storage_area_.reset();
}

void DOMStorageArea::CreateWebStorageAreaIfNecessary() {
  if (!storage_area_.get())
    storage_area_.reset(owner_->CreateWebStorageArea(origin_));
}
