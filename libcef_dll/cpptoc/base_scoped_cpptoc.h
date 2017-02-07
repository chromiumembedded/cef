// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_CPPTOC_BASE_CPPTOC_H_
#define CEF_LIBCEF_DLL_CPPTOC_BASE_CPPTOC_H_
#pragma once

#include "include/cef_base.h"
#include "include/capi/cef_base_capi.h"
#include "libcef_dll/cpptoc/cpptoc_scoped.h"

#if !defined(WRAPPING_CEF_SHARED)
#error This file can be included wrapper-side only
#endif

// Wrap a C++ class with a C structure.
class CefBaseScopedCppToC
    : public CefCppToCScoped<CefBaseScopedCppToC, CefBaseScoped,
                             cef_base_scoped_t> {
 public:
  CefBaseScopedCppToC();
};

#endif  // CEF_LIBCEF_DLL_CPPTOC_BASE_CPPTOC_H_
