// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_RESOURCE_BUNDLE_IMPL_H_
#define CEF_LIBCEF_COMMON_RESOURCE_BUNDLE_IMPL_H_
#pragma once


#include "include/cef_resource_bundle.h"

class CefResourceBundleImpl : public CefResourceBundle {
 public:
  CefResourceBundleImpl();

  // CefResourceBundle methods.
  CefString GetLocalizedString(int string_id) override;
  bool GetDataResource(int resource_id,
                       void*& data,
                       size_t& data_size) override;
  bool GetDataResourceForScale(int resource_id,
                               ScaleFactor scale_factor,
                               void*& data,
                               size_t& data_size) override;

 private:
  IMPLEMENT_REFCOUNTING(CefResourceBundleImpl);
  DISALLOW_COPY_AND_ASSIGN(CefResourceBundleImpl);
};

#endif  // CEF_LIBCEF_COMMON_RESOURCE_BUNDLE_IMPL_H_
