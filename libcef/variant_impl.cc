// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "variant_impl.h"
#include "variant_np_util.h"

#include "base/compiler_specific.h"

#include "config.h"
MSVC_PUSH_WARNING_LEVEL(0);
#include "DOMWindow.h"
#include "Frame.h"
#include "npruntime_priv.h" // for NPN_InitializeVariantWithStringCopy
MSVC_POP_WARNING();

#undef LOG
#include "base/logging.h"
#include "base/string_util.h"
#include "webkit/glue/webframe.h"


CefVariantImpl::CefVariantImpl()
{
  variant_.type = NPVariantType_Null;
  webframe_ = NULL;
}

CefVariantImpl::CefVariantImpl(WebFrame *webframe)
{
  variant_.type = NPVariantType_Null;
  webframe_ = webframe;
}

// Note that Set() performs a deep copy, which is necessary to safely
// call SetNull() on the value in the destructor.
CefVariantImpl::CefVariantImpl(const CefVariantImpl& original)
{
  Lock();
  variant_.type = NPVariantType_Null;
  Set(*original.GetNPVariant());
  Unlock();
}

// See comment for copy constructor, above.
CefVariantImpl& CefVariantImpl::operator=(const CefVariantImpl& original)
{
  if (&original != this) {
    Lock();
    Set(*original.GetNPVariant());
    Unlock();
  }
  return *this;
}

CefVariantImpl::~CefVariantImpl()
{
  SetNull();
}

CefVariant::Type CefVariantImpl::GetType()
{
  CefVariant::Type type = VARIANT_TYPE_NULL;
  
  Lock();

  // determine the data type of the underlying NPVariant value
  switch (variant_.type) {
  case NPVariantType_Bool:
    type = VARIANT_TYPE_BOOL;
    break;
  case NPVariantType_Int32:
    type = VARIANT_TYPE_INT;
    break;
  case NPVariantType_Double:
    type = VARIANT_TYPE_DOUBLE;
    break;
  case NPVariantType_String:
    type = VARIANT_TYPE_STRING;
    break;
  case NPVariantType_Object:
    {
      // determine the most appropriate array type for the NPVariant object
      NPVariantType nptype;
      if(_NPN_ArrayObjectToVectorTypeHint(variant_.value.objectValue, nptype)) {
        switch(nptype) {
        case NPVariantType_Bool:
          type = VARIANT_TYPE_BOOL_ARRAY;
          break;
        case NPVariantType_Int32:
          type = VARIANT_TYPE_INT_ARRAY;
          break;
        case NPVariantType_Double:
          type = VARIANT_TYPE_DOUBLE_ARRAY;
          break;
        case NPVariantType_String:
          type = VARIANT_TYPE_STRING_ARRAY;
          break;
        }
      }
    }
    break;
  }

  Unlock();

  return type;
}

void CefVariantImpl::SetNull()
{
  Lock();
  NPN_ReleaseVariantValue(&variant_);
  variant_.type = NPVariantType_Null;
  Unlock();
}

void CefVariantImpl::SetBool(bool val)
{
  Lock();
  if(variant_.type != NPVariantType_Bool) {
    SetNull();
    variant_.type = NPVariantType_Bool;
  }
  variant_.value.boolValue = val;
  Unlock();
}

void CefVariantImpl::SetInt(int val)
{
  Lock();
  if(variant_.type != NPVariantType_Int32) {
    SetNull();
    variant_.type = NPVariantType_Int32;
  }
  variant_.value.intValue = val;
  Unlock();
}

void CefVariantImpl::SetDouble(double val)
{
  Lock();
  if(variant_.type != NPVariantType_Double) {
    SetNull();
    variant_.type = NPVariantType_Double;
  }
  variant_.value.doubleValue = val;
  Unlock();
}

void CefVariantImpl::SetString(const std::wstring& val)
{
  Lock();
  SetNull();
  variant_.type = NPVariantType_String;
  std::string str = WideToUTF8(val);
  NPString new_string = {str.c_str(),
                         static_cast<uint32_t>(str.size())};
  _NPN_InitializeVariantWithStringCopy(&variant_, &new_string);
  Unlock();
}

void CefVariantImpl::SetBoolArray(const std::vector<bool>& val)
{
  Lock();
  DCHECK(webframe_ != NULL);
  WebCore::Frame* frame =
      static_cast<WebCore::Frame*>(webframe_->GetFrameImplementation());
  WebCore::DOMWindow* domwindow = frame->domWindow();
  NPObject* npobject = _NPN_BooleanVectorToArrayObject(domwindow, val);
  DCHECK(npobject != NULL);
  Set(npobject);
  Unlock();
}

void CefVariantImpl::SetIntArray(const std::vector<int>& val)
{
  Lock();
  DCHECK(webframe_ != NULL);
  WebCore::Frame* frame =
      static_cast<WebCore::Frame*>(webframe_->GetFrameImplementation());
  WebCore::DOMWindow* domwindow = frame->domWindow();
  NPObject* npobject = _NPN_IntVectorToArrayObject(domwindow, val);
  DCHECK(npobject != NULL);
  Set(npobject);
  Unlock();
}

