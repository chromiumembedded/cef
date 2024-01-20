// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/values_impl.h"

#include <algorithm>
#include <vector>

#include "base/memory/ptr_util.h"

namespace {

// Removes empty dictionaries from |dict|, potentially nested.
// Does not modify empty lists.
// From chrome/browser/chromeos/extensions/echo_private/echo_private_api.cc
void RemoveEmptyValueDicts(base::Value::Dict& dict) {
  auto it = dict.begin();
  while (it != dict.end()) {
    base::Value& value = it->second;
    if (value.is_dict()) {
      base::Value::Dict& sub_dict = value.GetDict();
      RemoveEmptyValueDicts(sub_dict);
      if (sub_dict.empty()) {
        it = dict.erase(it);
        continue;
      }
    }
    it++;
  }
}

}  // namespace

// CefValueImpl implementation.

// static
CefRefPtr<CefValue> CefValue::Create() {
  // Start with VTYPE_NULL instead of VTYPE_INVALID for backwards compatibility.
  return new CefValueImpl(base::Value());
}

// static
CefRefPtr<CefValue> CefValueImpl::GetOrCreateRefOrCopy(
    base::Value* value,
    void* parent_value,
    bool read_only,
    CefValueController* controller) {
  DCHECK(value);

  if (value->is_blob()) {
    return new CefValueImpl(
        CefBinaryValueImpl::GetOrCreateRef(value, parent_value, controller));
  }

  if (value->is_dict()) {
    return new CefValueImpl(CefDictionaryValueImpl::GetOrCreateRef(
        value, parent_value, read_only, controller));
  }

  if (value->is_list()) {
    return new CefValueImpl(CefListValueImpl::GetOrCreateRef(
        value, parent_value, read_only, controller));
  }

  return new CefValueImpl(value->Clone());
}

CefValueImpl::CefValueImpl() = default;

CefValueImpl::CefValueImpl(base::Value value) {
  SetValue(std::move(value));
}

CefValueImpl::CefValueImpl(CefRefPtr<CefBinaryValue> value)
    : binary_value_(value) {}

CefValueImpl::CefValueImpl(CefRefPtr<CefDictionaryValue> value)
    : dictionary_value_(value) {}

CefValueImpl::CefValueImpl(CefRefPtr<CefListValue> value)
    : list_value_(value) {}

CefValueImpl::~CefValueImpl() = default;

void CefValueImpl::SetValue(base::Value value) {
  base::AutoLock lock_scope(lock_);
  SetValueInternal(absl::make_optional(std::move(value)));
}

base::Value CefValueImpl::CopyValue() {
  base::AutoLock lock_scope(lock_);

  if (binary_value_) {
    return static_cast<CefBinaryValueImpl*>(binary_value_.get())->CopyValue();
  }

  if (dictionary_value_) {
    return static_cast<CefDictionaryValueImpl*>(dictionary_value_.get())
        ->CopyValue();
  }

  if (list_value_) {
    return static_cast<CefListValueImpl*>(list_value_.get())->CopyValue();
  }

  return value_->Clone();
}

std::unique_ptr<base::Value> CefValueImpl::CopyOrDetachValue(
    CefValueController* new_controller) {
  base::AutoLock lock_scope(lock_);

  if (binary_value_) {
    return static_cast<CefBinaryValueImpl*>(binary_value_.get())
        ->CopyOrDetachValue(new_controller);
  }

  if (dictionary_value_) {
    return static_cast<CefDictionaryValueImpl*>(dictionary_value_.get())
        ->CopyOrDetachValue(new_controller);
  }

  if (list_value_) {
    return static_cast<CefListValueImpl*>(list_value_.get())
        ->CopyOrDetachValue(new_controller);
  }

  return std::make_unique<base::Value>(value_->Clone());
}

void CefValueImpl::SwapValue(base::Value* new_value,
                             void* new_parent_value,
                             CefValueController* new_controller) {
  base::AutoLock lock_scope(lock_);

  if (binary_value_) {
    binary_value_ = CefBinaryValueImpl::GetOrCreateRef(
        new_value, new_parent_value, new_controller);
  } else if (dictionary_value_) {
    dictionary_value_ = CefDictionaryValueImpl::GetOrCreateRef(
        new_value, new_parent_value, false, new_controller);
  } else if (list_value_) {
    list_value_ = CefListValueImpl::GetOrCreateRef(new_value, new_parent_value,
                                                   false, new_controller);
  }
}

bool CefValueImpl::IsValid() {
  base::AutoLock lock_scope(lock_);

  if (binary_value_) {
    return binary_value_->IsValid();
  }
  if (dictionary_value_) {
    return dictionary_value_->IsValid();
  }
  if (list_value_) {
    return list_value_->IsValid();
  }

  return (value_ != nullptr);
}

bool CefValueImpl::IsOwned() {
  base::AutoLock lock_scope(lock_);

  if (binary_value_) {
    return binary_value_->IsOwned();
  }
  if (dictionary_value_) {
    return dictionary_value_->IsOwned();
  }
  if (list_value_) {
    return list_value_->IsOwned();
  }

  return false;
}

bool CefValueImpl::IsReadOnly() {
  base::AutoLock lock_scope(lock_);

  if (binary_value_) {
    return true;
  }
  if (dictionary_value_) {
    return dictionary_value_->IsReadOnly();
  }
  if (list_value_) {
    return list_value_->IsReadOnly();
  }

  return false;
}

