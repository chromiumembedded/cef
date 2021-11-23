// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_ALLOY_ALLOY_CONTENT_CLIENT_H_
#define CEF_LIBCEF_COMMON_ALLOY_ALLOY_CONTENT_CLIENT_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/common/content_client.h"
#include "content/public/common/pepper_plugin_info.h"

class AlloyContentClient : public content::ContentClient {
 public:
  AlloyContentClient();
  ~AlloyContentClient() override;

  // content::ContentClient overrides.
  void AddPepperPlugins(
      std::vector<content::PepperPluginInfo>* plugins) override;
  void AddContentDecryptionModules(
      std::vector<content::CdmInfo>* cdms,
      std::vector<media::CdmHostFilePath>* cdm_host_file_paths) override;
  void AddAdditionalSchemes(Schemes* schemes) override;
  std::u16string GetLocalizedString(int message_id) override;
  std::u16string GetLocalizedString(int message_id,
                                    const std::u16string& replacement) override;
  base::StringPiece GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;

  static void SetPDFEntryFunctions(
      content::PepperPluginInfo::GetInterfaceFunc get_interface,
      content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
      content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module);
};

#endif  // CEF_LIBCEF_COMMON_ALLOY_ALLOY_CONTENT_CLIENT_H_
