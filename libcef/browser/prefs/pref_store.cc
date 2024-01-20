// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/prefs/pref_store.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/values.h"

CefPrefStore::CefPrefStore() = default;

bool CefPrefStore::GetValue(base::StringPiece key,
                            const base::Value** value) const {
  return prefs_.GetValue(key, value);
}

base::Value::Dict CefPrefStore::GetValues() const {
  return prefs_.AsDict();
}

bool CefPrefStore::GetMutableValue(const std::string& key,
                                   base::Value** value) {
  return prefs_.GetValue(key, value);
}

void CefPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void CefPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool CefPrefStore::HasObservers() const {
  return !observers_.empty();
}

bool CefPrefStore::IsInitializationComplete() const {
  return init_complete_;
}

void CefPrefStore::SetValue(const std::string& key,
                            base::Value value,
                            uint32_t flags) {
  if (prefs_.SetValue(key, std::move(value))) {
    committed_ = false;
    NotifyPrefValueChanged(key);
  }
}

void CefPrefStore::SetValueSilently(const std::string& key,
                                    base::Value value,
                                    uint32_t flags) {
  if (prefs_.SetValue(key, std::move(value))) {
    committed_ = false;
  }
}

void CefPrefStore::RemoveValuesByPrefixSilently(const std::string& prefix) {
  prefs_.ClearWithPrefix(prefix);
  committed_ = false;
}

void CefPrefStore::RemoveValue(const std::string& key, uint32_t flags) {
  if (prefs_.RemoveValue(key)) {
    committed_ = false;
    NotifyPrefValueChanged(key);
  }
}

bool CefPrefStore::ReadOnly() const {
  return read_only_;
}

PersistentPrefStore::PrefReadError CefPrefStore::GetReadError() const {
  return read_error_;
}

PersistentPrefStore::PrefReadError CefPrefStore::ReadPrefs() {
  NotifyInitializationCompleted();
  return read_error_;
}

void CefPrefStore::ReadPrefsAsync(ReadErrorDelegate* error_delegate) {
  DCHECK(!pending_async_read_);
  error_delegate_.reset(error_delegate);
  if (block_async_read_) {
    pending_async_read_ = true;
  } else {
    NotifyInitializationCompleted();
  }
}

void CefPrefStore::CommitPendingWrite(
    base::OnceClosure done_callback,
    base::OnceClosure synchronous_done_callback) {
  committed_ = true;
  PersistentPrefStore::CommitPendingWrite(std::move(done_callback),
                                          std::move(synchronous_done_callback));
}

void CefPrefStore::SchedulePendingLossyWrites() {}

void CefPrefStore::OnStoreDeletionFromDisk() {}

void CefPrefStore::SetInitializationCompleted() {
  NotifyInitializationCompleted();
}

void CefPrefStore::NotifyPrefValueChanged(const std::string& key) {
  for (Observer& observer : observers_) {
    observer.OnPrefValueChanged(key);
  }
}

void CefPrefStore::NotifyInitializationCompleted() {
  DCHECK(!init_complete_);
  init_complete_ = true;
  if (read_success_ && read_error_ != PREF_READ_ERROR_NONE && error_delegate_) {
    error_delegate_->OnError(read_error_);
  }
  for (Observer& observer : observers_) {
    observer.OnInitializationCompleted(read_success_);
  }
}

void CefPrefStore::ReportValueChanged(const std::string& key, uint32_t flags) {
  for (Observer& observer : observers_) {
    observer.OnPrefValueChanged(key);
  }
}

void CefPrefStore::SetString(const std::string& key, const std::string& value) {
  SetValue(key, base::Value(value), DEFAULT_PREF_WRITE_FLAGS);
}

void CefPrefStore::SetInteger(const std::string& key, int value) {
  SetValue(key, base::Value(value), DEFAULT_PREF_WRITE_FLAGS);
}

void CefPrefStore::SetBoolean(const std::string& key, bool value) {
  SetValue(key, base::Value(value), DEFAULT_PREF_WRITE_FLAGS);
}

bool CefPrefStore::GetString(const std::string& key, std::string* value) const {
  const base::Value* stored_value;
  if (!prefs_.GetValue(key, &stored_value) || !stored_value) {
    return false;
  }

  if (value && stored_value->is_string()) {
    *value = stored_value->GetString();
    return true;
  }
  return stored_value->is_string();
}

bool CefPrefStore::GetInteger(const std::string& key, int* value) const {
  const base::Value* stored_value;
  if (!prefs_.GetValue(key, &stored_value) || !stored_value) {
    return false;
  }

  if (value && stored_value->is_int()) {
    *value = stored_value->GetInt();
    return true;
  }
  return stored_value->is_int();
}

bool CefPrefStore::GetBoolean(const std::string& key, bool* value) const {
  const base::Value* stored_value;
  if (!prefs_.GetValue(key, &stored_value) || !stored_value) {
    return false;
  }

  if (value && stored_value->is_bool()) {
    *value = stored_value->GetBool();
    return true;
  }
  return stored_value->is_bool();
}

void CefPrefStore::SetBlockAsyncRead(bool block_async_read) {
  DCHECK(!init_complete_);
  block_async_read_ = block_async_read;
  if (pending_async_read_ && !block_async_read_) {
    NotifyInitializationCompleted();
  }
}

void CefPrefStore::set_read_only(bool read_only) {
  read_only_ = read_only;
}

void CefPrefStore::set_read_success(bool read_success) {
  DCHECK(!init_complete_);
  read_success_ = read_success;
}

void CefPrefStore::set_read_error(
    PersistentPrefStore::PrefReadError read_error) {
  DCHECK(!init_complete_);
  read_error_ = read_error;
}

CefPrefStore::~CefPrefStore() = default;
