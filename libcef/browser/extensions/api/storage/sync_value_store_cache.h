// Copyright 2017 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "extensions/browser/api/storage/settings_storage_quota_enforcer.h"
#include "extensions/browser/api/storage/value_store_cache.h"

namespace value_store {
class ValueStoreFactory;
}

namespace extensions::cef {

// Based on LocalValueStoreCache
// ValueStoreCache for the SYNC namespace. It owns a backend for apps and
// another for extensions. Each backend takes care of persistence.
class SyncValueStoreCache : public ValueStoreCache {
 public:
  explicit SyncValueStoreCache(
      scoped_refptr<value_store::ValueStoreFactory> factory);

  SyncValueStoreCache(const SyncValueStoreCache&) = delete;
  SyncValueStoreCache& operator=(const SyncValueStoreCache&) = delete;

  ~SyncValueStoreCache() override;

  // ValueStoreCache implementation:
  void RunWithValueStoreForExtension(
      StorageCallback callback,
      scoped_refptr<const Extension> extension) override;
  void DeleteStorageSoon(const std::string& extension_id) override;

 private:
  using StorageMap =
      std::map<std::string, std::unique_ptr<value_store::ValueStore>>;

  value_store::ValueStore* GetStorage(const Extension* extension);

  // The Factory to use for creating new ValueStores.
  const scoped_refptr<value_store::ValueStoreFactory> storage_factory_;

  // Quota limits (see SettingsStorageQuotaEnforcer).
  const SettingsStorageQuotaEnforcer::Limits quota_;

  // The collection of ValueStores for local storage.
  StorageMap storage_map_;
};

}  // namespace extensions::cef

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_
