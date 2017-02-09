// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_CTOCPP_BASE_REF_COUNTED_CTOCPP_H_
#define CEF_LIBCEF_DLL_CTOCPP_BASE_REF_COUNTED_CTOCPP_H_
#pragma once

#include "include/cef_base.h"
#include "include/capi/cef_base_capi.h"
#include "libcef_dll/ctocpp/ctocpp_ref_counted.h"

#if !defined(BUILDING_CEF_SHARED)
#error This file can be included DLL-side only
#endif

// Wrap a C structure with a C++ class.
class CefBaseRefCountedCToCpp
    : public CefCToCppRefCounted<CefBaseRefCountedCToCpp, CefBaseRefCounted,
                                 cef_base_ref_counted_t> {
 public:
  CefBaseRefCountedCToCpp();
};

#endif  // CEF_LIBCEF_DLL_CTOCPP_BASE_REF_COUNTED_CTOCPP_H_
