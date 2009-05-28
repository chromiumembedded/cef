// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cpptoc/v8value_cpptoc.h"
#include "ctocpp/v8handler_ctocpp.h"


int CEF_CALLBACK v8value_is_undefined(struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->IsUndefined();
}

int CEF_CALLBACK v8value_is_null(struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->IsNull();
}

int CEF_CALLBACK v8value_is_bool(struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->IsBool();
}

int CEF_CALLBACK v8value_is_int(struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->IsInt();
}

int CEF_CALLBACK v8value_is_double(struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->IsDouble();
}

int CEF_CALLBACK v8value_is_string(struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->IsString();
}

int CEF_CALLBACK v8value_is_object(struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->IsObject();
}

int CEF_CALLBACK v8value_is_array(struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->IsArray();
}

int CEF_CALLBACK v8value_is_function(struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->IsFunction();
}

int CEF_CALLBACK v8value_get_bool_value(struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->GetBoolValue();
}

int CEF_CALLBACK v8value_get_int_value(struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->GetIntValue();
}

double CEF_CALLBACK v8value_get_double_value(struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->GetDoubleValue();
}

cef_string_t CEF_CALLBACK v8value_get_string_value(struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  std::wstring valueStr = CefV8ValueCppToC::Get(v8value)->GetStringValue();
  if(!valueStr.empty())
    return cef_string_alloc(valueStr.c_str());
  return NULL;
}

int CEF_CALLBACK v8value_has_value_bykey(struct _cef_v8value_t* v8value,
   const wchar_t* key)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  std::wstring keyStr;
  if(key)
    keyStr = key;

  return CefV8ValueCppToC::Get(v8value)->HasValue(keyStr);
}

int CEF_CALLBACK v8value_has_value_byindex(struct _cef_v8value_t* v8value,
    int index)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->HasValue(index);
}

int CEF_CALLBACK v8value_delete_value_bykey(struct _cef_v8value_t* v8value,
    const wchar_t* key)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  std::wstring keyStr;
  if(key)
    keyStr = key;

  return CefV8ValueCppToC::Get(v8value)->DeleteValue(keyStr);
}

int CEF_CALLBACK v8value_delete_value_byindex(struct _cef_v8value_t* v8value,
    int index)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->DeleteValue(index);
}

struct _cef_v8value_t* CEF_CALLBACK v8value_get_value_bykey(
    struct _cef_v8value_t* v8value, const wchar_t* key)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  std::wstring keyStr;
  if(key)
    keyStr = key;

  CefRefPtr<CefV8Value> valuePtr =
      CefV8ValueCppToC::Get(v8value)->GetValue(keyStr);
  return CefV8ValueCppToC::Wrap(valuePtr);
}

struct _cef_v8value_t* CEF_CALLBACK v8value_get_value_byindex(
    struct _cef_v8value_t* v8value, int index)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  CefRefPtr<CefV8Value> valuePtr =
      CefV8ValueCppToC::Get(v8value)->GetValue(index);
  return CefV8ValueCppToC::Wrap(valuePtr);
}

int CEF_CALLBACK v8value_set_value_bykey(struct _cef_v8value_t* v8value,
    const wchar_t* key, struct _cef_v8value_t* new_value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  std::wstring keyStr;
  if(key)
    keyStr = key;

  CefRefPtr<CefV8Value> valuePtr = CefV8ValueCppToC::Unwrap(new_value);
  return CefV8ValueCppToC::Get(v8value)->SetValue(keyStr, valuePtr);
}

int CEF_CALLBACK v8value_set_value_byindex(struct _cef_v8value_t* v8value,
    int index, struct _cef_v8value_t* new_value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  CefRefPtr<CefV8Value> valuePtr = CefV8ValueCppToC::Unwrap(new_value);
  return CefV8ValueCppToC::Get(v8value)->SetValue(index, valuePtr);
}

int CEF_CALLBACK v8value_get_keys(struct _cef_v8value_t* v8value,
    cef_string_list_t list)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  std::vector<std::wstring> keysList;
  CefV8ValueCppToC::Get(v8value)->GetKeys(keysList);
  size_t size = keysList.size();
  for(size_t i = 0; i < size; ++i)
    cef_string_list_append(list, keysList[i].c_str());
  return size;
}

