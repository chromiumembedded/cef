// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
// Otherwise there will be compile errors in wtf/MathExtras.h.
#define _USE_MATH_DEFINES

#include <map>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "build/build_config.h"

// Enable deprecation warnings for MSVC and Clang. See http://crbug.com/585142.
#if BUILDFLAG(IS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wdeprecated-declarations"
#else
#pragma warning(push)
#pragma warning(default : 4996)
#endif
#endif

#include "libcef/renderer/v8_impl.h"

#include "libcef/common/app_manager.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/task_runner_impl.h"
#include "libcef/common/tracker.h"
#include "libcef/renderer/blink_glue.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/render_frame_util.h"
#include "libcef/renderer/thread_util.h"

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_controller.h"
#include "url/gurl.h"

namespace {

static const char kCefTrackObject[] = "Cef::TrackObject";

void MessageListenerCallbackImpl(v8::Handle<v8::Message> message,
                                 v8::Handle<v8::Value> data);

// The following *Private functions are convenience wrappers for methods on
// v8::Object with the corresponding names.
// Based on extensions/renderer/object_backed_native_handler.cc.

void SetPrivate(v8::Local<v8::Context> context,
                v8::Local<v8::Object> obj,
                const char* key,
                v8::Local<v8::Value> value) {
  v8::Isolate* isolate = context->GetIsolate();
  obj->SetPrivate(context,
                  v8::Private::ForApi(
                      isolate, v8::String::NewFromUtf8(
                                   isolate, key, v8::NewStringType::kNormal)
                                   .ToLocalChecked()),
                  value)
      .FromJust();
}

bool GetPrivate(v8::Local<v8::Context> context,
                v8::Local<v8::Object> obj,
                const char* key,
                v8::Local<v8::Value>* result) {
  v8::Isolate* isolate = context->GetIsolate();
  return obj
      ->GetPrivate(context,
                   v8::Private::ForApi(
                       isolate, v8::String::NewFromUtf8(
                                    isolate, key, v8::NewStringType::kNormal)
                                    .ToLocalChecked()))
      .ToLocal(result);
}

// Chromium uses the default Isolate for the main render process thread and a
// new Isolate for each WebWorker thread. Continue this pattern by tracking
// Isolate information on a per-thread basis. This implementation will need to
// be re-worked (perhaps using a map keyed on v8::Isolate::GetCurrent()) if
// in the future Chromium begins using the same Isolate across multiple threads.
class CefV8IsolateManager;
ABSL_CONST_INIT thread_local CefV8IsolateManager* g_isolate_manager = nullptr;

// Manages memory and state information associated with a single Isolate.
class CefV8IsolateManager {
 public:
  CefV8IsolateManager()
      : resetter_(&g_isolate_manager, this),
        isolate_(v8::Isolate::GetCurrent()),
        task_runner_(CEF_RENDER_TASK_RUNNER()) {
    DCHECK(isolate_);
    DCHECK(task_runner_.get());
  }
  ~CefV8IsolateManager() = default;

  static CefV8IsolateManager* Get() { return g_isolate_manager; }

  void Destroy() {
    DCHECK_EQ(isolate_, v8::Isolate::GetCurrent());
    DCHECK(context_map_.empty());
    delete this;
  }

  scoped_refptr<CefV8ContextState> GetContextState(
      v8::Local<v8::Context> context) {
    DCHECK_EQ(isolate_, v8::Isolate::GetCurrent());
    DCHECK(context.IsEmpty() || isolate_ == context->GetIsolate());

    if (context.IsEmpty()) {
      if (isolate_->InContext()) {
        context = isolate_->GetCurrentContext();
      } else {
        return scoped_refptr<CefV8ContextState>();
      }
    }

    int hash = context->Global()->GetIdentityHash();
    ContextMap::const_iterator it = context_map_.find(hash);
    if (it != context_map_.end()) {
      return it->second;
    }

    scoped_refptr<CefV8ContextState> state = new CefV8ContextState();
    context_map_.insert(std::make_pair(hash, state));

    return state;
  }

  void ReleaseContext(v8::Local<v8::Context> context) {
    DCHECK_EQ(isolate_, v8::Isolate::GetCurrent());
    DCHECK_EQ(isolate_, context->GetIsolate());

    int hash = context->Global()->GetIdentityHash();
    ContextMap::iterator it = context_map_.find(hash);
    if (it != context_map_.end()) {
      it->second->Detach();
      context_map_.erase(it);
    }
  }

  void AddGlobalTrackObject(CefTrackNode* object) {
    DCHECK_EQ(isolate_, v8::Isolate::GetCurrent());
    global_manager_.Add(object);
  }

  void DeleteGlobalTrackObject(CefTrackNode* object) {
    DCHECK_EQ(isolate_, v8::Isolate::GetCurrent());
    global_manager_.Delete(object);
  }

  void SetUncaughtExceptionStackSize(int stack_size) {
    if (stack_size <= 0) {
      return;
    }

    if (!message_listener_registered_) {
      isolate_->AddMessageListener(&MessageListenerCallbackImpl);
      message_listener_registered_ = true;
    }

    isolate_->SetCaptureStackTraceForUncaughtExceptions(
        true, stack_size, v8::StackTrace::kDetailed);
  }

  void SetWorkerAttributes(int worker_id, const GURL& worker_url) {
    worker_id_ = worker_id;
    worker_url_ = worker_url;
  }

  v8::Isolate* isolate() const { return isolate_; }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  int worker_id() const { return worker_id_; }

  const GURL& worker_url() const { return worker_url_; }

 private:
  const base::AutoReset<CefV8IsolateManager*> resetter_;

  v8::Isolate* isolate_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  using ContextMap = std::map<int, scoped_refptr<CefV8ContextState>>;
  ContextMap context_map_;

  // Used for globally tracked objects that are not associated with a particular
  // context.
  CefTrackManager global_manager_;

  // True if the message listener has been registered.
  bool message_listener_registered_ = false;

  // Attributes associated with WebWorker threads.
  int worker_id_ = 0;
  GURL worker_url_;
};

class V8TrackObject : public CefTrackNode {
 public:
  explicit V8TrackObject(v8::Isolate* isolate) : isolate_(isolate) {
    DCHECK(isolate_);
    isolate_->AdjustAmountOfExternalAllocatedMemory(
        static_cast<int>(sizeof(V8TrackObject)));
  }
  ~V8TrackObject() override {
    isolate_->AdjustAmountOfExternalAllocatedMemory(
        -static_cast<int>(sizeof(V8TrackObject)) - external_memory_);
  }

  inline int GetExternallyAllocatedMemory() { return external_memory_; }

  int AdjustExternallyAllocatedMemory(int change_in_bytes) {
    int new_value = external_memory_ + change_in_bytes;
    if (new_value < 0) {
      DCHECK(false) << "External memory usage cannot be less than 0 bytes";
      change_in_bytes = -(external_memory_);
      new_value = 0;
    }

    if (change_in_bytes != 0) {
      isolate_->AdjustAmountOfExternalAllocatedMemory(change_in_bytes);
    }
    external_memory_ = new_value;

    return new_value;
  }

  inline void SetAccessor(CefRefPtr<CefV8Accessor> accessor) {
    accessor_ = accessor;
  }

  inline CefRefPtr<CefV8Accessor> GetAccessor() { return accessor_; }

  inline void SetInterceptor(CefRefPtr<CefV8Interceptor> interceptor) {
    interceptor_ = interceptor;
  }

  inline CefRefPtr<CefV8Interceptor> GetInterceptor() { return interceptor_; }

  inline void SetHandler(CefRefPtr<CefV8Handler> handler) {
    handler_ = handler;
  }

  inline CefRefPtr<CefV8Handler> GetHandler() { return handler_; }

  inline void SetUserData(CefRefPtr<CefBaseRefCounted> user_data) {
    user_data_ = user_data;
  }

  inline CefRefPtr<CefBaseRefCounted> GetUserData() { return user_data_; }

  // Attach this track object to the specified V8 object.
  void AttachTo(v8::Local<v8::Context> context, v8::Local<v8::Object> object) {
    SetPrivate(context, object, kCefTrackObject,
               v8::External::New(isolate_, this));
  }

  // Retrieve the track object for the specified V8 object.
  static V8TrackObject* Unwrap(v8::Local<v8::Context> context,
                               v8::Local<v8::Object> object) {
    v8::Local<v8::Value> value;
    if (GetPrivate(context, object, kCefTrackObject, &value) &&
        value->IsExternal()) {
      return static_cast<V8TrackObject*>(v8::External::Cast(*value)->Value());
    }

    return nullptr;
  }

 private:
  v8::Isolate* isolate_;
  CefRefPtr<CefV8Accessor> accessor_;
  CefRefPtr<CefV8Interceptor> interceptor_;
  CefRefPtr<CefV8Handler> handler_;
  CefRefPtr<CefBaseRefCounted> user_data_;
  int external_memory_ = 0;
};

class V8TrackString : public CefTrackNode {
 public:
  explicit V8TrackString(const std::string& str) : string_(str) {}
  const char* GetString() { return string_.c_str(); }

 private:
  std::string string_;
};

class V8TrackArrayBuffer : public CefTrackNode {
 public:
  explicit V8TrackArrayBuffer(
      v8::Isolate* isolate,
      CefRefPtr<CefV8ArrayBufferReleaseCallback> release_callback)
      : isolate_(isolate), release_callback_(release_callback) {
    DCHECK(isolate_);
    isolate_->AdjustAmountOfExternalAllocatedMemory(
        static_cast<int>(sizeof(V8TrackArrayBuffer)));
  }

  ~V8TrackArrayBuffer() override {
    isolate_->AdjustAmountOfExternalAllocatedMemory(
        -static_cast<int>(sizeof(V8TrackArrayBuffer)));
  }

  CefRefPtr<CefV8ArrayBufferReleaseCallback> GetReleaseCallback() {
    return release_callback_;
  }

