// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "ctocpp/variant_ctocpp.h"


CefVariant::Type CefVariantCToCpp::GetType()
{
  if(CEF_MEMBER_MISSING(struct_, get_type))
    return VARIANT_TYPE_NULL;

  return struct_->get_type(struct_);
}

void CefVariantCToCpp::SetNull()
{
  if(CEF_MEMBER_MISSING(struct_, set_null))
    return;

  return struct_->set_null(struct_);
}

void CefVariantCToCpp::SetBool(bool val)
{
  if(CEF_MEMBER_MISSING(struct_, set_bool))
    return;

  return struct_->set_bool(struct_, val);
}

void CefVariantCToCpp::SetInt(int val)
{
  if(CEF_MEMBER_MISSING(struct_, set_int))
    return;

  return struct_->set_int(struct_, val);
}

void CefVariantCToCpp::SetDouble(double val)
{
  if(CEF_MEMBER_MISSING(struct_, set_double))
    return;

  return struct_->set_double(struct_, val);
}

void CefVariantCToCpp::SetString(const std::wstring& val)
{
  if(CEF_MEMBER_MISSING(struct_, set_string))
    return;

  return struct_->set_string(struct_, val.c_str());
}

void CefVariantCToCpp::SetBoolArray(const std::vector<bool>& val)
{
  if(CEF_MEMBER_MISSING(struct_, set_bool_array))
    return;

  int valSize = (int)val.size();
  int* valArray = NULL;
  if(valSize > 0) {
    valArray = new int[valSize];
    if(!valArray)
      return;
    for(int i = 0; i < valSize; ++i) {
      valArray[i] = val[i];
    }
  }

  struct_->set_bool_array(struct_, valSize, valArray);

  if(valArray)
    delete [] valArray;
}

void CefVariantCToCpp::SetIntArray(const std::vector<int>& val)
{
  if(CEF_MEMBER_MISSING(struct_, set_int_array))
    return;

  int valSize = (int)val.size();
  int* valArray = NULL;
  if(valSize > 0) {
    valArray = new int[valSize];
    if(!valArray)
      return;
    for(int i = 0; i < valSize; ++i) {
      valArray[i] = val[i];
    }
  }

  struct_->set_int_array(struct_, valSize, valArray);

  if(valArray)
    delete [] valArray;
}

void CefVariantCToCpp::SetDoubleArray(const std::vector<double>& val)
{
  if(CEF_MEMBER_MISSING(struct_, set_double_array))
    return;

  int valSize = (int)val.size();
  double* valArray = NULL;
  if(valSize > 0) {
    valArray = new double[valSize];
    if(!valArray)
      return;
    for(int i = 0; i < valSize; ++i) {
      valArray[i] = val[i];
    }
  }

  struct_->set_double_array(struct_, valSize, valArray);

  if(valArray)
    delete [] valArray;
}

void CefVariantCToCpp::SetStringArray(const std::vector<std::wstring>& val)
{
  if(CEF_MEMBER_MISSING(struct_, set_string_array))
    return;

  int valSize = (int)val.size();
  cef_string_t* valArray = NULL;
  if(valSize > 0) {
    valArray = new cef_string_t[valSize];
    if(!valArray)
      return;
    for(int i = 0; i < valSize; ++i) {
      valArray[i] = cef_string_alloc(val[i].c_str());
    }
  }

  struct_->set_string_array(struct_, valSize, valArray);

  if(valArray) {
    for(int i = 0; i < valSize; ++i) {
      cef_string_free(valArray[i]);
    }
    delete [] valArray;
  }
}

bool CefVariantCToCpp::GetBool()
{
  if(CEF_MEMBER_MISSING(struct_, get_bool))
    return false;

  return struct_->get_bool(struct_);  
}

int CefVariantCToCpp::GetInt()
{
  if(CEF_MEMBER_MISSING(struct_, get_int))
    return 0;

  return struct_->get_int(struct_);  
}

double CefVariantCToCpp::GetDouble()
{
  if(CEF_MEMBER_MISSING(struct_, get_double))
    return 0;

  return struct_->get_double(struct_);  
}

std::wstring CefVariantCToCpp::GetString()
{
  std::wstring str;
  if(CEF_MEMBER_MISSING(struct_, get_string))
    return str;

  cef_string_t cef_str = struct_->get_string(struct_);  
  if(cef_str) {
    str = cef_str;
    cef_string_free(cef_str);
  }

  return str;
}

bool CefVariantCToCpp::GetBoolArray(std::vector<bool>& val)
{
  if(CEF_MEMBER_MISSING(struct_, get_bool_array))
    return false;

  int valSize = GetArraySize();
  if(valSize < 0)
    return false;
  if(valSize == 0)
    return true;

  int* valArray = new int[valSize];
  if(!valArray)
    return false;

  bool rv = struct_->get_bool_array(struct_, valSize, valArray);
  if(rv) {
    for(int i = 0; i < valSize; ++i)
      val.push_back(valArray[i] ? true : false);
  }
  delete [] valArray;

  return rv;
}

bool CefVariantCToCpp::GetIntArray(std::vector<int>& val)
{
  if(CEF_MEMBER_MISSING(struct_, get_int_array))
    return false;

  int valSize = GetArraySize();
  if(valSize < 0)
    return false;
  if(valSize == 0)
    return true;

  int* valArray = new int[valSize];
  if(!valArray)
    return false;

  bool rv = struct_->get_int_array(struct_, valSize, valArray);
  if(rv) {
    for(int i = 0; i < valSize; ++i)
      val.push_back(valArray[i]);
  }
  delete [] valArray;

  return rv;
}

bool CefVariantCToCpp::GetDoubleArray(std::vector<double>& val)
{
  if(CEF_MEMBER_MISSING(struct_, get_double_array))
    return false;

  int valSize = GetArraySize();
  if(valSize < 0)
    return false;
  if(valSize == 0)
    return true;

  double* valArray = new double[valSize];
  if(!valArray)
    return false;

  bool rv = struct_->get_double_array(struct_, valSize, valArray);
  if(rv) {
    for(int i = 0; i < valSize; ++i)
      val.push_back(valArray[i]);
  }
  delete [] valArray;

  return rv;
}

bool CefVariantCToCpp::GetStringArray(std::vector<std::wstring>& val)
{
  if(CEF_MEMBER_MISSING(struct_, get_string_array))
    return false;

  int valSize = GetArraySize();
  if(valSize < 0)
    return false;
  if(valSize == 0)
    return true;

  cef_string_t* valArray = new cef_string_t[valSize];
  if(!valArray)
    return false;

  bool rv = struct_->get_string_array(struct_, valSize, valArray);
  if(rv) {
    for(int i = 0; i < valSize; ++i) {
      val.push_back(valArray[i]);
      cef_string_free(valArray[i]);
    }
  }
  delete [] valArray;

  return rv;
}

int CefVariantCToCpp::GetArraySize()
{
  if(CEF_MEMBER_MISSING(struct_, get_array_size))
    return -1;

  return struct_->get_array_size(struct_);
}

#ifdef _DEBUG
long CefCToCpp<CefVariant, cef_variant_t>::DebugObjCt = 0;
#endif