bool CefValueImpl::IsSame(CefRefPtr<CefValue> that) {
  if (that.get() == this) {
    return true;
  }
  if (!that.get() || that->GetType() != GetType()) {
    return false;
  }

  CefValueImpl* impl = static_cast<CefValueImpl*>(that.get());

  base::AutoLock lock_scope(lock_);
  base::AutoLock lock_scope2(impl->lock_);

  if (binary_value_) {
    return binary_value_->IsSame(impl->binary_value_);
  }
  if (dictionary_value_) {
    return dictionary_value_->IsSame(impl->dictionary_value_);
  }
  if (list_value_) {
    return list_value_->IsSame(impl->list_value_);
  }

  // Simple types are never the same.
  return false;
}

bool CefValueImpl::IsEqual(CefRefPtr<CefValue> that) {
  if (that.get() == this) {
    return true;
  }
  if (!that.get() || that->GetType() != GetType()) {
    return false;
  }

  CefValueImpl* impl = static_cast<CefValueImpl*>(that.get());

  base::AutoLock lock_scope(lock_);
  base::AutoLock lock_scope2(impl->lock_);

  if (binary_value_) {
    return binary_value_->IsEqual(impl->binary_value_);
  }
  if (dictionary_value_) {
    return dictionary_value_->IsEqual(impl->dictionary_value_);
  }
  if (list_value_) {
    return list_value_->IsEqual(impl->list_value_);
  }

  if (!value_) {  // Invalid types are equal.
    return true;
  }

  return *value_ == *(impl->value_.get());
}

CefRefPtr<CefValue> CefValueImpl::Copy() {
  base::AutoLock lock_scope(lock_);

  if (binary_value_) {
    return new CefValueImpl(binary_value_->Copy());
  }
  if (dictionary_value_) {
    return new CefValueImpl(dictionary_value_->Copy(false));
  }
  if (list_value_) {
    return new CefValueImpl(list_value_->Copy());
  }
  if (value_) {
    return new CefValueImpl(value_->Clone());
  }

  return new CefValueImpl();
}

CefValueType CefValueImpl::GetType() {
  base::AutoLock lock_scope(lock_);

  if (binary_value_) {
    return VTYPE_BINARY;
  }
  if (dictionary_value_) {
    return VTYPE_DICTIONARY;
  }
  if (list_value_) {
    return VTYPE_LIST;
  }

  if (value_) {
    switch (value_->type()) {
      case base::Value::Type::NONE:
        return VTYPE_NULL;
      case base::Value::Type::BOOLEAN:
        return VTYPE_BOOL;
      case base::Value::Type::INTEGER:
        return VTYPE_INT;
      case base::Value::Type::DOUBLE:
        return VTYPE_DOUBLE;
      case base::Value::Type::STRING:
        return VTYPE_STRING;
      default:
        DCHECK(false);
        break;
    }
  }

  return VTYPE_INVALID;
}

bool CefValueImpl::GetBool() {
  base::AutoLock lock_scope(lock_);

  bool ret_value = false;
  if (value_ && value_->is_bool()) {
    ret_value = value_->GetBool();
  }
  return ret_value;
}

int CefValueImpl::GetInt() {
  base::AutoLock lock_scope(lock_);

  int ret_value = 0;
  if (value_ && value_->is_int()) {
    ret_value = value_->GetInt();
  }
  return ret_value;
}

double CefValueImpl::GetDouble() {
  base::AutoLock lock_scope(lock_);

  double ret_value = 0;
  if (value_ && value_->is_double()) {
    ret_value = value_->GetDouble();
  }
  return ret_value;
}

CefString CefValueImpl::GetString() {
  base::AutoLock lock_scope(lock_);

  std::string ret_value;
  if (value_ && value_->is_string()) {
    ret_value = value_->GetString();
  }
  return ret_value;
}

CefRefPtr<CefBinaryValue> CefValueImpl::GetBinary() {
  base::AutoLock lock_scope(lock_);
  return binary_value_;
}

CefRefPtr<CefDictionaryValue> CefValueImpl::GetDictionary() {
  base::AutoLock lock_scope(lock_);
  return dictionary_value_;
}

CefRefPtr<CefListValue> CefValueImpl::GetList() {
  base::AutoLock lock_scope(lock_);
  return list_value_;
}

bool CefValueImpl::SetNull() {
  SetValue(base::Value());
  return true;
}

bool CefValueImpl::SetBool(bool value) {
  SetValue(base::Value(value));
  return true;
}

bool CefValueImpl::SetInt(int value) {
  SetValue(base::Value(value));
  return true;
}

bool CefValueImpl::SetDouble(double value) {
  SetValue(base::Value(value));
  return true;
}

bool CefValueImpl::SetString(const CefString& value) {
  SetValue(base::Value(value.ToString()));
  return true;
}

bool CefValueImpl::SetBinary(CefRefPtr<CefBinaryValue> value) {
  base::AutoLock lock_scope(lock_);
  SetValueInternal(absl::nullopt);
  binary_value_ = value;
  return true;
}

bool CefValueImpl::SetDictionary(CefRefPtr<CefDictionaryValue> value) {
  base::AutoLock lock_scope(lock_);
  SetValueInternal(absl::nullopt);
  dictionary_value_ = value;
  return true;
}

bool CefValueImpl::SetList(CefRefPtr<CefListValue> value) {
  base::AutoLock lock_scope(lock_);
  SetValueInternal(absl::nullopt);
  list_value_ = value;
  return true;
}

