// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/values_impl.h"

#include <algorithm>
#include <vector>

#include "base/memory/ptr_util.h"

// CefValueImpl implementation.

// static
CefRefPtr<CefValue> CefValue::Create() {
  return new CefValueImpl(new base::Value());
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
    base::DictionaryValue* dict_value =
        static_cast<base::DictionaryValue*>(value);
    return new CefValueImpl(CefDictionaryValueImpl::GetOrCreateRef(
        dict_value, parent_value, read_only, controller));
  }

  if (value->is_list()) {
    base::ListValue* list_value = static_cast<base::ListValue*>(value);
    return new CefValueImpl(CefListValueImpl::GetOrCreateRef(
        list_value, parent_value, read_only, controller));
  }

  return new CefValueImpl(value->CreateDeepCopy().release());
}

CefValueImpl::CefValueImpl() {}

CefValueImpl::CefValueImpl(base::Value* value) {
  SetValue(value);
}

CefValueImpl::CefValueImpl(CefRefPtr<CefBinaryValue> value)
    : binary_value_(value) {}

CefValueImpl::CefValueImpl(CefRefPtr<CefDictionaryValue> value)
    : dictionary_value_(value) {}

CefValueImpl::CefValueImpl(CefRefPtr<CefListValue> value)
    : list_value_(value) {}

CefValueImpl::~CefValueImpl() {}

void CefValueImpl::SetValue(base::Value* value) {
  base::AutoLock lock_scope(lock_);
  SetValueInternal(value);
}

base::Value* CefValueImpl::CopyOrDetachValue(
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

  return value_->CreateDeepCopy().release();
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
        static_cast<base::DictionaryValue*>(new_value), new_parent_value, false,
        new_controller);
  } else if (list_value_) {
    list_value_ = CefListValueImpl::GetOrCreateRef(
        static_cast<base::ListValue*>(new_value), new_parent_value, false,
        new_controller);
  }
}

bool CefValueImpl::IsValid() {
  base::AutoLock lock_scope(lock_);

  if (binary_value_)
    return binary_value_->IsValid();
  if (dictionary_value_)
    return dictionary_value_->IsValid();
  if (list_value_)
    return list_value_->IsValid();

  return (value_ != nullptr);
}

bool CefValueImpl::IsOwned() {
  base::AutoLock lock_scope(lock_);

  if (binary_value_)
    return binary_value_->IsOwned();
  if (dictionary_value_)
    return dictionary_value_->IsOwned();
  if (list_value_)
    return list_value_->IsOwned();

  return false;
}

bool CefValueImpl::IsReadOnly() {
  base::AutoLock lock_scope(lock_);

  if (binary_value_)
    return true;
  if (dictionary_value_)
    return dictionary_value_->IsReadOnly();
  if (list_value_)
    return list_value_->IsReadOnly();

  return false;
}

bool CefValueImpl::IsSame(CefRefPtr<CefValue> that) {
  if (that.get() == this)
    return true;
  if (!that.get() || that->GetType() != GetType())
    return false;

  CefValueImpl* impl = static_cast<CefValueImpl*>(that.get());

  base::AutoLock lock_scope(lock_);
  base::AutoLock lock_scope2(impl->lock_);

  if (binary_value_)
    return binary_value_->IsSame(impl->binary_value_);
  if (dictionary_value_)
    return dictionary_value_->IsSame(impl->dictionary_value_);
  if (list_value_)
    return list_value_->IsSame(impl->list_value_);

  // Simple types are never the same.
  return false;
}

bool CefValueImpl::IsEqual(CefRefPtr<CefValue> that) {
  if (that.get() == this)
    return true;
  if (!that.get() || that->GetType() != GetType())
    return false;

  CefValueImpl* impl = static_cast<CefValueImpl*>(that.get());

  base::AutoLock lock_scope(lock_);
  base::AutoLock lock_scope2(impl->lock_);

  if (binary_value_)
    return binary_value_->IsEqual(impl->binary_value_);
  if (dictionary_value_)
    return dictionary_value_->IsEqual(impl->dictionary_value_);
  if (list_value_)
    return list_value_->IsEqual(impl->list_value_);

  if (!value_)  // Invalid types are equal.
    return true;

  return value_->Equals(impl->value_.get());
}

