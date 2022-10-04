// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_CHROME_CHROME_CONTENT_CLIENT_CEF_H_
#define CEF_LIBCEF_COMMON_CHROME_CHROME_CONTENT_CLIENT_CEF_H_

#include "chrome/common/chrome_content_client.h"

class ChromeContentClientCef : public ChromeContentClient {
 public:
  ChromeContentClientCef();
  ~ChromeContentClientCef() override;

  // content::ContentClient overrides.
  void AddContentDecryptionModules(
      std::vector<content::CdmInfo>* cdms,
      std::vector<media::CdmHostFilePath>* cdm_host_file_paths) override;
  void AddAdditionalSchemes(Schemes* schemes) override;
};

#endif  // CEF_LIBCEF_COMMON_CHROME_CHROME_CONTENT_CLIENT_CEF_H_
