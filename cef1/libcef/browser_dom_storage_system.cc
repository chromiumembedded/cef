// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_dom_storage_system.h"
#include "libcef/cef_context.h"
#include "libcef/cef_thread.h"

#include "base/auto_reset.h"
#include "base/path_service.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageArea.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageEventDispatcher.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageNamespace.h"
#include "webkit/database/database_util.h"
#include "webkit/dom_storage/dom_storage_area.h"
#include "webkit/dom_storage/dom_storage_host.h"
#include "webkit/dom_storage/dom_storage_task_runner.h"

using dom_storage::DomStorageContext;
using dom_storage::DomStorageHost;
using dom_storage::DomStorageSession;
using dom_storage::DomStorageWorkerPoolTaskRunner;
using webkit_database::DatabaseUtil;
using WebKit::WebStorageArea;
using WebKit::WebStorageNamespace;
using WebKit::WebStorageEventDispatcher;
using WebKit::WebString;
using WebKit::WebURL;

namespace {
const int kInvalidNamespaceId = -1;
}

class BrowserDomStorageSystem::NamespaceImpl : public WebStorageNamespace {
 public:
  explicit NamespaceImpl(const base::WeakPtr<BrowserDomStorageSystem>& parent);
  NamespaceImpl(const base::WeakPtr<BrowserDomStorageSystem>& parent,
                int session_namespace_id);
  virtual ~NamespaceImpl();
  virtual WebStorageArea* createStorageArea(const WebString& origin) OVERRIDE;
  virtual WebStorageNamespace* copy() OVERRIDE;
  virtual bool isSameNamespace(const WebStorageNamespace&) const OVERRIDE;

 private:
  DomStorageContext* Context() {
    if (!parent_.get())
      return NULL;
    return parent_->context_.get();
  }

  base::WeakPtr<BrowserDomStorageSystem> parent_;
  int namespace_id_;
};

class BrowserDomStorageSystem::AreaImpl : public WebStorageArea {
 public:
  AreaImpl(const base::WeakPtr<BrowserDomStorageSystem>& parent,
          int namespace_id, const GURL& origin);
  virtual ~AreaImpl();
  virtual unsigned length() OVERRIDE;
  virtual WebString key(unsigned index) OVERRIDE;
  virtual WebString getItem(const WebString& key) OVERRIDE;
  virtual void setItem(const WebString& key, const WebString& newValue,
                       const WebURL& pageUrl, Result&) OVERRIDE;
  virtual void removeItem(const WebString& key,
                          const WebURL& pageUrl) OVERRIDE;
  virtual void clear(const WebURL& pageUrl) OVERRIDE;

 private:
  DomStorageHost* Host() {
    if (!parent_.get())
      return NULL;
    return parent_->host_.get();
  }

  base::WeakPtr<BrowserDomStorageSystem> parent_;
  int connection_id_;
};

// NamespaceImpl -----------------------------

BrowserDomStorageSystem::NamespaceImpl::NamespaceImpl(
    const base::WeakPtr<BrowserDomStorageSystem>& parent)
    : parent_(parent),
      namespace_id_(dom_storage::kLocalStorageNamespaceId) {
}

BrowserDomStorageSystem::NamespaceImpl::NamespaceImpl(
    const base::WeakPtr<BrowserDomStorageSystem>& parent,
    int session_namespace_id)
    : parent_(parent),
      namespace_id_(session_namespace_id) {
}

BrowserDomStorageSystem::NamespaceImpl::~NamespaceImpl() {
  if (namespace_id_ == dom_storage::kLocalStorageNamespaceId ||
      namespace_id_ == kInvalidNamespaceId || !Context()) {
    return;
  }
  Context()->DeleteSessionNamespace(namespace_id_, false);
}

WebStorageArea* BrowserDomStorageSystem::NamespaceImpl::createStorageArea(
    const WebString& origin) {
  return new AreaImpl(parent_, namespace_id_, GURL(origin));
}

WebStorageNamespace* BrowserDomStorageSystem::NamespaceImpl::copy() {
  DCHECK_NE(dom_storage::kLocalStorageNamespaceId, namespace_id_);
  int new_id = kInvalidNamespaceId;
  if (Context()) {
    new_id = Context()->AllocateSessionId();
    Context()->CloneSessionNamespace(namespace_id_, new_id, std::string());
  }
  return new NamespaceImpl(parent_, new_id);
}

bool BrowserDomStorageSystem::NamespaceImpl::isSameNamespace(
    const WebStorageNamespace& other) const {
  const NamespaceImpl* other_impl = static_cast<const NamespaceImpl*>(&other);
  return namespace_id_ == other_impl->namespace_id_;
}

// AreaImpl -----------------------------

BrowserDomStorageSystem::AreaImpl::AreaImpl(
    const base::WeakPtr<BrowserDomStorageSystem>& parent,
    int namespace_id, const GURL& origin)
    : parent_(parent),
      connection_id_(0) {
  if (Host()) {
    connection_id_ = (parent_->next_connection_id_)++;
    Host()->OpenStorageArea(connection_id_, namespace_id, origin);
  }
}

BrowserDomStorageSystem::AreaImpl::~AreaImpl() {
  if (Host())
    Host()->CloseStorageArea(connection_id_);
}

unsigned BrowserDomStorageSystem::AreaImpl::length() {
  if (Host())
    return Host()->GetAreaLength(connection_id_);
  return 0;
}