  // Attach this track object to the specified V8 object.
  void AttachTo(v8::Local<v8::Context> context,
                v8::Local<v8::ArrayBuffer> arrayBuffer) {
    SetPrivate(context, arrayBuffer, kCefTrackObject,
               v8::External::New(isolate_, this));
  }

  // Retrieve the track object for the specified V8 object.
  static V8TrackArrayBuffer* Unwrap(v8::Local<v8::Context> context,
                                    v8::Local<v8::Object> object) {
    v8::Local<v8::Value> value;
    if (GetPrivate(context, object, kCefTrackObject, &value) &&
        value->IsExternal()) {
      return static_cast<V8TrackArrayBuffer*>(
          v8::External::Cast(*value)->Value());
    }

    return nullptr;
  }

 private:
  v8::Isolate* isolate_;
  CefRefPtr<CefV8ArrayBufferReleaseCallback> release_callback_;
};

// Object wrapped in a v8::External and passed as the Data argument to
// v8::FunctionTemplate::New.
class V8FunctionData {
 public:
  static v8::Local<v8::External> Create(v8::Isolate* isolate,
                                        const CefString& function_name,
                                        CefRefPtr<CefV8Handler> handler) {
    // |data| will be deleted if/when the returned v8::External is GC'd.
    V8FunctionData* data = new V8FunctionData(isolate, function_name, handler);
    return data->CreateExternal();
  }

  static V8FunctionData* Unwrap(v8::Local<v8::Value> data) {
    DCHECK(data->IsExternal());
    return static_cast<V8FunctionData*>(v8::External::Cast(*data)->Value());
  }

  CefString function_name() const { return function_name_; }

  CefV8Handler* handler() const {
    if (!handler_) {
      return nullptr;
    }
    return handler_.get();
  }

 private:
  V8FunctionData(v8::Isolate* isolate,
                 const CefString& function_name,
                 CefRefPtr<CefV8Handler> handler)
      : isolate_(isolate), function_name_(function_name), handler_(handler) {
    DCHECK(isolate_);
    DCHECK(handler_);
  }

  ~V8FunctionData() {
    isolate_->AdjustAmountOfExternalAllocatedMemory(
        -static_cast<int>(sizeof(V8FunctionData)));
    handler_ = nullptr;
    function_name_ = "FreedFunction";
  }

  v8::Local<v8::External> CreateExternal() {
    v8::Local<v8::External> external = v8::External::New(isolate_, this);

    isolate_->AdjustAmountOfExternalAllocatedMemory(
        static_cast<int>(sizeof(V8FunctionData)));

    handle_.Reset(isolate_, external);
    handle_.SetWeak(this, FirstWeakCallback, v8::WeakCallbackType::kParameter);

    return external;
  }

  static void FirstWeakCallback(
      const v8::WeakCallbackInfo<V8FunctionData>& data) {
    V8FunctionData* wrapper = data.GetParameter();
    wrapper->handle_.Reset();
    data.SetSecondPassCallback(SecondWeakCallback);
  }

  static void SecondWeakCallback(
      const v8::WeakCallbackInfo<V8FunctionData>& data) {
    V8FunctionData* wrapper = data.GetParameter();
    delete wrapper;
  }

  v8::Isolate* isolate_;
  CefString function_name_;
  CefRefPtr<CefV8Handler> handler_;
  v8::Persistent<v8::External> handle_;
};

// Convert a CefString to a V8::String.
v8::Local<v8::String> GetV8String(v8::Isolate* isolate, const CefString& str) {
#if defined(CEF_STRING_TYPE_UTF16)
  // Already a UTF16 string.
  return v8::String::NewFromTwoByte(
             isolate,
             reinterpret_cast<uint16_t*>(
                 const_cast<CefString::char_type*>(str.c_str())),
             v8::NewStringType::kNormal, str.length())
      .ToLocalChecked();
#elif defined(CEF_STRING_TYPE_UTF8)
  // Already a UTF8 string.
  return v8::String::NewFromUtf8(isolate, const_cast<char*>(str.c_str()),
                                 v8::NewStringType::kNormal, str.length())
      .ToLocalChecked();
#else
  // Convert the string to UTF8.
  std::string tmpStr = str;
  return v8::String::NewFromUtf8(isolate, tmpStr.c_str(),
                                 v8::NewStringType::kNormal, tmpStr.length())
      .ToLocalChecked();
#endif
}

#if defined(CEF_STRING_TYPE_UTF16)
void v8impl_string_dtor(char16_t* str) {
  delete[] str;
}
#elif defined(CEF_STRING_TYPE_UTF8)
void v8impl_string_dtor(char* str) {
  delete[] str;
}
#endif

// Convert a v8::String to CefString.
void GetCefString(v8::Isolate* isolate,
                  v8::Local<v8::String> str,
                  CefString& out) {
  if (str.IsEmpty()) {
    return;
  }

#if defined(CEF_STRING_TYPE_WIDE)
  // Allocate enough space for a worst-case conversion.
  int len = str->Utf8Length();
  if (len == 0) {
    return;
  }
  char* buf = new char[len + 1];
  str->WriteUtf8(isolate, buf, len + 1);

  // Perform conversion to the wide type.
  cef_string_t* retws = out.GetWritableStruct();
  cef_string_utf8_to_wide(buf, len, retws);

  delete[] buf;
#else  // !defined(CEF_STRING_TYPE_WIDE)
#if defined(CEF_STRING_TYPE_UTF16)
  int len = str->Length();
  if (len == 0) {
    return;
  }
  char16_t* buf = new char16_t[len + 1];
  str->Write(isolate, reinterpret_cast<uint16_t*>(buf), 0, len + 1);
#else
  // Allocate enough space for a worst-case conversion.
  int len = str->Utf8Length();
  if (len == 0) {
    return;
  }
  char* buf = new char[len + 1];
  str->WriteUtf8(isolate, buf, len + 1);
#endif

  // Don't perform an extra string copy.
  out.clear();
  cef_string_t* retws = out.GetWritableStruct();
  retws->str = buf;
  retws->length = len;
  retws->dtor = v8impl_string_dtor;
#endif  // !defined(CEF_STRING_TYPE_WIDE)
}

// V8 function callback.
void FunctionCallbackImpl(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  V8FunctionData* data = V8FunctionData::Unwrap(info.Data());
  if (!data->handler()) {
    // handler has gone away, bail!
    info.GetReturnValue().SetUndefined();
    return;
  }
  CefV8ValueList params;
  for (int i = 0; i < info.Length(); i++) {
    params.push_back(new CefV8ValueImpl(isolate, context, info[i]));
  }

  CefRefPtr<CefV8Value> object =
      new CefV8ValueImpl(isolate, context, info.This());
  CefRefPtr<CefV8Value> retval;
  CefString exception;

  if (data->handler()->Execute(data->function_name(), object, params, retval,
                               exception)) {
    if (!exception.empty()) {
      info.GetReturnValue().Set(isolate->ThrowException(
          v8::Exception::Error(GetV8String(isolate, exception))));
      return;
    } else {
      CefV8ValueImpl* rv = static_cast<CefV8ValueImpl*>(retval.get());
      if (rv && rv->IsValid()) {
        info.GetReturnValue().Set(rv->GetV8Value(true));
        return;
      }
    }
  }

  info.GetReturnValue().SetUndefined();
}

// V8 Accessor callbacks
void AccessorNameGetterCallbackImpl(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Object> obj = info.This();

  CefRefPtr<CefV8Accessor> accessorPtr;

  V8TrackObject* tracker = V8TrackObject::Unwrap(context, obj);
  if (tracker) {
    accessorPtr = tracker->GetAccessor();
  }

  if (accessorPtr.get()) {
    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Value> object = new CefV8ValueImpl(isolate, context, obj);
    CefString name, exception;
    GetCefString(isolate, v8::Local<v8::String>::Cast(property), name);
    if (accessorPtr->Get(name, object, retval, exception)) {
      if (!exception.empty()) {
        info.GetReturnValue().Set(isolate->ThrowException(
            v8::Exception::Error(GetV8String(isolate, exception))));
        return;
      } else {
        CefV8ValueImpl* rv = static_cast<CefV8ValueImpl*>(retval.get());
        if (rv && rv->IsValid()) {
          info.GetReturnValue().Set(rv->GetV8Value(true));
          return;
        }
      }
    }
  }

  return info.GetReturnValue().SetUndefined();
}

void AccessorNameSetterCallbackImpl(
    v8::Local<v8::Name> property,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Object> obj = info.This();

  CefRefPtr<CefV8Accessor> accessorPtr;

  V8TrackObject* tracker = V8TrackObject::Unwrap(context, obj);
  if (tracker) {
    accessorPtr = tracker->GetAccessor();
  }

  if (accessorPtr.get()) {
    CefRefPtr<CefV8Value> object = new CefV8ValueImpl(isolate, context, obj);
    CefRefPtr<CefV8Value> cefValue =
        new CefV8ValueImpl(isolate, context, value);
    CefString name, exception;
    GetCefString(isolate, v8::Local<v8::String>::Cast(property), name);
    accessorPtr->Set(name, object, cefValue, exception);
    if (!exception.empty()) {
      isolate->ThrowException(
          v8::Exception::Error(GetV8String(isolate, exception)));
      return;
    }
  }
}

// Two helper functions for V8 Interceptor callbacks.
CefString PropertyToIndex(v8::Isolate* isolate, v8::Local<v8::Name> property) {
  CefString name;
  GetCefString(isolate, property.As<v8::String>(), name);
  return name;
}

int PropertyToIndex(v8::Isolate* isolate, uint32_t index) {
  return static_cast<int>(index);
}

// V8 Interceptor callbacks.
// T == v8::Local<v8::Name> for named property handlers and
// T == uint32_t for indexed property handlers
template <typename T>
void InterceptorGetterCallbackImpl(
    T property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Handle<v8::Object> obj = info.This();
  CefRefPtr<CefV8Interceptor> interceptorPtr;

  V8TrackObject* tracker = V8TrackObject::Unwrap(context, obj);
  if (tracker) {
    interceptorPtr = tracker->GetInterceptor();
  }
  if (!interceptorPtr.get()) {
    return;
  }

  CefRefPtr<CefV8Value> object = new CefV8ValueImpl(isolate, context, obj);
  CefRefPtr<CefV8Value> retval;
  CefString exception;
  interceptorPtr->Get(PropertyToIndex(isolate, property), object, retval,
                      exception);
  if (!exception.empty()) {
    info.GetReturnValue().Set(isolate->ThrowException(
        v8::Exception::Error(GetV8String(isolate, exception))));
  } else {
    CefV8ValueImpl* retval_impl = static_cast<CefV8ValueImpl*>(retval.get());
    if (retval_impl && retval_impl->IsValid()) {
      info.GetReturnValue().Set(retval_impl->GetV8Value(true));
    }
  }
}

template <typename T>
void InterceptorSetterCallbackImpl(
    T property,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Handle<v8::Object> obj = info.This();
  CefRefPtr<CefV8Interceptor> interceptorPtr;

  V8TrackObject* tracker = V8TrackObject::Unwrap(context, obj);
  if (tracker) {
    interceptorPtr = tracker->GetInterceptor();
  }

  if (!interceptorPtr.get()) {
    return;
  }
  CefRefPtr<CefV8Value> object = new CefV8ValueImpl(isolate, context, obj);
  CefRefPtr<CefV8Value> cefValue = new CefV8ValueImpl(isolate, context, value);
  CefString exception;
  interceptorPtr->Set(PropertyToIndex(isolate, property), object, cefValue,
                      exception);
  if (!exception.empty()) {
    isolate->ThrowException(
        v8::Exception::Error(GetV8String(isolate, exception)));
  }
}

// V8 extension registration.

class ExtensionWrapper : public v8::Extension {
 public:
  ExtensionWrapper(const char* extension_name,
                   const char* javascript_code,
                   CefV8Handler* handler)
      : v8::Extension(extension_name, javascript_code), handler_(handler) {}

