// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _VARIANT_CTOCPP_H
#define _VARIANT_CTOCPP_H

#ifndef USING_CEF_SHARED
#pragma message("Warning: "__FILE__" may be accessed wrapper-side only")
#else // USING_CEF_SHARED

#include "cef.h"
#include "cef_capi.h"
#include "ctocpp.h"


// Wrap a C variant structure with a C++ variant class.
// This class may be instantiated and accessed wrapper-side only.
class CefVariantCToCpp : public CefCToCpp<CefVariant, cef_variant_t>
{
public:
  CefVariantCToCpp(cef_variant_t* str)
    : CefCToCpp<CefVariant, cef_variant_t>(str) {}
  virtual ~CefVariantCToCpp() {}

  // CefVariant methods
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
  virtual int GetArraySize();
};


#endif // USING_CEF_SHARED
#endif // _VARIANT_CTOCPP_H
