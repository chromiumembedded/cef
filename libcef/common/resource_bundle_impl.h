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

  CefResourceBundleImpl(const CefResourceBundleImpl&) = delete;
  CefResourceBundleImpl& operator=(const CefResourceBundleImpl&) = delete;

  // CefResourceBundle methods.
  CefString GetLocalizedString(int string_id) override;
  CefRefPtr<CefBinaryValue> GetDataResource(int resource_id) override;
  CefRefPtr<CefBinaryValue> GetDataResourceForScale(
      int resource_id,
      ScaleFactor scale_factor) override;

 private:
  IMPLEMENT_REFCOUNTING(CefResourceBundleImpl);
};

#endif  // CEF_LIBCEF_COMMON_RESOURCE_BUNDLE_IMPL_H_
