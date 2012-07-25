// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_V8_IMPL_H_
#define CEF_LIBCEF_V8_IMPL_H_
#pragma once

#include <vector>

#include "include/cef_v8.h"
#include "v8/include/v8.h"
#include "libcef/cef_thread.h"
#include "base/memory/ref_counted.h"

class CefTrackNode;

namespace WebKit {
class WebFrame;
};

// Template for V8 Handle types. This class is used to ensure that V8 objects
// are only released on the UI thread.
template <typename v8class>
class CefV8Handle :
    public base::RefCountedThreadSafe<CefV8Handle<v8class>,
                                      CefThread::DeleteOnUIThread> {
 public:
  typedef v8::Handle<v8class> handleType;
  typedef v8::Persistent<v8class> persistentType;

  CefV8Handle(handleType v)
      : handle_(persistentType::New(v)) {
  }
  ~CefV8Handle() {
    handle_.Dispose();
    handle_.Clear();
  }

  handleType GetHandle() { return handle_; }

 protected:
  persistentType handle_;

  DISALLOW_COPY_AND_ASSIGN(CefV8Handle);
};

// Specialization for v8::Value with empty implementation to avoid incorrect
// usage.
template <>
class CefV8Handle<v8::Value> {
};


class CefV8ContextImpl : public CefV8Context {
 public:
  explicit CefV8ContextImpl(v8::Handle<v8::Context> context);
  virtual ~CefV8ContextImpl();

  virtual CefRefPtr<CefBrowser> GetBrowser() OVERRIDE;
  virtual CefRefPtr<CefFrame> GetFrame() OVERRIDE;
  virtual CefRefPtr<CefV8Value> GetGlobal() OVERRIDE;
  virtual bool Enter() OVERRIDE;
  virtual bool Exit() OVERRIDE;
  virtual bool IsSame(CefRefPtr<CefV8Context> that) OVERRIDE;
  virtual bool Eval(const CefString& code,
                    CefRefPtr<CefV8Value>& retval,
                    CefRefPtr<CefV8Exception>& exception) OVERRIDE;

  v8::Local<v8::Context> GetContext();
  WebKit::WebFrame* GetWebFrame();

  v8::Handle<v8::Context> GetHandle() { return handle_->GetHandle(); }

 protected:
  typedef CefV8Handle<v8::Context> Handle;
  scoped_refptr<Handle> handle_;

#ifndef NDEBUG
  // Used in debug builds to catch missing Exits in destructor.
  int enter_count_;
#endif

  IMPLEMENT_REFCOUNTING(CefV8ContextImpl);
  DISALLOW_COPY_AND_ASSIGN(CefV8ContextImpl);
};

class CefV8ValueImpl : public CefV8Value {
 public:
  CefV8ValueImpl(v8::Handle<v8::Value> value, CefTrackNode* tracker = NULL);
  virtual ~CefV8ValueImpl();

  virtual bool IsUndefined() OVERRIDE;
  virtual bool IsNull() OVERRIDE;
  virtual bool IsBool() OVERRIDE;
  virtual bool IsInt() OVERRIDE;
  virtual bool IsUInt() OVERRIDE;
  virtual bool IsDouble() OVERRIDE;
  virtual bool IsDate() OVERRIDE;
  virtual bool IsString() OVERRIDE;
  virtual bool IsObject() OVERRIDE;
  virtual bool IsArray() OVERRIDE;
  virtual bool IsFunction() OVERRIDE;
  virtual bool IsSame(CefRefPtr<CefV8Value> value) OVERRIDE;
  virtual bool GetBoolValue() OVERRIDE;
  virtual int32 GetIntValue() OVERRIDE;
  virtual uint32 GetUIntValue() OVERRIDE;
  virtual double GetDoubleValue() OVERRIDE;
  virtual CefTime GetDateValue() OVERRIDE;
  virtual CefString GetStringValue() OVERRIDE;
  virtual bool IsUserCreated() OVERRIDE;
  virtual bool HasException() OVERRIDE;
  virtual CefRefPtr<CefV8Exception> GetException() OVERRIDE;
  virtual bool ClearException() OVERRIDE;
  virtual bool WillRethrowExceptions() OVERRIDE;
  virtual bool SetRethrowExceptions(bool rethrow) OVERRIDE;
  virtual bool HasValue(const CefString& key) OVERRIDE;
  virtual bool HasValue(int index) OVERRIDE;
  virtual bool DeleteValue(const CefString& key) OVERRIDE;
  virtual bool DeleteValue(int index) OVERRIDE;
  virtual CefRefPtr<CefV8Value> GetValue(const CefString& key) OVERRIDE;
  virtual CefRefPtr<CefV8Value> GetValue(int index) OVERRIDE;
  virtual bool SetValue(const CefString& key, CefRefPtr<CefV8Value> value,
                        PropertyAttribute attribute) OVERRIDE;
  virtual bool SetValue(int index, CefRefPtr<CefV8Value> value) OVERRIDE;
  virtual bool SetValue(const CefString& key, AccessControl settings,
                        PropertyAttribute attribute) OVERRIDE;
  virtual bool GetKeys(std::vector<CefString>& keys) OVERRIDE;
  virtual bool SetUserData(CefRefPtr<CefBase> user_data) OVERRIDE;
  virtual CefRefPtr<CefBase> GetUserData() OVERRIDE;
  virtual int GetExternallyAllocatedMemory() OVERRIDE;
  virtual int AdjustExternallyAllocatedMemory(int change_in_bytes) OVERRIDE;
  virtual int GetArrayLength() OVERRIDE;
  virtual CefString GetFunctionName() OVERRIDE;
  virtual CefRefPtr<CefV8Handler> GetFunctionHandler() OVERRIDE;
  virtual CefRefPtr<CefV8Value> ExecuteFunction(
      CefRefPtr<CefV8Value> object,
      const CefV8ValueList& arguments) OVERRIDE;
  virtual CefRefPtr<CefV8Value> ExecuteFunctionWithContext(
      CefRefPtr<CefV8Context> context,
      CefRefPtr<CefV8Value> object,
      const CefV8ValueList& arguments) OVERRIDE;