void CefValueImpl::SetValueInternal(absl::optional<base::Value> value) {
  lock_.AssertAcquired();

  value_.reset(nullptr);
  binary_value_ = nullptr;
  dictionary_value_ = nullptr;
  list_value_ = nullptr;

  if (value) {
    switch ((*value).type()) {
      case base::Value::Type::BINARY:
        binary_value_ = new CefBinaryValueImpl(std::move(*value));
        break;
      case base::Value::Type::DICT:
        dictionary_value_ =
            new CefDictionaryValueImpl(std::move(*value), /*read_only=*/false);
        break;
      case base::Value::Type::LIST:
        list_value_ =
            new CefListValueImpl(std::move(*value), /*read_only=*/false);
        break;
      default:
        value_ = std::make_unique<base::Value>(std::move(*value));
        break;
    }
  }
}

CefValueController* CefValueImpl::GetValueController() const {
  lock_.AssertAcquired();

  if (binary_value_) {
    return static_cast<CefBinaryValueImpl*>(binary_value_.get())->controller();
  } else if (dictionary_value_) {
    return static_cast<CefDictionaryValueImpl*>(dictionary_value_.get())
        ->controller();
  } else if (list_value_) {
    return static_cast<CefListValueImpl*>(list_value_.get())->controller();
  }

  return nullptr;
}

void CefValueImpl::AcquireLock() NO_THREAD_SAFETY_ANALYSIS {
  lock_.Acquire();

  CefValueController* controller = GetValueController();
  if (controller) {
    controller->lock();
  }
}

void CefValueImpl::ReleaseLock() NO_THREAD_SAFETY_ANALYSIS {
  CefValueController* controller = GetValueController();
  if (controller) {
    controller->AssertLockAcquired();
    controller->unlock();
  }

  lock_.Release();
}

base::Value* CefValueImpl::GetValueUnsafe() const {
  lock_.AssertAcquired();

  if (binary_value_) {
    return static_cast<CefBinaryValueImpl*>(binary_value_.get())
        ->GetValueUnsafe();
  } else if (dictionary_value_) {
    return static_cast<CefDictionaryValueImpl*>(dictionary_value_.get())
        ->GetValueUnsafe();
  } else if (list_value_) {
    return static_cast<CefListValueImpl*>(list_value_.get())->GetValueUnsafe();
  }

  return value_.get();
}

// CefBinaryValueImpl implementation.

CefRefPtr<CefBinaryValue> CefBinaryValue::Create(const void* data,
                                                 size_t data_size) {
  DCHECK(data);
  DCHECK_GT(data_size, (size_t)0);
  if (!data || data_size == 0) {
    return nullptr;
  }

  const auto ptr = static_cast<const uint8_t*>(data);
  return new CefBinaryValueImpl(base::make_span(ptr, data_size));
}

// static
CefRefPtr<CefBinaryValue> CefBinaryValueImpl::GetOrCreateRef(
    base::Value* value,
    void* parent_value,
    CefValueController* controller) {
  DCHECK(value);
  DCHECK(parent_value);
  DCHECK(controller);

  CefValueController::Object* object = controller->Get(value);
  if (object) {
    return static_cast<CefBinaryValueImpl*>(object);
  }

  return new CefBinaryValueImpl(value, parent_value,
                                CefBinaryValueImpl::kReference, controller);
}

CefBinaryValueImpl::CefBinaryValueImpl(base::Value value)
    : CefBinaryValueImpl(new base::Value(std::move(value)),
                         /*will_delete=*/true) {}

CefBinaryValueImpl::CefBinaryValueImpl(base::Value* value, bool will_delete)
    : CefBinaryValueImpl(value,
                         nullptr,
                         will_delete ? kOwnerWillDelete : kOwnerNoDelete,
                         nullptr) {}

CefBinaryValueImpl::CefBinaryValueImpl(base::span<const uint8_t> value)
    : CefBinaryValueImpl(new base::Value(value),
                         nullptr,
                         kOwnerWillDelete,
                         nullptr) {}

base::Value CefBinaryValueImpl::CopyValue() {
  CEF_VALUE_VERIFY_RETURN(false, base::Value());
  return const_value().Clone();
}

std::unique_ptr<base::Value> CefBinaryValueImpl::CopyOrDetachValue(
    CefValueController* new_controller) {
  if (!will_delete()) {
    // Copy the value.
    return std::make_unique<base::Value>(CopyValue());
  }

  // Take ownership of the value.
  auto value = base::WrapUnique(Detach(new_controller));
  DCHECK(value.get());
  return value;
}

bool CefBinaryValueImpl::IsSameValue(const base::Value* that) {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return (&const_value() == that);
}

bool CefBinaryValueImpl::IsEqualValue(const base::Value* that) {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value() == *that;
}

base::Value* CefBinaryValueImpl::GetValueUnsafe() const {
  if (!VerifyAttached()) {
    return nullptr;
  }
  controller()->AssertLockAcquired();
  return mutable_value_unchecked();
}

bool CefBinaryValueImpl::IsValid() {
  return !detached();
}

bool CefBinaryValueImpl::IsOwned() {
  return !will_delete();
}

bool CefBinaryValueImpl::IsSame(CefRefPtr<CefBinaryValue> that) {
  if (!that.get()) {
    return false;
  }
  if (that.get() == this) {
    return true;
  }

  CEF_VALUE_VERIFY_RETURN(false, false);
  return static_cast<CefBinaryValueImpl*>(that.get())
      ->IsSameValue(&const_value());
}