CefRefPtr<CefValue> CefValueImpl::Copy() {
  base::AutoLock lock_scope(lock_);

  if (binary_value_)
    return new CefValueImpl(binary_value_->Copy());
  if (dictionary_value_)
    return new CefValueImpl(dictionary_value_->Copy(false));
  if (list_value_)
    return new CefValueImpl(list_value_->Copy());
  if (value_)
    return new CefValueImpl(value_->CreateDeepCopy().release());

  return new CefValueImpl();
}

CefValueType CefValueImpl::GetType() {
  base::AutoLock lock_scope(lock_);

  if (binary_value_)
    return VTYPE_BINARY;
  if (dictionary_value_)
    return VTYPE_DICTIONARY;
  if (list_value_)
    return VTYPE_LIST;

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
        NOTREACHED();
        break;
    }
  }

  return VTYPE_INVALID;
}

bool CefValueImpl::GetBool() {
  base::AutoLock lock_scope(lock_);

  bool ret_value = false;
  if (value_ && value_->is_bool())
    ret_value = value_->GetBool();
  return ret_value;
}

int CefValueImpl::GetInt() {
  base::AutoLock lock_scope(lock_);

  int ret_value = 0;
  if (value_ && value_->is_int())
    ret_value = value_->GetInt();
  return ret_value;
}

double CefValueImpl::GetDouble() {
  base::AutoLock lock_scope(lock_);

  double ret_value = 0;
  if (value_ && value_->is_double())
    ret_value = value_->GetDouble();
  return ret_value;
}

CefString CefValueImpl::GetString() {
  base::AutoLock lock_scope(lock_);

  std::string ret_value;
  if (value_ && value_->is_string())
    ret_value = value_->GetString();
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
  SetValue(new base::Value());
  return true;
}

bool CefValueImpl::SetBool(bool value) {
  SetValue(new base::Value(value));
  return true;
}

bool CefValueImpl::SetInt(int value) {
  SetValue(new base::Value(value));
  return true;
}

bool CefValueImpl::SetDouble(double value) {
  SetValue(new base::Value(value));
  return true;
}

bool CefValueImpl::SetString(const CefString& value) {
  SetValue(new base::Value(value.ToString()));
  return true;
}

bool CefValueImpl::SetBinary(CefRefPtr<CefBinaryValue> value) {
  base::AutoLock lock_scope(lock_);
  SetValueInternal(nullptr);
  binary_value_ = value;
  return true;
}

bool CefValueImpl::SetDictionary(CefRefPtr<CefDictionaryValue> value) {
  base::AutoLock lock_scope(lock_);
  SetValueInternal(nullptr);
  dictionary_value_ = value;
  return true;
}

bool CefValueImpl::SetList(CefRefPtr<CefListValue> value) {
  base::AutoLock lock_scope(lock_);
  SetValueInternal(nullptr);
  list_value_ = value;
  return true;
}

