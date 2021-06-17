// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_CTOCPP_CTOCPP_SCOPED_H_
#define CEF_LIBCEF_DLL_CTOCPP_CTOCPP_SCOPED_H_
#pragma once

#include "include/base/cef_logging.h"
#include "include/base/cef_macros.h"
#include "include/capi/cef_base_capi.h"
#include "include/cef_base.h"
#include "libcef_dll/wrapper_types.h"

// Wrap a C structure with a C++ class. This is used when the implementation
// exists on the other side of the DLL boundary but will have methods called on
// this side of the DLL boundary.
template <class ClassName, class BaseName, class StructName>
class CefCToCppScoped : public BaseName {
 public:
  // Create a new wrapper instance for a structure reference received from the
  // other side. The caller owns the CToCpp wrapper instance but not necessarily
  // the underling object on the CppToC side (depends if s->del is non-NULL).
  // The returned wrapper object can be used as either a scoped argument or to
  // pass ownership. For example:
  //
  // void my_method(my_type1_t* struct1, my_type2_t* struct2) {
  //  // Passes ownership to MyMethod1().
  //  MyMethod1(MyType1CToCpp::Wrap(struct1));
  //
  //  // Passes reference to MyMethod2().
  //  CefOwnPtr<MyType1> obj2 = MyType2CToCpp::Wrap(struct2);
  //  MyMethod2(obj2.get());
  //  // |obj2| is deleted when my_method() goes out of scope.
  // }
  //
  // void MyMethod1(CefOwnPtr<MyType1> obj1) {
  //   // |obj1| is deleted when MyMethod1() goes out of scope.
  // }
  //
  // void MyMethod2(CefRawPtr<MyType2> obj2) {
  // }
  static CefOwnPtr<BaseName> Wrap(StructName* s);

  // Retrieve the underlying structure reference from a wrapper instance for
  // return back to the other side. Ownership will be passed back to the other
  // side and the wrapper will be deleted. For example:
  //
  // void MyMethod(CefOwnPtr<MyType> obj) {
  //   // Ownership of the underlying MyType object is passed to my_method().
  //   my_method(MyTypeCToCpp::UnwrapOwn(std::move(obj)));
  //   // |obj| is now NULL.
  // }
  static StructName* UnwrapOwn(CefOwnPtr<BaseName> c);

  // Retrieve the underlying structure reference from a wrapper instance for
  // return back to the other side. Ownership does not change. For example:
  //
  // void MyMethod(CefRawPtr<MyType> obj) {
  //   // A reference is passed to my_method(). Ownership does not change.
  //   my_method2(MyTypeCToCpp::UnwrapRaw(obj));
  // }
  static StructName* UnwrapRaw(CefRawPtr<BaseName> c);

  // Override delete operator to properly delete the WrapperStruct.
  // ~CefCToCppScoped will be called first followed by this method.
  static void operator delete(void* ptr);

 protected:
  CefCToCppScoped() {}
  virtual ~CefCToCppScoped() {}

  // If returning the structure across the DLL boundary use Unwrap() instead.
  StructName* GetStruct() const {
    WrapperStruct* wrapperStruct = GetWrapperStruct(this);
    // Verify that the wrapper offset was calculated correctly.
    DCHECK_EQ(kWrapperType, wrapperStruct->type_);
    return wrapperStruct->struct_;
  }

 private:
  // Used to associate this wrapper object and the structure reference received
  // from the other side.
  struct WrapperStruct;

  static WrapperStruct* GetWrapperStruct(const BaseName* obj);

  // Unwrap as the derived type.
  static StructName* UnwrapDerivedOwn(CefWrapperType type,
                                      CefOwnPtr<BaseName> c);
  static StructName* UnwrapDerivedRaw(CefWrapperType type,
                                      CefRawPtr<BaseName> c);

  static CefWrapperType kWrapperType;

  DISALLOW_COPY_AND_ASSIGN(CefCToCppScoped);
};