bool CefBinaryValueImpl::IsEqual(CefRefPtr<CefBinaryValue> that) {
  if (!that.get()) {
    return false;
  }
  if (that.get() == this) {
    return true;
  }

  CEF_VALUE_VERIFY_RETURN(false, false);
  return static_cast<CefBinaryValueImpl*>(that.get())
      ->IsEqualValue(&const_value());
}

CefRefPtr<CefBinaryValue> CefBinaryValueImpl::Copy() {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);
  return new CefBinaryValueImpl(const_value().Clone());
}

const void* CefBinaryValueImpl::GetRawData() {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);
  return const_value().GetBlob().data();
}

size_t CefBinaryValueImpl::GetSize() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().GetBlob().size();
}

size_t CefBinaryValueImpl::GetData(void* buffer,
                                   size_t buffer_size,
                                   size_t data_offset) {
  DCHECK(buffer);
  DCHECK_GT(buffer_size, (size_t)0);
  if (!buffer || buffer_size == 0) {
    return 0;
  }

  CEF_VALUE_VERIFY_RETURN(false, 0);

  size_t size = const_value().GetBlob().size();
  DCHECK_LT(data_offset, size);
  if (data_offset >= size) {
    return 0;
  }

  size = std::min(buffer_size, size - data_offset);
  auto* data = const_value().GetBlob().data();
  memcpy(buffer, data + data_offset, size);
  return size;
}

CefBinaryValueImpl::CefBinaryValueImpl(base::Value* value,
                                       void* parent_value,
                                       ValueMode value_mode,
                                       CefValueController* controller)
    : CefValueBase<CefBinaryValue, base::Value>(value,
                                                parent_value,
                                                value_mode,
                                                /*read_only=*/true,
                                                controller) {}

// CefDictionaryValueImpl implementation.

// static
CefRefPtr<CefDictionaryValue> CefDictionaryValue::Create() {
  return new CefDictionaryValueImpl(base::Value(base::Value::Type::DICT),
                                    /*read_only=*/false);
}

// static
CefRefPtr<CefDictionaryValue> CefDictionaryValueImpl::GetOrCreateRef(
    base::Value* value,
    void* parent_value,
    bool read_only,
    CefValueController* controller) {
  CefValueController::Object* object = controller->Get(value);
  if (object) {
    return static_cast<CefDictionaryValueImpl*>(object);
  }

  return new CefDictionaryValueImpl(value, parent_value,
                                    CefDictionaryValueImpl::kReference,
                                    read_only, controller);
}

CefDictionaryValueImpl::CefDictionaryValueImpl(base::Value value,
                                               bool read_only)
    : CefDictionaryValueImpl(new base::Value(std::move(value)),
                             /*will_delete=*/true,
                             read_only) {}

CefDictionaryValueImpl::CefDictionaryValueImpl(base::Value::Dict value,
                                               bool read_only)
    : CefDictionaryValueImpl(base::Value(std::move(value)), read_only) {}

CefDictionaryValueImpl::CefDictionaryValueImpl(base::Value* value,
                                               bool will_delete,
                                               bool read_only)
    : CefDictionaryValueImpl(value,
                             nullptr,
                             will_delete ? kOwnerWillDelete : kOwnerNoDelete,
                             read_only,
                             nullptr) {}

base::Value CefDictionaryValueImpl::CopyValue() {
  CEF_VALUE_VERIFY_RETURN(false, base::Value());
  return const_value().Clone();
}

std::unique_ptr<base::Value> CefDictionaryValueImpl::CopyOrDetachValue(
    CefValueController* new_controller) {
  if (!will_delete()) {
    // Copy the value.
    return std::make_unique<base::Value>(CopyValue());
  }

  // Take ownership of the value.
  auto value = base::WrapUnique(Detach(new_controller));
  DCHECK(value.get());
  return value;
}

bool CefDictionaryValueImpl::IsSameValue(const base::Value* that) {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return (&const_value() == that);
}

bool CefDictionaryValueImpl::IsEqualValue(const base::Value* that) {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value() == *that;
}

base::Value* CefDictionaryValueImpl::GetValueUnsafe() const {
  if (!VerifyAttached()) {
    return nullptr;
  }
  controller()->AssertLockAcquired();
  return mutable_value_unchecked();
}

bool CefDictionaryValueImpl::IsValid() {
  return !detached();
}

bool CefDictionaryValueImpl::IsOwned() {
  return !will_delete();
}

bool CefDictionaryValueImpl::IsReadOnly() {
  return read_only();
}

bool CefDictionaryValueImpl::IsSame(CefRefPtr<CefDictionaryValue> that) {
  if (!that.get()) {
    return false;
  }
  if (that.get() == this) {
    return true;
  }

  CEF_VALUE_VERIFY_RETURN(false, false);
  return static_cast<CefDictionaryValueImpl*>(that.get())
      ->IsSameValue(&const_value());
}

bool CefDictionaryValueImpl::IsEqual(CefRefPtr<CefDictionaryValue> that) {
  if (!that.get()) {
    return false;
  }
  if (that.get() == this) {
    return true;
  }

  CEF_VALUE_VERIFY_RETURN(false, false);
  return static_cast<CefDictionaryValueImpl*>(that.get())
      ->IsEqualValue(&const_value());
}

CefRefPtr<CefDictionaryValue> CefDictionaryValueImpl::Copy(
    bool exclude_empty_children) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  auto value = const_value().Clone();
  if (exclude_empty_children) {
    RemoveEmptyValueDicts(value.GetDict());
  }

  return new CefDictionaryValueImpl(std::move(value), /*read_only=*/false);
}