void CefValueImpl::SetValueInternal(base::Value* value) {
  lock_.AssertAcquired();

  value_.reset(nullptr);
  binary_value_ = nullptr;
  dictionary_value_ = nullptr;
  list_value_ = nullptr;

  if (value) {
    switch (value->type()) {
      case base::Value::Type::BINARY:
        binary_value_ = new CefBinaryValueImpl(value, true);
        return;
      case base::Value::Type::DICTIONARY:
        dictionary_value_ = new CefDictionaryValueImpl(
            static_cast<base::DictionaryValue*>(value), true, false);
        return;
      case base::Value::Type::LIST:
        list_value_ = new CefListValueImpl(static_cast<base::ListValue*>(value),
                                           true, false);
        return;
      default:
        value_.reset(value);
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
  if (controller)
    controller->lock();
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
  if (!data || data_size == 0)
    return nullptr;

  return new CefBinaryValueImpl(static_cast<char*>(const_cast<void*>(data)),
                                data_size);
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
  if (object)
    return static_cast<CefBinaryValueImpl*>(object);

  return new CefBinaryValueImpl(value, parent_value,
                                CefBinaryValueImpl::kReference, controller);
}

CefBinaryValueImpl::CefBinaryValueImpl(base::Value* value, bool will_delete)
    : CefValueBase<CefBinaryValue, base::Value>(
          value,
          nullptr,
          will_delete ? kOwnerWillDelete : kOwnerNoDelete,
          true,
          nullptr) {}

CefBinaryValueImpl::CefBinaryValueImpl(char* data, size_t data_size)
    : CefValueBase<CefBinaryValue, base::Value>(
          new base::Value(std::vector<char>(data, data + data_size)),
          nullptr,
          kOwnerWillDelete,
          true,
          nullptr) {}

base::Value* CefBinaryValueImpl::CopyValue() {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);
  return const_value().CreateDeepCopy().release();
}

base::Value* CefBinaryValueImpl::CopyOrDetachValue(
    CefValueController* new_controller) {
  base::Value* new_value;

  if (!will_delete()) {
    // Copy the value.
    new_value = CopyValue();
  } else {
    // Take ownership of the value.
    new_value = Detach(new_controller);
  }

  DCHECK(new_value);
  return new_value;
}

bool CefBinaryValueImpl::IsSameValue(const base::Value* that) {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return (&const_value() == that);
}

bool CefBinaryValueImpl::IsEqualValue(const base::Value* that) {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().Equals(that);
}

base::Value* CefBinaryValueImpl::GetValueUnsafe() {
  if (!VerifyAttached())
    return nullptr;
  controller()->AssertLockAcquired();
  return const_cast<base::Value*>(&const_value());
}

bool CefBinaryValueImpl::IsValid() {
  return !detached();
}

bool CefBinaryValueImpl::IsOwned() {
  return !will_delete();
}

bool CefBinaryValueImpl::IsSame(CefRefPtr<CefBinaryValue> that) {
  if (!that.get())
    return false;
  if (that.get() == this)
    return true;

  CEF_VALUE_VERIFY_RETURN(false, false);
  return static_cast<CefBinaryValueImpl*>(that.get())
      ->IsSameValue(&const_value());
}

bool CefBinaryValueImpl::IsEqual(CefRefPtr<CefBinaryValue> that) {
  if (!that.get())
    return false;
  if (that.get() == this)
    return true;

  CEF_VALUE_VERIFY_RETURN(false, false);
  return static_cast<CefBinaryValueImpl*>(that.get())
      ->IsEqualValue(&const_value());
}

CefRefPtr<CefBinaryValue> CefBinaryValueImpl::Copy() {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);
  return new CefBinaryValueImpl(const_value().CreateDeepCopy().release(),
                                nullptr, CefBinaryValueImpl::kOwnerWillDelete,
                                nullptr);
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
  if (!buffer || buffer_size == 0)
    return 0;

  CEF_VALUE_VERIFY_RETURN(false, 0);

  size_t size = const_value().GetBlob().size();
  DCHECK_LT(data_offset, size);
  if (data_offset >= size)
    return 0;

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
                                                true,
                                                controller) {}

// CefDictionaryValueImpl implementation.

// static
CefRefPtr<CefDictionaryValue> CefDictionaryValue::Create() {
  return new CefDictionaryValueImpl(new base::DictionaryValue(), true, false);
}

