// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "jscontainer.h"
#include "variant_impl.h"

// Here's the control flow of a JS method getting forwarded to a class.
// - Something calls our NPObject with a function like "Invoke".
// - CefNPObject's static invoke() function forwards it to its attached
//   CefJSContainer's Invoke() method.
// - CefJSContainer has then overridden Invoke() to look up the function
//   name in its internal map of methods, and then calls the appropriate
//   method.

#include "base/compiler_specific.h"
#include "config.h"

// This is required for the KJS build due to an artifact of the
// npruntime_priv.h file from JavaScriptCore/bindings.
MSVC_PUSH_DISABLE_WARNING(4067)
#include "npruntime_priv.h"
MSVC_POP_WARNING()

#if USE(JSC)
MSVC_PUSH_WARNING_LEVEL(0)
#include <runtime/JSLock.h>
MSVC_POP_WARNING()
#endif

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webview.h"


// Our special NPObject type.  We extend an NPObject with a pointer to a
// CefJSContainer, which is just a C++ interface that we forward all NPObject
// callbacks to.
struct CefNPObject {
  NPObject parent;  // This must be the first field in the struct.
  CefJSContainer* container;
  WebFrame* webframe;

  //
  // All following objects and functions are static, and just used to interface
  // with NPObject/NPClass.
  //

  // An NPClass associates static functions of CefNPObject with the
  // function pointers used by the JS runtime. 
  static NPClass np_class_;

  // Allocate a new NPObject with the specified class.
  static NPObject* allocate(NPP npp, NPClass* aClass);

  // Free an object.
  static void deallocate(NPObject* obj);

  // Returns true if the C++ class associated with this NPObject exposes the
  // given property.  Called by the JS runtime.
  static bool hasProperty(NPObject *obj, NPIdentifier ident);

  // Returns true if the C++ class associated with this NPObject exposes the
  // given method.  Called by the JS runtime.
  static bool hasMethod(NPObject *obj, NPIdentifier ident);

  // If the given method is exposed by the C++ class associated with this
  // NPObject, invokes it with the given args and returns a result.  Otherwise,
  // returns "undefined" (in the JavaScript sense).  Called by the JS runtime.
  static bool invoke(NPObject *obj, NPIdentifier ident, 
                     const NPVariant *args, uint32_t arg_count, 
                     NPVariant *result);

  // If the given property is exposed by the C++ class associated with this
  // NPObject, returns its value.  Otherwise, returns "undefined" (in the
  // JavaScript sense).  Called by the JS runtime.
  static bool getProperty(NPObject *obj, NPIdentifier ident, 
                          NPVariant *result);

  // If the given property is exposed by the C++ class associated with this
  // NPObject, sets its value.  Otherwise, does nothing. Called by the JS 
  // runtime.
  static bool setProperty(NPObject *obj, NPIdentifier ident, 
                          const NPVariant *value);  
};

// Build CefNPObject's static function pointers into an NPClass, for use
// in constructing NPObjects for the C++ classes.
NPClass CefNPObject::np_class_ = {
  NP_CLASS_STRUCT_VERSION,
  CefNPObject::allocate,
  CefNPObject::deallocate,
  /* NPInvalidateFunctionPtr */ NULL,
  CefNPObject::hasMethod,
  CefNPObject::invoke,
  /* NPInvokeDefaultFunctionPtr */ NULL,
  CefNPObject::hasProperty,
  CefNPObject::getProperty,
  CefNPObject::setProperty,
  /* NPRemovePropertyFunctionPtr */ NULL
};

/* static */ NPObject* CefNPObject::allocate(NPP npp, NPClass* aClass) {
  CefNPObject* obj = new CefNPObject;
  // obj->parent will be initialized by the NPObject code calling this.
  obj->container = NULL;
  obj->webframe = NULL;
  return &obj->parent;
}

/* static */ void CefNPObject::deallocate(NPObject* np_obj) {
  CefNPObject* obj = reinterpret_cast<CefNPObject*>(np_obj);
  delete obj;
}

/* static */ bool CefNPObject::hasMethod(NPObject* np_obj, 
                                         NPIdentifier ident) {
  CefNPObject* obj = reinterpret_cast<CefNPObject*>(np_obj);
  return obj->container->HasMethod(ident);
}

/* static */ bool CefNPObject::hasProperty(NPObject* np_obj, 
                                           NPIdentifier ident) {
  CefNPObject* obj = reinterpret_cast<CefNPObject*>(np_obj);
  return obj->container->HasProperty(ident);
}

/* static */ bool CefNPObject::invoke(NPObject* np_obj, NPIdentifier ident, 
                                      const NPVariant* args, uint32_t arg_count, 
                                      NPVariant* result) {
  CefNPObject* obj = reinterpret_cast<CefNPObject*>(np_obj);
  return obj->container->Invoke(ident, obj->webframe, args, arg_count, result);
}

/* static */ bool CefNPObject::getProperty(NPObject* np_obj, 
                                           NPIdentifier ident, 
                                           NPVariant* result) {
  CefNPObject* obj = reinterpret_cast<CefNPObject*>(np_obj);
  return obj->container->GetProperty(ident, obj->webframe, result);
}

