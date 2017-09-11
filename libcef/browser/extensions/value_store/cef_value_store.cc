// Copyright 2017 The Chromium Embedded Framework Authors.
// Portions copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/value_store/cef_value_store.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

CefValueStore::CefValueStore() : read_count_(0), write_count_(0) {}

CefValueStore::~CefValueStore() {}

size_t CefValueStore::GetBytesInUse(const std::string& key) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED();
  return 0;
}

size_t CefValueStore::GetBytesInUse(const std::vector<std::string>& keys) {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED();
  return 0;
}

size_t CefValueStore::GetBytesInUse() {
  // Let SettingsStorageQuotaEnforcer implement this.
  NOTREACHED();
  return 0;
}

ValueStore::ReadResult CefValueStore::Get(const std::string& key) {
  return Get(std::vector<std::string>(1, key));
}

ValueStore::ReadResult CefValueStore::Get(
    const std::vector<std::string>& keys) {
  read_count_++;
  base::DictionaryValue* settings = new base::DictionaryValue();
  for (std::vector<std::string>::const_iterator it = keys.begin();
       it != keys.end(); ++it) {
    base::Value* value = NULL;
    if (storage_.GetWithoutPathExpansion(*it, &value)) {
      settings->SetWithoutPathExpansion(*it, value->CreateDeepCopy());
    }
  }
  return MakeReadResult(base::WrapUnique(settings), status_);
}

ValueStore::ReadResult CefValueStore::Get() {
  read_count_++;
  return MakeReadResult(storage_.CreateDeepCopy(), status_);
}

ValueStore::WriteResult CefValueStore::Set(WriteOptions options,
                                           const std::string& key,
                                           const base::Value& value) {
  base::DictionaryValue settings;
  settings.SetWithoutPathExpansion(key, value.CreateDeepCopy());
  return Set(options, settings);
}

ValueStore::WriteResult CefValueStore::Set(
    WriteOptions options,
    const base::DictionaryValue& settings) {
  write_count_++;
  std::unique_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());
  for (base::DictionaryValue::Iterator it(settings); !it.IsAtEnd();
       it.Advance()) {
    base::Value* old_value = NULL;
    if (!storage_.GetWithoutPathExpansion(it.key(), &old_value) ||
        !old_value->Equals(&it.value())) {
      changes->push_back(ValueStoreChange(
          it.key(), old_value ? old_value->CreateDeepCopy() : nullptr,
          it.value().CreateDeepCopy()));
      storage_.SetWithoutPathExpansion(it.key(), it.value().CreateDeepCopy());
    }
  }
  return MakeWriteResult(std::move(changes), status_);
}

ValueStore::WriteResult CefValueStore::Remove(const std::string& key) {
  return Remove(std::vector<std::string>(1, key));
}

ValueStore::WriteResult CefValueStore::Remove(
    const std::vector<std::string>& keys) {
  write_count_++;
  std::unique_ptr<ValueStoreChangeList> changes(new ValueStoreChangeList());
  for (std::vector<std::string>::const_iterator it = keys.begin();
       it != keys.end(); ++it) {
    std::unique_ptr<base::Value> old_value;
    if (storage_.RemoveWithoutPathExpansion(*it, &old_value)) {
      changes->push_back(ValueStoreChange(*it, std::move(old_value), nullptr));
    }
  }
  return MakeWriteResult(std::move(changes), status_);
}

ValueStore::WriteResult CefValueStore::Clear() {
  std::vector<std::string> keys;
  for (base::DictionaryValue::Iterator it(storage_); !it.IsAtEnd();
       it.Advance()) {
    keys.push_back(it.key());
  }
  return Remove(keys);
}
