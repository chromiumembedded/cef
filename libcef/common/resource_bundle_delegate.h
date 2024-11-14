// Copyright 2019 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_RESOURCE_BUNDLE_DELEGATE_H_
#define CEF_LIBCEF_COMMON_RESOURCE_BUNDLE_DELEGATE_H_
#pragma once

#include "ui/base/resource/resource_bundle.h"

class CefResourceBundleDelegate : public ui::ResourceBundle::Delegate {
 public:
  CefResourceBundleDelegate();

  CefResourceBundleDelegate(const CefResourceBundleDelegate&) = delete;
  CefResourceBundleDelegate& operator=(const CefResourceBundleDelegate&) =
      delete;

 private:
  // ui::ResourceBundle::Delegate methods.
  base::FilePath GetPathForResourcePack(
      const base::FilePath& pack_path,
      ui::ResourceScaleFactor scale_factor) override;
  base::FilePath GetPathForLocalePack(const base::FilePath& pack_path,
                                      const std::string& locale) override;
  gfx::Image GetImageNamed(int resource_id) override;
  gfx::Image GetNativeImageNamed(int resource_id) override;
  bool HasDataResource(int resource_id) const override;
  base::RefCountedMemory* LoadDataResourceBytes(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  std::optional<std::string> LoadDataResourceString(int resource_id) override;
  bool GetRawDataResource(int resource_id,
                          ui::ResourceScaleFactor scale_factor,
                          std::string_view* value) const override;
  bool GetLocalizedString(int message_id, std::u16string* value) const override;
};

#endif  // CEF_LIBCEF_COMMON_RESOURCE_BUNDLE_DELEGATE_H_