size_t CefDictionaryValueImpl::GetSize() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().GetDict().size();
}

bool CefDictionaryValueImpl::Clear() {
  CEF_VALUE_VERIFY_RETURN(true, false);

  // Detach any dependent values.
  controller()->RemoveDependencies(mutable_value());

  mutable_value()->GetDict().clear();
  return true;
}

bool CefDictionaryValueImpl::HasKey(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().GetDict().contains(base::StringPiece(key.ToString()));
}

bool CefDictionaryValueImpl::GetKeys(KeyList& keys) {
  CEF_VALUE_VERIFY_RETURN(false, 0);

  for (const auto item : const_value().GetDict()) {
    keys.push_back(item.first);
  }

  return true;
}

bool CefDictionaryValueImpl::Remove(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  return RemoveInternal(key);
}

CefValueType CefDictionaryValueImpl::GetType(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, VTYPE_INVALID);

  const base::Value* value =
      const_value().GetDict().Find(base::StringPiece(key.ToString()));
  if (value) {
    switch (value->type()) {
      case base::Value::Type::NONE:
        return VTYPE_NULL;
      case base::Value::Type::BOOLEAN:
        return VTYPE_BOOL;
      case base::Value::Type::INTEGER:
        return VTYPE_INT;
      case base::Value::Type::DOUBLE:
        return VTYPE_DOUBLE;
      case base::Value::Type::STRING:
        return VTYPE_STRING;
      case base::Value::Type::BINARY:
        return VTYPE_BINARY;
      case base::Value::Type::DICT:
        return VTYPE_DICTIONARY;
      case base::Value::Type::LIST:
        return VTYPE_LIST;
    }
  }

  return VTYPE_INVALID;
}

CefRefPtr<CefValue> CefDictionaryValueImpl::GetValue(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const base::Value* value =
      const_value().GetDict().Find(base::StringPiece(key.ToString()));
  if (value) {
    return CefValueImpl::GetOrCreateRefOrCopy(const_cast<base::Value*>(value),
                                              mutable_value_unchecked(),
                                              read_only(), controller());
  }

  return nullptr;
}

bool CefDictionaryValueImpl::GetBool(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, false);

  bool ret_value = false;

  const base::Value* value =
      const_value().GetDict().Find(base::StringPiece(key.ToString()));
  if (value && value->is_bool()) {
    ret_value = value->GetBool();
  }

  return ret_value;
}

int CefDictionaryValueImpl::GetInt(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, 0);

  int ret_value = 0;

  const base::Value* value =
      const_value().GetDict().Find(base::StringPiece(key.ToString()));
  if (value && value->is_int()) {
    ret_value = value->GetInt();
  }

  return ret_value;
}

double CefDictionaryValueImpl::GetDouble(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, 0);

  double ret_value = 0;

  const base::Value* value =
      const_value().GetDict().Find(base::StringPiece(key.ToString()));
  if (value && value->is_double()) {
    ret_value = value->GetDouble();
  }

  return ret_value;
}

CefString CefDictionaryValueImpl::GetString(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, CefString());

  std::string ret_value;

  const base::Value* value =
      const_value().GetDict().Find(base::StringPiece(key.ToString()));
  if (value && value->is_string()) {
    ret_value = value->GetString();
  }

  return ret_value;
}

CefRefPtr<CefBinaryValue> CefDictionaryValueImpl::GetBinary(
    const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const base::Value* value =
      const_value().GetDict().Find(base::StringPiece(key.ToString()));
  if (value && value->is_blob()) {
    base::Value* binary_value = const_cast<base::Value*>(value);
    return CefBinaryValueImpl::GetOrCreateRef(
        binary_value, mutable_value_unchecked(), controller());
  }

  return nullptr;
}

CefRefPtr<CefDictionaryValue> CefDictionaryValueImpl::GetDictionary(
    const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const base::Value* value =
      const_value().GetDict().Find(base::StringPiece(key.ToString()));
  if (value && value->is_dict()) {
    base::Value* dict_value = const_cast<base::Value*>(value);
    return CefDictionaryValueImpl::GetOrCreateRef(
        dict_value, mutable_value_unchecked(), read_only(), controller());
  }

  return nullptr;
}

CefRefPtr<CefListValue> CefDictionaryValueImpl::GetList(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const base::Value* value =
      const_value().GetDict().Find(base::StringPiece(key.ToString()));
  if (value && value->is_list()) {
    base::Value* list_value = const_cast<base::Value*>(value);
    return CefListValueImpl::GetOrCreateRef(
        list_value, mutable_value_unchecked(), read_only(), controller());
  }

  return nullptr;
}

bool CefDictionaryValueImpl::SetValue(const CefString& key,
                                      CefRefPtr<CefValue> value) {
  CEF_VALUE_VERIFY_RETURN(true, false);

  CefValueImpl* impl = static_cast<CefValueImpl*>(value.get());
  DCHECK(impl);

  base::Value* actual_value =
      SetInternal(key, impl->CopyOrDetachValue(controller()));
  impl->SwapValue(actual_value, mutable_value(), controller());
  return true;
}

bool CefDictionaryValueImpl::SetNull(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(key, std::make_unique<base::Value>());
  return true;
}

bool CefDictionaryValueImpl::SetBool(const CefString& key, bool value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(key, std::make_unique<base::Value>(value));
  return true;
}