  v8::Handle<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Handle<v8::String> name) override {
    if (!handler_) {
      return v8::Local<v8::FunctionTemplate>();
    }

    CefString func_name;
    GetCefString(isolate, name, func_name);

    v8::Local<v8::External> function_data =
        V8FunctionData::Create(isolate, func_name, handler_);

    return v8::FunctionTemplate::New(isolate, FunctionCallbackImpl,
                                     function_data);
  }

 private:
  CefV8Handler* handler_;
};

class CefV8ExceptionImpl : public CefV8Exception {
 public:
  CefV8ExceptionImpl(v8::Local<v8::Context> context,
                     v8::Local<v8::Message> message) {
    if (message.IsEmpty()) {
      return;
    }

    v8::Isolate* isolate = context->GetIsolate();
    GetCefString(isolate, message->Get(), message_);
    v8::MaybeLocal<v8::String> source_line = message->GetSourceLine(context);
    if (!source_line.IsEmpty()) {
      GetCefString(isolate, source_line.ToLocalChecked(), source_line_);
    }

    if (!message->GetScriptResourceName().IsEmpty()) {
      GetCefString(
          isolate,
          message->GetScriptResourceName()->ToString(context).ToLocalChecked(),
          script_);
    }

    v8::Maybe<int> line_number = message->GetLineNumber(context);
    if (!line_number.IsNothing()) {
      line_number_ = line_number.ToChecked();
    }
    start_position_ = message->GetStartPosition();
    end_position_ = message->GetEndPosition();
    start_column_ = message->GetStartColumn(context).FromJust();
    end_column_ = message->GetEndColumn(context).FromJust();
  }

  CefString GetMessage() override { return message_; }
  CefString GetSourceLine() override { return source_line_; }
  CefString GetScriptResourceName() override { return script_; }
  int GetLineNumber() override { return line_number_; }
  int GetStartPosition() override { return start_position_; }
  int GetEndPosition() override { return end_position_; }
  int GetStartColumn() override { return start_column_; }
  int GetEndColumn() override { return end_column_; }

 protected:
  CefString message_;
  CefString source_line_;
  CefString script_;
  int line_number_ = 0;
  int start_position_ = 0;
  int end_position_ = 0;
  int start_column_ = 0;
  int end_column_ = 0;

  IMPLEMENT_REFCOUNTING(CefV8ExceptionImpl);
};

void MessageListenerCallbackImpl(v8::Handle<v8::Message> message,
                                 v8::Handle<v8::Value> data) {
  CefRefPtr<CefApp> application = CefAppManager::Get()->GetApplication();
  if (!application.get()) {
    return;
  }

  CefRefPtr<CefRenderProcessHandler> handler =
      application->GetRenderProcessHandler();
  if (!handler.get()) {
    return;
  }

  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
  v8::Local<v8::StackTrace> v8Stack = message->GetStackTrace();
  CefRefPtr<CefV8StackTrace> stackTrace =
      new CefV8StackTraceImpl(isolate, v8Stack);

  CefRefPtr<CefV8Exception> exception = new CefV8ExceptionImpl(
      static_cast<CefV8ContextImpl*>(context.get())->GetV8Context(), message);

  CefRefPtr<CefBrowser> browser = context->GetBrowser();
  if (browser) {
    handler->OnUncaughtException(browser, context->GetFrame(), context,
                                 exception, stackTrace);
  }
}

}  // namespace

// Global functions.

void CefV8IsolateCreated() {
  new CefV8IsolateManager();
}

void CefV8IsolateDestroyed() {
  auto* isolate_manager = CefV8IsolateManager::Get();
  isolate_manager->Destroy();
}

void CefV8ReleaseContext(v8::Local<v8::Context> context) {
  CefV8IsolateManager::Get()->ReleaseContext(context);
}

void CefV8SetUncaughtExceptionStackSize(int stack_size) {
  CefV8IsolateManager::Get()->SetUncaughtExceptionStackSize(stack_size);
}

void CefV8SetWorkerAttributes(int worker_id, const GURL& worker_url) {
  CefV8IsolateManager::Get()->SetWorkerAttributes(worker_id, worker_url);
}

bool CefRegisterExtension(const CefString& extension_name,
                          const CefString& javascript_code,
                          CefRefPtr<CefV8Handler> handler) {
  // Verify that this method was called on the correct thread.
  CEF_REQUIRE_RT_RETURN(false);

  auto* isolate_manager = CefV8IsolateManager::Get();

  V8TrackString* name = new V8TrackString(extension_name);
  isolate_manager->AddGlobalTrackObject(name);
  V8TrackString* code = new V8TrackString(javascript_code);
  isolate_manager->AddGlobalTrackObject(code);

  if (handler.get()) {
    // The reference will be released when the process exits.
    V8TrackObject* object = new V8TrackObject(isolate_manager->isolate());
    object->SetHandler(handler);
    isolate_manager->AddGlobalTrackObject(object);
  }

  std::unique_ptr<v8::Extension> wrapper(new ExtensionWrapper(
      name->GetString(), code->GetString(), handler.get()));

  blink::WebScriptController::RegisterExtension(std::move(wrapper));
  return true;
}

// Helper macros

#define CEF_V8_HAS_ISOLATE() (!!CefV8IsolateManager::Get())
#define CEF_V8_REQUIRE_ISOLATE_RETURN(var)      \
  if (!CEF_V8_HAS_ISOLATE()) {                  \
    DCHECK(false) << "V8 isolate is not valid"; \
    return var;                                 \
  }

#define CEF_V8_CURRENTLY_ON_MLT() \
  (!handle_.get() || handle_->BelongsToCurrentThread())
#define CEF_V8_REQUIRE_MLT_RETURN(var)             \
  CEF_V8_REQUIRE_ISOLATE_RETURN(var);              \
  if (!CEF_V8_CURRENTLY_ON_MLT()) {                \
    DCHECK(false) << "called on incorrect thread"; \
    return var;                                    \
  }

#define CEF_V8_HANDLE_IS_VALID() (handle_.get() && handle_->IsValid())
#define CEF_V8_REQUIRE_VALID_HANDLE_RETURN(ret) \
  CEF_V8_REQUIRE_MLT_RETURN(ret);               \
  if (!CEF_V8_HANDLE_IS_VALID()) {              \
    DCHECK(false) << "V8 handle is not valid";  \
    return ret;                                 \
  }

#define CEF_V8_IS_VALID()                               \
  (CEF_V8_HAS_ISOLATE() && CEF_V8_CURRENTLY_ON_MLT() && \
   CEF_V8_HANDLE_IS_VALID())

#define CEF_V8_REQUIRE_OBJECT_RETURN(ret)         \
  CEF_V8_REQUIRE_VALID_HANDLE_RETURN(ret);        \
  if (type_ != TYPE_OBJECT) {                     \
    DCHECK(false) << "V8 value is not an object"; \
    return ret;                                   \
  }

// CefV8HandleBase

CefV8HandleBase::~CefV8HandleBase() {
  DCHECK(BelongsToCurrentThread());
}

bool CefV8HandleBase::BelongsToCurrentThread() const {
  return task_runner_->RunsTasksInCurrentSequence();
}

CefV8HandleBase::CefV8HandleBase(v8::Isolate* isolate,
                                 v8::Local<v8::Context> context)
    : isolate_(isolate) {
  DCHECK(isolate_);

  CefV8IsolateManager* manager = CefV8IsolateManager::Get();
  DCHECK(manager);
  DCHECK_EQ(isolate_, manager->isolate());

  task_runner_ = manager->task_runner();
  context_state_ = manager->GetContextState(context);
}

// CefV8Context