// static
CefRefPtr<CefDictionaryValue> CefDictionaryValueImpl::GetOrCreateRef(
    base::DictionaryValue* value,
    void* parent_value,
    bool read_only,
    CefValueController* controller) {
  CefValueController::Object* object = controller->Get(value);
  if (object)
    return static_cast<CefDictionaryValueImpl*>(object);

  return new CefDictionaryValueImpl(value, parent_value,
                                    CefDictionaryValueImpl::kReference,
                                    read_only, controller);
}

CefDictionaryValueImpl::CefDictionaryValueImpl(base::DictionaryValue* value,
                                               bool will_delete,
                                               bool read_only)
    : CefValueBase<CefDictionaryValue, base::DictionaryValue>(
          value,
          nullptr,
          will_delete ? kOwnerWillDelete : kOwnerNoDelete,
          read_only,
          nullptr) {}

base::DictionaryValue* CefDictionaryValueImpl::CopyValue() {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);
  return const_value().CreateDeepCopy().release();
}

base::DictionaryValue* CefDictionaryValueImpl::CopyOrDetachValue(
    CefValueController* new_controller) {
  base::DictionaryValue* new_value;

  if (!will_delete()) {
    // Copy the value.
    new_value = CopyValue();
  } else {
    // Take ownership of the value.
    new_value = Detach(new_controller);
  }

  DCHECK(new_value);
  return new_value;
}

bool CefDictionaryValueImpl::IsSameValue(const base::DictionaryValue* that) {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return (&const_value() == that);
}

bool CefDictionaryValueImpl::IsEqualValue(const base::DictionaryValue* that) {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().Equals(that);
}

base::DictionaryValue* CefDictionaryValueImpl::GetValueUnsafe() {
  if (!VerifyAttached())
    return nullptr;
  controller()->AssertLockAcquired();
  return const_cast<base::DictionaryValue*>(&const_value());
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
  if (!that.get())
    return false;
  if (that.get() == this)
    return true;

  CEF_VALUE_VERIFY_RETURN(false, false);
  return static_cast<CefDictionaryValueImpl*>(that.get())
      ->IsSameValue(&const_value());
}

bool CefDictionaryValueImpl::IsEqual(CefRefPtr<CefDictionaryValue> that) {
  if (!that.get())
    return false;
  if (that.get() == this)
    return true;

  CEF_VALUE_VERIFY_RETURN(false, false);
  return static_cast<CefDictionaryValueImpl*>(that.get())
      ->IsEqualValue(&const_value());
}

CefRefPtr<CefDictionaryValue> CefDictionaryValueImpl::Copy(
    bool exclude_empty_children) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  base::DictionaryValue* value;
  if (exclude_empty_children) {
    value = const_cast<base::DictionaryValue&>(const_value())
                .DeepCopyWithoutEmptyChildren()
                .release();
  } else {
    value = const_value().CreateDeepCopy().release();
  }

  return new CefDictionaryValueImpl(
      value, nullptr, CefDictionaryValueImpl::kOwnerWillDelete, false, nullptr);
}

size_t CefDictionaryValueImpl::GetSize() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().DictSize();
}

bool CefDictionaryValueImpl::Clear() {
  CEF_VALUE_VERIFY_RETURN(true, false);

  // Detach any dependent values.
  controller()->RemoveDependencies(mutable_value());

  mutable_value()->DictClear();
  return true;
}

bool CefDictionaryValueImpl::HasKey(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().HasKey(base::StringPiece(key));
}

bool CefDictionaryValueImpl::GetKeys(KeyList& keys) {
  CEF_VALUE_VERIFY_RETURN(false, 0);

  for (base::DictionaryValue::Iterator i(const_value()); !i.IsAtEnd();
       i.Advance()) {
    keys.push_back(i.key());
  }

  return true;
}

bool CefDictionaryValueImpl::Remove(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  return RemoveInternal(key);
}

CefValueType CefDictionaryValueImpl::GetType(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, VTYPE_INVALID);

  const base::Value* value = const_value().FindKey(base::StringPiece(key));
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
      case base::Value::Type::DICTIONARY:
        return VTYPE_DICTIONARY;
      case base::Value::Type::LIST:
        return VTYPE_LIST;
    }
  }

  return VTYPE_INVALID;
}

