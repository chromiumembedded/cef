// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_V8_IMPL_H_
#define CEF_LIBCEF_RENDERER_V8_IMPL_H_
#pragma once

#include <vector>

#include "include/cef_v8.h"
#include "libcef/common/tracker.h"

#include "v8/include/v8.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"

class CefTrackNode;
class GURL;

namespace WebKit {
class WebFrame;
};

// Call after a V8 Isolate has been created and entered for the first time.
void CefV8IsolateCreated();

// Call before a V8 Isolate is exited and destroyed.
void CefV8IsolateDestroyed();

// Call to detach all handles associated with the specified context.
void CefV8ReleaseContext(v8::Handle<v8::Context> context);

// Set the stack size for uncaught exceptions.
void CefV8SetUncaughtExceptionStackSize(int stack_size);

// Set attributes associated with a WebWorker thread.
void CefV8SetWorkerAttributes(int worker_id, const GURL& worker_url);

// Used to detach handles when the associated context is released.
class CefV8ContextState : public base::RefCounted<CefV8ContextState> {
 public:
  CefV8ContextState() : valid_(true) {}
  virtual ~CefV8ContextState() {}

  bool IsValid() { return valid_; }
  void Detach() {
    DCHECK(valid_);
    valid_ = false;
    track_manager_.DeleteAll();
  }

  void AddTrackObject(CefTrackNode* object) {
    DCHECK(valid_);
    track_manager_.Add(object);
  }

  void DeleteTrackObject(CefTrackNode* object) {
    DCHECK(valid_);
    track_manager_.Delete(object);
  }

 private:
  bool valid_;
  CefTrackManager track_manager_;
};


// Use this template in conjuction with RefCountedThreadSafe to ensure that a
// V8 object is deleted on the correct thread.
struct CefV8DeleteOnMessageLoopThread {
  template<typename T>
  static void Destruct(const T* x) {
    if (x->task_runner()->RunsTasksOnCurrentThread()) {
      delete x;
    } else {
      if (!x->task_runner()->DeleteSoon(FROM_HERE, x)) {
#if defined(UNIT_TEST)
        // Only logged under unit testing because leaks at shutdown
        // are acceptable under normal circumstances.
        LOG(ERROR) << "DeleteSoon failed on thread " << thread;
#endif  // UNIT_TEST
      }
    }
  }
};

// Base class for V8 Handle types.
class CefV8HandleBase :
    public base::RefCountedThreadSafe<CefV8HandleBase,
                                      CefV8DeleteOnMessageLoopThread> {
 public:
  virtual ~CefV8HandleBase();

  // Returns true if there is no underlying context or if the underlying context
  // is valid.
  bool IsValid() const {
    return (!context_state_.get() || context_state_->IsValid());
  }

  bool BelongsToCurrentThread() const;

  v8::Isolate* isolate() const { return isolate_; }
  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }

 protected:
  // |context| is the context that owns this handle. If empty the current
  // context will be used.
  explicit CefV8HandleBase(v8::Handle<v8::Context> context);

 protected:
  v8::Isolate* isolate_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<CefV8ContextState> context_state_;
};

// Template for V8 Handle types. This class is used to ensure that V8 objects
// are only released on the render thread.
template <typename v8class>
class CefV8Handle : public CefV8HandleBase {
 public:
  typedef v8::Handle<v8class> handleType;
  typedef v8::Persistent<v8class> persistentType;

  CefV8Handle(v8::Handle<v8::Context> context, handleType v)
      : CefV8HandleBase(context),
        handle_(isolate(), v) {
  }
  virtual ~CefV8Handle() {
    handle_.Reset();
    handle_.Clear();
  }

  handleType GetNewV8Handle() {
    DCHECK(IsValid());
    return handleType::New(isolate(), handle_);
  }

  persistentType& GetPersistentV8Handle() {
    return handle_;
  }

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

  virtual CefRefPtr<CefTaskRunner> GetTaskRunner() OVERRIDE;
  virtual bool IsValid() OVERRIDE;
  virtual CefRefPtr<CefBrowser> GetBrowser() OVERRIDE;
  virtual CefRefPtr<CefFrame> GetFrame() OVERRIDE;
  virtual CefRefPtr<CefV8Value> GetGlobal() OVERRIDE;
  virtual bool Enter() OVERRIDE;
  virtual bool Exit() OVERRIDE;
  virtual bool IsSame(CefRefPtr<CefV8Context> that) OVERRIDE;
  virtual bool Eval(const CefString& code,
                    CefRefPtr<CefV8Value>& retval,
                    CefRefPtr<CefV8Exception>& exception) OVERRIDE;

