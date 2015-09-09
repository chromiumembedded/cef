// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/common/resource_bundle_impl.h"

#include "ui/base/resource/resource_bundle.h"

CefResourceBundleImpl::CefResourceBundleImpl() {
}

CefString CefResourceBundleImpl::GetLocalizedString(int string_id) {
  if (!ui::ResourceBundle::HasSharedInstance())
    return CefString();

  return ui::ResourceBundle::GetSharedInstance().GetLocalizedString(string_id);
}

bool CefResourceBundleImpl::GetDataResource(int resource_id,
                                            void*& data,
                                            size_t& data_size) {
  return GetDataResourceForScale(resource_id, SCALE_FACTOR_NONE, data,
                                 data_size);
}

bool CefResourceBundleImpl::GetDataResourceForScale(int resource_id,
                                                    ScaleFactor scale_factor,
                                                    void*& data,
                                                    size_t& data_size) {
  if (!ui::ResourceBundle::HasSharedInstance())
    return false;

  const base::StringPiece& result =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
          resource_id, static_cast<ui::ScaleFactor>(scale_factor));
  if (result.empty())
    return false;

  data = const_cast<char*>(result.data());
  data_size = result.size();
  return true;
}

// static
CefRefPtr<CefResourceBundle> CefResourceBundle::GetGlobal() {
  return new CefResourceBundleImpl();
}