CefRefPtr<CefValue> CefDictionaryValueImpl::GetValue(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const base::Value* value = const_value().FindKey(base::StringPiece(key));
  if (value) {
    return CefValueImpl::GetOrCreateRefOrCopy(
        const_cast<base::Value*>(value),
        const_cast<base::DictionaryValue*>(&const_value()), read_only(),
        controller());
  }

  return nullptr;
}

bool CefDictionaryValueImpl::GetBool(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, false);

  bool ret_value = false;

  const base::Value* value = const_value().FindKey(base::StringPiece(key));
  if (value && value->is_bool()) {
    ret_value = value->GetBool();
  }

  return ret_value;
}

int CefDictionaryValueImpl::GetInt(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, 0);

  int ret_value = 0;

  const base::Value* value = const_value().FindKey(base::StringPiece(key));
  if (value && value->is_int()) {
    ret_value = value->GetInt();
  }

  return ret_value;
}

double CefDictionaryValueImpl::GetDouble(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, 0);

  double ret_value = 0;

  const base::Value* value = const_value().FindKey(base::StringPiece(key));
  if (value && value->is_double()) {
    ret_value = value->GetDouble();
  }

  return ret_value;
}

CefString CefDictionaryValueImpl::GetString(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, CefString());

  std::string ret_value;

  const base::Value* value = const_value().FindKey(base::StringPiece(key));
  if (value && value->is_string()) {
    ret_value = value->GetString();
  }

  return ret_value;
}

CefRefPtr<CefBinaryValue> CefDictionaryValueImpl::GetBinary(
    const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const base::Value* value = const_value().FindKey(base::StringPiece(key));
  if (value && value->is_blob()) {
    base::Value* binary_value = const_cast<base::Value*>(value);
    return CefBinaryValueImpl::GetOrCreateRef(
        binary_value, const_cast<base::DictionaryValue*>(&const_value()),
        controller());
  }

  return nullptr;
}

CefRefPtr<CefDictionaryValue> CefDictionaryValueImpl::GetDictionary(
    const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const base::Value* value = const_value().FindKey(base::StringPiece(key));
  if (value && value->is_dict()) {
    base::DictionaryValue* dict_value =
        static_cast<base::DictionaryValue*>(const_cast<base::Value*>(value));
    return CefDictionaryValueImpl::GetOrCreateRef(
        dict_value, const_cast<base::DictionaryValue*>(&const_value()),
        read_only(), controller());
  }

  return nullptr;
}

CefRefPtr<CefListValue> CefDictionaryValueImpl::GetList(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const base::Value* value = const_value().FindKey(base::StringPiece(key));
  if (value && value->is_list()) {
    base::ListValue* list_value =
        static_cast<base::ListValue*>(const_cast<base::Value*>(value));
    return CefListValueImpl::GetOrCreateRef(
        list_value, const_cast<base::DictionaryValue*>(&const_value()),
        read_only(), controller());
  }

  return nullptr;
}

bool CefDictionaryValueImpl::SetValue(const CefString& key,
                                      CefRefPtr<CefValue> value) {
  CEF_VALUE_VERIFY_RETURN(true, false);

  CefValueImpl* impl = static_cast<CefValueImpl*>(value.get());
  DCHECK(impl);

  base::Value* new_value = impl->CopyOrDetachValue(controller());
  base::Value* actual_value = SetInternal(key, new_value);
  impl->SwapValue(actual_value, mutable_value(), controller());
  return true;
}

bool CefDictionaryValueImpl::SetNull(const CefString& key) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(key, new base::Value());
  return true;
}

bool CefDictionaryValueImpl::SetBool(const CefString& key, bool value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(key, new base::Value(value));
  return true;
}

