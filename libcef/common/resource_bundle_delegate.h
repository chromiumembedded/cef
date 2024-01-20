// Copyright 2019 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_RESOURCE_BUNDLE_DELEGATE_H_
#define CEF_LIBCEF_COMMON_RESOURCE_BUNDLE_DELEGATE_H_
#pragma once

#include "ui/base/resource/resource_bundle.h"

class AlloyContentClient;

class CefResourceBundleDelegate : public ui::ResourceBundle::Delegate {
 public:
  CefResourceBundleDelegate() = default;

  void set_pack_loading_disabled(bool val) { pack_loading_disabled_ = val; }
  bool pack_loading_disabled() const { return pack_loading_disabled_; }
  void set_allow_pack_file_load(bool val) { allow_pack_file_load_ = val; }
  bool allow_pack_file_load() const { return allow_pack_file_load_; }

 private:
  // ui::ResourceBundle::Delegate methods.
  base::FilePath GetPathForResourcePack(
      const base::FilePath& pack_path,
      ui::ResourceScaleFactor scale_factor) override;
  base::FilePath GetPathForLocalePack(const base::FilePath& pack_path,
                                      const std::string& locale) override;
  gfx::Image GetImageNamed(int resource_id) override;
  gfx::Image GetNativeImageNamed(int resource_id) override;
  base::RefCountedMemory* LoadDataResourceBytes(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  absl::optional<std::string> LoadDataResourceString(int resource_id) override;
  bool GetRawDataResource(int resource_id,
                          ui::ResourceScaleFactor scale_factor,
                          base::StringPiece* value) const override;
  bool GetLocalizedString(int message_id, std::u16string* value) const override;

 private:
  bool pack_loading_disabled_ = false;
  bool allow_pack_file_load_ = false;
};

#endif  // CEF_LIBCEF_COMMON_RESOURCE_BUNDLE_DELEGATE_H_
