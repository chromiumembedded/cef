// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef_dll/cpptoc/base_scoped_cpptoc.h"

CefBaseScopedCppToC::CefBaseScopedCppToC() {}

template <>
CefOwnPtr<CefBaseScoped>
CefCppToCScoped<CefBaseScopedCppToC, CefBaseScoped, cef_base_scoped_t>::
    UnwrapDerivedOwn(CefWrapperType type, cef_base_scoped_t* s) {
  NOTREACHED();
  return CefOwnPtr<CefBaseScoped>();
}

template <>
CefRawPtr<CefBaseScoped>
CefCppToCScoped<CefBaseScopedCppToC, CefBaseScoped, cef_base_scoped_t>::
    UnwrapDerivedRaw(CefWrapperType type, cef_base_scoped_t* s) {
  NOTREACHED();
  return nullptr;
}

template <>
CefWrapperType CefCppToCScoped<CefBaseScopedCppToC,
                               CefBaseScoped,
                               cef_base_scoped_t>::kWrapperType =
    WT_BASE_SCOPED;