bool CefDictionaryValueImpl::SetInt(const CefString& key, int value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(key, new base::Value(value));
  return true;
}

bool CefDictionaryValueImpl::SetDouble(const CefString& key, double value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(key, new base::Value(value));
  return true;
}

bool CefDictionaryValueImpl::SetString(const CefString& key,
                                       const CefString& value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(key, new base::Value(value.ToString()));
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
      const_value().FindKey(base::StringPiece(key));
  if (!actual_value)
    return false;

  // |actual_value| is no longer valid after this call.
  absl::optional<base::Value> out_value =
      mutable_value()->ExtractKey(base::StringPiece(key));
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

base::Value* CefDictionaryValueImpl::SetInternal(const CefString& key,
                                                 base::Value* value) {
  DCHECK(value);
  RemoveInternal(key);
  mutable_value()->SetWithoutPathExpansion(
      base::StringPiece(key), base::WrapUnique<base::Value>(value));
  return value;
}

CefDictionaryValueImpl::CefDictionaryValueImpl(base::DictionaryValue* value,
                                               void* parent_value,
                                               ValueMode value_mode,
                                               bool read_only,
                                               CefValueController* controller)
    : CefValueBase<CefDictionaryValue, base::DictionaryValue>(value,
                                                              parent_value,
                                                              value_mode,
                                                              read_only,
                                                              controller) {}

// CefListValueImpl implementation.

// static
CefRefPtr<CefListValue> CefListValue::Create() {
  return new CefListValueImpl(new base::ListValue(), true, false);
}

// static
CefRefPtr<CefListValue> CefListValueImpl::GetOrCreateRef(
    base::ListValue* value,
    void* parent_value,
    bool read_only,
    CefValueController* controller) {
  CefValueController::Object* object = controller->Get(value);
  if (object)
    return static_cast<CefListValueImpl*>(object);

  return new CefListValueImpl(value, parent_value, CefListValueImpl::kReference,
                              read_only, controller);
}

CefListValueImpl::CefListValueImpl(base::ListValue* value,
                                   bool will_delete,
                                   bool read_only)
    : CefValueBase<CefListValue, base::ListValue>(
          value,
          nullptr,
          will_delete ? kOwnerWillDelete : kOwnerNoDelete,
          read_only,
          nullptr) {}

base::ListValue* CefListValueImpl::CopyValue() {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);
  return static_cast<base::ListValue*>(
      const_value().CreateDeepCopy().release());
}

base::ListValue* CefListValueImpl::CopyOrDetachValue(
    CefValueController* new_controller) {
  base::ListValue* new_value;

  if (!will_delete()) {
    // Copy the value.
    new_value = CopyValue();
  } else {
    // Take ownership of the value.
    new_value = Detach(new_controller);
  }

  DCHECK(new_value);
  return new_value;
}

bool CefListValueImpl::IsSameValue(const base::ListValue* that) {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return (&const_value() == that);
}

bool CefListValueImpl::IsEqualValue(const base::ListValue* that) {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().Equals(that);
}

base::ListValue* CefListValueImpl::GetValueUnsafe() {
  if (!VerifyAttached())
    return nullptr;
  controller()->AssertLockAcquired();
  return const_cast<base::ListValue*>(&const_value());
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
  if (!that.get())
    return false;
  if (that.get() == this)
    return true;

  CEF_VALUE_VERIFY_RETURN(false, false);
  return static_cast<CefListValueImpl*>(that.get())
      ->IsSameValue(&const_value());
}

bool CefListValueImpl::IsEqual(CefRefPtr<CefListValue> that) {
  if (!that.get())
    return false;
  if (that.get() == this)
    return true;

  CEF_VALUE_VERIFY_RETURN(false, false);
  return static_cast<CefListValueImpl*>(that.get())
      ->IsEqualValue(&const_value());
}