// static
CefRefPtr<CefV8Context> CefV8Context::GetCurrentContext() {
  CefRefPtr<CefV8Context> context;
  CEF_V8_REQUIRE_ISOLATE_RETURN(context);
  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  if (isolate->InContext()) {
    v8::HandleScope handle_scope(isolate);
    context = new CefV8ContextImpl(isolate, isolate->GetCurrentContext());
  }
  return context;
}

// static
CefRefPtr<CefV8Context> CefV8Context::GetEnteredContext() {
  CefRefPtr<CefV8Context> context;
  CEF_V8_REQUIRE_ISOLATE_RETURN(context);
  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  if (isolate->InContext()) {
    v8::HandleScope handle_scope(isolate);
    context =
        new CefV8ContextImpl(isolate, isolate->GetEnteredOrMicrotaskContext());
  }
  return context;
}

// static
bool CefV8Context::InContext() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(false);
  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  return isolate->InContext();
}

// CefV8ContextImpl

CefV8ContextImpl::CefV8ContextImpl(v8::Isolate* isolate,
                                   v8::Local<v8::Context> context)
    : handle_(new Handle(isolate, context, context)),
      microtask_queue_(blink_glue::GetMicrotaskQueue(context)) {}

CefV8ContextImpl::~CefV8ContextImpl() {
  DLOG_ASSERT(0 == enter_count_);
}

CefRefPtr<CefTaskRunner> CefV8ContextImpl::GetTaskRunner() {
  return new CefTaskRunnerImpl(handle_->task_runner());
}

bool CefV8ContextImpl::IsValid() {
  return CEF_V8_IS_VALID();
}

CefRefPtr<CefBrowser> CefV8ContextImpl::GetBrowser() {
  CefRefPtr<CefBrowser> browser;
  CEF_V8_REQUIRE_VALID_HANDLE_RETURN(browser);

  blink::WebLocalFrame* webframe = GetWebFrame();
  if (webframe) {
    browser = CefBrowserImpl::GetBrowserForMainFrame(webframe->Top());
  }

  return browser;
}

CefRefPtr<CefFrame> CefV8ContextImpl::GetFrame() {
  CefRefPtr<CefFrame> frame;
  CEF_V8_REQUIRE_VALID_HANDLE_RETURN(frame);

  blink::WebLocalFrame* webframe = GetWebFrame();
  if (webframe) {
    CefRefPtr<CefBrowserImpl> browser =
        CefBrowserImpl::GetBrowserForMainFrame(webframe->Top());
    if (browser) {
      frame = browser->GetWebFrameImpl(webframe).get();
    }
  }

  return frame;
}

CefRefPtr<CefV8Value> CefV8ContextImpl::GetGlobal() {
  CEF_V8_REQUIRE_VALID_HANDLE_RETURN(nullptr);

  if (blink_glue::IsScriptForbidden()) {
    return nullptr;
  }

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetV8Context();
  v8::Context::Scope context_scope(context);
  return new CefV8ValueImpl(isolate, context, context->Global());
}

bool CefV8ContextImpl::Enter() {
  CEF_V8_REQUIRE_VALID_HANDLE_RETURN(false);

  if (blink_glue::IsScriptForbidden()) {
    return false;
  }

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  if (!microtasks_scope_) {
    // Increment the MicrotasksScope recursion level.
    microtasks_scope_ = std::make_unique<v8::MicrotasksScope>(
        isolate, microtask_queue_, v8::MicrotasksScope::kRunMicrotasks);
  }

  ++enter_count_;
  handle_->GetNewV8Handle()->Enter();

  return true;
}

bool CefV8ContextImpl::Exit() {
  CEF_V8_REQUIRE_VALID_HANDLE_RETURN(false);

  if (blink_glue::IsScriptForbidden()) {
    return false;
  }

  if (enter_count_ <= 0) {
    LOG(ERROR) << "Call to CefV8Context::Exit() without matching call to "
                  "CefV8Context::Enter()";
    return false;
  }

  v8::HandleScope handle_scope(handle_->isolate());

  handle_->GetNewV8Handle()->Exit();

  if (--enter_count_ == 0) {
    // Decrement the MicrotasksScope recursion level.
    microtasks_scope_.reset(nullptr);
  }

  return true;
}

bool CefV8ContextImpl::IsSame(CefRefPtr<CefV8Context> that) {
  CEF_V8_REQUIRE_VALID_HANDLE_RETURN(false);

  CefV8ContextImpl* impl = static_cast<CefV8ContextImpl*>(that.get());
  if (!impl || !impl->IsValid()) {
    return false;
  }

  return (handle_->GetPersistentV8Handle() ==
          impl->handle_->GetPersistentV8Handle());
}

bool CefV8ContextImpl::Eval(const CefString& code,
                            const CefString& script_url,
                            int start_line,
                            CefRefPtr<CefV8Value>& retval,
                            CefRefPtr<CefV8Exception>& exception) {
  retval = nullptr;
  exception = nullptr;

  CEF_V8_REQUIRE_VALID_HANDLE_RETURN(false);

  if (blink_glue::IsScriptForbidden()) {
    return false;
  }

  if (code.empty()) {
    DCHECK(false) << "invalid input parameter";
    return false;
  }

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetV8Context();
  v8::Context::Scope context_scope(context);

  const blink::WebString& source =
      blink::WebString::FromUTF16(code.ToString16());
  const blink::WebString& source_url =
      blink::WebString::FromUTF16(script_url.ToString16());

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  v8::Local<v8::Value> func_rv = blink_glue::ExecuteV8ScriptAndReturnValue(
      source, source_url, start_line, context, try_catch);

  if (try_catch.HasCaught()) {
    exception = new CefV8ExceptionImpl(context, try_catch.Message());
    return false;
  } else if (!func_rv.IsEmpty()) {
    retval = new CefV8ValueImpl(isolate, context, func_rv);
    return true;
  }

  DCHECK(false);
  return false;
}

v8::Local<v8::Context> CefV8ContextImpl::GetV8Context() {
  return handle_->GetNewV8Handle();
}

blink::WebLocalFrame* CefV8ContextImpl::GetWebFrame() {
  CEF_REQUIRE_RT();

  if (blink_glue::IsScriptForbidden()) {
    return nullptr;
  }

  v8::HandleScope handle_scope(handle_->isolate());
  v8::Local<v8::Context> context = GetV8Context();
  v8::Context::Scope context_scope(context);
  return blink::WebLocalFrame::FrameForContext(context);
}

// CefV8ValueImpl::Handle

CefV8ValueImpl::Handle::Handle(v8::Isolate* isolate,
                               v8::Local<v8::Context> context,
                               handleType v,
                               CefTrackNode* tracker)
    : CefV8HandleBase(isolate, context),
      handle_(isolate, v),
      tracker_(tracker) {}

CefV8ValueImpl::Handle::~Handle() {
  DCHECK(BelongsToCurrentThread());

  if (tracker_) {
    if (is_set_weak_) {
      if (context_state_.get()) {
        // If the associated context is still valid then delete |tracker_|.
        // Otherwise, |tracker_| will already have been deleted.
        if (context_state_->IsValid()) {
          context_state_->DeleteTrackObject(tracker_);
        }
      } else {
        CefV8IsolateManager::Get()->DeleteGlobalTrackObject(tracker_);
      }
    } else {
      delete tracker_;
    }
  }

  if (is_set_weak_) {
    isolate_->AdjustAmountOfExternalAllocatedMemory(
        -static_cast<int>(sizeof(Handle)));
  } else {
    // SetWeak was not called so reset now.
    handle_.Reset();
  }
}

CefV8ValueImpl::Handle::handleType CefV8ValueImpl::Handle::GetNewV8Handle(
    bool should_persist) {
  DCHECK(IsValid());
  if (should_persist && !should_persist_) {
    should_persist_ = true;
  }
  return handleType::New(isolate(), handle_);
}

CefV8ValueImpl::Handle::persistentType&
CefV8ValueImpl::Handle::GetPersistentV8Handle() {
  return handle_;
}

void CefV8ValueImpl::Handle::SetWeakIfNecessary() {
  if (!BelongsToCurrentThread()) {
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CefV8ValueImpl::Handle::SetWeakIfNecessary, this));
    return;
  }

  // Persist the handle (call SetWeak) if:
  // A. The handle has been passed into a V8 function or used as a return value
  //    from a V8 callback, and
  // B. The associated context, if any, is still valid.
  if (should_persist_ && (!context_state_.get() || context_state_->IsValid())) {
    is_set_weak_ = true;

    if (tracker_) {
      if (context_state_.get()) {
        // |tracker_| will be deleted when:
        // A. The associated context is released, or
        // B. SecondWeakCallback is called for the weak handle.
        DCHECK(context_state_->IsValid());
        context_state_->AddTrackObject(tracker_);
      } else {
        // |tracker_| will be deleted when:
        // A. The process shuts down, or
        // B. SecondWeakCallback is called for the weak handle.
        CefV8IsolateManager::Get()->AddGlobalTrackObject(tracker_);
      }
    }

    isolate_->AdjustAmountOfExternalAllocatedMemory(
        static_cast<int>(sizeof(Handle)));

    // The added reference will be released in SecondWeakCallback.
    AddRef();
    handle_.SetWeak(this, FirstWeakCallback, v8::WeakCallbackType::kParameter);
  }
}

// static
void CefV8ValueImpl::Handle::FirstWeakCallback(
    const v8::WeakCallbackInfo<Handle>& data) {
  Handle* wrapper = data.GetParameter();
  wrapper->handle_.Reset();
  data.SetSecondPassCallback(SecondWeakCallback);
}

// static
void CefV8ValueImpl::Handle::SecondWeakCallback(
    const v8::WeakCallbackInfo<Handle>& data) {
  Handle* wrapper = data.GetParameter();
  wrapper->Release();
}

// CefV8Value

