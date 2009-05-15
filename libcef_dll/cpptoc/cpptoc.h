// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CPPTOC_H
#define _CPPTOC_H

#include "cef.h"
#include "cef_capi.h"
#include "../cef_logging.h"


// Wrap a C++ class with a C structure.
template <class ClassName, class StructName>
class CefCppToC : public CefThreadSafeBase<CefBase>
{
public:
  // Structure representation with pointer to the C++ class.
  struct Struct
  {
    StructName struct_;
    CefCppToC<ClassName, StructName>* class_;
  };

  CefCppToC(ClassName* cls)
    : class_(cls)
  {
    DCHECK(cls);

    struct_.class_ = this;
    
    // zero the underlying structure and set base members
    memset(&struct_.struct_, 0, sizeof(StructName));
    struct_.struct_.base.size = sizeof(StructName);
    struct_.struct_.base.add_ref = struct_add_ref;
    struct_.struct_.base.release = struct_release;
    struct_.struct_.base.get_refct = struct_get_refct;

#ifdef _DEBUG
    CefAtomicIncrement(&DebugObjCt);
#endif
  }
  virtual ~CefCppToC()
  {
#ifdef _DEBUG
    CefAtomicDecrement(&DebugObjCt);
#endif
  }

  ClassName* GetClass() { return class_; }

  // If returning the structure across the DLL boundary you should call
  // AddRef() on this CefCppToC object.  On the other side of the DLL boundary,
  // call UnderlyingRelease() on the wrapping CefCToCpp object.
  StructName* GetStruct() { return &struct_.struct_; }

  // CefBase methods increment/decrement reference counts on both this object
  // and the underlying wrapper class.
  virtual int AddRef()
  {
    UnderlyingAddRef();
    return CefThreadSafeBase<CefBase>::AddRef();
  }
  virtual int Release()
  {
    UnderlyingRelease();
    return CefThreadSafeBase<CefBase>::Release();
  }

  // Increment/decrement reference counts on only the underlying class.
  int UnderlyingAddRef() { return class_->AddRef(); }
  int UnderlyingRelease() { return class_->Release(); }
  int UnderlyingGetRefCt() { return class_->GetRefCt(); }

#ifdef _DEBUG
  // Simple tracking of allocated objects.
  static long DebugObjCt;
#endif

private:
  static int CEF_CALLBACK struct_add_ref(struct _cef_base_t* base)
  {
    DCHECK(base);
    if(!base)
      return 0;

    Struct* impl = reinterpret_cast<Struct*>(base);
    return impl->class_->AddRef();
  }

  static int CEF_CALLBACK struct_release(struct _cef_base_t* base)
  {
    DCHECK(base);
    if(!base)
      return 0;

    Struct* impl = reinterpret_cast<Struct*>(base);
    return impl->class_->Release();
  }

  static int CEF_CALLBACK struct_get_refct(struct _cef_base_t* base)
  {
    DCHECK(base);
    if(!base)
      return 0;

    Struct* impl = reinterpret_cast<Struct*>(base);
    return impl->class_->GetRefCt();
  }

protected:
  Struct struct_;
  ClassName* class_;
};

#endif // _CPPTOC_H
