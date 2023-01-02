// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/chrome/chrome_content_client_cef.h"

#include "libcef/common/app_manager.h"

#include "chrome/common/media/cdm_registration.h"

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "libcef/common/cdm_host_file_path.h"
#endif

ChromeContentClientCef::ChromeContentClientCef() = default;
ChromeContentClientCef::~ChromeContentClientCef() = default;

void ChromeContentClientCef::AddContentDecryptionModules(
    std::vector<content::CdmInfo>* cdms,
    std::vector<media::CdmHostFilePath>* cdm_host_file_paths) {
  if (cdms) {
    RegisterCdmInfo(cdms);
  }

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  if (cdm_host_file_paths) {
    cef::AddCdmHostFilePaths(cdm_host_file_paths);
  }
#endif
}

void ChromeContentClientCef::AddAdditionalSchemes(Schemes* schemes) {
  ChromeContentClient::AddAdditionalSchemes(schemes);
  CefAppManager::Get()->AddAdditionalSchemes(schemes);
}
