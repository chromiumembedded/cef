// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_VALUES_IMPL_H_
#define CEF_LIBCEF_COMMON_VALUES_IMPL_H_
#pragma once

#include <memory>
#include <vector>

#include "include/cef_values.h"
#include "libcef/common/value_base.h"

#include "base/threading/platform_thread.h"
#include "base/values.h"

// CefValue implementation
class CefValueImpl : public CefValue {
 public:
  // Get or create a reference to a complex value or copy a simple value.
  static CefRefPtr<CefValue> GetOrCreateRefOrCopy(
      base::Value* value,
      void* parent_value,
      bool read_only,
      CefValueController* controller);

  CefValueImpl();

  // Take ownership of |value|. Do not pass in a value owned by something else
  // (use GetOrCreateRefOrCopy instead).
  explicit CefValueImpl(base::Value value);

  // Keep a reference to |value|.
  explicit CefValueImpl(CefRefPtr<CefBinaryValue> value);
  explicit CefValueImpl(CefRefPtr<CefDictionaryValue> value);
  explicit CefValueImpl(CefRefPtr<CefListValue> value);

  CefValueImpl(const CefValueImpl&) = delete;
  CefValueImpl& operator=(const CefValueImpl&) = delete;

  ~CefValueImpl() override;

  // Take ownership of |value|. Do not pass in a value owned by something else
  // (use GetOrCreateRefOrCopy or Set*() instead).
  void SetValue(base::Value value);

  // Return a copy of the value.
  [[nodiscard]] base::Value CopyValue();

  // Copy a simple value or transfer ownership of a complex value. If ownership
  // of the value is tranferred then this object's internal reference to the
  // value will be updated and remain valid. base::Value now uses move semantics
  // so we need to perform the copy and swap in two steps.
  [[nodiscard]] std::unique_ptr<base::Value> CopyOrDetachValue(
      CefValueController* new_controller);
  void SwapValue(base::Value* new_value,
                 void* new_parent_value,
                 CefValueController* new_controller);

  // Returns a reference to the underlying data. Access must be protected by
  // calling AcquireLock/ReleaseLock (e.g. use ScopedLockedValue).
  base::Value* GetValueUnsafe() const;

  // CefValue methods.
  bool IsValid() override;
  bool IsOwned() override;
  bool IsReadOnly() override;
  bool IsSame(CefRefPtr<CefValue> that) override;
  bool IsEqual(CefRefPtr<CefValue> that) override;
  CefRefPtr<CefValue> Copy() override;
  CefValueType GetType() override;
  bool GetBool() override;
  int GetInt() override;
  double GetDouble() override;
  CefString GetString() override;
  CefRefPtr<CefBinaryValue> GetBinary() override;
  CefRefPtr<CefDictionaryValue> GetDictionary() override;
  CefRefPtr<CefListValue> GetList() override;
  bool SetNull() override;
  bool SetBool(bool value) override;
  bool SetInt(int value) override;
  bool SetDouble(double value) override;
  bool SetString(const CefString& value) override;
  bool SetBinary(CefRefPtr<CefBinaryValue> value) override;
  bool SetDictionary(CefRefPtr<CefDictionaryValue> value) override;
  bool SetList(CefRefPtr<CefListValue> value) override;

  // Ensures exclusive access to the underlying data for the life of this scoped
  // object.
  class ScopedLockedValue {
   public:
    explicit ScopedLockedValue(CefRefPtr<CefValueImpl> impl) : impl_(impl) {
      impl_->AcquireLock();
    }

    ScopedLockedValue(const ScopedLockedValue&) = delete;
    ScopedLockedValue& operator=(const ScopedLockedValue&) = delete;

    ~ScopedLockedValue() { impl_->ReleaseLock(); }

    base::Value* value() const { return impl_->GetValueUnsafe(); }

   private:
    CefRefPtr<CefValueImpl> impl_;
  };

 private:
  void SetValueInternal(absl::optional<base::Value> value);

  // Returns the controller for the current value, if any.
  CefValueController* GetValueController() const;

  // Explicitly lock/unlock this object and the underlying data.
  void AcquireLock();
  void ReleaseLock();

  // Access to all members must be protected by |lock_|.
  base::Lock lock_;

  // Simple values only.
  std::unique_ptr<base::Value> value_;

  // Complex values.
  CefRefPtr<CefBinaryValue> binary_value_;
  CefRefPtr<CefDictionaryValue> dictionary_value_;
  CefRefPtr<CefListValue> list_value_;

  IMPLEMENT_REFCOUNTING(CefValueImpl);
};

// CefBinaryValue implementation
class CefBinaryValueImpl : public CefValueBase<CefBinaryValue, base::Value> {
 public:
  // Get or create a reference value.
  static CefRefPtr<CefBinaryValue> GetOrCreateRef(
      base::Value* value,
      void* parent_value,
      CefValueController* controller);