CefRefPtr<CefListValue> CefListValueImpl::Copy() {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  return new CefListValueImpl(
      static_cast<base::ListValue*>(const_value().CreateDeepCopy().release()),
      nullptr, CefListValueImpl::kOwnerWillDelete, false, nullptr);
}

bool CefListValueImpl::SetSize(size_t size) {
  CEF_VALUE_VERIFY_RETURN(true, false);

  size_t current_size = const_value().GetList().size();
  if (size < current_size) {
    // Clean up any values above the requested size.
    for (size_t i = current_size - 1; i >= size; --i)
      RemoveInternal(i);
  } else if (size > 0) {
    // Expand the list size.
    // TODO: This approach seems inefficient. See https://crbug.com/1187066#c17
    // for background.
    auto list = mutable_value()->GetList();
    while (list.size() < size)
      mutable_value()->Append(base::Value());
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

  mutable_value()->ClearList();
  return true;
}

bool CefListValueImpl::Remove(size_t index) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  return RemoveInternal(index);
}

CefValueType CefListValueImpl::GetType(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, VTYPE_INVALID);

  const base::Value* out_value = nullptr;
  if (const_value().Get(index, &out_value)) {
    switch (out_value->type()) {
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
      case base::Value::Type::DICTIONARY:
        return VTYPE_DICTIONARY;
      case base::Value::Type::LIST:
        return VTYPE_LIST;
    }
  }

  return VTYPE_INVALID;
}

CefRefPtr<CefValue> CefListValueImpl::GetValue(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const base::Value* out_value = nullptr;
  if (const_value().Get(index, &out_value)) {
    return CefValueImpl::GetOrCreateRefOrCopy(
        const_cast<base::Value*>(out_value),
        const_cast<base::ListValue*>(&const_value()), read_only(),
        controller());
  }

  return nullptr;
}

bool CefListValueImpl::GetBool(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, false);

  const base::Value* out_value = nullptr;
  bool ret_value = false;

  if (const_value().Get(index, &out_value) && out_value->is_bool()) {
    ret_value = out_value->GetBool();
  }

  return ret_value;
}

int CefListValueImpl::GetInt(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, 0);

  const base::Value* out_value = nullptr;
  int ret_value = 0;

  if (const_value().Get(index, &out_value) && out_value->is_int()) {
    ret_value = out_value->GetInt();
  }

  return ret_value;
}

double CefListValueImpl::GetDouble(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, 0);

  const base::Value* out_value = nullptr;
  double ret_value = 0;

  if (const_value().Get(index, &out_value) && out_value->is_double()) {
    ret_value = out_value->GetDouble();
  }

  return ret_value;
}

CefString CefListValueImpl::GetString(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, CefString());

  const base::Value* out_value = nullptr;
  std::string ret_value;

  if (const_value().Get(index, &out_value) && out_value->is_string()) {
    ret_value = out_value->GetString();
  }

  return ret_value;
}

CefRefPtr<CefBinaryValue> CefListValueImpl::GetBinary(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const base::Value* out_value = nullptr;

  if (const_value().Get(index, &out_value) && out_value->is_blob()) {
    base::Value* binary_value = const_cast<base::Value*>(out_value);
    return CefBinaryValueImpl::GetOrCreateRef(
        binary_value, const_cast<base::ListValue*>(&const_value()),
        controller());
  }

  return nullptr;
}

CefRefPtr<CefDictionaryValue> CefListValueImpl::GetDictionary(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const base::Value* out_value = nullptr;

  if (const_value().Get(index, &out_value) && out_value->is_dict()) {
    base::DictionaryValue* dict_value = static_cast<base::DictionaryValue*>(
        const_cast<base::Value*>(out_value));
    return CefDictionaryValueImpl::GetOrCreateRef(
        dict_value, const_cast<base::ListValue*>(&const_value()), read_only(),
        controller());
  }

  return nullptr;
}

