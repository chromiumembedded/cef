// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef_dll/cpptoc/base_ref_counted_cpptoc.h"

CefBaseRefCountedCppToC::CefBaseRefCountedCppToC() {
}

template<> CefRefPtr<CefBaseRefCounted> CefCppToCRefCounted<
    CefBaseRefCountedCppToC, CefBaseRefCounted, cef_base_ref_counted_t>::
    UnwrapDerived(CefWrapperType type, cef_base_ref_counted_t* s) {
  NOTREACHED();
  return NULL;
}

#if DCHECK_IS_ON()
template<> base::AtomicRefCount CefCppToCRefCounted<CefBaseRefCountedCppToC,
    CefBaseRefCounted, cef_base_ref_counted_t>::DebugObjCt = 0;
#endif

template<> CefWrapperType CefCppToCRefCounted<CefBaseRefCountedCppToC,
    CefBaseRefCounted, cef_base_ref_counted_t>::kWrapperType =
    WT_BASE_REF_COUNTED;