  v8::Handle<v8::Context> GetV8Context();
  WebKit::WebFrame* GetWebFrame();

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
  CefV8ValueImpl();
  explicit CefV8ValueImpl(v8::Handle<v8::Value> value);
  virtual ~CefV8ValueImpl();

  // Used for initializing the CefV8ValueImpl. Should be called a single time
  // after the CefV8ValueImpl is created.
  void InitFromV8Value(v8::Handle<v8::Value> value);
  void InitUndefined();
  void InitNull();
  void InitBool(bool value);
  void InitInt(int32 value);
  void InitUInt(uint32 value);
  void InitDouble(double value);
  void InitDate(const CefTime& value);
  void InitString(CefString& value);
  void InitObject(v8::Handle<v8::Value> value, CefTrackNode* tracker);

  // Creates a new V8 value for the underlying value or returns the existing
  // object handle.
  v8::Handle<v8::Value> GetV8Value(bool should_persist);

  virtual bool IsValid() OVERRIDE;
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

 protected:
  // Test for and record any exception.
  bool HasCaught(v8::TryCatch& try_catch);

  class Handle : public CefV8HandleBase {
   public:
    typedef v8::Handle<v8::Value> handleType;
    typedef v8::Persistent<v8::Value> persistentType;

    Handle(v8::Handle<v8::Context> context, handleType v, CefTrackNode* tracker)
        : CefV8HandleBase(context),
          handle_(isolate(), v),
          tracker_(tracker),
          should_persist_(false) {
    }
    virtual ~Handle();

    handleType GetNewV8Handle(bool should_persist) {
      DCHECK(IsValid());
      if (should_persist && !should_persist_)
        should_persist_ = true;
      return handleType::New(isolate(), handle_);
    }

    persistentType& GetPersistentV8Handle() {
      return handle_;
    }

   private:
    persistentType handle_;

    // For Object and Function types, we need to hold on to a reference to their
    // internal data or function handler objects that are reference counted.
    CefTrackNode* tracker_;

    // True if the handle needs to persist due to it being passed into V8.
    bool should_persist_;

    DISALLOW_COPY_AND_ASSIGN(Handle);
  };
 
  enum {
    TYPE_INVALID = 0,
    TYPE_UNDEFINED,
    TYPE_NULL,
    TYPE_BOOL,
    TYPE_INT,
    TYPE_UINT,
    TYPE_DOUBLE,
    TYPE_DATE,
    TYPE_STRING,
    TYPE_OBJECT,
  } type_;

  union {
    bool bool_value_;
    int32 int_value_;
    uint32 uint_value_;
    double double_value_;
    cef_time_t date_value_;
    cef_string_t string_value_;
  };

  // Used with Object, Function and Array types.
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

  virtual bool IsValid() OVERRIDE;
  virtual int GetFrameCount() OVERRIDE;
  virtual CefRefPtr<CefV8StackFrame> GetFrame(int index) OVERRIDE;

 protected:
  std::vector<CefRefPtr<CefV8StackFrame> > frames_;

  IMPLEMENT_REFCOUNTING(CefV8StackTraceImpl);
  DISALLOW_COPY_AND_ASSIGN(CefV8StackTraceImpl);
};

class CefV8StackFrameImpl : public CefV8StackFrame {
 public:
  explicit CefV8StackFrameImpl(v8::Handle<v8::StackFrame> handle);
  virtual ~CefV8StackFrameImpl();

  virtual bool IsValid() OVERRIDE;
  virtual CefString GetScriptName() OVERRIDE;
  virtual CefString GetScriptNameOrSourceURL() OVERRIDE;
  virtual CefString GetFunctionName() OVERRIDE;
  virtual int GetLineNumber() OVERRIDE;
  virtual int GetColumn() OVERRIDE;
  virtual bool IsEval() OVERRIDE;
  virtual bool IsConstructor() OVERRIDE;

 protected:
  CefString script_name_;
  CefString script_name_or_source_url_;
  CefString function_name_;
  int line_number_;
  int column_;
  bool is_eval_;
  bool is_constructor_;

  IMPLEMENT_REFCOUNTING(CefV8StackFrameImpl);
  DISALLOW_COPY_AND_ASSIGN(CefV8StackFrameImpl);
};

#endif  // CEF_LIBCEF_RENDERER_V8_IMPL_H_