// static
CefRefPtr<CefV8Value> CefV8Value::CreateUndefined() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(nullptr);
  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  CefRefPtr<CefV8ValueImpl> impl = new CefV8ValueImpl(isolate);
  impl->InitUndefined();
  return impl.get();
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateNull() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(nullptr);
  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  CefRefPtr<CefV8ValueImpl> impl = new CefV8ValueImpl(isolate);
  impl->InitNull();
  return impl.get();
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateBool(bool value) {
  CEF_V8_REQUIRE_ISOLATE_RETURN(nullptr);
  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  CefRefPtr<CefV8ValueImpl> impl = new CefV8ValueImpl(isolate);
  impl->InitBool(value);
  return impl.get();
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateInt(int32_t value) {
  CEF_V8_REQUIRE_ISOLATE_RETURN(nullptr);
  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  CefRefPtr<CefV8ValueImpl> impl = new CefV8ValueImpl(isolate);
  impl->InitInt(value);
  return impl.get();
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateUInt(uint32_t value) {
  CEF_V8_REQUIRE_ISOLATE_RETURN(nullptr);
  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  CefRefPtr<CefV8ValueImpl> impl = new CefV8ValueImpl(isolate);
  impl->InitUInt(value);
  return impl.get();
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateDouble(double value) {
  CEF_V8_REQUIRE_ISOLATE_RETURN(nullptr);
  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  CefRefPtr<CefV8ValueImpl> impl = new CefV8ValueImpl(isolate);
  impl->InitDouble(value);
  return impl.get();
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateDate(CefBaseTime value) {
  CEF_V8_REQUIRE_ISOLATE_RETURN(nullptr);
  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  CefRefPtr<CefV8ValueImpl> impl = new CefV8ValueImpl(isolate);
  impl->InitDate(value);
  return impl.get();
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateString(const CefString& value) {
  CEF_V8_REQUIRE_ISOLATE_RETURN(nullptr);
  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  CefRefPtr<CefV8ValueImpl> impl = new CefV8ValueImpl(isolate);
  CefString str(value);
  impl->InitString(str);
  return impl.get();
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateObject(
    CefRefPtr<CefV8Accessor> accessor,
    CefRefPtr<CefV8Interceptor> interceptor) {
  CEF_V8_REQUIRE_ISOLATE_RETURN(nullptr);

  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return nullptr;
  }

  // Create the new V8 object. If an interceptor is passed, create object from
  // template and set property handlers.
  v8::Local<v8::Object> obj;
  if (interceptor.get()) {
    v8::Local<v8::ObjectTemplate> tmpl = v8::ObjectTemplate::New(isolate);
    tmpl->SetHandler(v8::NamedPropertyHandlerConfiguration(
        InterceptorGetterCallbackImpl<v8::Local<v8::Name>>,
        InterceptorSetterCallbackImpl<v8::Local<v8::Name>>, nullptr, nullptr,
        nullptr, v8::Local<v8::Value>(),
        v8::PropertyHandlerFlags::kOnlyInterceptStrings));

    tmpl->SetIndexedPropertyHandler(InterceptorGetterCallbackImpl<uint32_t>,
                                    InterceptorSetterCallbackImpl<uint32_t>);

    v8::MaybeLocal<v8::Object> maybe_object = tmpl->NewInstance(context);
    if (!maybe_object.ToLocal<v8::Object>(&obj)) {
      DCHECK(false) << "Failed to create V8 Object with interceptor";
      return nullptr;
    }
  } else {
    obj = v8::Object::New(isolate);
  }

  // Create a tracker object that will cause the user data and/or accessor
  // and/or interceptor reference to be released when the V8 object is
  // destroyed.
  V8TrackObject* tracker = new V8TrackObject(isolate);
  tracker->SetAccessor(accessor);
  tracker->SetInterceptor(interceptor);

  // Attach the tracker object.
  tracker->AttachTo(context, obj);

  CefRefPtr<CefV8ValueImpl> impl = new CefV8ValueImpl(isolate);
  impl->InitObject(obj, tracker);
  return impl.get();
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateArray(int length) {
  CEF_V8_REQUIRE_ISOLATE_RETURN(nullptr);

  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return nullptr;
  }

  // Create a tracker object that will cause the user data reference to be
  // released when the V8 object is destroyed.
  V8TrackObject* tracker = new V8TrackObject(isolate);

  // Create the new V8 array.
  v8::Local<v8::Array> arr = v8::Array::New(isolate, length);

  // Attach the tracker object.
  tracker->AttachTo(context, arr);

  CefRefPtr<CefV8ValueImpl> impl = new CefV8ValueImpl(isolate);
  impl->InitObject(arr, tracker);
  return impl.get();
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateArrayBuffer(
    void* buffer,
    size_t length,
    CefRefPtr<CefV8ArrayBufferReleaseCallback> release_callback) {
  CEF_V8_REQUIRE_ISOLATE_RETURN(nullptr);

  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return nullptr;
  }

  // Create a tracker object that will cause the user data reference to be
  // released when the V8 object is destroyed.
  V8TrackArrayBuffer* tracker =
      new V8TrackArrayBuffer(isolate, release_callback);

  if (release_callback) {
    release_callback->AddRef();
  }

  auto deleter = [](void* data, size_t length, void* deleter_data) {
    auto* release_callback =
        reinterpret_cast<CefV8ArrayBufferReleaseCallback*>(deleter_data);
    if (release_callback) {
      release_callback->ReleaseBuffer(data);
      release_callback->Release();
    }
  };

  std::unique_ptr<v8::BackingStore> backing = v8::ArrayBuffer::NewBackingStore(
      buffer, length, deleter, release_callback.get());
  v8::Local<v8::ArrayBuffer> ab =
      v8::ArrayBuffer::New(isolate, std::move(backing));

  // Attach the tracker object.
  tracker->AttachTo(context, ab);

  CefRefPtr<CefV8ValueImpl> impl = new CefV8ValueImpl(isolate);
  impl->InitObject(ab, tracker);
  return impl.get();
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateFunction(
    const CefString& name,
    CefRefPtr<CefV8Handler> handler) {
  CEF_V8_REQUIRE_ISOLATE_RETURN(nullptr);

  if (!handler.get()) {
    DCHECK(false) << "invalid parameter";
    return nullptr;
  }

  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return nullptr;
  }

  v8::Local<v8::External> function_data =
      V8FunctionData::Create(isolate, name, handler);

  // Create a new V8 function template.
  v8::Local<v8::FunctionTemplate> tmpl =
      v8::FunctionTemplate::New(isolate, FunctionCallbackImpl, function_data);

  // Retrieve the function object and set the name.
  v8::MaybeLocal<v8::Function> maybe_func = tmpl->GetFunction(context);
  v8::Local<v8::Function> func;
  if (!maybe_func.ToLocal(&func)) {
    DCHECK(false) << "failed to create V8 function";
    return nullptr;
  }

  func->SetName(GetV8String(isolate, name));

  // Create a tracker object that will cause the user data and/or handler
  // reference to be released when the V8 object is destroyed.
  V8TrackObject* tracker = new V8TrackObject(isolate);
  tracker->SetHandler(handler);

  // Attach the tracker object.
  tracker->AttachTo(context, func);

  // Create the CefV8ValueImpl and provide a tracker object that will cause
  // the handler reference to be released when the V8 object is destroyed.
  CefRefPtr<CefV8ValueImpl> impl = new CefV8ValueImpl(isolate);
  impl->InitObject(func, tracker);
  return impl.get();
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreatePromise() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(nullptr);
  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return nullptr;
  }

  v8::Local<v8::Promise::Resolver> promise_resolver =
      v8::Promise::Resolver::New(context).ToLocalChecked();

  // Create a tracker object that will cause the user data reference to be
  // released when the V8 object is destroyed.
  V8TrackObject* tracker = new V8TrackObject(isolate);

  // Attach the tracker object.
  tracker->AttachTo(context, promise_resolver);

  CefRefPtr<CefV8ValueImpl> impl = new CefV8ValueImpl(isolate);
  impl->InitObject(promise_resolver, tracker);
  return impl.get();
}

// CefV8ValueImpl

CefV8ValueImpl::CefV8ValueImpl(v8::Isolate* isolate)
    : isolate_(isolate), type_(TYPE_INVALID), rethrow_exceptions_(false) {
  DCHECK(isolate_);
}

CefV8ValueImpl::CefV8ValueImpl(v8::Isolate* isolate,
                               v8::Local<v8::Context> context,
                               v8::Local<v8::Value> value)
    : isolate_(isolate), type_(TYPE_INVALID), rethrow_exceptions_(false) {
  DCHECK(isolate_);
  InitFromV8Value(context, value);
}

CefV8ValueImpl::~CefV8ValueImpl() {
  if (type_ == TYPE_STRING) {
    cef_string_clear(&string_value_);
  }
  if (handle_.get()) {
    handle_->SetWeakIfNecessary();
  }
}

void CefV8ValueImpl::InitFromV8Value(v8::Local<v8::Context> context,
                                     v8::Local<v8::Value> value) {
  if (value->IsUndefined()) {
    InitUndefined();
  } else if (value->IsNull()) {
    InitNull();
  } else if (value->IsTrue()) {
    InitBool(true);
  } else if (value->IsFalse()) {
    InitBool(false);
  } else if (value->IsBoolean()) {
    InitBool(value->ToBoolean(context->GetIsolate())->Value());
  } else if (value->IsInt32()) {
    InitInt(value->ToInt32(context).ToLocalChecked()->Value());
  } else if (value->IsUint32()) {
    InitUInt(value->ToUint32(context).ToLocalChecked()->Value());
  } else if (value->IsNumber()) {
    InitDouble(value->ToNumber(context).ToLocalChecked()->Value());
  } else if (value->IsDate()) {
    // Convert from milliseconds to seconds.
    InitDate(base::Time::FromMillisecondsSinceUnixEpoch(
        value->ToNumber(context).ToLocalChecked()->Value()));
  } else if (value->IsString()) {
    CefString rv;
    GetCefString(context->GetIsolate(),
                 value->ToString(context).ToLocalChecked(), rv);
    InitString(rv);
  } else if (value->IsObject()) {
    InitObject(value, nullptr);
  }
}

void CefV8ValueImpl::InitUndefined() {
  DCHECK_EQ(type_, TYPE_INVALID);
  type_ = TYPE_UNDEFINED;
}

void CefV8ValueImpl::InitNull() {
  DCHECK_EQ(type_, TYPE_INVALID);
  type_ = TYPE_NULL;
}

void CefV8ValueImpl::InitBool(bool value) {
  DCHECK_EQ(type_, TYPE_INVALID);
  type_ = TYPE_BOOL;
  bool_value_ = value;
}

void CefV8ValueImpl::InitInt(int32_t value) {
  DCHECK_EQ(type_, TYPE_INVALID);
  type_ = TYPE_INT;
  int_value_ = value;
}

void CefV8ValueImpl::InitUInt(uint32_t value) {
  DCHECK_EQ(type_, TYPE_INVALID);
  type_ = TYPE_UINT;
  uint_value_ = value;
}

void CefV8ValueImpl::InitDouble(double value) {
  DCHECK_EQ(type_, TYPE_INVALID);
  type_ = TYPE_DOUBLE;
  double_value_ = value;
}

void CefV8ValueImpl::InitDate(CefBaseTime value) {
  DCHECK_EQ(type_, TYPE_INVALID);
  type_ = TYPE_DATE;
  date_value_ = value;
}

void CefV8ValueImpl::InitString(CefString& value) {
  DCHECK_EQ(type_, TYPE_INVALID);
  type_ = TYPE_STRING;
  // Take ownership of the underling string value.
  const cef_string_t* str = value.GetStruct();
  if (str) {
    string_value_ = *str;
    cef_string_t* writable_struct = value.GetWritableStruct();
    writable_struct->str = nullptr;
    writable_struct->length = 0;
  } else {
    string_value_.str = nullptr;
    string_value_.length = 0;
  }
}

void CefV8ValueImpl::InitObject(v8::Local<v8::Value> value,
                                CefTrackNode* tracker) {
  DCHECK_EQ(type_, TYPE_INVALID);
  type_ = TYPE_OBJECT;
  handle_ = new Handle(isolate_, v8::Local<v8::Context>(), value, tracker);
}

v8::Local<v8::Value> CefV8ValueImpl::GetV8Value(bool should_persist) {
  switch (type_) {
    case TYPE_UNDEFINED:
      return v8::Undefined(isolate_);
    case TYPE_NULL:
      return v8::Null(isolate_);
    case TYPE_BOOL:
      return v8::Boolean::New(isolate_, bool_value_);
    case TYPE_INT:
      return v8::Int32::New(isolate_, int_value_);
    case TYPE_UINT:
      return v8::Uint32::NewFromUnsigned(isolate_, uint_value_);
    case TYPE_DOUBLE:
      return v8::Number::New(isolate_, double_value_);
    case TYPE_DATE:
      // Convert from seconds to milliseconds.
      return v8::Date::New(isolate_->GetCurrentContext(),
                           static_cast<base::Time>(CefBaseTime(date_value_))
                               .InMillisecondsFSinceUnixEpochIgnoringNull())
          .ToLocalChecked();
    case TYPE_STRING:
      return GetV8String(isolate_, CefString(&string_value_));
    case TYPE_OBJECT:
      return handle_->GetNewV8Handle(should_persist);
    default:
      break;
  }

  DCHECK(false) << "Invalid type for CefV8ValueImpl";
  return v8::Local<v8::Value>();
}

bool CefV8ValueImpl::IsValid() {
  if (!CEF_V8_HAS_ISOLATE() || type_ == TYPE_INVALID ||
      (type_ == TYPE_OBJECT &&
       (!handle_->BelongsToCurrentThread() || !handle_->IsValid()))) {
    return false;
  }
  return true;
}

bool CefV8ValueImpl::IsUndefined() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(false);
  return (type_ == TYPE_UNDEFINED);
}

bool CefV8ValueImpl::IsNull() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(false);
  return (type_ == TYPE_NULL);
}

bool CefV8ValueImpl::IsBool() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(false);
  return (type_ == TYPE_BOOL);
}

bool CefV8ValueImpl::IsInt() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(false);
  return (type_ == TYPE_INT || type_ == TYPE_UINT);
}

