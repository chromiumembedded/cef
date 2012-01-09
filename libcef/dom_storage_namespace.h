// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DOM_STORAGE_NAMESPACE_H_
#define CEF_LIBCEF_DOM_STORAGE_NAMESPACE_H_
#pragma once

#include "libcef/dom_storage_common.h"

#include <vector>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

class DOMStorageArea;
class DOMStorageContext;
class FilePath;

namespace WebKit {
class WebStorageArea;
class WebStorageNamespace;
}

// Only to be used on the WebKit thread.
class DOMStorageNamespace {
 public:
  static DOMStorageNamespace* CreateLocalStorageNamespace(
      DOMStorageContext* dom_storage_context, const FilePath& data_dir_path);
  static DOMStorageNamespace* CreateSessionStorageNamespace(
      DOMStorageContext* dom_storage_context, int64 namespace_id);

  ~DOMStorageNamespace();

  DOMStorageArea* GetStorageArea(const string16& origin,
      bool allocation_allowed);
  DOMStorageNamespace* Copy(int64 clone_namespace_id);

  void GetStorageAreas(std::vector<DOMStorageArea*>& areas,
      bool skip_empty) const;

  void PurgeMemory();

  const DOMStorageContext* dom_storage_context() const {
    return dom_storage_context_;
  }
  int64 id() const { return id_; }
  const WebKit::WebString& data_dir_path() const { return data_dir_path_; }
  DOMStorageType dom_storage_type() const { return dom_storage_type_; }

  // Creates a WebStorageArea for the given origin.  This should only be called
  // by an owned DOMStorageArea.
  WebKit::WebStorageArea* CreateWebStorageArea(const string16& origin);

 private:
  // Called by the static factory methods above.
  DOMStorageNamespace(DOMStorageContext* dom_storage_context,
                      int64 id,
                      const WebKit::WebString& data_dir_path,
                      DOMStorageType storage_type);

  // Creates the underlying WebStorageNamespace on demand.
  void CreateWebStorageNamespaceIfNecessary();

  // All the storage areas we own.
  typedef base::hash_map<string16,  // NOLINT(build/include_what_you_use)
      DOMStorageArea*> OriginToStorageAreaMap;
  OriginToStorageAreaMap origin_to_storage_area_;

  // The DOMStorageContext that owns us.
  DOMStorageContext* dom_storage_context_;

  // The WebKit storage namespace we manage.
  scoped_ptr<WebKit::WebStorageNamespace> storage_namespace_;

  // Our id.  Unique to our parent context class.
  int64 id_;

  // The path used to create us, so we can recreate our WebStorageNamespace on
  // demand.
  WebKit::WebString data_dir_path_;

  // SessionStorage vs. LocalStorage.
  const DOMStorageType dom_storage_type_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageNamespace);
};

#endif  // CEF_LIBCEF_DOM_STORAGE_NAMESPACE_H_
