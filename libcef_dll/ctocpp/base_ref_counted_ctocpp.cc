// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef_dll/ctocpp/base_ref_counted_ctocpp.h"

CefBaseRefCountedCToCpp::CefBaseRefCountedCToCpp() {
}

template<> cef_base_ref_counted_t* CefCToCppRefCounted<CefBaseRefCountedCToCpp,
    CefBaseRefCounted, cef_base_ref_counted_t>::UnwrapDerived(
    CefWrapperType type, CefBaseRefCounted* c) {
  NOTREACHED();
  return NULL;
}

#if DCHECK_IS_ON()
template<> base::AtomicRefCount CefCToCppRefCounted<CefBaseRefCountedCToCpp,
    CefBaseRefCounted, cef_base_ref_counted_t>::DebugObjCt = 0;
#endif

template<> CefWrapperType CefCToCppRefCounted<CefBaseRefCountedCToCpp,
    CefBaseRefCounted, cef_base_ref_counted_t>::kWrapperType =
    WT_BASE_REF_COUNTED;