bool CefV8ValueImpl::IsUInt() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(false);
  return (type_ == TYPE_INT || type_ == TYPE_UINT);
}

bool CefV8ValueImpl::IsDouble() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(false);
  return (type_ == TYPE_INT || type_ == TYPE_UINT || type_ == TYPE_DOUBLE);
}

bool CefV8ValueImpl::IsDate() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(false);
  return (type_ == TYPE_DATE);
}

bool CefV8ValueImpl::IsString() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(false);
  return (type_ == TYPE_STRING);
}

bool CefV8ValueImpl::IsObject() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(false);
  return (type_ == TYPE_OBJECT);
}

bool CefV8ValueImpl::IsArray() {
  CEF_V8_REQUIRE_MLT_RETURN(false);
  if (type_ == TYPE_OBJECT) {
    v8::HandleScope handle_scope(handle_->isolate());
    return handle_->GetNewV8Handle(false)->IsArray();
  } else {
    return false;
  }
}

bool CefV8ValueImpl::IsArrayBuffer() {
  CEF_V8_REQUIRE_MLT_RETURN(false);
  if (type_ == TYPE_OBJECT) {
    v8::HandleScope handle_scope(handle_->isolate());
    return handle_->GetNewV8Handle(false)->IsArrayBuffer();
  } else {
    return false;
  }
}

bool CefV8ValueImpl::IsFunction() {
  CEF_V8_REQUIRE_MLT_RETURN(false);
  if (type_ == TYPE_OBJECT) {
    v8::HandleScope handle_scope(handle_->isolate());
    return handle_->GetNewV8Handle(false)->IsFunction();
  } else {
    return false;
  }
}

bool CefV8ValueImpl::IsPromise() {
  CEF_V8_REQUIRE_MLT_RETURN(false);
  if (type_ == TYPE_OBJECT) {
    v8::HandleScope handle_scope(handle_->isolate());
    return handle_->GetNewV8Handle(false)->IsPromise();
  } else {
    return false;
  }
}

bool CefV8ValueImpl::IsSame(CefRefPtr<CefV8Value> that) {
  CEF_V8_REQUIRE_MLT_RETURN(false);

  CefV8ValueImpl* thatValue = static_cast<CefV8ValueImpl*>(that.get());
  if (!thatValue || !thatValue->IsValid() || type_ != thatValue->type_) {
    return false;
  }

  switch (type_) {
    case TYPE_UNDEFINED:
    case TYPE_NULL:
      return true;
    case TYPE_BOOL:
      return (bool_value_ == thatValue->bool_value_);
    case TYPE_INT:
      return (int_value_ == thatValue->int_value_);
    case TYPE_UINT:
      return (uint_value_ == thatValue->uint_value_);
    case TYPE_DOUBLE:
      return (double_value_ == thatValue->double_value_);
    case TYPE_DATE:
      return (date_value_.val == thatValue->date_value_.val);
    case TYPE_STRING:
      return (CefString(&string_value_) ==
              CefString(&thatValue->string_value_));
    case TYPE_OBJECT: {
      return (handle_->GetPersistentV8Handle() ==
              thatValue->handle_->GetPersistentV8Handle());
    }
    default:
      break;
  }

  return false;
}

bool CefV8ValueImpl::GetBoolValue() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(false);
  if (type_ == TYPE_BOOL) {
    return bool_value_;
  }
  return false;
}

int32_t CefV8ValueImpl::GetIntValue() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(0);
  if (type_ == TYPE_INT || type_ == TYPE_UINT) {
    return int_value_;
  }
  return 0;
}

uint32_t CefV8ValueImpl::GetUIntValue() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(0);
  if (type_ == TYPE_INT || type_ == TYPE_UINT) {
    return uint_value_;
  }
  return 0;
}

double CefV8ValueImpl::GetDoubleValue() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(0.);
  if (type_ == TYPE_DOUBLE) {
    return double_value_;
  } else if (type_ == TYPE_INT) {
    return int_value_;
  } else if (type_ == TYPE_UINT) {
    return uint_value_;
  }
  return 0.;
}

CefBaseTime CefV8ValueImpl::GetDateValue() {
  CEF_V8_REQUIRE_ISOLATE_RETURN(CefBaseTime());
  if (type_ == TYPE_DATE) {
    return date_value_;
  }
  return CefBaseTime();
}

CefString CefV8ValueImpl::GetStringValue() {
  CefString rv;
  CEF_V8_REQUIRE_ISOLATE_RETURN(rv);
  if (type_ == TYPE_STRING) {
    rv = CefString(&string_value_);
  }
  return rv;
}

bool CefV8ValueImpl::IsUserCreated() {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return false;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();

  V8TrackObject* tracker = V8TrackObject::Unwrap(context, obj);
  return (tracker != nullptr);
}

bool CefV8ValueImpl::HasException() {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  return (last_exception_.get() != nullptr);
}

CefRefPtr<CefV8Exception> CefV8ValueImpl::GetException() {
  CEF_V8_REQUIRE_OBJECT_RETURN(nullptr);

  return last_exception_;
}

bool CefV8ValueImpl::ClearException() {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  last_exception_ = nullptr;
  return true;
}

bool CefV8ValueImpl::WillRethrowExceptions() {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  return rethrow_exceptions_;
}

bool CefV8ValueImpl::SetRethrowExceptions(bool rethrow) {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  rethrow_exceptions_ = rethrow;
  return true;
}

bool CefV8ValueImpl::HasValue(const CefString& key) {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return false;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();
  return obj->Has(context, GetV8String(isolate, key)).FromJust();
}

