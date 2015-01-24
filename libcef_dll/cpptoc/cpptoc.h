// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_CPPTOC_CPPTOC_H_
#define CEF_LIBCEF_DLL_CPPTOC_CPPTOC_H_
#pragma once

#include "include/base/cef_logging.h"
#include "include/base/cef_macros.h"
#include "include/cef_base.h"
#include "include/capi/cef_base_capi.h"


// Wrap a C++ class with a C structure.  This is used when the class
// implementation exists on this side of the DLL boundary but will have methods
// called from the other side of the DLL boundary.
template <class ClassName, class BaseName, class StructName>
class CefCppToC : public CefBase {
 public:
  // Structure representation with pointer to the C++ class.
  struct Struct {
    StructName struct_;
    CefCppToC<ClassName, BaseName, StructName>* class_;
  };

  // Use this method to retrieve the underlying class instance from our
  // own structure when the structure is passed as the required first
  // parameter of a C API function call. No explicit reference counting
  // is done in this case.
  static CefRefPtr<BaseName> Get(StructName* s) {
    DCHECK(s);

    // Cast our structure to the wrapper structure type.
    Struct* wrapperStruct = reinterpret_cast<Struct*>(s);
    // Return the underlying object instance.
    return wrapperStruct->class_->GetClass();
  }

  // Use this method to create a wrapper structure for passing our class
  // instance to the other side.
  static StructName* Wrap(CefRefPtr<BaseName> c) {
    if (!c.get())
      return NULL;

    // Wrap our object with the CefCppToC class.
    ClassName* wrapper = new ClassName(c.get());
    // Add a reference to our wrapper object that will be released once our
    // structure arrives on the other side.
    wrapper->AddRef();
    // Return the structure pointer that can now be passed to the other side.
    return wrapper->GetStruct();
  }

  // Use this method to retrieve the underlying class instance when receiving
  // our wrapper structure back from the other side.
  static CefRefPtr<BaseName> Unwrap(StructName* s) {
    if (!s)
      return NULL;

    // Cast our structure to the wrapper structure type.
    Struct* wrapperStruct = reinterpret_cast<Struct*>(s);
    // Add the underlying object instance to a smart pointer.
    CefRefPtr<BaseName> objectPtr(wrapperStruct->class_->GetClass());
    // Release the reference to our wrapper object that was added before the
    // structure was passed back to us.
    wrapperStruct->class_->Release();
    // Return the underlying object instance.
    return objectPtr;
  }

  explicit CefCppToC(BaseName* cls)
    : class_(cls) {
    DCHECK(cls);

    struct_.class_ = this;

    // zero the underlying structure and set base members
    memset(&struct_.struct_, 0, sizeof(StructName));
    struct_.struct_.base.size = sizeof(StructName);
    struct_.struct_.base.add_ref = struct_add_ref;
    struct_.struct_.base.release = struct_release;
    struct_.struct_.base.has_one_ref = struct_has_one_ref;

#ifndef NDEBUG
    base::AtomicRefCountInc(&DebugObjCt);
#endif
  }
  virtual ~CefCppToC() {
#ifndef NDEBUG
    base::AtomicRefCountDec(&DebugObjCt);
#endif
  }

  BaseName* GetClass() { return class_; }

  // If returning the structure across the DLL boundary you should call
  // AddRef() on this CefCppToC object.  On the other side of the DLL boundary,
  // call UnderlyingRelease() on the wrapping CefCToCpp object.
  StructName* GetStruct() { return &struct_.struct_; }

  // CefBase methods increment/decrement reference counts on both this object
  // and the underlying wrapper class.
  void AddRef() const {
    UnderlyingAddRef();
    ref_count_.AddRef();
  }
  bool Release() const {
    UnderlyingRelease();
    if (ref_count_.Release()) {
      delete this;
      return true;
    }
    return false;
  }
  bool HasOneRef() const { return ref_count_.HasOneRef(); }

  // Increment/decrement reference counts on only the underlying class.
  void UnderlyingAddRef() const { class_->AddRef(); }
  bool UnderlyingRelease() const { return class_->Release(); }
  bool UnderlyingHasOneRef() const { return class_->HasOneRef(); }

#ifndef NDEBUG
  // Simple tracking of allocated objects.
  static base::AtomicRefCount DebugObjCt;  // NOLINT(runtime/int)
#endif

 protected:
  Struct struct_;
  BaseName* class_;

 private:
  static void CEF_CALLBACK struct_add_ref(struct _cef_base_t* base) {
    DCHECK(base);
    if (!base)
      return;

    Struct* impl = reinterpret_cast<Struct*>(base);
    impl->class_->AddRef();
  }

  static int CEF_CALLBACK struct_release(struct _cef_base_t* base) {
    DCHECK(base);
    if (!base)
      return 0;

    Struct* impl = reinterpret_cast<Struct*>(base);
    return impl->class_->Release();
  }

  static int CEF_CALLBACK struct_has_one_ref(struct _cef_base_t* base) {
    DCHECK(base);
    if (!base)
      return 0;

    Struct* impl = reinterpret_cast<Struct*>(base);
    return impl->class_->HasOneRef();
  }

  CefRefCount ref_count_;

  DISALLOW_COPY_AND_ASSIGN(CefCppToC);
};

#endif  // CEF_LIBCEF_DLL_CPPTOC_CPPTOC_H_
