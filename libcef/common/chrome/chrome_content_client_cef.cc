// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/chrome/chrome_content_client_cef.h"

#include "libcef/common/app_manager.h"

ChromeContentClientCef::ChromeContentClientCef() = default;
ChromeContentClientCef::~ChromeContentClientCef() = default;

void ChromeContentClientCef::AddAdditionalSchemes(Schemes* schemes) {
  ChromeContentClient::AddAdditionalSchemes(schemes);
  CefAppManager::Get()->AddAdditionalSchemes(schemes);
}