bool CefV8ValueImpl::HasValue(int index) {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  if (index < 0) {
    DCHECK(false) << "invalid input parameter";
    return false;
  }

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return false;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();
  return obj->Has(context, index).FromJust();
}

bool CefV8ValueImpl::DeleteValue(const CefString& key) {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return false;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);
  v8::Maybe<bool> del = obj->Delete(context, GetV8String(isolate, key));
  return (!HasCaught(context, try_catch) && del.FromJust());
}

bool CefV8ValueImpl::DeleteValue(int index) {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  if (index < 0) {
    DCHECK(false) << "invalid input parameter";
    return false;
  }

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return false;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);
  v8::Maybe<bool> del = obj->Delete(context, index);
  return (!HasCaught(context, try_catch) && del.FromJust());
}

CefRefPtr<CefV8Value> CefV8ValueImpl::GetValue(const CefString& key) {
  CEF_V8_REQUIRE_OBJECT_RETURN(nullptr);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return nullptr;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);
  v8::MaybeLocal<v8::Value> ret_value =
      obj->Get(context, GetV8String(isolate, key));
  if (!HasCaught(context, try_catch) && !ret_value.IsEmpty()) {
    return new CefV8ValueImpl(isolate, context, ret_value.ToLocalChecked());
  }
  return nullptr;
}

CefRefPtr<CefV8Value> CefV8ValueImpl::GetValue(int index) {
  CEF_V8_REQUIRE_OBJECT_RETURN(nullptr);

  if (index < 0) {
    DCHECK(false) << "invalid input parameter";
    return nullptr;
  }

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return nullptr;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);
  v8::MaybeLocal<v8::Value> ret_value =
      obj->Get(context, v8::Number::New(isolate, index));
  if (!HasCaught(context, try_catch) && !ret_value.IsEmpty()) {
    return new CefV8ValueImpl(isolate, context, ret_value.ToLocalChecked());
  }
  return nullptr;
}

bool CefV8ValueImpl::SetValue(const CefString& key,
                              CefRefPtr<CefV8Value> value,
                              PropertyAttribute attribute) {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  CefV8ValueImpl* impl = static_cast<CefV8ValueImpl*>(value.get());
  if (impl && impl->IsValid()) {
    v8::Isolate* isolate = handle_->isolate();
    v8::HandleScope handle_scope(isolate);

    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    if (context.IsEmpty()) {
      DCHECK(false) << "not currently in a V8 context";
      return false;
    }

    v8::Local<v8::Value> v8value = handle_->GetNewV8Handle(false);
    v8::Local<v8::Object> obj = v8value->ToObject(context).ToLocalChecked();

    v8::TryCatch try_catch(isolate);
    try_catch.SetVerbose(true);
    // TODO(cef): This usage may not exactly match the previous implementation.
    // Set will trigger interceptors and/or accessors whereas DefineOwnProperty
    // will not. It might be better to split this functionality into separate
    // methods.
    if (attribute == V8_PROPERTY_ATTRIBUTE_NONE) {
      v8::Maybe<bool> set =
          obj->Set(context, GetV8String(isolate, key), impl->GetV8Value(true));
      return (!HasCaught(context, try_catch) && set.FromJust());
    } else {
      v8::Maybe<bool> set = obj->DefineOwnProperty(
          context, GetV8String(isolate, key), impl->GetV8Value(true),
          static_cast<v8::PropertyAttribute>(attribute));
      return (!HasCaught(context, try_catch) && set.FromJust());
    }
  } else {
    DCHECK(false) << "invalid input parameter";
    return false;
  }
}

bool CefV8ValueImpl::SetValue(int index, CefRefPtr<CefV8Value> value) {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  if (index < 0) {
    DCHECK(false) << "invalid input parameter";
    return false;
  }

  CefV8ValueImpl* impl = static_cast<CefV8ValueImpl*>(value.get());
  if (impl && impl->IsValid()) {
    v8::Isolate* isolate = handle_->isolate();
    v8::HandleScope handle_scope(isolate);

    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    if (context.IsEmpty()) {
      DCHECK(false) << "not currently in a V8 context";
      return false;
    }

    v8::Local<v8::Value> v8value = handle_->GetNewV8Handle(false);
    v8::Local<v8::Object> obj = v8value->ToObject(context).ToLocalChecked();

    v8::TryCatch try_catch(isolate);
    try_catch.SetVerbose(true);
    v8::Maybe<bool> set = obj->Set(context, index, impl->GetV8Value(true));
    return (!HasCaught(context, try_catch) && set.FromJust());
  } else {
    DCHECK(false) << "invalid input parameter";
    return false;
  }
}

bool CefV8ValueImpl::SetValue(const CefString& key,
                              AccessControl settings,
                              PropertyAttribute attribute) {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return false;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();

  CefRefPtr<CefV8Accessor> accessorPtr;

  V8TrackObject* tracker = V8TrackObject::Unwrap(context, obj);
  if (tracker) {
    accessorPtr = tracker->GetAccessor();
  }

  // Verify that an accessor exists for this object.
  if (!accessorPtr.get()) {
    return false;
  }

  v8::AccessorNameGetterCallback getter = AccessorNameGetterCallbackImpl;
  v8::AccessorNameSetterCallback setter =
      (attribute & V8_PROPERTY_ATTRIBUTE_READONLY)
          ? nullptr
          : AccessorNameSetterCallbackImpl;

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);
  v8::Maybe<bool> set =
      obj->SetAccessor(context, GetV8String(isolate, key), getter, setter, obj,
                       static_cast<v8::AccessControl>(settings),
                       static_cast<v8::PropertyAttribute>(attribute));
  return (!HasCaught(context, try_catch) && set.FromJust());
}

bool CefV8ValueImpl::GetKeys(std::vector<CefString>& keys) {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return false;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();
  v8::Local<v8::Array> arr_keys =
      obj->GetPropertyNames(context).ToLocalChecked();

  uint32_t len = arr_keys->Length();
  for (uint32_t i = 0; i < len; ++i) {
    v8::Local<v8::Value> arr_value =
        arr_keys->Get(context, v8::Integer::New(isolate, i)).ToLocalChecked();
    CefString str;
    GetCefString(isolate, arr_value->ToString(context).ToLocalChecked(), str);
    keys.push_back(str);
  }
  return true;
}

bool CefV8ValueImpl::SetUserData(CefRefPtr<CefBaseRefCounted> user_data) {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return false;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();

  V8TrackObject* tracker = V8TrackObject::Unwrap(context, obj);
  if (tracker) {
    tracker->SetUserData(user_data);
    return true;
  }

  return false;
}

CefRefPtr<CefBaseRefCounted> CefV8ValueImpl::GetUserData() {
  CEF_V8_REQUIRE_OBJECT_RETURN(nullptr);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return nullptr;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();

  V8TrackObject* tracker = V8TrackObject::Unwrap(context, obj);
  if (tracker) {
    return tracker->GetUserData();
  }

  return nullptr;
}

int CefV8ValueImpl::GetExternallyAllocatedMemory() {
  CEF_V8_REQUIRE_OBJECT_RETURN(0);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return 0;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();

  V8TrackObject* tracker = V8TrackObject::Unwrap(context, obj);
  if (tracker) {
    return tracker->GetExternallyAllocatedMemory();
  }

  return 0;
}

int CefV8ValueImpl::AdjustExternallyAllocatedMemory(int change_in_bytes) {
  CEF_V8_REQUIRE_OBJECT_RETURN(0);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return 0;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();

  V8TrackObject* tracker = V8TrackObject::Unwrap(context, obj);
  if (tracker) {
    return tracker->AdjustExternallyAllocatedMemory(change_in_bytes);
  }

  return 0;
}

int CefV8ValueImpl::GetArrayLength() {
  CEF_V8_REQUIRE_OBJECT_RETURN(0);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return 0;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  if (!value->IsArray()) {
    DCHECK(false) << "V8 value is not an array";
    return 0;
  }

  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();
  v8::Local<v8::Array> arr = v8::Local<v8::Array>::Cast(obj);
  return arr->Length();
}

CefRefPtr<CefV8ArrayBufferReleaseCallback>
CefV8ValueImpl::GetArrayBufferReleaseCallback() {
  CEF_V8_REQUIRE_OBJECT_RETURN(nullptr);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return nullptr;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  if (!value->IsArrayBuffer()) {
    DCHECK(false) << "V8 value is not an array buffer";
    return nullptr;
  }

  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();

  V8TrackArrayBuffer* tracker = V8TrackArrayBuffer::Unwrap(context, obj);
  if (tracker) {
    return tracker->GetReleaseCallback();
  }

  return nullptr;
}

bool CefV8ValueImpl::NeuterArrayBuffer() {
  CEF_V8_REQUIRE_OBJECT_RETURN(0);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return false;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  if (!value->IsArrayBuffer()) {
    DCHECK(false) << "V8 value is not an array buffer";
    return false;
  }
  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();
  v8::Local<v8::ArrayBuffer> arr = v8::Local<v8::ArrayBuffer>::Cast(obj);
  if (!arr->IsDetachable()) {
    return false;
  }
  arr->Detach();

  return true;
}

size_t CefV8ValueImpl::GetArrayBufferByteLength() {
  size_t rv = 0;
  CEF_V8_REQUIRE_ISOLATE_RETURN(rv);
  if (type_ != TYPE_OBJECT) {
    DCHECK(false) << "V8 value is not an object";
    return rv;
  }

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return rv;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  if (!value->IsArrayBuffer()) {
    DCHECK(false) << "V8 value is not an array buffer";
    return rv;
  }

  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();
  return v8::Local<v8::ArrayBuffer>::Cast(obj)->ByteLength();
}

