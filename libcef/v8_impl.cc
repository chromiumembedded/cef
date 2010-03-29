// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "v8_impl.h"
#include "context.h"
#include "tracker.h"
#include "base/lazy_instance.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptController.h"

// Memory manager.

base::LazyInstance<CefTrackManager> g_v8_tracker(base::LINKER_INITIALIZED);

class TrackBase : public CefTrackObject
{
public:
  TrackBase(CefBase* base) { base_ = base; }

protected:
  CefRefPtr<CefBase> base_;
};

class TrackString : public CefTrackObject
{
public:
  TrackString(const std::string& str) : string_(str) {}
  const char* GetString() { return string_.c_str(); }

private:
  std::string string_;
};


static void TrackAdd(CefTrackObject* object)
{
  g_v8_tracker.Pointer()->Add(object);
}

static void TrackDelete(CefTrackObject* object)
{
  g_v8_tracker.Pointer()->Delete(object);
}

// Callback for weak persistent reference destruction.
static void TrackDestructor(v8::Persistent<v8::Value> object,
                            void* parameter)
{
  if(parameter)
    TrackDelete(static_cast<CefTrackObject*>(parameter));
}


// Convert a wide string to a V8 string.
static v8::Handle<v8::String> GetV8String(const std::wstring& str)
{
  return v8::String::New(
      reinterpret_cast<const uint16_t*>(str.c_str()), str.length());
}

// Convert a V8 string to a wide string.
static std::wstring GetWString(v8::Handle<v8::String> str)
{
  uint16_t* buf = new uint16_t[str->Length()+1];
  str->Write(buf);
  std::wstring value = reinterpret_cast<wchar_t*>(buf);
  delete [] buf;
  return value;
}


// V8 function callback
static v8::Handle<v8::Value> FunctionCallbackImpl(const v8::Arguments& args)
{
  v8::HandleScope handle_scope;
  CefV8Handler* handler =
      static_cast<CefV8Handler*>(v8::External::Unwrap(args.Data()));
  
  CefV8ValueList params;
  for(int i = 0; i < args.Length(); i++)
    params.push_back(new CefV8ValueImpl(args[i]));

  std::wstring func_name =
      GetWString(v8::Handle<v8::String>::Cast(args.Callee()->GetName()));
  CefRefPtr<CefV8Value> object = new CefV8ValueImpl(args.This());
  CefRefPtr<CefV8Value> retval;
  std::wstring exception;
  v8::Handle<v8::Value> value = v8::Null();

  if(handler->Execute(func_name, object, params, retval, exception)) {
    if(!exception.empty())
      value = v8::ThrowException(GetV8String(exception));
    else {
      CefV8ValueImpl* rv = static_cast<CefV8ValueImpl*>(retval.get());
      if(rv)
        value = rv->GetValue();
    }
  }

  return value;
}


// V8 extension registration.

class ExtensionWrapper : public v8::Extension {
public:
  ExtensionWrapper(const char* extension_name,
                   const char* javascript_code,
                   CefV8Handler* handler)
    : v8::Extension(extension_name, javascript_code), handler_(handler)
  {
    // The reference will be released when the application exits.
    TrackAdd(new TrackBase(handler));
  }
  
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
    v8::Handle<v8::String> name)
  {
    return v8::FunctionTemplate::New(FunctionCallbackImpl,
                                     v8::External::New(handler_));
  }

  void UIT_RegisterExtension()
  {
    WebKit::WebScriptController::registerExtension(this);
  }

  void AddRef() {}
  void Release() {}

  static bool ImplementsThreadSafeReferenceCounting() { return true; }

private:
  CefV8Handler* handler_;
};

bool CefRegisterExtension(const std::wstring& extension_name,
                          const std::wstring& javascript_code,
                          CefRefPtr<CefV8Handler> handler)
{
  // Verify that the context is already initialized
  if(!_Context.get())
    return false;
  
  if(!handler.get())
    return false;

  TrackString* name = new TrackString(WideToUTF8(extension_name));
  TrackAdd(name);
  TrackString* code = new TrackString(WideToUTF8(javascript_code));
  TrackAdd(name);
  
  ExtensionWrapper* wrapper = new ExtensionWrapper(name->GetString(),
      code->GetString(), handler.get());
  
  PostTask(FROM_HERE, NewRunnableMethod(wrapper,
      &ExtensionWrapper::UIT_RegisterExtension));
  return true;
}


// CefV8Value

