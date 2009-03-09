// Copyright (c) 2008 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "precompiled_libcef.h"
#include "variant_np_util.h"

#include "config.h"

#include <v8.h>
#include "NPV8Object.h"
#include "v8_proxy.h"

#undef LOG
#include "base/string_util.h"
#include "bindings/npruntime.h"


// npScriptObjectClass defined in webkit\port\bindings\v8\NPV8Object.cpp
extern NPClass* npScriptObjectClass;


NPObject* _NPN_StringVectorToArrayObject(WebCore::DOMWindow* domwindow,
                                         const std::vector<std::string>& vec) {
  v8::Local<v8::Array> array = v8::Array::New();
  for (uint32_t index = 0; index < vec.size(); ++index) {
    array->Set(v8::Integer::New(index),
        v8::String::New(vec[index].c_str(), vec[index].length()));
  }

  return npCreateV8ScriptObject(0, array, domwindow);
}

NPObject* _NPN_WStringVectorToArrayObject(WebCore::DOMWindow* domwindow,
                                        const std::vector<std::wstring>& vec) {
  v8::Local<v8::Array> array = v8::Array::New();
  for (uint32_t index = 0; index < vec.size(); ++index) {
    std::string str = WideToUTF8(vec[index].c_str());
    array->Set(v8::Integer::New(index),
        v8::String::New(str.c_str(), str.length()));
  }

  return npCreateV8ScriptObject(0, array, domwindow);
}

NPObject* _NPN_IntVectorToArrayObject(WebCore::DOMWindow* domwindow,
                                      const std::vector<int>& vec) {
  v8::Local<v8::Array> array = v8::Array::New();
  for (uint32_t index = 0; index < vec.size(); ++index) {
    array->Set(v8::Integer::New(index), v8::Int32::New(vec[index]));
  }

  return npCreateV8ScriptObject(0, array, domwindow);
}

NPObject* _NPN_DoubleVectorToArrayObject(WebCore::DOMWindow* domwindow,
                                         const std::vector<double>& vec) {
  v8::Local<v8::Array> array = v8::Array::New();
  for (uint32_t index = 0; index < vec.size(); ++index) {
    array->Set(v8::Integer::New(index), v8::Number::New(vec[index]));
  }

  return npCreateV8ScriptObject(0, array, domwindow);
}

NPObject* _NPN_BooleanVectorToArrayObject(WebCore::DOMWindow* domwindow,
                                          const std::vector<bool>& vec) {
  v8::Local<v8::Array> array = v8::Array::New();
  for (uint32_t index = 0; index < vec.size(); ++index) {
    array->Set(v8::Integer::New(index), v8::Boolean::New(vec[index]));
  }

  return npCreateV8ScriptObject(0, array, domwindow);
}

bool _NPN_ArrayObjectToStringVector(NPObject* npobject,
                                    std::vector<std::string>& vec) {
  if (npobject == NULL || npobject->_class != npScriptObjectClass)
    return false;

  V8NPObject *object = reinterpret_cast<V8NPObject*>(npobject);
  if (!object->v8Object->IsArray())
    return false;

  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(object->v8Object);
  for (uint32_t i = 0; i < array->Length(); i++) {
    v8::Local<v8::Value> value = array->Get(v8::Integer::New(i));
    v8::Local<v8::String> sval = value->ToString();
    uint16_t* buf = new uint16_t[sval->Length()+1];
    sval->Write(buf);
    std::string utf8 = WideToUTF8(reinterpret_cast<wchar_t*>(buf));
    vec.push_back(utf8);
    delete[] buf;
  }

  return true;
}

