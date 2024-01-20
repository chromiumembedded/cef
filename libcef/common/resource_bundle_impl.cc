// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/common/resource_bundle_impl.h"

#include "base/memory/ref_counted_memory.h"
#include "ui/base/resource/resource_bundle.h"

CefResourceBundleImpl::CefResourceBundleImpl() = default;

CefString CefResourceBundleImpl::GetLocalizedString(int string_id) {
  if (!ui::ResourceBundle::HasSharedInstance()) {
    return CefString();
  }

  return ui::ResourceBundle::GetSharedInstance().GetLocalizedString(string_id);
}

CefRefPtr<CefBinaryValue> CefResourceBundleImpl::GetDataResource(
    int resource_id) {
  return GetDataResourceForScale(resource_id, SCALE_FACTOR_NONE);
}

CefRefPtr<CefBinaryValue> CefResourceBundleImpl::GetDataResourceForScale(
    int resource_id,
    ScaleFactor scale_factor) {
  if (!ui::ResourceBundle::HasSharedInstance()) {
    return nullptr;
  }

  base::RefCountedMemory* result =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          resource_id, static_cast<ui::ResourceScaleFactor>(scale_factor));
  if (!result) {
    return nullptr;
  }

  return CefBinaryValue::Create(result->data(), result->size());
}

// static
CefRefPtr<CefResourceBundle> CefResourceBundle::GetGlobal() {
  return new CefResourceBundleImpl();
}