void CefVariantImpl::SetDoubleArray(const std::vector<double>& val)
{
  Lock();
  DCHECK(webframe_ != NULL);
  WebCore::Frame* frame =
      static_cast<WebCore::Frame*>(webframe_->GetFrameImplementation());
  WebCore::DOMWindow* domwindow = frame->domWindow();
  NPObject* npobject = _NPN_DoubleVectorToArrayObject(domwindow, val);
  DCHECK(npobject != NULL);
  Set(npobject);
  Unlock();
}

void CefVariantImpl::SetStringArray(const std::vector<std::wstring>& val)
{
  Lock();
  DCHECK(webframe_ != NULL);
  WebCore::Frame* frame =
      static_cast<WebCore::Frame*>(webframe_->GetFrameImplementation());
  WebCore::DOMWindow* domwindow = frame->domWindow();
  NPObject* npobject = _NPN_WStringVectorToArrayObject(domwindow, val);
  DCHECK(npobject != NULL);
  Set(npobject);
  Unlock();
}

bool CefVariantImpl::GetBool()
{
  Lock();
  DCHECK(variant_.type == NPVariantType_Bool);
  bool rv = variant_.value.boolValue;
  Unlock();
  return rv;
}

int CefVariantImpl::GetInt()
{
  Lock();
  DCHECK(variant_.type == NPVariantType_Int32);
  int rv = variant_.value.intValue;
  Unlock();
  return rv;
}

double CefVariantImpl::GetDouble()
{
  Lock();
  DCHECK(variant_.type == NPVariantType_Double);
  double rv = variant_.value.doubleValue;
  Unlock();
  return rv;
}

std::wstring CefVariantImpl::GetString()
{
  Lock();
  DCHECK(variant_.type == NPVariantType_String);
  std::wstring rv = UTF8ToWide(
      std::string(
          variant_.value.stringValue.UTF8Characters,
          variant_.value.stringValue.UTF8Length));
  Unlock();
  return rv;
}

bool CefVariantImpl::GetBoolArray(std::vector<bool>& val)
{
  Lock();
  DCHECK(variant_.type == NPVariantType_Object);
  bool rv = _NPN_ArrayObjectToBooleanVector(variant_.value.objectValue, val);
  Unlock();
  return rv;
}

bool CefVariantImpl::GetIntArray(std::vector<int>& val)
{
  Lock();
  DCHECK(variant_.type == NPVariantType_Object);
  bool rv = _NPN_ArrayObjectToIntVector(variant_.value.objectValue, val);
  Unlock();
  return rv;
}

bool CefVariantImpl::GetDoubleArray(std::vector<double>& val)
{
  Lock();
  DCHECK(variant_.type == NPVariantType_Object);
  bool rv = _NPN_ArrayObjectToDoubleVector(variant_.value.objectValue, val);
  Unlock();
  return rv;
}

bool CefVariantImpl::GetStringArray(std::vector<std::wstring>& val)
{
  Lock();
  DCHECK(variant_.type == NPVariantType_Object);
  bool rv = _NPN_ArrayObjectToWStringVector(variant_.value.objectValue, val);
  Unlock();
  return rv;
}

int CefVariantImpl::GetArraySize()
{
  Lock();
  DCHECK(variant_.type == NPVariantType_Object);
  int rv = _NPN_ArrayObjectGetVectorSize(variant_.value.objectValue);
  Unlock();
  return rv;
}

void CefVariantImpl::CopyToNPVariant(NPVariant* result)
{
  Lock();
  result->type = variant_.type;
  switch (variant_.type) {
    case NPVariantType_Bool:
      result->value.boolValue = variant_.value.boolValue;
      break;
    case NPVariantType_Int32:
      result->value.intValue = variant_.value.intValue;
      break;
    case NPVariantType_Double:
      result->value.doubleValue = variant_.value.doubleValue;
      break;
    case NPVariantType_String:
      _NPN_InitializeVariantWithStringCopy(result, &variant_.value.stringValue);
      break;
    case NPVariantType_Null:
    case NPVariantType_Void:
      // Nothing to set.
      break;
    case NPVariantType_Object:
      result->type = NPVariantType_Object;
      result->value.objectValue = NPN_RetainObject(variant_.value.objectValue);
      break;
  }
  Unlock();
}

void CefVariantImpl::Set(NPObject* val)
{
  Lock();
  SetNull();
  variant_.type = NPVariantType_Object;
  variant_.value.objectValue = NPN_RetainObject(val);
  Unlock();
}

void CefVariantImpl::Set(const NPString& val)
{
  Lock();
  SetNull();
  variant_.type = NPVariantType_String;
  _NPN_InitializeVariantWithStringCopy(&variant_, &val);
  Unlock();
}

void CefVariantImpl::Set(const NPVariant& val)
{
  Lock();
  SetNull();
  switch (val.type) {
  case NPVariantType_Bool:
    SetBool(val.value.boolValue);
    break;
  case NPVariantType_Int32:
    SetInt(val.value.intValue);
    break;
  case NPVariantType_Double:
    SetDouble(val.value.doubleValue);
    break;
  case NPVariantType_String:
    Set(val.value.stringValue);
    break;
  case NPVariantType_Object:
    Set(val.value.objectValue);
    break;
  }
  Unlock();
}
