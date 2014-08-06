// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_CTOCPP_BASE_CTOCPP_H_
#define CEF_LIBCEF_DLL_CTOCPP_BASE_CTOCPP_H_
#pragma once

#include "include/base/cef_logging.h"
#include "include/base/cef_macros.h"
#include "include/cef_base.h"
#include "include/capi/cef_base_capi.h"


// CefCToCpp implementation for CefBase.
class CefBaseCToCpp : public CefBase {
 public:
  // Use this method to create a wrapper class instance for a structure
  // received from the other side.
  static CefRefPtr<CefBase> Wrap(cef_base_t* s) {
    if (!s)
      return NULL;

    // Wrap their structure with the CefCToCpp object.
    CefBaseCToCpp* wrapper = new CefBaseCToCpp(s);
    // Put the wrapper object in a smart pointer.
    CefRefPtr<CefBase> wrapperPtr(wrapper);
    // Release the reference that was added to the CefCppToC wrapper object on
    // the other side before their structure was passed to us.
    wrapper->UnderlyingRelease();
    // Return the smart pointer.
    return wrapperPtr;
  }

  // Use this method to retrieve the underlying structure from a wrapper class
  // instance for return back to the other side.
  static cef_base_t* Unwrap(CefRefPtr<CefBase> c) {
    if (!c.get())
      return NULL;

    // Cast the object to our wrapper class type.
    CefBaseCToCpp* wrapper = static_cast<CefBaseCToCpp*>(c.get());
    // Add a reference to the CefCppToC wrapper object on the other side that
    // will be released once the structure is received.
    wrapper->UnderlyingAddRef();
    // Return their original structure.
    return wrapper->GetStruct();
  }

  explicit CefBaseCToCpp(cef_base_t* str)
    : struct_(str) {
    DCHECK(str);
  }
  virtual ~CefBaseCToCpp() {}

  // If returning the structure across the DLL boundary you should call
  // UnderlyingAddRef() on this wrapping CefCToCpp object.  On the other side of
  // the DLL  boundary, call Release() on the CefCppToC object.
  cef_base_t* GetStruct() { return struct_; }

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
    if (struct_->add_ref)
      struct_->add_ref(struct_);
  }
  bool UnderlyingRelease() const {
    if (!struct_->release)
      return false;
    return struct_->release(struct_) ? true : false;
  }
  bool UnderlyingHasOneRef() const {
    if (!struct_->has_one_ref)
      return false;
    return struct_->has_one_ref(struct_) ? true : false;
  }

 private:
  CefRefCount ref_count_;
  cef_base_t* struct_;

  DISALLOW_COPY_AND_ASSIGN(CefBaseCToCpp);
};


#endif  // CEF_LIBCEF_DLL_CTOCPP_BASE_CTOCPP_H_