  v8::Handle<v8::Value> GetHandle() { return handle_->GetHandle(); }

 protected:
  // Test for and record any exception.
  bool HasCaught(v8::TryCatch& try_catch);

  class Handle :
      public base::RefCountedThreadSafe<Handle, CefThread::DeleteOnUIThread> {
   public:
    typedef v8::Handle<v8::Value> handleType;
    typedef v8::Persistent<v8::Value> persistentType;

    Handle(handleType v, CefTrackNode* tracker)
        : handle_(persistentType::New(v)),
          tracker_(tracker) {
    }
    ~Handle();

    handleType GetHandle() { return handle_; }

   private:
    persistentType handle_;

    // For Object and Function types, we need to hold on to a reference to their
    // internal data or function handler objects that are reference counted.
    CefTrackNode* tracker_;

    DISALLOW_COPY_AND_ASSIGN(Handle);
  };
  scoped_refptr<Handle> handle_;

  CefRefPtr<CefV8Exception> last_exception_;
  bool rethrow_exceptions_;

  IMPLEMENT_REFCOUNTING(CefV8ValueImpl);
  DISALLOW_COPY_AND_ASSIGN(CefV8ValueImpl);
};

class CefV8StackTraceImpl : public CefV8StackTrace {
 public:
  explicit CefV8StackTraceImpl(v8::Handle<v8::StackTrace> handle);
  virtual ~CefV8StackTraceImpl();

  virtual int GetFrameCount() OVERRIDE;
  virtual CefRefPtr<CefV8StackFrame> GetFrame(int index) OVERRIDE;

  v8::Handle<v8::StackTrace> GetHandle() { return handle_->GetHandle(); }

 protected:
  typedef CefV8Handle<v8::StackTrace> Handle;
  scoped_refptr<Handle> handle_;

  IMPLEMENT_REFCOUNTING(CefV8StackTraceImpl);
  DISALLOW_COPY_AND_ASSIGN(CefV8StackTraceImpl);
};

class CefV8StackFrameImpl : public CefV8StackFrame {
 public:
  explicit CefV8StackFrameImpl(v8::Handle<v8::StackFrame> handle);
  virtual ~CefV8StackFrameImpl();

  virtual CefString GetScriptName() OVERRIDE;
  virtual CefString GetScriptNameOrSourceURL() OVERRIDE;
  virtual CefString GetFunctionName() OVERRIDE;
  virtual int GetLineNumber() OVERRIDE;
  virtual int GetColumn() OVERRIDE;
  virtual bool IsEval() OVERRIDE;
  virtual bool IsConstructor() OVERRIDE;

  v8::Handle<v8::StackFrame> GetHandle() { return handle_->GetHandle(); }

 protected:
  typedef CefV8Handle<v8::StackFrame> Handle;
  scoped_refptr<Handle> handle_;

  IMPLEMENT_REFCOUNTING(CefV8StackFrameImpl);
  DISALLOW_COPY_AND_ASSIGN(CefV8StackFrameImpl);
};

#endif  // CEF_LIBCEF_V8_IMPL_H_