struct _cef_base_t* CEF_CALLBACK v8value_get_user_data(
    struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  CefRefPtr<CefBase> base = CefV8ValueCppToC::Get(v8value)->GetUserData();
  if(base.get())
    return CefBaseCToCpp::Unwrap(base);
  return NULL;
}

int CEF_CALLBACK v8value_get_array_length(struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  return CefV8ValueCppToC::Get(v8value)->GetArrayLength();
}

cef_string_t CEF_CALLBACK v8value_get_function_name(
    struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  std::wstring functionNameStr =
      CefV8ValueCppToC::Get(v8value)->GetFunctionName();
  if(!functionNameStr.empty())
    return cef_string_alloc(functionNameStr.c_str());
  return NULL;
}

struct _cef_v8handler_t* CEF_CALLBACK v8value_get_function_handler(
    struct _cef_v8value_t* v8value)
{
  DCHECK(v8value);
  if(!v8value)
    return 0;

  CefRefPtr<CefV8Handler> handlerPtr =
      CefV8ValueCppToC::Get(v8value)->GetFunctionHandler();
  if(handlerPtr.get())
    return CefV8HandlerCToCpp::Unwrap(handlerPtr);
  return NULL;
}

int CEF_CALLBACK v8value_execute_function(struct _cef_v8value_t* v8value,
    struct _cef_v8value_t* object, size_t numargs,
    struct _cef_v8value_t** args, struct _cef_v8value_t** retval,
    cef_string_t* exception)
{
  DCHECK(v8value);
  DCHECK(object);
  if(!v8value || !object)
    return 0;

  CefRefPtr<CefV8Value> objectPtr = CefV8ValueCppToC::Unwrap(object);
  CefV8ValueList argsList;
  for(size_t i = 0; i < numargs; i++)
    argsList.push_back(CefV8ValueCppToC::Unwrap(args[i]));
  CefRefPtr<CefV8Value> retvalPtr;
  std::wstring exceptionStr;

  bool rv = CefV8ValueCppToC::Get(v8value)->ExecuteFunction(objectPtr,
      argsList, retvalPtr, exceptionStr);
  if(retvalPtr.get() && retval)
    *retval = CefV8ValueCppToC::Wrap(retvalPtr);
  if(!exceptionStr.empty() && exception)
    *exception = cef_string_alloc(exceptionStr.c_str());

  return rv;
}


CefV8ValueCppToC::CefV8ValueCppToC(CefV8Value* cls)
    : CefCppToC<CefV8ValueCppToC, CefV8Value, cef_v8value_t>(cls)
{
  struct_.struct_.is_undefined = v8value_is_undefined;
  struct_.struct_.is_null = v8value_is_null;
  struct_.struct_.is_bool = v8value_is_bool;
  struct_.struct_.is_int = v8value_is_int;
  struct_.struct_.is_double = v8value_is_double;
  struct_.struct_.is_string = v8value_is_string;
  struct_.struct_.is_object = v8value_is_object;
  struct_.struct_.is_array = v8value_is_array;
  struct_.struct_.is_function = v8value_is_function;
  struct_.struct_.get_bool_value = v8value_get_bool_value;
  struct_.struct_.get_int_value = v8value_get_int_value;
  struct_.struct_.get_double_value = v8value_get_double_value;
  struct_.struct_.get_string_value = v8value_get_string_value;
  struct_.struct_.has_value_bykey = v8value_has_value_bykey;
  struct_.struct_.has_value_byindex = v8value_has_value_byindex;
  struct_.struct_.delete_value_bykey = v8value_delete_value_bykey;
  struct_.struct_.delete_value_byindex = v8value_delete_value_byindex;
  struct_.struct_.get_value_bykey = v8value_get_value_bykey;
  struct_.struct_.get_value_byindex = v8value_get_value_byindex;
  struct_.struct_.set_value_bykey = v8value_set_value_bykey;
  struct_.struct_.set_value_byindex = v8value_set_value_byindex;
  struct_.struct_.get_keys = v8value_get_keys;
  struct_.struct_.get_user_data = v8value_get_user_data;
  struct_.struct_.get_array_length = v8value_get_array_length;
  struct_.struct_.get_function_name = v8value_get_function_name;
  struct_.struct_.get_function_handler = v8value_get_function_handler;
  struct_.struct_.execute_function = v8value_execute_function;
}

#ifdef _DEBUG
long CefCppToC<CefV8ValueCppToC, CefV8Value, cef_v8value_t>::DebugObjCt = 0;
#endif
