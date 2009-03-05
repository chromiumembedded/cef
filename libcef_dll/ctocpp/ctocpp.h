// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CTOCPP_H
#define _CTOCPP_H

#include "cef.h"
#include "cef_capi.h"
#include "base/logging.h"


// Wrap a C structure with a C++ class.
template <class ClassName, class StructName>
class CefCToCpp : public CefThreadSafeBase<ClassName>
{
public:
  CefCToCpp(StructName* str)
    : struct_(str)
  {
    DCHECK(str);
  }
  virtual ~CefCToCpp()
  {
  }

  // If returning the structure across the DLL boundary you should call
  // UnderlyingAddRef() on this wrapping CefCToCpp object.  On the other side of
  // the DLL  boundary, call Release() on the CefCppToC object.
  StructName* GetStruct() { return struct_; }

  // CefBase methods increment/decrement reference counts on both this object
  // and the underlying wrapped structure.
  virtual int AddRef()
  {
    UnderlyingAddRef();
    return CefThreadSafeBase<ClassName>::AddRef();
  }
  virtual int Release()
  {
    UnderlyingRelease();
    return CefThreadSafeBase<ClassName>::Release();
  }

  // Increment/decrement reference counts on only the underlying class.
  int UnderlyingAddRef()
  {
    if(!struct_->base.add_ref)
      return 0;
    return struct_->base.add_ref(&struct_->base);
  }
  int UnderlyingRelease()
  {
    if(!struct_->base.release)
      return 0;
    return struct_->base.release(&struct_->base);
  }

protected:
  StructName* struct_;
};


#endif // _CTOCPP_H