// static
CefRefPtr<CefV8Value> CefV8Value::CreateUndefined()
{
  v8::HandleScope handle_scope;
  return new CefV8ValueImpl(v8::Undefined());
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateNull()
{
  v8::HandleScope handle_scope;
  return new CefV8ValueImpl(v8::Null());
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateBool(bool value)
{
  v8::HandleScope handle_scope;
  return new CefV8ValueImpl(v8::Boolean::New(value));
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateInt(int value)
{
  v8::HandleScope handle_scope;
  return new CefV8ValueImpl(v8::Int32::New(value));
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateDouble(double value)
{
  v8::HandleScope handle_scope;
  return new CefV8ValueImpl(v8::Number::New(value));
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateString(const std::wstring& value)
{
  v8::HandleScope handle_scope;
  return new CefV8ValueImpl(GetV8String(value));
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateObject(CefRefPtr<CefBase> user_data)
{
  v8::HandleScope handle_scope;
  CefV8ValueImpl* impl = new CefV8ValueImpl();

  // Create the new V8 object.
  v8::Local<v8::Object> obj = v8::Object::New();

  TrackBase *tracker = NULL;
  if(user_data.get()) {
    // Attach the user data to the V8 object.
    v8::Local<v8::Value> data = v8::External::Wrap(user_data.get());
    obj->Set(v8::String::New("Cef::UserData"), data);

    // Provide a tracker object that will cause the user data reference to be
    // released when the V8 object is destroyed.
    tracker = new TrackBase(user_data);
  }

  // Attach to the CefV8ValueImpl.
  impl->Attach(obj, tracker);
  return impl;
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateArray()
{
  v8::HandleScope handle_scope;
  return new CefV8ValueImpl(v8::Array::New());
}

// static
CefRefPtr<CefV8Value> CefV8Value::CreateFunction(const std::wstring& name,
                                                 CefRefPtr<CefV8Handler> handler)
{
  v8::HandleScope handle_scope;
  CefV8ValueImpl* impl = new CefV8ValueImpl();
  
  // Create a new V8 function template with one internal field.
  v8::Local<v8::FunctionTemplate> tmpl = v8::FunctionTemplate::New();
  
  v8::Local<v8::Value> data = v8::External::Wrap(handler.get());

  // Set the function handler callback.
  tmpl->SetCallHandler(FunctionCallbackImpl, data);

  // Retrieve the function object and set the name.
  v8::Local<v8::Function> func = tmpl->GetFunction();
  func->SetName(GetV8String(name));

  // Attach the handler instance to the V8 object.
  func->Set(v8::String::New("Cef::Handler"), data);

  // Attach to the CefV8ValueImpl and provide a tracker object that will cause
  // the handler reference to be released when the V8 object is destroyed.
  impl->Attach(func, new TrackBase(handler));
  return impl;
}


// CefV8ValueImpl

CefV8ValueImpl::CefV8ValueImpl()
  : tracker_(NULL)
{
}

CefV8ValueImpl::CefV8ValueImpl(v8::Handle<v8::Value> value,
                               CefTrackObject* tracker)
{
  Attach(value, tracker);
}

CefV8ValueImpl::~CefV8ValueImpl()
{
  Detach();
}

bool CefV8ValueImpl::Attach(v8::Handle<v8::Value> value,
                            CefTrackObject* tracker)
{
  bool rv = false;
  Lock();
  if(v8_value_.IsEmpty()) {
    v8_value_ = v8::Persistent<v8::Value>::New(value);
    tracker_ = tracker;
    rv = true;
  }
  Unlock();
  return rv;
}

void CefV8ValueImpl::Detach()
{
  Lock();
  if(tracker_)
    TrackAdd(tracker_);
  v8_value_.MakeWeak(tracker_, TrackDestructor);
  v8_value_.Clear();
  tracker_ = NULL;
  Unlock();
}

v8::Handle<v8::Value> CefV8ValueImpl::GetValue()
{
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> rv;
  Lock();
  rv = v8_value_;
  Unlock();
  return rv;
}

bool CefV8ValueImpl::IsReservedKey(const std::wstring& key)
{
  return (key.find(L"Cef::") == 0 || key.find(L"v8::") == 0);
}

bool CefV8ValueImpl::IsUndefined()
{
  Lock();
  bool rv = v8_value_->IsUndefined();
  Unlock();
  return rv;
}

bool CefV8ValueImpl::IsNull()
{
  Lock();
  bool rv = v8_value_->IsNull();
  Unlock();
  return rv;
}

bool CefV8ValueImpl::IsBool()
{
  Lock();
  bool rv = (v8_value_->IsBoolean() || v8_value_->IsTrue()
             || v8_value_->IsFalse());
  Unlock();
  return rv;
}

bool CefV8ValueImpl::IsInt()
{
  Lock();
  bool rv = v8_value_->IsInt32();
  Unlock();
  return rv;
}

bool CefV8ValueImpl::IsDouble()
{
  Lock();
  bool rv = (v8_value_->IsNumber() || v8_value_->IsDate());
  Unlock();
  return rv;
}

bool CefV8ValueImpl::IsString()
{
  Lock();
  bool rv = v8_value_->IsString();
  Unlock();
  return rv;
}

bool CefV8ValueImpl::IsObject()
{
  Lock();
  bool rv = v8_value_->IsObject();
  Unlock();
  return rv;
}

bool CefV8ValueImpl::IsArray()
{
  Lock();
  bool rv = v8_value_->IsArray();
  Unlock();
  return rv;
}

bool CefV8ValueImpl::IsFunction()
{
  Lock();
  bool rv = v8_value_->IsFunction();
  Unlock();
  return rv;
}

bool CefV8ValueImpl::GetBoolValue()
{
  bool rv = false;
  Lock();
  if(v8_value_->IsTrue())
    rv = true;
  else if(v8_value_->IsFalse())
    rv = false;
  else {
    v8::HandleScope handle_scope;
    v8::Local<v8::Boolean> val = v8_value_->ToBoolean();
    rv = val->Value();
  }
  Unlock();
  return rv;
}

int CefV8ValueImpl::GetIntValue()
{
  int rv = 0;
  Lock();
  v8::HandleScope handle_scope;
  v8::Local<v8::Int32> val = v8_value_->ToInt32();
  rv = val->Value();
  Unlock();
  return rv;
}

double CefV8ValueImpl::GetDoubleValue()
{
  double rv = 0.;
  Lock();
  v8::HandleScope handle_scope;
  v8::Local<v8::Number> val = v8_value_->ToNumber();
  rv = val->Value();
  Unlock();
  return rv;
}

std::wstring CefV8ValueImpl::GetStringValue()
{
  std::wstring rv;
  Lock();
  v8::HandleScope handle_scope;
  rv = GetWString(v8_value_->ToString());
  Unlock();
  return rv;
}

bool CefV8ValueImpl::HasValue(const std::wstring& key)
{
  if(IsReservedKey(key))
    return false;

  bool rv = false;
  Lock();
  if(v8_value_->IsObject()) {
    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = v8_value_->ToObject();
    rv = obj->Has(GetV8String(key));
  }
  Unlock();
  return rv;
}

bool CefV8ValueImpl::HasValue(int index)
{
  bool rv = false;
  Lock();
  if(v8_value_->IsObject()) {
    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = v8_value_->ToObject();
    rv = obj->Has(index);
  }
  Unlock();
  return rv;
}

bool CefV8ValueImpl::DeleteValue(const std::wstring& key)
{
  if(IsReservedKey(key))
    return false;

  bool rv = false;
  Lock();
  if(v8_value_->IsObject()) {
    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = v8_value_->ToObject();
    rv = obj->Delete(GetV8String(key));
  }
  Unlock();
  return rv;
}

bool CefV8ValueImpl::DeleteValue(int index)
{
  bool rv = false;
  Lock();
  if(v8_value_->IsObject()) {
    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = v8_value_->ToObject();
    rv = obj->Delete(index);
  }
  Unlock();
  return rv;
}

CefRefPtr<CefV8Value> CefV8ValueImpl::GetValue(const std::wstring& key)
{
  if(IsReservedKey(key))
    return false;

  CefRefPtr<CefV8Value> rv;
  Lock();
  if(v8_value_->IsObject()) {
    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = v8_value_->ToObject();
    rv = new CefV8ValueImpl(obj->Get(GetV8String(key)));
  }
  Unlock();
  return rv;
}

CefRefPtr<CefV8Value> CefV8ValueImpl::GetValue(int index)
{
  CefRefPtr<CefV8Value> rv;
  Lock();
  if(v8_value_->IsObject()) {
    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = v8_value_->ToObject();
    rv = new CefV8ValueImpl(obj->Get(v8::Number::New(index)));
  }
  Unlock();
  return rv;
}

bool CefV8ValueImpl::SetValue(const std::wstring& key,
                              CefRefPtr<CefV8Value> value)
{
  if(IsReservedKey(key))
    return false;

  bool rv = false;
  Lock();
  if(v8_value_->IsObject()) {
    CefV8ValueImpl *impl = static_cast<CefV8ValueImpl*>(value.get());
    if(impl) {
      v8::HandleScope handle_scope;
      v8::Local<v8::Object> obj = v8_value_->ToObject();
      rv = obj->Set(GetV8String(key), impl->GetValue());
    }
  }
  Unlock();
  return rv;
}

bool CefV8ValueImpl::SetValue(int index, CefRefPtr<CefV8Value> value)
{
  bool rv = false;
  Lock();
  if(v8_value_->IsObject()) {
    CefV8ValueImpl *impl = static_cast<CefV8ValueImpl*>(value.get());
    if(impl) {
      v8::HandleScope handle_scope;
      v8::Local<v8::Object> obj = v8_value_->ToObject();
      rv = obj->Set(v8::Number::New(index), impl->GetValue());
    }
  }
  Unlock();
  return rv;
}

bool CefV8ValueImpl::GetKeys(std::vector<std::wstring>& keys)
{
  bool rv = false;
  Lock();
  if(v8_value_->IsObject()) {
    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = v8_value_->ToObject();
    v8::Local<v8::Array> arr_keys = obj->GetPropertyNames();
    uint32_t len = arr_keys->Length();
    for(uint32_t i = 0; i < len; ++i) {
      v8::Local<v8::Value> value = arr_keys->Get(v8::Integer::New(i));
      std::wstring str = GetWString(value->ToString());
      if(!IsReservedKey(str))
        keys.push_back(str);
    }
    rv = true;
  }
  Unlock();
  return rv;
}

CefRefPtr<CefBase> CefV8ValueImpl::GetUserData()
{
  CefRefPtr<CefBase> rv;
  Lock();
  if(v8_value_->IsObject()) {
    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = v8_value_->ToObject();
    v8::Local<v8::String> key = v8::String::New("Cef::UserData");
    if(obj->Has(key))
      rv = static_cast<CefBase*>(v8::External::Unwrap(obj->Get(key)));
  }
  Unlock();
  return rv;
}

int CefV8ValueImpl::GetArrayLength()
{
  int rv = 0;
  Lock();
  if(v8_value_->IsArray()) {
    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = v8_value_->ToObject();
    v8::Local<v8::Array> arr = v8::Local<v8::Array>::Cast(obj);
    rv = arr->Length();
  }
  Unlock();
  return rv;
}

std::wstring CefV8ValueImpl::GetFunctionName()
{
  std::wstring rv;
  Lock();
  if(v8_value_->IsFunction()) {
    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = v8_value_->ToObject();
    v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(obj);
    rv = GetWString(v8::Handle<v8::String>::Cast(func->GetName()));
  }
  Unlock();
  return rv;
}

CefRefPtr<CefV8Handler> CefV8ValueImpl::GetFunctionHandler()
{
  CefRefPtr<CefV8Handler> rv;
  Lock();
  if(v8_value_->IsFunction()) {
    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = v8_value_->ToObject();
    v8::Local<v8::String> key = v8::String::New("Cef::Handler");
    if(obj->Has(key))
      rv = static_cast<CefV8Handler*>(v8::External::Unwrap(obj->Get(key)));
  }
  Unlock();
  return rv;
}

bool CefV8ValueImpl::ExecuteFunction(CefRefPtr<CefV8Value> object,
                                     const CefV8ValueList& arguments,
                                     CefRefPtr<CefV8Value>& retval,
                                     std::wstring& exception)
{
  bool rv = false;
  Lock();
  if(v8_value_->IsFunction() && object.get() && object->IsObject()) {
    v8::HandleScope handle_scope;
    v8::Local<v8::Object> obj = v8_value_->ToObject();
    v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(obj);

    CefV8ValueImpl* recv_impl = static_cast<CefV8ValueImpl*>(object.get());
    v8::Handle<v8::Object> recv =
        v8::Handle<v8::Object>::Cast(recv_impl->GetValue());

    int argc = arguments.size();
    v8::Handle<v8::Value> *argv = NULL;
    if(argc > 0) {
      argv = new v8::Handle<v8::Value>[argc];
      for(int i = 0; i < argc; ++i) {
        argv[i] =
            static_cast<CefV8ValueImpl*>(arguments[i].get())->GetValue();
      }
    }

    v8::TryCatch try_catch;
    v8::Local<v8::Value> func_rv = func->Call(recv, argc, argv);
    if (try_catch.HasCaught())
      exception = GetWString(try_catch.Message()->Get());
    else
      retval = new CefV8ValueImpl(func_rv);

    rv = true;
  }
  Unlock();
  return rv;
}
