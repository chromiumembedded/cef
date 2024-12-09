// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_CPPTOC_BASE_CPPTOC_H_
#define CEF_LIBCEF_DLL_CPPTOC_BASE_CPPTOC_H_
#pragma once

#include "include/capi/cef_base_capi.h"
#include "include/cef_base.h"
#include "libcef_dll/cpptoc/cpptoc_scoped.h"

#if !defined(WRAPPING_CEF_SHARED)
#error This file can be included wrapper-side only
#endif

// Wrap a C++ class with a C structure.
class CefBaseScopedCppToC : public CefCppToCScoped<CefBaseScopedCppToC,
                                                   CefBaseScoped,
                                                   cef_base_scoped_t> {
 public:
  CefBaseScopedCppToC();
};

constexpr auto CefBaseScopedCppToC_WrapOwn = CefBaseScopedCppToC::WrapOwn;
constexpr auto CefBaseScopedCppToC_WrapRaw = CefBaseScopedCppToC::WrapRaw;
constexpr auto CefBaseScopedCppToC_UnwrapOwn = CefBaseScopedCppToC::UnwrapOwn;
constexpr auto CefBaseScopedCppToC_UnwrapRaw = CefBaseScopedCppToC::UnwrapRaw;
constexpr auto CefBaseScopedCppToC_GetWrapper = CefBaseScopedCppToC::GetWrapper;

inline cef_base_scoped_t* CefBaseScopedCppToC_WrapRawAndRelease(
    CefRawPtr<CefBaseScoped> c) {
  auto [ownerPtr, structPtr] = CefBaseScopedCppToC_WrapRaw(c);
  ownerPtr.release();
  return structPtr;
}

#endif  // CEF_LIBCEF_DLL_CPPTOC_BASE_CPPTOC_H_
