// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_CTOCPP_CTOCPP_H_
#define CEF_LIBCEF_DLL_CTOCPP_CTOCPP_H_
#pragma once

#include "include/base/cef_logging.h"
#include "include/base/cef_macros.h"
#include "include/cef_base.h"
#include "include/capi/cef_base_capi.h"

// Wrap a C structure with a C++ class.  This is used when the implementation
// exists on the other side of the DLL boundary but will have methods called on
// this side of the DLL boundary.
template <class ClassName, class BaseName, class StructName>
class CefCToCpp : public BaseName {
 public:
  // Use this method to create a wrapper class instance for a structure
  // received from the other side.
  static CefRefPtr<BaseName> Wrap(StructName* s) {
    if (!s)
      return NULL;

    // Wrap their structure with the CefCToCpp object.
    ClassName* wrapper = new ClassName(s);
    // Put the wrapper object in a smart pointer.
    CefRefPtr<BaseName> wrapperPtr(wrapper);
    // Release the reference that was added to the CefCppToC wrapper object on
    // the other side before their structure was passed to us.
    wrapper->UnderlyingRelease();
    // Return the smart pointer.
    return wrapperPtr;
  }

  // Use this method to retrieve the underlying structure from a wrapper class
  // instance for return back to the other side.
  static StructName* Unwrap(CefRefPtr<BaseName> c) {
    if (!c.get())
      return NULL;

    // Cast the object to our wrapper class type.
    ClassName* wrapper = static_cast<ClassName*>(c.get());
    // Add a reference to the CefCppToC wrapper object on the other side that
    // will be released once the structure is received.
    wrapper->UnderlyingAddRef();
    // Return their original structure.
    return wrapper->GetStruct();
  }

  explicit CefCToCpp(StructName* str)
    : struct_(str) {
    DCHECK(str);

#ifndef NDEBUG
    base::AtomicRefCountInc(&DebugObjCt);
#endif
  }
  virtual ~CefCToCpp() {
#ifndef NDEBUG
    base::AtomicRefCountDec(&DebugObjCt);
#endif
  }

  // If returning the structure across the DLL boundary you should call
  // UnderlyingAddRef() on this wrapping CefCToCpp object.  On the other side of
  // the DLL  boundary, call Release() on the CefCppToC object.
  StructName* GetStruct() { return struct_; }

  // CefBase methods increment/decrement reference counts on both this object
  // and the underlying wrapped structure.
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
  void UnderlyingAddRef() const {
    if (struct_->base.add_ref)
      struct_->base.add_ref(&struct_->base);
  }
  bool UnderlyingRelease() const {
    if (!struct_->base.release)
      return false;
    return struct_->base.release(&struct_->base) ? true : false;
  }
  bool UnderlyingHasOneRef() const {
    if (!struct_->base.has_one_ref)
      return false;
    return struct_->base.has_one_ref(&struct_->base) ? true : false;
  }

#ifndef NDEBUG
  // Simple tracking of allocated objects.
  static base::AtomicRefCount DebugObjCt;  // NOLINT(runtime/int)
#endif

 protected:
  StructName* struct_;

 private:
  CefRefCount ref_count_;

  DISALLOW_COPY_AND_ASSIGN(CefCToCpp);
};

#endif  // CEF_LIBCEF_DLL_CTOCPP_CTOCPP_H_