  // Take ownership of |value|. Do not pass in a value owned by something else
  // (use GetOrCreateRef or constructor variant with |will_delete| argument).
  explicit CefBinaryValueImpl(base::Value value);

  // Reference an existing value (set |will_delete| to false) or take ownership
  // of an existing value (set |will_delete| to true). When referencing an
  // existing value you must explicitly call Detach(nullptr) when |value| is no
  // longer valid. Use GetOrCreateRef instead of this constructor if |value| is
  // owned by some other object and you do not plan to explicitly call
  // Detach(nullptr).
  CefBinaryValueImpl(base::Value* value, bool will_delete);

  // The data will always be copied.
  explicit CefBinaryValueImpl(base::span<const uint8_t> value);

  CefBinaryValueImpl(const CefBinaryValueImpl&) = delete;
  CefBinaryValueImpl& operator=(const CefBinaryValueImpl&) = delete;

  // Return a copy of the value.
  [[nodiscard]] base::Value CopyValue();

  // If this value is a reference then return a copy. Otherwise, detach and
  // transfer ownership of the value.
  [[nodiscard]] std::unique_ptr<base::Value> CopyOrDetachValue(
      CefValueController* new_controller);

  bool IsSameValue(const base::Value* that);
  bool IsEqualValue(const base::Value* that);

  // Returns the underlying value. Access must be protected by calling
  // lock/unlock on the controller.
  base::Value* GetValueUnsafe() const;

  // CefBinaryValue methods.
  bool IsValid() override;
  bool IsOwned() override;
  bool IsSame(CefRefPtr<CefBinaryValue> that) override;
  bool IsEqual(CefRefPtr<CefBinaryValue> that) override;
  CefRefPtr<CefBinaryValue> Copy() override;
  const void* GetRawData() override;
  size_t GetSize() override;
  size_t GetData(void* buffer, size_t buffer_size, size_t data_offset) override;

 private:
  // See the CefValueBase constructor for usage. Binary values are always
  // read-only.
  CefBinaryValueImpl(base::Value* value,
                     void* parent_value,
                     ValueMode value_mode,
                     CefValueController* controller);
};

// CefDictionaryValue implementation
class CefDictionaryValueImpl
    : public CefValueBase<CefDictionaryValue, base::Value> {
 public:
  // Get or create a reference value.
  static CefRefPtr<CefDictionaryValue> GetOrCreateRef(
      base::Value* value,
      void* parent_value,
      bool read_only,
      CefValueController* controller);

  // Take ownership of |value|. Do not pass in a value owned by something else
  // (use GetOrCreateRef or constructor variant with |will_delete| argument).
  CefDictionaryValueImpl(base::Value value, bool read_only);
  CefDictionaryValueImpl(base::Value::Dict value, bool read_only);

  // Reference an existing value (set |will_delete| to false) or take ownership
  // of an existing value (set |will_delete| to true). When referencing an
  // existing value you must explicitly call Detach(nullptr) when |value| is no
  // longer valid. Use GetOrCreateRef instead of this constructor if |value| is
  // owned by some other object and you do not plan to explicitly call
  // Detach(nullptr).
  CefDictionaryValueImpl(base::Value* value, bool will_delete, bool read_only);

  CefDictionaryValueImpl(const CefDictionaryValueImpl&) = delete;
  CefDictionaryValueImpl& operator=(const CefDictionaryValueImpl&) = delete;

  // Return a copy of the value.
  [[nodiscard]] base::Value CopyValue();

  // If this value is a reference then return a copy. Otherwise, detach and
  // transfer ownership of the value.
  [[nodiscard]] std::unique_ptr<base::Value> CopyOrDetachValue(
      CefValueController* new_controller);

  bool IsSameValue(const base::Value* that);
  bool IsEqualValue(const base::Value* that);

  // Returns the underlying value. Access must be protected by calling
  // lock/unlock on the controller.
  base::Value* GetValueUnsafe() const;

  // CefDictionaryValue methods.
  bool IsValid() override;
  bool IsOwned() override;
  bool IsReadOnly() override;
  bool IsSame(CefRefPtr<CefDictionaryValue> that) override;
  bool IsEqual(CefRefPtr<CefDictionaryValue> that) override;
  CefRefPtr<CefDictionaryValue> Copy(bool exclude_empty_children) override;
  size_t GetSize() override;
  bool Clear() override;
  bool HasKey(const CefString& key) override;
  bool GetKeys(KeyList& keys) override;
  bool Remove(const CefString& key) override;
  CefValueType GetType(const CefString& key) override;
  CefRefPtr<CefValue> GetValue(const CefString& key) override;
  bool GetBool(const CefString& key) override;
  int GetInt(const CefString& key) override;
  double GetDouble(const CefString& key) override;
  CefString GetString(const CefString& key) override;
  CefRefPtr<CefBinaryValue> GetBinary(const CefString& key) override;
  CefRefPtr<CefDictionaryValue> GetDictionary(const CefString& key) override;
  CefRefPtr<CefListValue> GetList(const CefString& key) override;
  bool SetValue(const CefString& key, CefRefPtr<CefValue> value) override;
  bool SetNull(const CefString& key) override;
  bool SetBool(const CefString& key, bool value) override;
  bool SetInt(const CefString& key, int value) override;
  bool SetDouble(const CefString& key, double value) override;
  bool SetString(const CefString& key, const CefString& value) override;
  bool SetBinary(const CefString& key,
                 CefRefPtr<CefBinaryValue> value) override;
  bool SetDictionary(const CefString& key,
                     CefRefPtr<CefDictionaryValue> value) override;
  bool SetList(const CefString& key, CefRefPtr<CefListValue> value) override;

 private:
  // See the CefValueBase constructor for usage.
  CefDictionaryValueImpl(base::Value* value,
                         void* parent_value,
                         ValueMode value_mode,
                         bool read_only,
                         CefValueController* controller);

  bool RemoveInternal(const CefString& key);
  base::Value* SetInternal(const CefString& key,
                           std::unique_ptr<base::Value> value);
};

