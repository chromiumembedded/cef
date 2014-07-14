// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_CPPTOC_BASE_CPPTOC_H_
#define CEF_LIBCEF_DLL_CPPTOC_BASE_CPPTOC_H_
#pragma once

#include "include/base/cef_logging.h"
#include "include/base/cef_macros.h"
#include "include/cef_base.h"
#include "include/capi/cef_base_capi.h"

// CefCppToC implementation for CefBase.
class CefBaseCppToC : public CefBase {
 public:
  // Use this method to retrieve the underlying class instance from our
  // own structure when the structure is passed as the required first
  // parameter of a C API function call. No explicit reference counting
  // is done in this case.
  static CefRefPtr<CefBase> Get(cef_base_t* s) {
    DCHECK(s);

    // Cast our structure to the wrapper structure type.
    CefBaseCppToC::Struct* wrapperStruct =
        reinterpret_cast<CefBaseCppToC::Struct*>(s);
    // Return the underlying object instance.
    return wrapperStruct->class_->GetClass();
  }

  // Use this method to create a wrapper structure for passing our class
  // instance to the other side.
  static cef_base_t* Wrap(CefRefPtr<CefBase> c) {
    if (!c.get())
      return NULL;

    // Wrap our object with the CefCppToC class.
    CefBaseCppToC* wrapper = new CefBaseCppToC(c);
    // Add a reference to our wrapper object that will be released once our
    // structure arrives on the other side.
    wrapper->AddRef();
    // Return the structure pointer that can now be passed to the other side.
    return wrapper->GetStruct();
  }

  // Use this method to retrieve the underlying class instance when receiving
  // our wrapper structure back from the other side.
  static CefRefPtr<CefBase> Unwrap(cef_base_t* s) {
    if (!s)
      return NULL;

    // Cast our structure to the wrapper structure type.
    CefBaseCppToC::Struct* wrapperStruct =
        reinterpret_cast<CefBaseCppToC::Struct*>(s);
    // Add the underlying object instance to a smart pointer.
    CefRefPtr<CefBase> objectPtr(wrapperStruct->class_->GetClass());
    // Release the reference to our wrapper object that was added before the
    // structure was passed back to us.
    wrapperStruct->class_->Release();
    // Return the underlying object instance.
    return objectPtr;
  }

  // Structure representation with pointer to the C++ class.
  struct Struct {
    cef_base_t struct_;
    CefBaseCppToC* class_;
  };

  explicit CefBaseCppToC(CefBase* cls)
    : class_(cls) {
    DCHECK(cls);

    struct_.class_ = this;

    // zero the underlying structure and set base members
    memset(&struct_.struct_, 0, sizeof(cef_base_t));
    struct_.struct_.size = sizeof(cef_base_t);
    struct_.struct_.add_ref = struct_add_ref;
    struct_.struct_.release = struct_release;
    struct_.struct_.has_one_ref = struct_has_one_ref;
  }
  virtual ~CefBaseCppToC() {}

  CefBase* GetClass() { return class_; }

  // If returning the structure across the DLL boundary you should call
  // AddRef() on this CefCppToC object.  On the other side of the DLL boundary,
  // call UnderlyingRelease() on the wrapping CefCToCpp object.
  cef_base_t* GetStruct() { return &struct_.struct_; }

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
  Struct struct_;
  CefBase* class_;

  DISALLOW_COPY_AND_ASSIGN(CefBaseCppToC);
};

#endif  // CEF_LIBCEF_DLL_CPPTOC_BASE_CPPTOC_H_
