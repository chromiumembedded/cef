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

class CefV8ContextImpl : public CefV8Context
{
public:
  CefV8ContextImpl(v8::Handle<v8::Context> context);
  virtual ~CefV8ContextImpl();

  virtual CefRefPtr<CefBrowser> GetBrowser() OVERRIDE;
  virtual CefRefPtr<CefFrame> GetFrame() OVERRIDE;
  virtual CefRefPtr<CefV8Value> GetGlobal() OVERRIDE;
  virtual bool Enter() OVERRIDE;
  virtual bool Exit() OVERRIDE;

  v8::Local<v8::Context> GetContext();
  WebKit::WebFrame* GetWebFrame();

protected:
  scoped_refptr<CefV8ContextHandle> v8_context_;

#ifndef NDEBUG
  // Used in debug builds to catch missing Exits in destructor.
  int enter_count_;
#endif

  IMPLEMENT_REFCOUNTING(CefV8ContextImpl);
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

class CefV8ValueImpl : public CefV8Value
{
public:
  CefV8ValueImpl(v8::Handle<v8::Value> value, CefTrackObject* tracker = NULL);
  virtual ~CefV8ValueImpl();

  virtual bool IsUndefined() OVERRIDE;
  virtual bool IsNull() OVERRIDE;
  virtual bool IsBool() OVERRIDE;
  virtual bool IsInt() OVERRIDE;
  virtual bool IsDouble() OVERRIDE;
  virtual bool IsDate() OVERRIDE;
  virtual bool IsString() OVERRIDE;
  virtual bool IsObject() OVERRIDE;
  virtual bool IsArray() OVERRIDE;
  virtual bool IsFunction() OVERRIDE;
  virtual bool IsSame(CefRefPtr<CefV8Value> value) OVERRIDE;
  virtual bool GetBoolValue() OVERRIDE;
  virtual int GetIntValue() OVERRIDE;
  virtual double GetDoubleValue() OVERRIDE;
  virtual CefTime GetDateValue() OVERRIDE;
  virtual CefString GetStringValue() OVERRIDE;
  virtual bool HasValue(const CefString& key) OVERRIDE;
  virtual bool HasValue(int index) OVERRIDE;
  virtual bool DeleteValue(const CefString& key) OVERRIDE;
  virtual bool DeleteValue(int index) OVERRIDE;
  virtual CefRefPtr<CefV8Value> GetValue(const CefString& key) OVERRIDE;
  virtual CefRefPtr<CefV8Value> GetValue(int index) OVERRIDE;
  virtual bool SetValue(const CefString& key, CefRefPtr<CefV8Value> value)
      OVERRIDE;
  virtual bool SetValue(int index, CefRefPtr<CefV8Value> value) OVERRIDE;
  virtual bool SetValue(const CefString& key, AccessControl settings, 
                        PropertyAttribute attribute) OVERRIDE;
  virtual bool GetKeys(std::vector<CefString>& keys) OVERRIDE;
  virtual CefRefPtr<CefBase> GetUserData() OVERRIDE;
  virtual int GetArrayLength() OVERRIDE;
  virtual CefString GetFunctionName() OVERRIDE;
  virtual CefRefPtr<CefV8Handler> GetFunctionHandler() OVERRIDE;
  virtual bool ExecuteFunction(CefRefPtr<CefV8Value> object,
                               const CefV8ValueList& arguments,
                               CefRefPtr<CefV8Value>& retval,
                               CefString& exception) OVERRIDE;
  virtual bool ExecuteFunctionWithContext(
                               CefRefPtr<CefV8Context> context,
                               CefRefPtr<CefV8Value> object,
                               const CefV8ValueList& arguments,
                               CefRefPtr<CefV8Value>& retval,
                               CefString& exception) OVERRIDE;

  inline v8::Handle<v8::Value> GetHandle()
  {
    DCHECK(v8_value_.get());
    return v8_value_->GetHandle();
  }

  bool IsReservedKey(const CefString& key);

protected:
  scoped_refptr<CefV8ValueHandle> v8_value_;

  IMPLEMENT_REFCOUNTING(CefV8ValueImpl);
};

#endif //_V8_IMPL_H