// CefListValue implementation
class CefListValueImpl : public CefValueBase<CefListValue, base::Value> {
 public:
  // Get or create a reference value.
  static CefRefPtr<CefListValue> GetOrCreateRef(base::Value* value,
                                                void* parent_value,
                                                bool read_only,
                                                CefValueController* controller);

  // Take ownership of |value|. Do not pass in a value owned by something else
  // (use GetOrCreateRef or constructor variant with |will_delete| argument).
  CefListValueImpl(base::Value value, bool read_only);
  CefListValueImpl(base::Value::List value, bool read_only);

  // Reference an existing value (set |will_delete| to false) or take ownership
  // of an existing value (set |will_delete| to true). When referencing an
  // existing value you must explicitly call Detach(nullptr) when |value| is no
  // longer valid. Use GetOrCreateRef instead of this constructor if |value| is
  // owned by some other object and you do not plan to explicitly call
  // Detach(nullptr).
  CefListValueImpl(base::Value* value, bool will_delete, bool read_only);

  CefListValueImpl(const CefListValueImpl&) = delete;
  CefListValueImpl& operator=(const CefListValueImpl&) = delete;

  // Return a copy of the value.
  [[nodiscard]] base::Value CopyValue();

  // If this value is a reference then return a copy. Otherwise, detach and
  // transfer ownership of the value.
  [[nodiscard]] std::unique_ptr<base::Value> CopyOrDetachValue(
      CefValueController* new_controller);

  bool IsSameValue(const base::Value* that);
  bool IsEqualValue(const base::Value* that);

  // Returns the underlying value. Access must be protected by calling
  // lock/unlock on the controller.
  base::Value* GetValueUnsafe() const;

  // CefListValue methods.
  bool IsValid() override;
  bool IsOwned() override;
  bool IsReadOnly() override;
  bool IsSame(CefRefPtr<CefListValue> that) override;
  bool IsEqual(CefRefPtr<CefListValue> that) override;
  CefRefPtr<CefListValue> Copy() override;
  bool SetSize(size_t size) override;
  size_t GetSize() override;
  bool Clear() override;
  bool Remove(size_t index) override;
  CefValueType GetType(size_t index) override;
  CefRefPtr<CefValue> GetValue(size_t index) override;
  bool GetBool(size_t index) override;
  int GetInt(size_t index) override;
  double GetDouble(size_t index) override;
  CefString GetString(size_t index) override;
  CefRefPtr<CefBinaryValue> GetBinary(size_t index) override;
  CefRefPtr<CefDictionaryValue> GetDictionary(size_t index) override;
  CefRefPtr<CefListValue> GetList(size_t index) override;
  bool SetValue(size_t index, CefRefPtr<CefValue> value) override;
  bool SetNull(size_t index) override;
  bool SetBool(size_t index, bool value) override;
  bool SetInt(size_t index, int value) override;
  bool SetDouble(size_t index, double value) override;
  bool SetString(size_t index, const CefString& value) override;
  bool SetBinary(size_t index, CefRefPtr<CefBinaryValue> value) override;
  bool SetDictionary(size_t index,
                     CefRefPtr<CefDictionaryValue> value) override;
  bool SetList(size_t index, CefRefPtr<CefListValue> value) override;

 private:
  // See the CefValueBase constructor for usage.
  CefListValueImpl(base::Value* value,
                   void* parent_value,
                   ValueMode value_mode,
                   bool read_only,
                   CefValueController* controller);

  bool RemoveInternal(size_t index);
  base::Value* SetInternal(size_t index, std::unique_ptr<base::Value> value);
};

#endif  // CEF_LIBCEF_COMMON_VALUES_IMPL_H_
