// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DOM_STORAGE_SYSTEM_H_
#define CEF_LIBCEF_BROWSER_DOM_STORAGE_SYSTEM_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "webkit/dom_storage/dom_storage_context.h"

namespace dom_storage {
class DomStorageHost;
}
namespace WebKit {
class WebStorageNamespace;
}

// Class that composes dom_storage classes together for use
// in simple single process environments like test_shell and DRT.
class BrowserDomStorageSystem
    : public dom_storage::DomStorageContext::EventObserver {
 public:
  static BrowserDomStorageSystem& instance() { return *g_instance_; }

  BrowserDomStorageSystem();
  virtual ~BrowserDomStorageSystem();

  // The Create<<>> calls are bound to WebKit api that the embedder
  // is responsible for implementing. These factories are called strictly
  // on the 'main' webkit thread. Ditto the methods on the returned
  // objects. SimplDomStorageSystem manufactures implementations of the
  // WebStorageNamespace and WebStorageArea interfaces that ultimately
  // plumb Get, Set, Remove, and Clear javascript calls to the dom_storage
  // classes. The caller (webkit/webcore) takes ownership of the returned
  // instances and will delete them when done.
  WebKit::WebStorageNamespace* CreateLocalStorageNamespace();
  WebKit::WebStorageNamespace* CreateSessionStorageNamespace();

 private:
  // Inner classes that implement the WebKit WebStorageNamespace and
  // WebStorageArea interfaces in terms of dom_storage classes.
  class NamespaceImpl;
  class AreaImpl;

  // DomStorageContext::EventObserver implementation which
  // calls into webkit/webcore to dispatch events.
  virtual void OnDomStorageItemSet(
      const dom_storage::DomStorageArea* area,
      const string16& key,
      const string16& new_value,
      const NullableString16& old_value,
      const GURL& page_url) OVERRIDE;
  virtual void OnDomStorageItemRemoved(
      const dom_storage::DomStorageArea* area,
      const string16& key,
      const string16& old_value,
      const GURL& page_url) OVERRIDE;
  virtual void OnDomStorageAreaCleared(
      const dom_storage::DomStorageArea* area,
      const GURL& page_url) OVERRIDE;

  void DispatchDomStorageEvent(
      const dom_storage::DomStorageArea* area,
      const GURL& page_url,
      const NullableString16& key,
      const NullableString16& new_value,
      const NullableString16& old_value);

  base::WeakPtrFactory<BrowserDomStorageSystem> weak_factory_;
  scoped_refptr<dom_storage::DomStorageContext> context_;
  scoped_ptr<dom_storage::DomStorageHost> host_;
  AreaImpl* area_being_processed_;
  int next_connection_id_;

  static BrowserDomStorageSystem* g_instance_;
};

#endif  // CEF_LIBCEF_BROWSER_DOM_STORAGE_SYSTEM_H_
