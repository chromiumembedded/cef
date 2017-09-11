// Copyright 2017 The Chromium Embedded Framework Authors.
// Portions copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/value_store/cef_value_store_factory.h"

#include "base/memory/ptr_util.h"
#include "extensions/browser/value_store/leveldb_value_store.h"
#include "libcef/browser/extensions/value_store/cef_value_store.h"

namespace {

const char kUMAClientName[] = "Cef";

}  // namespace

namespace extensions {

using SettingsNamespace = settings_namespace::Namespace;

CefValueStoreFactory::StorageHelper::StorageHelper() = default;

CefValueStoreFactory::StorageHelper::~StorageHelper() = default;

std::set<ExtensionId> CefValueStoreFactory::StorageHelper::GetKnownExtensionIDs(
    ModelType model_type) const {
  std::set<ExtensionId> ids;
  switch (model_type) {
    case ValueStoreFactory::ModelType::APP:
      for (const auto& key : app_stores_)
        ids.insert(key.first);
      break;
    case ValueStoreFactory::ModelType::EXTENSION:
      for (const auto& key : extension_stores_)
        ids.insert(key.first);
      break;
  }
  return ids;
}

void CefValueStoreFactory::StorageHelper::Reset() {
  app_stores_.clear();
  extension_stores_.clear();
}

ValueStore* CefValueStoreFactory::StorageHelper::AddValueStore(
    const ExtensionId& extension_id,
    ValueStore* value_store,
    ModelType model_type) {
  if (model_type == ValueStoreFactory::ModelType::APP) {
    DCHECK(app_stores_.find(extension_id) == app_stores_.end());
    app_stores_[extension_id] = value_store;
  } else {
    DCHECK(extension_stores_.find(extension_id) == extension_stores_.end());
    extension_stores_[extension_id] = value_store;
  }
  return value_store;
}

void CefValueStoreFactory::StorageHelper::DeleteSettings(
    const ExtensionId& extension_id,
    ModelType model_type) {
  switch (model_type) {
    case ValueStoreFactory::ModelType::APP:
      app_stores_.erase(extension_id);
      break;
    case ValueStoreFactory::ModelType::EXTENSION:
      extension_stores_.erase(extension_id);
      break;
  }
}

bool CefValueStoreFactory::StorageHelper::HasSettings(
    const ExtensionId& extension_id,
    ModelType model_type) const {
  switch (model_type) {
    case ValueStoreFactory::ModelType::APP:
      return app_stores_.find(extension_id) != app_stores_.end();
    case ValueStoreFactory::ModelType::EXTENSION:
      return extension_stores_.find(extension_id) != extension_stores_.end();
  }
  NOTREACHED();
  return false;
}

ValueStore* CefValueStoreFactory::StorageHelper::GetExisting(
    const ExtensionId& extension_id) const {
  auto it = app_stores_.find(extension_id);
  if (it != app_stores_.end())
    return it->second;
  it = extension_stores_.find(extension_id);
  if (it != extension_stores_.end())
    return it->second;
  return nullptr;
}

CefValueStoreFactory::CefValueStoreFactory() = default;

CefValueStoreFactory::CefValueStoreFactory(const base::FilePath& db_path)
    : db_path_(db_path) {}

CefValueStoreFactory::~CefValueStoreFactory() {}

std::unique_ptr<ValueStore> CefValueStoreFactory::CreateRulesStore() {
  if (db_path_.empty())
    last_created_store_ = new CefValueStore();
  else
    last_created_store_ = new LeveldbValueStore(kUMAClientName, db_path_);
  return base::WrapUnique(last_created_store_);
}

std::unique_ptr<ValueStore> CefValueStoreFactory::CreateStateStore() {
  return CreateRulesStore();
}

CefValueStoreFactory::StorageHelper& CefValueStoreFactory::GetStorageHelper(
    SettingsNamespace settings_namespace) {
  switch (settings_namespace) {
    case settings_namespace::LOCAL:
      return local_helper_;
    case settings_namespace::SYNC:
      return sync_helper_;
    case settings_namespace::MANAGED:
      return managed_helper_;
    case settings_namespace::INVALID:
      break;
  }
  NOTREACHED();
  return local_helper_;
}

std::unique_ptr<ValueStore> CefValueStoreFactory::CreateSettingsStore(
    SettingsNamespace settings_namespace,
    ModelType model_type,
    const ExtensionId& extension_id) {
  std::unique_ptr<ValueStore> settings_store(CreateRulesStore());
  // Note: This factory is purposely keeping the raw pointers to each ValueStore
  //       created. Tests using CefValueStoreFactory must be careful to keep
  //       those ValueStore's alive for the duration of their test.
  GetStorageHelper(settings_namespace)
      .AddValueStore(extension_id, settings_store.get(), model_type);
  return settings_store;
}

ValueStore* CefValueStoreFactory::LastCreatedStore() const {
  return last_created_store_;
}

void CefValueStoreFactory::DeleteSettings(SettingsNamespace settings_namespace,
                                          ModelType model_type,
                                          const ExtensionId& extension_id) {
  GetStorageHelper(settings_namespace).DeleteSettings(extension_id, model_type);
}

bool CefValueStoreFactory::HasSettings(SettingsNamespace settings_namespace,
                                       ModelType model_type,
                                       const ExtensionId& extension_id) {
  return GetStorageHelper(settings_namespace)
      .HasSettings(extension_id, model_type);
}

std::set<ExtensionId> CefValueStoreFactory::GetKnownExtensionIDs(
    SettingsNamespace settings_namespace,
    ModelType model_type) const {
  return const_cast<CefValueStoreFactory*>(this)
      ->GetStorageHelper(settings_namespace)
      .GetKnownExtensionIDs(model_type);
}

ValueStore* CefValueStoreFactory::GetExisting(
    const ExtensionId& extension_id) const {
  ValueStore* existing_store = local_helper_.GetExisting(extension_id);
  if (existing_store)
    return existing_store;
  existing_store = sync_helper_.GetExisting(extension_id);
  if (existing_store)
    return existing_store;
  existing_store = managed_helper_.GetExisting(extension_id);
  DCHECK(existing_store != nullptr);
  return existing_store;
}

void CefValueStoreFactory::Reset() {
  last_created_store_ = nullptr;
  local_helper_.Reset();
  sync_helper_.Reset();
  managed_helper_.Reset();
}

}  // namespace extensions