WebString BrowserDomStorageSystem::AreaImpl::key(unsigned index) {
  if (Host())
    return Host()->GetAreaKey(connection_id_, index);
  return NullableString16(true);
}

WebString BrowserDomStorageSystem::AreaImpl::getItem(const WebString& key) {
  if (Host())
    return Host()->GetAreaItem(connection_id_, key);
  return NullableString16(true);
}

void BrowserDomStorageSystem::AreaImpl::setItem(
    const WebString& key, const WebString& newValue,
    const WebURL& pageUrl, Result& result) {
  result = ResultBlockedByQuota;
  if (!Host())
    return;

  AutoReset<AreaImpl*> auto_reset(&parent_->area_being_processed_, this);
  NullableString16 unused;
  if (!Host()->SetAreaItem(connection_id_, key, newValue, pageUrl,
                           &unused))
    return;

  result = ResultOK;
}

void BrowserDomStorageSystem::AreaImpl::removeItem(
    const WebString& key, const WebURL& pageUrl) {
  if (!Host())
    return;

  AutoReset<AreaImpl*> auto_reset(&parent_->area_being_processed_, this);
  string16 notused;
  Host()->RemoveAreaItem(connection_id_, key, pageUrl, &notused);
}

void BrowserDomStorageSystem::AreaImpl::clear(const WebURL& pageUrl) {
  if (!Host())
    return;

  AutoReset<AreaImpl*> auto_reset(&parent_->area_being_processed_, this);
  Host()->ClearArea(connection_id_, pageUrl);
}

// BrowserDomStorageSystem -----------------------------

BrowserDomStorageSystem* BrowserDomStorageSystem::g_instance_;

BrowserDomStorageSystem::BrowserDomStorageSystem()
    : weak_factory_(this),
      area_being_processed_(NULL),
      next_connection_id_(1) {
  FilePath local_storage_path;
  FilePath cache_path(_Context->cache_path());
  if (!cache_path.empty()) {
    local_storage_path = cache_path.Append(FILE_PATH_LITERAL("Local Storage"));
    if (!file_util::PathExists(local_storage_path) &&
        !file_util::CreateDirectory(local_storage_path)) {
      LOG(WARNING) << "Failed to create Local Storage directory";
      local_storage_path.clear();
    }
  }

  base::SequencedWorkerPool* worker_pool = _Context->blocking_pool();

  context_ = new DomStorageContext(local_storage_path, FilePath(), NULL,
      new DomStorageWorkerPoolTaskRunner(
          worker_pool,
          worker_pool->GetNamedSequenceToken("dom_storage_primary"),
          worker_pool->GetNamedSequenceToken("dom_storage_commit"),
          CefThread::GetMessageLoopProxyForThread(CefThread::FILE)));

  host_.reset(new DomStorageHost(context_));

  DCHECK(!g_instance_);
  g_instance_ = this;
  context_->AddEventObserver(this);
}

BrowserDomStorageSystem::~BrowserDomStorageSystem() {
  g_instance_ = NULL;
  host_.reset();
  context_->RemoveEventObserver(this);
}

WebStorageNamespace* BrowserDomStorageSystem::CreateLocalStorageNamespace() {
  return new NamespaceImpl(weak_factory_.GetWeakPtr());
}

WebStorageNamespace* BrowserDomStorageSystem::CreateSessionStorageNamespace() {
  int id = context_->AllocateSessionId();
  context_->CreateSessionNamespace(id, std::string());
  return new NamespaceImpl(weak_factory_.GetWeakPtr(), id);
}

void BrowserDomStorageSystem::OnDomStorageItemSet(
    const dom_storage::DomStorageArea* area,
    const string16& key,
    const string16& new_value,
    const NullableString16& old_value,
    const GURL& page_url) {
  DispatchDomStorageEvent(area, page_url,
                          NullableString16(key, false),
                          NullableString16(new_value, false),
                          old_value);
}

void BrowserDomStorageSystem::OnDomStorageItemRemoved(
    const dom_storage::DomStorageArea* area,
    const string16& key,
    const string16& old_value,
    const GURL& page_url) {
  DispatchDomStorageEvent(area, page_url,
                          NullableString16(key, false),
                          NullableString16(true),
                          NullableString16(old_value, false));
}

void BrowserDomStorageSystem::OnDomStorageAreaCleared(
    const dom_storage::DomStorageArea* area,
    const GURL& page_url) {
  DispatchDomStorageEvent(area, page_url,
                          NullableString16(true),
                          NullableString16(true),
                          NullableString16(true));
}

void BrowserDomStorageSystem::DispatchDomStorageEvent(
    const dom_storage::DomStorageArea* area,
    const GURL& page_url,
    const NullableString16& key,
    const NullableString16& new_value,
    const NullableString16& old_value) {
  DCHECK(area_being_processed_);
  if (area->namespace_id() == dom_storage::kLocalStorageNamespaceId) {
    WebStorageEventDispatcher::dispatchLocalStorageEvent(
        key,
        old_value,
        new_value,
        area->origin(),
        page_url,
        area_being_processed_,
        true  /* originatedInProcess */);
  } else {
    NamespaceImpl session_namespace_for_event_dispatch(
        base::WeakPtr<BrowserDomStorageSystem>(), area->namespace_id());
    WebStorageEventDispatcher::dispatchSessionStorageEvent(
        key,
        old_value,
        new_value,
        area->origin(),
        page_url,
        session_namespace_for_event_dispatch,
        area_being_processed_,
        true  /* originatedInProcess */);
  }
}
