// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _VARIANT_IMPL_H
#define _VARIANT_IMPL_H

#include "../include/cef.h"
#include "third_party/npapi/bindings/npruntime.h"

class WebFrame;

// Implementation of CefPostDataElement that provides a class wrapper for an
// NPVariant structure.
// Portions of the implementation are borrowed from webkit\glue\cpp_variant.cc
class CefVariantImpl : public CefThreadSafeBase<CefVariant>
{
public:
  CefVariantImpl();
  CefVariantImpl(WebFrame *webframe);
  ~CefVariantImpl();

  virtual Type GetType();
  virtual void SetNull();
  virtual void SetBool(bool val);
  virtual void SetInt(int val);
  virtual void SetDouble(double val);
  virtual void SetString(const std::wstring& val);
  virtual void SetBoolArray(const std::vector<bool>& val);
  virtual void SetIntArray(const std::vector<int>& val);
  virtual void SetDoubleArray(const std::vector<double>& val);
  virtual void SetStringArray(const std::vector<std::wstring>& val);
  virtual bool GetBool();
  virtual int GetInt();
  virtual double GetDouble();
  virtual std::wstring GetString();
  virtual bool GetBoolArray(std::vector<bool>& val);
  virtual bool GetIntArray(std::vector<int>& val);
  virtual bool GetDoubleArray(std::vector<double>& val);
  virtual bool GetStringArray(std::vector<std::wstring>& val);

  // These three methods all perform deep copies of any string data.  This
  // allows the local CefVariantImpl to be released by the destructor without
  // corrupting their sources. In performance-critical code, or when strings
  // are very long, avoid creating new CefVariantImpl.
  // In case of NPObject as the data, the copying involves ref-counting
  // as opposed to deep-copying. The ref-counting ensures that sources don't
  // get corrupted when the copies get destroyed.
  void CopyToNPVariant(NPVariant* result);
  CefVariantImpl& operator=(const CefVariantImpl& original);
  CefVariantImpl(const CefVariantImpl& original);

  // Note that setting a CefVariant to an NPObject involves ref-counting
  // the actual object. SetNull() should only be called if the CefVariant
  // is no longer needed. The other Set() methods handle this internally.
  // Also, the object's NPClass is expected to be a static object: neither
  // the NP runtime nor CefVariant will ever free it.
  void Set(NPObject* val);

  void Set(const NPString& val);
  void Set(const NPVariant& val);

  const NPVariant* GetNPVariant() const { return &variant_; }

protected:
  // Underlying NPVariant structure.
  NPVariant variant_;

  // Pointer to the WebFrame that represents the context for this CefVariant
  // object.  This pointer is used for creating new NPObjects in the Set*()
  // methods that accept array arguments.
  WebFrame* webframe_;
};

#endif // _VARIANT_IMPL_H
