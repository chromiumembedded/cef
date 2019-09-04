// Copyright 2019 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_RESOURCE_BUNDLE_DELEGATE_H_
#define CEF_LIBCEF_COMMON_RESOURCE_BUNDLE_DELEGATE_H_
#pragma once

#include "ui/base/resource/resource_bundle.h"

class CefContentClient;

class CefResourceBundleDelegate : public ui::ResourceBundle::Delegate {
 public:
  CefResourceBundleDelegate(CefContentClient* content_client)
      : content_client_(content_client) {}

 private:
  // ui::ResourceBundle::Delegate methods.
  base::FilePath GetPathForResourcePack(const base::FilePath& pack_path,
                                        ui::ScaleFactor scale_factor) override;
  base::FilePath GetPathForLocalePack(const base::FilePath& pack_path,
                                      const std::string& locale) override;
  gfx::Image GetImageNamed(int resource_id) override;
  gfx::Image GetNativeImageNamed(int resource_id) override;
  base::RefCountedStaticMemory* LoadDataResourceBytes(
      int resource_id,
      ui::ScaleFactor scale_factor) override;
  bool GetRawDataResource(int resource_id,
                          ui::ScaleFactor scale_factor,
                          base::StringPiece* value) override;
  bool GetLocalizedString(int message_id, base::string16* value) override;

 private:
  CefContentClient* content_client_;
};

#endif  // CEF_LIBCEF_COMMON_RESOURCE_BUNDLE_DELEGATE_H_