CefRefPtr<CefListValue> CefListValueImpl::GetList(size_t index) {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);

  const base::Value* out_value = nullptr;

  if (const_value().Get(index, &out_value) && out_value->is_list()) {
    base::ListValue* list_value =
        static_cast<base::ListValue*>(const_cast<base::Value*>(out_value));
    return CefListValueImpl::GetOrCreateRef(
        list_value, const_cast<base::ListValue*>(&const_value()), read_only(),
        controller());
  }

  return nullptr;
}

bool CefListValueImpl::SetValue(size_t index, CefRefPtr<CefValue> value) {
  CEF_VALUE_VERIFY_RETURN(true, false);

  CefValueImpl* impl = static_cast<CefValueImpl*>(value.get());
  DCHECK(impl);

  base::Value* new_value = impl->CopyOrDetachValue(controller());
  base::Value* actual_value = SetInternal(index, new_value);
  impl->SwapValue(actual_value, mutable_value(), controller());
  return true;
}

bool CefListValueImpl::SetNull(size_t index) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(index, new base::Value());
  return true;
}

bool CefListValueImpl::SetBool(size_t index, bool value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(index, new base::Value(value));
  return true;
}

bool CefListValueImpl::SetInt(size_t index, int value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(index, new base::Value(value));
  return true;
}

bool CefListValueImpl::SetDouble(size_t index, double value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(index, new base::Value(value));
  return true;
}

bool CefListValueImpl::SetString(size_t index, const CefString& value) {
  CEF_VALUE_VERIFY_RETURN(true, false);
  SetInternal(index, new base::Value(value.ToString()));
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
  auto list = mutable_value()->GetList();
  if (index >= list.size())
    return false;

  // The std::move() call below which removes the Value from the list will
  // return a new Value object with the moved contents of the Value that exists
  // in the implementation std::vector. Consequently we use Get() to retrieve
  // the actual Value pointer as it current exists first, for later comparison
  // purposes.
  const base::Value* actual_value = nullptr;
  if (!const_value().Get(index, &actual_value) || !actual_value)
    return false;

  // |actual_value| is no longer valid after this call.
  auto out_value = std::move(list[index]);
  mutable_value()->EraseListIter(list.begin() + index);

  // Remove the value.
  controller()->Remove(const_cast<base::Value*>(actual_value), true);

  // Only list and dictionary types may have dependencies.
  if (out_value.is_list() || out_value.is_dict()) {
    controller()->RemoveDependencies(const_cast<base::Value*>(actual_value));
  }

  return true;
}

base::Value* CefListValueImpl::SetInternal(size_t index, base::Value* value) {
  DCHECK(value);

  auto list = mutable_value()->GetList();
  if (RemoveInternal(index)) {
    CHECK_LE(index, list.size());
    mutable_value()->Insert(list.begin() + index, std::move(*value));
  } else {
    if (index >= list.size()) {
      // Expand the list size.
      // TODO: This approach seems inefficient. See
      // https://crbug.com/1187066#c17 for background.
      while (list.size() <= index)
        mutable_value()->Append(base::Value());
    }
    list[index] = std::move(*value);
  }

  // base::Value now uses move semantics which means that Insert()/Set() will
  // move the contents of the passed-in base::Value instead of keeping the same
  // object. Consequently we use Get() to retrieve the actual base::Value
  // pointer as it exists in the std::vector.
  const base::Value* actual_value = nullptr;
  const_value().Get(index, &actual_value);
  DCHECK(actual_value);

  // |value| will have been deleted at this point. Update the controller to
  // reference |actual_value| instead.
  controller()->Swap(value, const_cast<base::Value*>(actual_value));

  return const_cast<base::Value*>(actual_value);
}

CefListValueImpl::CefListValueImpl(base::ListValue* value,
                                   void* parent_value,
                                   ValueMode value_mode,
                                   bool read_only,
                                   CefValueController* controller)
    : CefValueBase<CefListValue, base::ListValue>(value,
                                                  parent_value,
                                                  value_mode,
                                                  read_only,
                                                  controller) {}
