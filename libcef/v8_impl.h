// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _V8_IMPL_H
#define _V8_IMPL_H

#include "include/cef.h"
#include "v8/include/v8.h"

class CefTrackObject;

class CefV8ValueImpl : public CefThreadSafeBase<CefV8Value>
{
public:
  CefV8ValueImpl();
  CefV8ValueImpl(v8::Handle<v8::Value> value, CefTrackObject* tracker = NULL);
  virtual ~CefV8ValueImpl();

  bool Attach(v8::Handle<v8::Value> value, CefTrackObject* tracker = NULL);
  void Detach();
  v8::Handle<v8::Value> GetValue();
  bool IsReservedKey(const std::wstring& key);

  virtual bool IsUndefined();
  virtual bool IsNull();
  virtual bool IsBool();
  virtual bool IsInt();
  virtual bool IsDouble();
  virtual bool IsString();
  virtual bool IsObject();
  virtual bool IsArray();
  virtual bool IsFunction();
  virtual bool GetBoolValue();
  virtual int GetIntValue();
  virtual double GetDoubleValue();
  virtual std::wstring GetStringValue();
  virtual bool HasValue(const std::wstring& key);
  virtual bool HasValue(int index);
  virtual bool DeleteValue(const std::wstring& key);
  virtual bool DeleteValue(int index);
  virtual CefRefPtr<CefV8Value> GetValue(const std::wstring& key);
  virtual CefRefPtr<CefV8Value> GetValue(int index);
  virtual bool SetValue(const std::wstring& key, CefRefPtr<CefV8Value> value);
  virtual bool SetValue(int index, CefRefPtr<CefV8Value> value);
  virtual bool GetKeys(std::vector<std::wstring>& keys);
  virtual CefRefPtr<CefBase> GetUserData();
  virtual int GetArrayLength();
  virtual std::wstring GetFunctionName();
  virtual CefRefPtr<CefV8Handler> GetFunctionHandler();
  virtual bool ExecuteFunction(CefRefPtr<CefV8Value> object,
                               CefV8ValueList& arguments,
                               CefRefPtr<CefV8Value>& retval,
                               std::wstring& exception);

protected:
  v8::Persistent<v8::Value> v8_value_;
  CefTrackObject* tracker_;
};

#endif //_V8_IMPL_H
