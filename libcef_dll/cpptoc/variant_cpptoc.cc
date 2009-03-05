// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cpptoc/variant_cpptoc.h"
#include "base/logging.h"


cef_variant_type_t CEF_CALLBACK variant_get_type(struct _cef_variant_t* variant)
{
  DCHECK(variant);
  if(!variant)
    return VARIANT_TYPE_NULL;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  return impl->class_->GetClass()->GetType();
}

void CEF_CALLBACK variant_set_null(struct _cef_variant_t* variant)
{
  DCHECK(variant);
  if(!variant)
    return;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  impl->class_->GetClass()->SetNull();
}

void CEF_CALLBACK variant_set_bool(struct _cef_variant_t* variant, int val)
{
  DCHECK(variant);
  if(!variant)
    return;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  impl->class_->GetClass()->SetBool(val ? true : false);
}

void CEF_CALLBACK variant_set_int(struct _cef_variant_t* variant, int val)
{
  DCHECK(variant);
  if(!variant)
    return;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  impl->class_->GetClass()->SetInt(val);
}

void CEF_CALLBACK variant_set_double(struct _cef_variant_t* variant, double val)
{
  DCHECK(variant);
  if(!variant)
    return;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  impl->class_->GetClass()->SetDouble(val);
}

void CEF_CALLBACK variant_set_string(struct _cef_variant_t* variant,
                                     const wchar_t* val)
{
  DCHECK(variant);
  if(!variant)
    return;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  impl->class_->GetClass()->SetString(val);
}

void CEF_CALLBACK variant_set_bool_array(struct _cef_variant_t* variant,
                                         size_t count, const int* vals)
{
  DCHECK(variant);
  if(!variant)
    return;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  
  std::vector<bool> vec;
  for(size_t i = 0; i < count; ++i)
    vec.push_back(vals[i] ? true : false);
  
  impl->class_->GetClass()->SetBoolArray(vec);
}

void CEF_CALLBACK variant_set_int_array(struct _cef_variant_t* variant,
                                        size_t count, const int* vals)
{
  DCHECK(variant);
  if(!variant)
    return;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  
  std::vector<int> vec;
  for(size_t i = 0; i < count; ++i)
    vec.push_back(vals[i]);
  
  impl->class_->GetClass()->SetIntArray(vec);
}

void CEF_CALLBACK variant_set_double_array(struct _cef_variant_t* variant,
                                           size_t count, const double* vals)
{
  DCHECK(variant);
  if(!variant)
    return;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  
  std::vector<double> vec;
  for(size_t i = 0; i < count; ++i)
    vec.push_back(vals[i]);
  
  impl->class_->GetClass()->SetDoubleArray(vec);
}

void CEF_CALLBACK variant_set_string_array(struct _cef_variant_t* variant,
                                           size_t count,
                                           const cef_string_t* vals)
{
  DCHECK(variant);
  if(!variant)
    return;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  
  std::vector<std::wstring> vec;
  for(size_t i = 0; i < count; ++i)
    vec.push_back(vals[i] ? vals[i] : std::wstring());
  
  impl->class_->GetClass()->SetStringArray(vec);
}


int CEF_CALLBACK variant_get_bool(struct _cef_variant_t* variant)
{
  DCHECK(variant);
  if(!variant)
    return 0;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  return impl->class_->GetClass()->GetBool();
}

int CEF_CALLBACK variant_get_int(struct _cef_variant_t* variant)
{
  DCHECK(variant);
  if(!variant)
    return 0;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  return impl->class_->GetClass()->GetInt();
}

double CEF_CALLBACK variant_get_double(struct _cef_variant_t* variant)
{
  DCHECK(variant);
  if(!variant)
    return 0;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  return impl->class_->GetClass()->GetDouble();
}

cef_string_t CEF_CALLBACK variant_get_string(struct _cef_variant_t* variant)
{
  DCHECK(variant);
  if(!variant)
    return NULL;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  
  std::wstring str;
  str = impl->class_->GetClass()->GetString();
  if(str.empty())
    return NULL;
  return cef_string_alloc(str.c_str());
}

int CEF_CALLBACK variant_get_array_size(struct _cef_variant_t* variant)
{
  DCHECK(variant);
  if(!variant)
    return 0;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  return impl->class_->GetClass()->GetArraySize();
}

size_t CEF_CALLBACK variant_get_bool_array(struct _cef_variant_t* variant,
                                           size_t maxcount, int* vals)
{
  DCHECK(variant);
  if(!variant)
    return 0;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  
  std::vector<bool> vec;
  impl->class_->GetClass()->GetBoolArray(vec);

  size_t ct = 0;
  for(; ct < maxcount && ct < vec.size(); ++ct)
    vals[ct] = vec[ct];
  return ct;
}

size_t CEF_CALLBACK variant_get_int_array(struct _cef_variant_t* variant,
                                          size_t maxcount, int* vals)
{
  DCHECK(variant);
  if(!variant)
    return 0;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  
  std::vector<int> vec;
  impl->class_->GetClass()->GetIntArray(vec);

  size_t ct = 0;
  for(; ct < maxcount && ct < vec.size(); ++ct)
    vals[ct] = vec[ct];
  return ct;
}

size_t CEF_CALLBACK variant_get_double_array(struct _cef_variant_t* variant,
                                             size_t maxcount, double* vals)
{
  DCHECK(variant);
  if(!variant)
    return 0;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  
  std::vector<double> vec;
  impl->class_->GetClass()->GetDoubleArray(vec);

  size_t ct = 0;
  for(; ct < maxcount && ct < vec.size(); ++ct)
    vals[ct] = vec[ct];
  return ct;
}

size_t CEF_CALLBACK variant_get_string_array(struct _cef_variant_t* variant,
                                             size_t maxcount,
                                             cef_string_t* vals)
{
  DCHECK(variant);
  if(!variant)
    return 0;

  CefVariantCppToC::Struct* impl =
      reinterpret_cast<CefVariantCppToC::Struct*>(variant);
  
  std::vector<std::wstring> vec;
  impl->class_->GetClass()->GetStringArray(vec);

  size_t ct = 0;
  for(; ct < maxcount && ct < vec.size(); ++ct)
    vals[ct] = cef_string_alloc(vec[ct].c_str());
  return ct;
}


CefVariantCppToC::CefVariantCppToC(CefRefPtr<CefVariant> cls)
    : CefCppToC<CefVariant, cef_variant_t>(cls)
{
  struct_.struct_.get_type = variant_get_type;
  struct_.struct_.set_null = variant_set_null;
  struct_.struct_.set_bool = variant_set_bool;
  struct_.struct_.set_int = variant_set_int;
  struct_.struct_.set_double = variant_set_double;
  struct_.struct_.set_string = variant_set_string;
  struct_.struct_.set_bool_array = variant_set_bool_array;
  struct_.struct_.set_int_array = variant_set_int_array;
  struct_.struct_.set_double_array = variant_set_double_array;
  struct_.struct_.set_string_array = variant_set_string_array;
  struct_.struct_.get_bool = variant_get_bool;
  struct_.struct_.get_int = variant_get_int;
  struct_.struct_.get_double = variant_get_double;
  struct_.struct_.get_string = variant_get_string;
  struct_.struct_.get_array_size = variant_get_array_size;
  struct_.struct_.get_bool_array = variant_get_bool_array;
  struct_.struct_.get_int_array = variant_get_int_array;
  struct_.struct_.get_double_array = variant_get_double_array;
  struct_.struct_.get_string_array = variant_get_string_array;
}