bool CefDictionaryValueImpl::SetInt(const CefString& key, int value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(key, std::make_unique<base::Value>(value));
  return true;
}

bool CefDictionaryValueImpl::SetDouble(const CefString& key, double value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(key, std::make_unique<base::Value>(value));
  return true;
}

bool CefDictionaryValueImpl::SetString(const CefString& key,
                                       const CefString& value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(key, std::make_unique<base::Value>(value.ToString()));
  return true;
}

bool CefDictionaryValueImpl::SetBinary(const CefString& key,
                                       CefRefPtr<CefBinaryValue> value) {
  CEF_VALUE_VERIFY_RETURN(true, false);

  CefBinaryValueImpl* impl = static_cast<CefBinaryValueImpl*>(value.get());
  DCHECK(impl);

  SetInternal(key, impl->CopyOrDetachValue(controller()));
  return true;
}

bool CefDictionaryValueImpl::SetDictionary(
    const CefString& key,
    CefRefPtr<CefDictionaryValue> value) {
  CEF_VALUE_VERIFY_RETURN(true, false);

  CefDictionaryValueImpl* impl =
      static_cast<CefDictionaryValueImpl*>(value.get());
  DCHECK(impl);

  SetInternal(key, impl->CopyOrDetachValue(controller()));
  return true;
}

bool CefDictionaryValueImpl::SetList(const CefString& key,
                                     CefRefPtr<CefListValue> value) {
  CEF_VALUE_VERIFY_RETURN(true, false);

  CefListValueImpl* impl = static_cast<CefListValueImpl*>(value.get());
  DCHECK(impl);

  SetInternal(key, impl->CopyOrDetachValue(controller()));
  return true;
}

bool CefDictionaryValueImpl::RemoveInternal(const CefString& key) {
  // The ExtractKey() call below which removes the Value from the dictionary
  // will return a new Value object with the moved contents of the Value that
  // exists in the implementation std::map. Consequently we use FindKey() to
  // retrieve the actual Value pointer as it current exists first, for later
  // comparison purposes.
  const base::Value* actual_value =
      const_value().GetDict().Find(base::StringPiece(key.ToString()));
  if (!actual_value) {
    return false;
  }

  // |actual_value| is no longer valid after this call.
  absl::optional<base::Value> out_value =
      mutable_value()->GetDict().Extract(base::StringPiece(key.ToString()));
  if (!out_value.has_value()) {
    return false;
  }

  // Remove the value.
  controller()->Remove(const_cast<base::Value*>(actual_value), true);

  // Only list and dictionary types may have dependencies.
  if (out_value->is_list() || out_value->is_dict()) {
    controller()->RemoveDependencies(const_cast<base::Value*>(actual_value));
  }

  return true;
}

base::Value* CefDictionaryValueImpl::SetInternal(
    const CefString& key,
    std::unique_ptr<base::Value> value) {
  DCHECK(value);

  RemoveInternal(key);

  // base::Value now uses move semantics which means that Set() will move the
  // contents of the passed-in base::Value instead of keeping the same object.
  // Set() then returns the actual Value pointer as it currently exists.
  base::Value* actual_value = mutable_value()->GetDict().Set(
      base::StringPiece(key.ToString()), std::move(*value));
  CHECK(actual_value);

  // |value| will be deleted when this method returns. Update the controller to
  // reference |actual_value| instead.
  controller()->Swap(value.get(), actual_value);

  return actual_value;
}

CefDictionaryValueImpl::CefDictionaryValueImpl(base::Value* value,
                                               void* parent_value,
                                               ValueMode value_mode,
                                               bool read_only,
                                               CefValueController* controller)
    : CefValueBase<CefDictionaryValue, base::Value>(value,
                                                    parent_value,
                                                    value_mode,
                                                    read_only,
                                                    controller) {
  DCHECK(value->is_dict());
}

// CefListValueImpl implementation.

// static
CefRefPtr<CefListValue> CefListValue::Create() {
  return new CefListValueImpl(base::Value(base::Value::Type::LIST),
                              /*read_only=*/false);
}

// static
CefRefPtr<CefListValue> CefListValueImpl::GetOrCreateRef(
    base::Value* value,
    void* parent_value,
    bool read_only,
    CefValueController* controller) {
  CefValueController::Object* object = controller->Get(value);
  if (object) {
    return static_cast<CefListValueImpl*>(object);
  }

  return new CefListValueImpl(value, parent_value, CefListValueImpl::kReference,
                              read_only, controller);
}

CefListValueImpl::CefListValueImpl(base::Value value, bool read_only)
    : CefListValueImpl(new base::Value(std::move(value)),
                       /*will_delete=*/true,
                       read_only) {}

CefListValueImpl::CefListValueImpl(base::Value::List value, bool read_only)
    : CefListValueImpl(base::Value(std::move(value)), read_only) {}

CefListValueImpl::CefListValueImpl(base::Value* value,
                                   bool will_delete,
                                   bool read_only)
    : CefListValueImpl(value,
                       nullptr,
                       will_delete ? kOwnerWillDelete : kOwnerNoDelete,
                       read_only,
                       nullptr) {}

base::Value CefListValueImpl::CopyValue() {
  CEF_VALUE_VERIFY_RETURN(false, base::Value());
  return const_value().Clone();
}

std::unique_ptr<base::Value> CefListValueImpl::CopyOrDetachValue(
    CefValueController* new_controller) {
  if (!will_delete()) {
    // Copy the value.
    return std::make_unique<base::Value>(CopyValue());
  }

  // Take ownership of the value.
  auto value = base::WrapUnique(Detach(new_controller));
  DCHECK(value.get());
  return value;
}