bool _NPN_ArrayObjectToWStringVector(NPObject* npobject,
                                     std::vector<std::wstring>& vec) {
  if (npobject == NULL || npobject->_class != npScriptObjectClass)
    return false;

  V8NPObject *object = reinterpret_cast<V8NPObject*>(npobject);
  if (!object->v8Object->IsArray())
    return false;

  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(object->v8Object);
  for (uint32_t i = 0; i < array->Length(); i++) {
    v8::Local<v8::Value> value = array->Get(v8::Integer::New(i));
    v8::Local<v8::String> sval = value->ToString();
    uint16_t* buf = new uint16_t[sval->Length()+1];
    sval->Write(buf);
    std::wstring utf16 = reinterpret_cast<wchar_t*>(buf);
    vec.push_back(utf16);
    delete[] buf;
  }

  return true;
}

bool _NPN_ArrayObjectToIntVector(NPObject* npobject,
                                 std::vector<int>& vec) {
  if (npobject == NULL || npobject->_class != npScriptObjectClass)
    return false;

  V8NPObject *object = reinterpret_cast<V8NPObject*>(npobject);
  if (!object->v8Object->IsArray())
    return false;

  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(object->v8Object);
  for (uint32_t i = 0; i < array->Length(); i++) {
    v8::Local<v8::Value> value = array->Get(v8::Integer::New(i));
    v8::Local<v8::Int32> ival = value->ToInt32();
    vec.push_back(ival->Value());
  }

  return true;
}

bool _NPN_ArrayObjectToDoubleVector(NPObject* npobject,
                                    std::vector<double>& vec) {
  if (npobject == NULL || npobject->_class != npScriptObjectClass)
    return false;

  V8NPObject *object = reinterpret_cast<V8NPObject*>(npobject);
  if (!object->v8Object->IsArray())
    return false;

  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(object->v8Object);
  for (uint32_t i = 0; i < array->Length(); i++) {
    v8::Local<v8::Value> value = array->Get(v8::Integer::New(i));
    v8::Local<v8::Number> dval = value->ToNumber();
    vec.push_back(dval->Value());
  }

  return true;
}

bool _NPN_ArrayObjectToBooleanVector(NPObject* npobject,
                                     std::vector<bool>& vec) {
  if (npobject == NULL || npobject->_class != npScriptObjectClass)
    return false;

  V8NPObject *object = reinterpret_cast<V8NPObject*>(npobject);
  if (!object->v8Object->IsArray())
    return false;

  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(object->v8Object);
  for (uint32_t i = 0; i < array->Length(); i++) {
    v8::Local<v8::Value> value = array->Get(v8::Integer::New(i));
    v8::Local<v8::Boolean> bval = value->ToBoolean();
    vec.push_back(bval->Value());
  }

  return true;
}

int _NPN_ArrayObjectGetVectorSize(NPObject* npobject)
{
  if (npobject == NULL || npobject->_class != npScriptObjectClass)
    return -1;

  V8NPObject *object = reinterpret_cast<V8NPObject*>(npobject);
  if (!object->v8Object->IsArray())
    return -1;

  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(object->v8Object);
  return array->Length();
}

bool _NPN_ArrayObjectToVectorTypeHint(NPObject* npobject,
    NPVariantType &typehint)
{
  if (npobject == NULL || npobject->_class != npScriptObjectClass)
    return false;

  V8NPObject *object = reinterpret_cast<V8NPObject*>(npobject);
  if (!object->v8Object->IsArray())
    return false;

  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(object->v8Object);
  if (array->Length() == 0)
    return false;

  typehint = NPVariantType_Null;

  for (uint32_t i = 0; i < array->Length(); i++) {
    v8::Local<v8::Value> value = array->Get(v8::Integer::New(i));
    if (value->IsBoolean() && typehint <= NPVariantType_Bool) {
      if (typehint != NPVariantType_Bool)
        typehint = NPVariantType_Bool;
    } else if (value->IsInt32() && typehint <= NPVariantType_Int32) {
      if (typehint != NPVariantType_Int32)
        typehint = NPVariantType_Int32;
    } else if (value->IsNumber() && typehint <= NPVariantType_Double) {
      if (typehint != NPVariantType_Double)
        typehint = NPVariantType_Double;
    } else {
      typehint = NPVariantType_String;
      // String is the least restrictive type, so we don't need to keep looking
      break;
    }
  }

  return true;
}
