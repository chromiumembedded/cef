// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_VALUES_IMPL_H_
#define CEF_LIBCEF_COMMON_VALUES_IMPL_H_
#pragma once

#include <vector>

#include "include/cef_values.h"
#include "libcef/common/value_base.h"

#include "base/values.h"
#include "base/threading/platform_thread.h"


// CefBinaryValue implementation
class CefBinaryValueImpl
    : public CefValueBase<CefBinaryValue, base::BinaryValue> {
 public:
  // Get or create a reference value.
  static CefRefPtr<CefBinaryValue> GetOrCreateRef(
      base::BinaryValue* value,
      void* parent_value,
      CefValueController* controller);

  // Simple constructor for referencing existing value objects.
  CefBinaryValueImpl(base::BinaryValue* value,
                     bool will_delete,
                     bool read_only);

  // Return a copy of the value.
  base::BinaryValue* CopyValue();

  // If a reference return a copy of the value otherwise detach the value to the
  // specified |new_controller|.
  base::BinaryValue* CopyOrDetachValue(CefValueController* new_controller);

  // CefBinaryValue methods.
  bool IsValid() override;
  bool IsOwned() override;
  CefRefPtr<CefBinaryValue> Copy() override;
  size_t GetSize() override;
  size_t GetData(void* buffer,
                 size_t buffer_size,
                 size_t data_offset) override;

 private:
  // See the CefValueBase constructor for usage. Binary values are always
  // read-only.
  CefBinaryValueImpl(base::BinaryValue* value,
                     void* parent_value,
                     ValueMode value_mode,
                     CefValueController* controller);
  // If |copy| is false this object will take ownership of the specified |data|
  // buffer instead of copying it.
  CefBinaryValueImpl(char* data,
                     size_t data_size,
                     bool copy);

  // For the Create() method.
  friend class CefBinaryValue;

  DISALLOW_COPY_AND_ASSIGN(CefBinaryValueImpl);
};


// CefDictionaryValue implementation
class CefDictionaryValueImpl
    : public CefValueBase<CefDictionaryValue, base::DictionaryValue> {
 public:
  // Get or create a reference value.
  static CefRefPtr<CefDictionaryValue> GetOrCreateRef(
      base::DictionaryValue* value,
      void* parent_value,
      bool read_only,
      CefValueController* controller);

  // Simple constructor for referencing existing value objects.
  CefDictionaryValueImpl(base::DictionaryValue* value,
                         bool will_delete,
                         bool read_only);

  // Return a copy of the value.
  base::DictionaryValue* CopyValue();

  // If a reference return a copy of the value otherwise detach the value to the
  // specified |new_controller|.
  base::DictionaryValue* CopyOrDetachValue(CefValueController* new_controller);

  // CefDictionaryValue methods.
  bool IsValid() override;
  bool IsOwned() override;
  bool IsReadOnly() override;
  CefRefPtr<CefDictionaryValue> Copy(
      bool exclude_empty_children) override;
  size_t GetSize() override;
  bool Clear() override;
  bool HasKey(const CefString& key) override;
  bool GetKeys(KeyList& keys) override;
  bool Remove(const CefString& key) override;
  CefValueType GetType(const CefString& key) override;
  bool GetBool(const CefString& key) override;
  int GetInt(const CefString& key) override;
  double GetDouble(const CefString& key) override;
  CefString GetString(const CefString& key) override;
  CefRefPtr<CefBinaryValue> GetBinary(const CefString& key) override;
  CefRefPtr<CefDictionaryValue> GetDictionary(
      const CefString& key) override;
  CefRefPtr<CefListValue> GetList(const CefString& key) override;
  bool SetNull(const CefString& key) override;
  bool SetBool(const CefString& key, bool value) override;
  bool SetInt(const CefString& key, int value) override;
  bool SetDouble(const CefString& key, double value) override;
  bool SetString(const CefString& key,
                 const CefString& value) override;
  bool SetBinary(const CefString& key,
      CefRefPtr<CefBinaryValue> value) override;
  bool SetDictionary(const CefString& key,
                     CefRefPtr<CefDictionaryValue> value) override;
  bool SetList(const CefString& key,
               CefRefPtr<CefListValue> value) override;

 private:
  // See the CefValueBase constructor for usage.
  CefDictionaryValueImpl(base::DictionaryValue* value,
                         void* parent_value,
                         ValueMode value_mode,
                         bool read_only,
                         CefValueController* controller);

  bool RemoveInternal(const CefString& key);

  // For the Create() method.
  friend class CefDictionaryValue;

  DISALLOW_COPY_AND_ASSIGN(CefDictionaryValueImpl);
};


// CefListValue implementation
class CefListValueImpl
    : public CefValueBase<CefListValue, base::ListValue> {
 public:
  // Get or create a reference value.
  static CefRefPtr<CefListValue> GetOrCreateRef(
      base::ListValue* value,
      void* parent_value,
      bool read_only,
      CefValueController* controller);

  // Simple constructor for referencing existing value objects.
  CefListValueImpl(base::ListValue* value,
                   bool will_delete,
                   bool read_only);

  // Return a copy of the value.
  base::ListValue* CopyValue();

  // If a reference return a copy of the value otherwise detach the value to the
  // specified |new_controller|.
  base::ListValue* CopyOrDetachValue(CefValueController* new_controller);

  /// CefListValue methods.
  bool IsValid() override;
  bool IsOwned() override;
  bool IsReadOnly() override;
  CefRefPtr<CefListValue> Copy() override;
  bool SetSize(size_t size) override;
  size_t GetSize() override;
  bool Clear() override;
  bool Remove(int index) override;
  CefValueType GetType(int index) override;
  bool GetBool(int index) override;
  int GetInt(int index) override;
  double GetDouble(int index) override;
  CefString GetString(int index) override;
  CefRefPtr<CefBinaryValue> GetBinary(int index) override;
  CefRefPtr<CefDictionaryValue> GetDictionary(int index) override;
  CefRefPtr<CefListValue> GetList(int index) override;
  bool SetNull(int index) override;
  bool SetBool(int index, bool value) override;
  bool SetInt(int index, int value) override;
  bool SetDouble(int index, double value) override;
  bool SetString(int index, const CefString& value) override;
  bool SetBinary(int index, CefRefPtr<CefBinaryValue> value) override;
  bool SetDictionary(int index,
                     CefRefPtr<CefDictionaryValue> value) override;
  bool SetList(int index, CefRefPtr<CefListValue> value) override;

 private:
  // See the CefValueBase constructor for usage.
  CefListValueImpl(base::ListValue* value,
                   void* parent_value,
                   ValueMode value_mode,
                   bool read_only,
                   CefValueController* controller);

  bool RemoveInternal(int index);

  // For the Create() method.
  friend class CefListValue;

  DISALLOW_COPY_AND_ASSIGN(CefListValueImpl);
};


#endif  // CEF_LIBCEF_COMMON_VALUES_IMPL_H_
