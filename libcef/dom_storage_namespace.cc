// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "libcef/dom_storage_namespace.h"
#include "libcef/dom_storage_area.h"
#include "libcef/dom_storage_context.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageArea.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageNamespace.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebStorageArea;
using WebKit::WebStorageNamespace;
using WebKit::WebString;

/* static */
DOMStorageNamespace* DOMStorageNamespace::CreateLocalStorageNamespace(
    DOMStorageContext* dom_storage_context, const FilePath& data_dir_path) {
  int64 id = kLocalStorageNamespaceId;
  DCHECK(!dom_storage_context->GetStorageNamespace(id, false));
  WebString path;
  if (!data_dir_path.empty())
    path = webkit_glue::FilePathToWebString(data_dir_path);
  return new DOMStorageNamespace(dom_storage_context, id, path,
                                 DOM_STORAGE_LOCAL);
}

/* static */
DOMStorageNamespace* DOMStorageNamespace::CreateSessionStorageNamespace(
    DOMStorageContext* dom_storage_context, int64 id) {
  DCHECK(!dom_storage_context->GetStorageNamespace(id, false));
  return new DOMStorageNamespace(dom_storage_context, id, WebString(),
                                 DOM_STORAGE_SESSION);
}

DOMStorageNamespace::DOMStorageNamespace(DOMStorageContext* dom_storage_context,
                                         int64 id,
                                         const WebString& data_dir_path,
                                         DOMStorageType dom_storage_type)
    : dom_storage_context_(dom_storage_context),
      id_(id),
      data_dir_path_(data_dir_path),
      dom_storage_type_(dom_storage_type) {
  DCHECK(dom_storage_context_);
}

DOMStorageNamespace::~DOMStorageNamespace() {
  // TODO(jorlow): If the DOMStorageContext is being destructed, there's no need
  //               to do these calls.  Maybe we should add a fast path?
  for (OriginToStorageAreaMap::iterator iter(origin_to_storage_area_.begin());
       iter != origin_to_storage_area_.end(); ++iter) {
    dom_storage_context_->UnregisterStorageArea(iter->second);
    delete iter->second;
  }
}

DOMStorageArea* DOMStorageNamespace::GetStorageArea(
    const string16& origin, bool allocation_allowed) {
  // We may have already created it for another dispatcher host.
  OriginToStorageAreaMap::iterator iter = origin_to_storage_area_.find(origin);
  if (iter != origin_to_storage_area_.end())
    return iter->second;

  if (!allocation_allowed)
    return NULL;

  // We need to create a new one.
  int64 id = dom_storage_context_->AllocateStorageAreaId();
  DCHECK(!dom_storage_context_->GetStorageArea(id));
  DOMStorageArea* storage_area = new DOMStorageArea(origin, id, this);
  origin_to_storage_area_[origin] = storage_area;
  dom_storage_context_->RegisterStorageArea(storage_area);
  return storage_area;
}

DOMStorageNamespace* DOMStorageNamespace::Copy(int64 id) {
  DCHECK(dom_storage_type_ == DOM_STORAGE_SESSION);
  DCHECK(!dom_storage_context_->GetStorageNamespace(id, false));
  DOMStorageNamespace* new_storage_namespace = new DOMStorageNamespace(
      dom_storage_context_, id, data_dir_path_, dom_storage_type_);
  // If we haven't used the namespace yet, there's nothing to copy.
  if (storage_namespace_.get())
    new_storage_namespace->storage_namespace_.reset(storage_namespace_->copy());
  return new_storage_namespace;
}

void DOMStorageNamespace::GetStorageAreas(std::vector<DOMStorageArea*>& areas,
    bool skip_empty) const {
  OriginToStorageAreaMap::const_iterator iter = origin_to_storage_area_.begin();
  for (;  iter != origin_to_storage_area_.end(); ++iter) {
    if (!skip_empty || iter->second->Length() > 0)
      areas.push_back(iter->second);
  }
}

void DOMStorageNamespace::PurgeMemory() {
  for (OriginToStorageAreaMap::iterator iter(origin_to_storage_area_.begin());
       iter != origin_to_storage_area_.end(); ++iter)
    iter->second->PurgeMemory();
  storage_namespace_.reset();
}

WebStorageArea* DOMStorageNamespace::CreateWebStorageArea(
    const string16& origin) {
  CreateWebStorageNamespaceIfNecessary();
  return storage_namespace_->createStorageArea(origin);
}

void DOMStorageNamespace::CreateWebStorageNamespaceIfNecessary() {
  if (storage_namespace_.get())
    return;

  if (dom_storage_type_ == DOM_STORAGE_LOCAL) {
    storage_namespace_.reset(
        WebStorageNamespace::createLocalStorageNamespace(data_dir_path_,
            DOMStorageContext::local_storage_quota()));
  } else {
    storage_namespace_.reset(WebStorageNamespace::createSessionStorageNamespace(
        DOMStorageContext::session_storage_quota()));
  }
}
