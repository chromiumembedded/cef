// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef_dll/ctocpp/base_scoped_ctocpp.h"

CefBaseScopedCToCpp::CefBaseScopedCToCpp() {
}

template<> cef_base_scoped_t*
CefCToCppScoped<CefBaseScopedCToCpp, CefBaseScoped, cef_base_scoped_t>::
    UnwrapDerivedOwn(CefWrapperType type, CefOwnPtr<CefBaseScoped> c) {
  NOTREACHED();
  return NULL;
}

template<> cef_base_scoped_t*
CefCToCppScoped<CefBaseScopedCToCpp, CefBaseScoped, cef_base_scoped_t>::
    UnwrapDerivedRaw(CefWrapperType type, CefRawPtr<CefBaseScoped> c) {
  NOTREACHED();
  return NULL;
}

#if DCHECK_IS_ON()
template<> base::AtomicRefCount CefCToCppScoped<CefBaseScopedCToCpp,
    CefBaseScoped, cef_base_scoped_t>::DebugObjCt = 0;
#endif

template<> CefWrapperType CefCToCppScoped<CefBaseScopedCToCpp,
    CefBaseScoped, cef_base_scoped_t>::kWrapperType = WT_BASE_SCOPED;