/* static */ bool CefNPObject::setProperty(NPObject* np_obj, 
                                           NPIdentifier ident,
                                           const NPVariant* value) {
  CefNPObject* obj = reinterpret_cast<CefNPObject*>(np_obj);
  return obj->container->SetProperty(ident, obj->webframe, value);
}

CefJSContainer::CefJSContainer(CefBrowser* browser,
                               CefRefPtr<CefJSHandler> handler)
  : browser_(browser), handler_(handler)
{
  DCHECK(browser_ != NULL);
  DCHECK(handler_.get() != NULL);
}

CefJSContainer::~CefJSContainer()
{
  // Unregister objects we created and bound to a frame.
  for (BoundObjectList::iterator i = bound_objects_.begin(); 
      i != bound_objects_.end(); ++i) {
#if USE(V8)
    _NPN_UnregisterObject(*i);
#endif
    NPN_ReleaseObject(*i);
  }
}

bool CefJSContainer::HasMethod(NPIdentifier ident)
{
  std::wstring name = UTF8ToWide(NPN_UTF8FromIdentifier(ident));
  return handler_->HasMethod(browser_, name);
}

bool CefJSContainer::HasProperty(NPIdentifier ident)
{
  std::wstring name = UTF8ToWide(NPN_UTF8FromIdentifier(ident));
  return handler_->HasProperty(browser_, name);
}

bool CefJSContainer::Invoke(NPIdentifier ident,
                            WebFrame* frame,
                            const NPVariant* args,
                            size_t arg_count, 
                            NPVariant* result)
{
  std::wstring name = UTF8ToWide(NPN_UTF8FromIdentifier(ident));
  
  // Build a VariantVector argument vector from the NPVariants coming in.
  CefJSHandler::VariantVector cef_args(arg_count);
  for (size_t i = 0; i < arg_count; i++) {
    cef_args[i] = new CefVariantImpl(frame);
    static_cast<CefVariantImpl*>(cef_args[i].get())->Set(args[i]);
  }

  CefRefPtr<CefVariant> cef_retval(new CefVariantImpl(frame));

  // Execute the handler method
  bool rv = handler_->ExecuteMethod(browser_, name, cef_args, cef_retval);
  if(rv) {
    // Assign the return value
    static_cast<CefVariantImpl*>(cef_retval.get())->CopyToNPVariant(result);
  }
  return rv;
}

bool CefJSContainer::GetProperty(NPIdentifier ident,
                                 WebFrame* frame,
                                 NPVariant* result)
{
  CefRefPtr<CefVariant> cef_result(new CefVariantImpl(frame));
  std::wstring name = UTF8ToWide(NPN_UTF8FromIdentifier(ident));
  
  // Execute the handler method
  bool rv = handler_->GetProperty(browser_, name, cef_result);
  if(rv) {
    // Assign the return value
    static_cast<CefVariantImpl*>(cef_result.get())->CopyToNPVariant(result);
  }
  return rv;
}

bool CefJSContainer::SetProperty(NPIdentifier ident,
                                 WebFrame* frame,
                                 const NPVariant* value)
{
  std::wstring name = UTF8ToWide(NPN_UTF8FromIdentifier(ident));

  // Assign the input value
  CefRefPtr<CefVariant> cef_value(new CefVariantImpl(frame));
  static_cast<CefVariantImpl*>(cef_value.get())->Set(*value);
  
  // Execute the handler method
  return handler_->SetProperty(browser_, name, cef_value);
}

// Check if the specified frame exists by comparing to all frames currently
// attached to the view.
static bool FrameExists(WebView* view, WebFrame* frame) {
  WebFrame* main_frame = view->GetMainFrame();
  WebFrame* it = main_frame;
  do {
    if (it == frame)
      return true;
    it = view->GetNextFrameAfter(it, true);
  } while (it != main_frame);

  return false;
}

void CefJSContainer::BindToJavascript(WebFrame* frame,
                                      const std::wstring& classname) {
#if USE(JSC)
  JSC::JSLock lock(false);
#endif

  NPObject* np_obj = NULL;
  CefNPObject* obj = NULL;

  Lock();
  WebView* view = frame->GetView();
  BoundObjectList::iterator it = bound_objects_.begin();
  for(; it != bound_objects_.end(); ) {
    obj = reinterpret_cast<CefNPObject*>(*it);
    if(obj->webframe == frame) {
      // An NPObject is already bound to this particular frame.
      np_obj = *it;
    } else if(!FrameExists(view, obj->webframe)) {
      // Remove bindings to non-existent frames.
#if USE(V8)
      _NPN_UnregisterObject(*it);
#endif
      NPN_ReleaseObject(*it);
      it = bound_objects_.erase(it);
      continue;
    }
    ++it;
  }
  Unlock();

  if(!np_obj) {
    // Create an NPObject using our static NPClass.  The first argument (a
    // plugin's instance handle) is passed through to the allocate function
    // directly, and we don't use it, so it's ok to be 0.
    np_obj = NPN_CreateObject(0, &CefNPObject::np_class_);
    obj = reinterpret_cast<CefNPObject*>(np_obj);
    obj->container = this;
    obj->webframe = frame;

    Lock();
    bound_objects_.push_back(np_obj);
    Unlock();
  }

  // BindToWindowObject will (indirectly) retain the np_object. We save it
  // so we can release it when we're destroyed.
  frame->BindToWindowObject(classname, np_obj);
}