bool CefListValueImpl::IsSameValue(const base::Value* that) {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return (&const_value() == that);
}

bool CefListValueImpl::IsEqualValue(const base::Value* that) {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value() == *that;
}

base::Value* CefListValueImpl::GetValueUnsafe() const {
  if (!VerifyAttached()) {
    return nullptr;
  }
  controller()->AssertLockAcquired();
  return mutable_value_unchecked();
}

bool CefListValueImpl::IsValid() {
  return !detached();
}

bool CefListValueImpl::IsOwned() {
  return !will_delete();
}

bool CefListValueImpl::IsReadOnly() {
  return read_only();
}

bool CefListValueImpl::IsSame(CefRefPtr<CefListValue> that) {
  if (!that.get()) {
    return false;
  }
  if (that.get() == this) {
    return true;
  }

  CEF_VALUE_VERIFY_RETURN(false, false);
  return static_cast<CefListValueImpl*>(that.get())
      ->IsSameValue(&const_value());
}

bool CefListValueImpl::IsEqual(CefRefPtr<CefListValue> that) {
  if (!that.get()) {
    return false;
  }
  if (that.get() == this) {
    return true;
  }

  CEF_VALUE_VERIFY_RETURN(false, false);
  return static_cast<CefListValueImpl*>(that.get())
      ->IsEqualValue(&const_value());
}

CefRefPtr<CefListValue> CefListValueImpl::Copy() {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  return new CefListValueImpl(const_value().Clone(), /*read_only=*/false);
}

bool CefListValueImpl::SetSize(size_t size) {
  CEF_VALUE_VERIFY_RETURN(true, false);

  size_t current_size = const_value().GetList().size();
  if (size < current_size) {
    // Clean up any values above the requested size.
    for (size_t i = current_size - 1; i >= size; --i) {
      RemoveInternal(i);
    }
  } else if (size > 0) {
    // Expand the list size.
    // TODO: This approach seems inefficient. See https://crbug.com/1187066#c17
    // for background.
    auto& list = mutable_value()->GetList();
    while (list.size() < size) {
      list.Append(base::Value());
    }
  }
  return true;
}

size_t CefListValueImpl::GetSize() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().GetList().size();
}

bool CefListValueImpl::Clear() {
  CEF_VALUE_VERIFY_RETURN(true, false);

  // Detach any dependent values.
  controller()->RemoveDependencies(mutable_value());

  mutable_value()->GetList().clear();
  return true;
}

bool CefListValueImpl::Remove(size_t index) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  return RemoveInternal(index);
}

CefValueType CefListValueImpl::GetType(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, VTYPE_INVALID);

  const auto& list = const_value().GetList();
  if (index < list.size()) {
    const base::Value& value = list[index];
    switch (value.type()) {
      case base::Value::Type::NONE:
        return VTYPE_NULL;
      case base::Value::Type::BOOLEAN:
        return VTYPE_BOOL;
      case base::Value::Type::INTEGER:
        return VTYPE_INT;
      case base::Value::Type::DOUBLE:
        return VTYPE_DOUBLE;
      case base::Value::Type::STRING:
        return VTYPE_STRING;
      case base::Value::Type::BINARY:
        return VTYPE_BINARY;
      case base::Value::Type::DICT:
        return VTYPE_DICTIONARY;
      case base::Value::Type::LIST:
        return VTYPE_LIST;
    }
  }

  return VTYPE_INVALID;
}

CefRefPtr<CefValue> CefListValueImpl::GetValue(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const auto& list = const_value().GetList();
  if (index < list.size()) {
    const base::Value& value = list[index];
    return CefValueImpl::GetOrCreateRefOrCopy(const_cast<base::Value*>(&value),
                                              mutable_value_unchecked(),
                                              read_only(), controller());
  }

  return nullptr;
}

bool CefListValueImpl::GetBool(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, false);

  bool ret_value = false;
  const auto& list = const_value().GetList();
  if (index < list.size()) {
    const base::Value& value = list[index];
    if (value.is_bool()) {
      ret_value = value.GetBool();
    }
  }

  return ret_value;
}

int CefListValueImpl::GetInt(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, 0);

  int ret_value = 0;
  const auto& list = const_value().GetList();
  if (index < list.size()) {
    const base::Value& value = list[index];
    if (value.is_int()) {
      ret_value = value.GetInt();
    }
  }

  return ret_value;
}

double CefListValueImpl::GetDouble(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, 0);

  double ret_value = 0;
  const auto& list = const_value().GetList();
  if (index < list.size()) {
    const base::Value& value = list[index];
    if (value.is_double()) {
      ret_value = value.GetDouble();
    }
  }

  return ret_value;
}

CefString CefListValueImpl::GetString(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, CefString());

  std::string ret_value;
  const auto& list = const_value().GetList();
  if (index < list.size()) {
    const base::Value& value = list[index];
    if (value.is_string()) {
      ret_value = value.GetString();
    }
  }

  return ret_value;
}

CefRefPtr<CefBinaryValue> CefListValueImpl::GetBinary(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const auto& list = const_value().GetList();
  if (index < list.size()) {
    base::Value& value = const_cast<base::Value&>(list[index]);
    if (value.is_blob()) {
      return CefBinaryValueImpl::GetOrCreateRef(
          &value, mutable_value_unchecked(), controller());
    }
  }

  return nullptr;
}