template <class ClassName, class BaseName, class StructName>
struct CefCToCppScoped<ClassName, BaseName, StructName>::WrapperStruct {
  CefWrapperType type_;
  StructName* struct_;
  ClassName wrapper_;
};

template <class ClassName, class BaseName, class StructName>
CefOwnPtr<BaseName> CefCToCppScoped<ClassName, BaseName, StructName>::Wrap(
    StructName* s) {
  if (!s)
    return CefOwnPtr<BaseName>();

  // Wrap their structure with the CefCToCpp object.
  WrapperStruct* wrapperStruct = new WrapperStruct;
  wrapperStruct->type_ = kWrapperType;
  wrapperStruct->struct_ = s;

  return CefOwnPtr<BaseName>(&wrapperStruct->wrapper_);
}

template <class ClassName, class BaseName, class StructName>
StructName* CefCToCppScoped<ClassName, BaseName, StructName>::UnwrapOwn(
    CefOwnPtr<BaseName> c) {
  if (!c.get())
    return nullptr;

  WrapperStruct* wrapperStruct = GetWrapperStruct(c.get());

  // If the type does not match this object then we need to unwrap as the
  // derived type.
  if (wrapperStruct->type_ != kWrapperType)
    return UnwrapDerivedOwn(wrapperStruct->type_, std::move(c));

  StructName* orig_struct = wrapperStruct->struct_;

#if DCHECK_IS_ON()
  // We should own the object currently.
  cef_base_scoped_t* base = reinterpret_cast<cef_base_scoped_t*>(orig_struct);
  DCHECK(base && base->del);
#endif

  // Don't delete the original object when the wrapper is deleted.
  wrapperStruct->struct_ = nullptr;

  // Return the original structure.
  return orig_struct;
  // The wrapper |c| is deleted when this method goes out of scope.
}

template <class ClassName, class BaseName, class StructName>
StructName* CefCToCppScoped<ClassName, BaseName, StructName>::UnwrapRaw(
    CefRawPtr<BaseName> c) {
  if (!c)
    return nullptr;

  WrapperStruct* wrapperStruct = GetWrapperStruct(c);

  // If the type does not match this object then we need to unwrap as the
  // derived type.
  if (wrapperStruct->type_ != kWrapperType)
    return UnwrapDerivedRaw(wrapperStruct->type_, c);

  // Return the original structure.
  return wrapperStruct->struct_;
}

template <class ClassName, class BaseName, class StructName>
NO_SANITIZE("cfi-icall")
void CefCToCppScoped<ClassName, BaseName, StructName>::operator delete(
    void* ptr) {
  WrapperStruct* wrapperStruct = GetWrapperStruct(static_cast<BaseName*>(ptr));
  // Verify that the wrapper offset was calculated correctly.
  DCHECK_EQ(kWrapperType, wrapperStruct->type_);

  // May be NULL if UnwrapOwn() was called.
  cef_base_scoped_t* base =
      reinterpret_cast<cef_base_scoped_t*>(wrapperStruct->struct_);

  // If we own the object (base->del != NULL) then notify the other side that
  // the object has been deleted.
  if (base && base->del)
    base->del(base);

  // Delete the wrapper structure without executing ~CefCToCppScoped() an
  // additional time.
  ::operator delete(wrapperStruct);
}

template <class ClassName, class BaseName, class StructName>
typename CefCToCppScoped<ClassName, BaseName, StructName>::WrapperStruct*
CefCToCppScoped<ClassName, BaseName, StructName>::GetWrapperStruct(
    const BaseName* obj) {
  // Offset using the WrapperStruct size instead of individual member sizes to
  // avoid problems due to platform/compiler differences in structure padding.
  return reinterpret_cast<WrapperStruct*>(
      reinterpret_cast<char*>(const_cast<BaseName*>(obj)) -
      (sizeof(WrapperStruct) - sizeof(ClassName)));
}

#endif  // CEF_LIBCEF_DLL_CTOCPP_CTOCPP_SCOPED_H_