void* CefV8ValueImpl::GetArrayBufferData() {
  void* rv = nullptr;
  CEF_V8_REQUIRE_ISOLATE_RETURN(rv);
  if (type_ != TYPE_OBJECT) {
    DCHECK(false) << "V8 value is not an object";
    return rv;
  }

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return rv;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  if (!value->IsArrayBuffer()) {
    DCHECK(false) << "V8 value is not an array buffer";
    return rv;
  }

  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();
  v8::Local<v8::ArrayBuffer> arr = v8::Local<v8::ArrayBuffer>::Cast(obj);
  return arr->Data();
}

CefString CefV8ValueImpl::GetFunctionName() {
  CefString rv;
  CEF_V8_REQUIRE_OBJECT_RETURN(rv);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return rv;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  if (!value->IsFunction()) {
    DCHECK(false) << "V8 value is not a function";
    return rv;
  }

  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();
  v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(obj);
  GetCefString(handle_->isolate(),
               v8::Handle<v8::String>::Cast(func->GetName()), rv);
  return rv;
}

CefRefPtr<CefV8Handler> CefV8ValueImpl::GetFunctionHandler() {
  CEF_V8_REQUIRE_OBJECT_RETURN(nullptr);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    DCHECK(false) << "not currently in a V8 context";
    return nullptr;
  }

  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  if (!value->IsFunction()) {
    DCHECK(false) << "V8 value is not a function";
    return nullptr;
  }

  v8::Local<v8::Object> obj = value->ToObject(context).ToLocalChecked();
  V8TrackObject* tracker = V8TrackObject::Unwrap(context, obj);
  if (tracker) {
    return tracker->GetHandler();
  }

  return nullptr;
}

CefRefPtr<CefV8Value> CefV8ValueImpl::ExecuteFunction(
    CefRefPtr<CefV8Value> object,
    const CefV8ValueList& arguments) {
  // An empty context value defaults to the current context.
  CefRefPtr<CefV8Context> context;
  return ExecuteFunctionWithContext(context, object, arguments);
}

CefRefPtr<CefV8Value> CefV8ValueImpl::ExecuteFunctionWithContext(
    CefRefPtr<CefV8Context> context,
    CefRefPtr<CefV8Value> object,
    const CefV8ValueList& arguments) {
  CEF_V8_REQUIRE_OBJECT_RETURN(nullptr);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  if (!value->IsFunction()) {
    DCHECK(false) << "V8 value is not a function";
    return nullptr;
  }

  if (context.get() && !context->IsValid()) {
    DCHECK(false) << "invalid V8 context parameter";
    return nullptr;
  }
  if (object.get() && (!object->IsValid() || !object->IsObject())) {
    DCHECK(false) << "invalid V8 object parameter";
    return nullptr;
  }

  int argc = arguments.size();
  if (argc > 0) {
    for (int i = 0; i < argc; ++i) {
      if (!arguments[i].get() || !arguments[i]->IsValid()) {
        DCHECK(false) << "invalid V8 arguments parameter";
        return nullptr;
      }
    }
  }

  v8::Local<v8::Context> context_local;
  if (context.get()) {
    CefV8ContextImpl* context_impl =
        static_cast<CefV8ContextImpl*>(context.get());
    context_local = context_impl->GetV8Context();
  } else {
    context_local = isolate->GetCurrentContext();
  }

  v8::Context::Scope context_scope(context_local);

  v8::Local<v8::Object> obj = value->ToObject(context_local).ToLocalChecked();
  v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(obj);
  v8::Local<v8::Object> recv;

  // Default to the global object if no object was provided.
  if (object.get()) {
    CefV8ValueImpl* recv_impl = static_cast<CefV8ValueImpl*>(object.get());
    recv = v8::Local<v8::Object>::Cast(recv_impl->GetV8Value(true));
  } else {
    recv = context_local->Global();
  }

  v8::Local<v8::Value>* argv = nullptr;
  if (argc > 0) {
    argv = new v8::Local<v8::Value>[argc];
    for (int i = 0; i < argc; ++i) {
      argv[i] =
          static_cast<CefV8ValueImpl*>(arguments[i].get())->GetV8Value(true);
    }
  }

  CefRefPtr<CefV8Value> retval;

  {
    v8::TryCatch try_catch(isolate);
    try_catch.SetVerbose(true);

    v8::MaybeLocal<v8::Value> func_rv = blink_glue::CallV8Function(
        context_local, func, recv, argc, argv, handle_->isolate());

    if (!HasCaught(context_local, try_catch) && !func_rv.IsEmpty()) {
      retval =
          new CefV8ValueImpl(isolate, context_local, func_rv.ToLocalChecked());
    }
  }

  if (argv) {
    delete[] argv;
  }

  return retval;
}

bool CefV8ValueImpl::ResolvePromise(CefRefPtr<CefV8Value> arg) {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  if (!value->IsPromise()) {
    DCHECK(false) << "V8 value is not a Promise";
    return false;
  }

  if (arg.get() && !arg->IsValid()) {
    DCHECK(false) << "invalid V8 arg parameter";
    return false;
  }

  v8::Local<v8::Context> context_local = isolate->GetCurrentContext();

  v8::Context::Scope context_scope(context_local);

  v8::Local<v8::Object> obj = value->ToObject(context_local).ToLocalChecked();
  v8::Local<v8::Promise::Resolver> promise =
      v8::Local<v8::Promise::Resolver>::Cast(obj);

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  if (arg.get()) {
    promise
        ->Resolve(context_local,
                  static_cast<CefV8ValueImpl*>(arg.get())->GetV8Value(true))
        .ToChecked();
  } else {
    promise->Resolve(context_local, v8::Undefined(isolate)).ToChecked();
  }

  return !HasCaught(context_local, try_catch);
}

bool CefV8ValueImpl::RejectPromise(const CefString& errorMsg) {
  CEF_V8_REQUIRE_OBJECT_RETURN(false);

  v8::Isolate* isolate = handle_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> value = handle_->GetNewV8Handle(false);
  if (!value->IsPromise()) {
    DCHECK(false) << "V8 value is not a Promise";
    return false;
  }

  v8::Local<v8::Context> context_local = isolate->GetCurrentContext();

  v8::Context::Scope context_scope(context_local);

  v8::Local<v8::Object> obj = value->ToObject(context_local).ToLocalChecked();
  v8::Local<v8::Promise::Resolver> promise =
      v8::Local<v8::Promise::Resolver>::Cast(obj);

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  promise
      ->Reject(context_local,
               v8::Exception::Error(GetV8String(isolate, errorMsg)))
      .ToChecked();

  return !HasCaught(context_local, try_catch);
}

bool CefV8ValueImpl::HasCaught(v8::Local<v8::Context> context,
                               v8::TryCatch& try_catch) {
  if (try_catch.HasCaught()) {
    last_exception_ = new CefV8ExceptionImpl(context, try_catch.Message());
    if (rethrow_exceptions_) {
      try_catch.ReThrow();
    }
    return true;
  } else {
    if (last_exception_.get()) {
      last_exception_ = nullptr;
    }
    return false;
  }
}

// CefV8StackTrace

// static
CefRefPtr<CefV8StackTrace> CefV8StackTrace::GetCurrent(int frame_limit) {
  CEF_V8_REQUIRE_ISOLATE_RETURN(nullptr);

  v8::Isolate* isolate = CefV8IsolateManager::Get()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
      isolate, frame_limit, v8::StackTrace::kDetailed);
  if (stackTrace.IsEmpty()) {
    return nullptr;
  }
  return new CefV8StackTraceImpl(isolate, stackTrace);
}

// CefV8StackTraceImpl

CefV8StackTraceImpl::CefV8StackTraceImpl(v8::Isolate* isolate,
                                         v8::Local<v8::StackTrace> handle) {
  if (!handle.IsEmpty()) {
    int frame_count = handle->GetFrameCount();
    if (frame_count > 0) {
      frames_.reserve(frame_count);
      for (int i = 0; i < frame_count; ++i) {
        frames_.push_back(
            new CefV8StackFrameImpl(isolate, handle->GetFrame(isolate, i)));
      }
    }
  }
}

CefV8StackTraceImpl::~CefV8StackTraceImpl() = default;

bool CefV8StackTraceImpl::IsValid() {
  return true;
}

int CefV8StackTraceImpl::GetFrameCount() {
  return frames_.size();
}

CefRefPtr<CefV8StackFrame> CefV8StackTraceImpl::GetFrame(int index) {
  if (index < 0 || index >= static_cast<int>(frames_.size())) {
    return nullptr;
  }
  return frames_[index];
}

// CefV8StackFrameImpl

CefV8StackFrameImpl::CefV8StackFrameImpl(v8::Isolate* isolate,
                                         v8::Local<v8::StackFrame> handle) {
  if (handle.IsEmpty()) {
    return;
  }
  GetCefString(isolate, handle->GetScriptName(), script_name_);
  GetCefString(isolate, handle->GetScriptNameOrSourceURL(),
               script_name_or_source_url_);
  GetCefString(isolate, handle->GetFunctionName(), function_name_);
  line_number_ = handle->GetLineNumber();
  column_ = handle->GetColumn();
  is_eval_ = handle->IsEval();
  is_constructor_ = handle->IsConstructor();
}

CefV8StackFrameImpl::~CefV8StackFrameImpl() = default;

bool CefV8StackFrameImpl::IsValid() {
  return true;
}

CefString CefV8StackFrameImpl::GetScriptName() {
  return script_name_;
}

CefString CefV8StackFrameImpl::GetScriptNameOrSourceURL() {
  return script_name_or_source_url_;
}

CefString CefV8StackFrameImpl::GetFunctionName() {
  return function_name_;
}

int CefV8StackFrameImpl::GetLineNumber() {
  return line_number_;
}

int CefV8StackFrameImpl::GetColumn() {
  return column_;
}

bool CefV8StackFrameImpl::IsEval() {
  return is_eval_;
}

bool CefV8StackFrameImpl::IsConstructor() {
  return is_constructor_;
}

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if BUILDFLAG(IS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif
#endif
