// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CPPTOC_H
#define _CPPTOC_H

#include "cef.h"
#include "cef_capi.h"
#include "../cef_logging.h"


// Wrap a C++ class with a C structure.  This is used when the class
// implementation exists on this side of the DLL boundary but will have methods
// called from the other side of the DLL boundary.
template <class ClassName, class BaseName, class StructName>
class CefCppToC : public CefThreadSafeBase<CefBase>
{
public:
  // Use this method to retrieve the underlying class instance from our
  // own structure when the structure is passed as the required first
  // parameter of a C API function call. No explicit reference counting
  // is done in this case.
  static CefRefPtr<BaseName> Get(StructName* s)
  {
    // Cast our structure to the wrapper structure type.
    ClassName::Struct* wrapperStruct =
        reinterpret_cast<ClassName::Struct*>(s);
    // Return the underlying object instance.
    return wrapperStruct->class_->GetClass();
  }
  
  // Use this method to create a wrapper structure for passing our class
  // instance to the other side.
  static StructName* Wrap(CefRefPtr<BaseName> c)
  {
    // Wrap our object with the CefCppToC class.
    ClassName* wrapper = new ClassName(c);
    // Add a reference to our wrapper object that will be released once our
    // structure arrives on the other side.
    wrapper->AddRef();
    // Return the structure pointer that can now be passed to the other side.
    return wrapper->GetStruct();
  }

  // Use this method to retrieve the underlying class instance when receiving
  // our wrapper structure back from the other side.
  static CefRefPtr<BaseName> Unwrap(StructName* s)
  {
    // Cast our structure to the wrapper structure type.
    ClassName::Struct* wrapperStruct =
        reinterpret_cast<ClassName::Struct*>(s);
    // Add the underlying object instance to a smart pointer.
    CefRefPtr<BaseName> objectPtr(wrapperStruct->class_->GetClass());
    // Release the reference to our wrapper object that was added before the
    // structure was passed back to us.
    wrapperStruct->class_->Release();
    // Return the underlying object instance.
    return objectPtr;
  }

  // Structure representation with pointer to the C++ class.
  struct Struct
  {
    StructName struct_;
    CefCppToC<ClassName,BaseName,StructName>* class_;
  };

  CefCppToC(BaseName* cls)
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

  BaseName* GetClass() { return class_; }

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
  BaseName* class_;
};

// CefCppToC implementation for CefBase.
class CefBaseCppToC : public CefThreadSafeBase<CefBase>
{
public:
  // Use this method to retrieve the underlying class instance from our
  // own structure when the structure is passed as the required first
  // parameter of a C API function call. No explicit reference counting
  // is done in this case.
  static CefRefPtr<CefBase> Get(cef_base_t* s)
  {
    // Cast our structure to the wrapper structure type.
    CefBaseCppToC::Struct* wrapperStruct =
        reinterpret_cast<CefBaseCppToC::Struct*>(s);
    // Return the underlying object instance.
    return wrapperStruct->class_->GetClass();
  }
  
  // Use this method to create a wrapper structure for passing our class
  // instance to the other side.
  static cef_base_t* Wrap(CefRefPtr<CefBase> c)
  {
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
  static CefRefPtr<CefBase> Unwrap(cef_base_t* s)
  {
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
  struct Struct
  {
    cef_base_t struct_;
    CefBaseCppToC* class_;
  };

  CefBaseCppToC(CefBase* cls)
    : class_(cls)
  {
    DCHECK(cls);

    struct_.class_ = this;
    
    // zero the underlying structure and set base members
    memset(&struct_.struct_, 0, sizeof(cef_base_t));
    struct_.struct_.size = sizeof(cef_base_t);
    struct_.struct_.add_ref = struct_add_ref;
    struct_.struct_.release = struct_release;
    struct_.struct_.get_refct = struct_get_refct;
  }
  virtual ~CefBaseCppToC() {}

  CefBase* GetClass() { return class_; }

  // If returning the structure across the DLL boundary you should call
  // AddRef() on this CefCppToC object.  On the other side of the DLL boundary,
  // call UnderlyingRelease() on the wrapping CefCToCpp object.
  cef_base_t* GetStruct() { return &struct_.struct_; }

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
  CefBase* class_;
};

#endif // _CPPTOC_H