CefRefPtr<CefDictionaryValue> CefListValueImpl::GetDictionary(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const auto& list = const_value().GetList();
  if (index < list.size()) {
    base::Value& value = const_cast<base::Value&>(list[index]);
    if (value.is_dict()) {
      return CefDictionaryValueImpl::GetOrCreateRef(
          &value, mutable_value_unchecked(), read_only(), controller());
    }
  }

  return nullptr;
}

CefRefPtr<CefListValue> CefListValueImpl::GetList(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const auto& list = const_value().GetList();
  if (index < list.size()) {
    base::Value& value = const_cast<base::Value&>(list[index]);
    if (value.is_list()) {
      return CefListValueImpl::GetOrCreateRef(&value, mutable_value_unchecked(),
                                              read_only(), controller());
    }
  }

  return nullptr;
}

bool CefListValueImpl::SetValue(size_t index, CefRefPtr<CefValue> value) {
  CEF_VALUE_VERIFY_RETURN(true, false);

  CefValueImpl* impl = static_cast<CefValueImpl*>(value.get());
  DCHECK(impl);

  base::Value* actual_value =
      SetInternal(index, impl->CopyOrDetachValue(controller()));
  impl->SwapValue(actual_value, mutable_value(), controller());
  return true;
}

bool CefListValueImpl::SetNull(size_t index) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(index, std::make_unique<base::Value>());
  return true;
}

bool CefListValueImpl::SetBool(size_t index, bool value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(index, std::make_unique<base::Value>(value));
  return true;
}

bool CefListValueImpl::SetInt(size_t index, int value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(index, std::make_unique<base::Value>(value));
  return true;
}

bool CefListValueImpl::SetDouble(size_t index, double value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(index, std::make_unique<base::Value>(value));
  return true;
}

bool CefListValueImpl::SetString(size_t index, const CefString& value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(index, std::make_unique<base::Value>(value.ToString()));
  return true;
}

bool CefListValueImpl::SetBinary(size_t index,
                                 CefRefPtr<CefBinaryValue> value) {
  CEF_VALUE_VERIFY_RETURN(true, false);

  CefBinaryValueImpl* impl = static_cast<CefBinaryValueImpl*>(value.get());
  DCHECK(impl);

  SetInternal(index, impl->CopyOrDetachValue(controller()));
  return true;
}

bool CefListValueImpl::SetDictionary(size_t index,
                                     CefRefPtr<CefDictionaryValue> value) {
  CEF_VALUE_VERIFY_RETURN(true, false);

  CefDictionaryValueImpl* impl =
      static_cast<CefDictionaryValueImpl*>(value.get());
  DCHECK(impl);

  SetInternal(index, impl->CopyOrDetachValue(controller()));
  return true;
}

bool CefListValueImpl::SetList(size_t index, CefRefPtr<CefListValue> value) {
  CEF_VALUE_VERIFY_RETURN(true, false);

  CefListValueImpl* impl = static_cast<CefListValueImpl*>(value.get());
  DCHECK(impl);

  SetInternal(index, impl->CopyOrDetachValue(controller()));
  return true;
}

bool CefListValueImpl::RemoveInternal(size_t index) {
  auto& list = mutable_value()->GetList();
  if (index >= list.size()) {
    return false;
  }

  // The std::move() call below which removes the Value from the list will
  // return a new Value object with the moved contents of the Value that exists
  // in the implementation std::vector. Consequently we use operator[] to
  // retrieve the actual Value pointer as it current exists first, for later
  // comparison purposes.
  const base::Value& actual_value = list[index];

  // |actual_value| is no longer valid after this call.
  auto out_value = std::move(list[index]);
  list.erase(list.begin() + index);

  // Remove the value.
  controller()->Remove(const_cast<base::Value*>(&actual_value), true);

  // Only list and dictionary types may have dependencies.
  if (out_value.is_list() || out_value.is_dict()) {
    controller()->RemoveDependencies(const_cast<base::Value*>(&actual_value));
  }

  return true;
}

base::Value* CefListValueImpl::SetInternal(size_t index,
                                           std::unique_ptr<base::Value> value) {
  DCHECK(value);

  auto& list = mutable_value()->GetList();
  if (RemoveInternal(index)) {
    CHECK_LE(index, list.size());
    list.Insert(list.begin() + index, std::move(*value));
  } else {
    if (index >= list.size()) {
      // Expand the list size.
      // TODO: This approach seems inefficient. See
      // https://crbug.com/1187066#c17 for background.
      while (list.size() <= index) {
        list.Append(base::Value());
      }
    }
    list[index] = std::move(*value);
  }

  // base::Value now uses move semantics which means that Insert()/Set() will
  // move the contents of the passed-in base::Value instead of keeping the same
  // object. Consequently we use operator[] to retrieve the actual base::Value
  // pointer as it exists in the std::vector.
  const base::Value& actual_value = list[index];

  // |value| will be deleted when this method returns. Update the controller to
  // reference |actual_value| instead.
  controller()->Swap(value.get(), const_cast<base::Value*>(&actual_value));

  return const_cast<base::Value*>(&actual_value);
}

CefListValueImpl::CefListValueImpl(base::Value* value,
                                   void* parent_value,
                                   ValueMode value_mode,
                                   bool read_only,
                                   CefValueController* controller)
    : CefValueBase<CefListValue, base::Value>(value,
                                              parent_value,
                                              value_mode,
                                              read_only,
                                              controller) {
  DCHECK(value->is_list());
}
