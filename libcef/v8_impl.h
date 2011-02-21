// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _V8_IMPL_H
#define _V8_IMPL_H

#include "include/cef.h"
#include "v8/include/v8.h"

class CefTrackObject;

namespace WebKit {
class WebFrame;
};

// Template for V8 Handle types. This class is used to ensure that V8 objects
// are only released on the UI thread.
template <class v8class>
class CefReleaseV8HandleOnUIThread:
    public base::RefCountedThreadSafe<CefReleaseV8HandleOnUIThread<v8class>,
                                      CefThread::DeleteOnUIThread>
{
public:
  typedef v8::Handle<v8class> handleType;
  typedef v8::Persistent<v8class> persistentType;
  typedef CefReleaseV8HandleOnUIThread<v8class> superType;

  CefReleaseV8HandleOnUIThread(handleType v)
  {
    v8_handle_ = persistentType::New(v);
  }
  virtual ~CefReleaseV8HandleOnUIThread()
  {
  }

  handleType GetHandle()
  {
    return v8_handle_;
  }

  persistentType v8_handle_;
};

// Special class for a v8::Context to ensure that it is deleted from the UI
// thread.
class CefV8ContextHandle : public CefReleaseV8HandleOnUIThread<v8::Context>
{
public:
  CefV8ContextHandle(handleType context): superType(context)
  {
  }

  // Context handles are disposed rather than makeweak.
  ~CefV8ContextHandle()
  {
    v8_handle_.Dispose();
    v8_handle_.Clear();
  }
};

class CefV8ContextImpl : public CefThreadSafeBase<CefV8Context>
{
public:
  CefV8ContextImpl(v8::Handle<v8::Context> context);
  virtual ~CefV8ContextImpl();

  virtual CefRefPtr<CefBrowser> GetBrowser();
  virtual CefRefPtr<CefFrame> GetFrame();
  virtual CefRefPtr<CefV8Value> GetGlobal();

  v8::Local<v8::Context> GetContext();
  WebKit::WebFrame* GetWebFrame();

protected:
  scoped_refptr<CefV8ContextHandle> v8_context_;
};

// Special class for a v8::Value to ensure that it is deleted from the UI
// thread.
class CefV8ValueHandle: public CefReleaseV8HandleOnUIThread<v8::Value>
{
public:
  CefV8ValueHandle(handleType value, CefTrackObject* tracker)
    : superType(value), tracker_(tracker)
  {
  }
  // Destructor implementation is provided in v8_impl.cc.
  ~CefV8ValueHandle();

private:
  // For Object and Function types, we need to hold on to a reference to their
  // internal data or function handler objects that are reference counted.
  CefTrackObject *tracker_;
};

class CefV8ValueImpl : public CefThreadSafeBase<CefV8Value>
{
public:
  CefV8ValueImpl(v8::Handle<v8::Value> value, CefTrackObject* tracker = NULL);
  virtual ~CefV8ValueImpl();

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
  virtual CefString GetStringValue();
  virtual bool HasValue(const CefString& key);
  virtual bool HasValue(int index);
  virtual bool DeleteValue(const CefString& key);
  virtual bool DeleteValue(int index);
  virtual CefRefPtr<CefV8Value> GetValue(const CefString& key);
  virtual CefRefPtr<CefV8Value> GetValue(int index);
  virtual bool SetValue(const CefString& key, CefRefPtr<CefV8Value> value);
  virtual bool SetValue(int index, CefRefPtr<CefV8Value> value);
  virtual bool GetKeys(std::vector<CefString>& keys);
  virtual CefRefPtr<CefBase> GetUserData();
  virtual int GetArrayLength();
  virtual CefString GetFunctionName();
  virtual CefRefPtr<CefV8Handler> GetFunctionHandler();
  virtual bool ExecuteFunction(CefRefPtr<CefV8Value> object,
                               const CefV8ValueList& arguments,
                               CefRefPtr<CefV8Value>& retval,
                               CefString& exception);
  virtual bool ExecuteFunctionWithContext(
                               CefRefPtr<CefV8Context> context,
                               CefRefPtr<CefV8Value> object,
                               const CefV8ValueList& arguments,
                               CefRefPtr<CefV8Value>& retval,
                               CefString& exception);

  inline v8::Handle<v8::Value> GetHandle()
  {
    DCHECK(v8_value_.get());
    return v8_value_->GetHandle();
  }

  bool IsReservedKey(const CefString& key);

protected:
  scoped_refptr<CefV8ValueHandle> v8_value_;
};

#endif //_V8_IMPL_H
