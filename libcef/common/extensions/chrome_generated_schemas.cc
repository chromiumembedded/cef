// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/common/extensions/chrome_generated_schemas.h"

#include "cef/libcef/browser/extensions/chrome_api_registration.h"
#include "chrome/common/extensions/api/generated_schemas.h"

namespace extensions::api::cef {

// static
std::string_view ChromeGeneratedSchemas::Get(const std::string& name) {
  if (!ChromeFunctionRegistry::IsSupported(name)) {
    return std::string_view();
  }
  return extensions::api::ChromeGeneratedSchemas::Get(name);
}

// static
bool ChromeGeneratedSchemas::IsGenerated(std::string name) {
  if (!ChromeFunctionRegistry::IsSupported(name)) {
    return false;
  }
  return extensions::api::ChromeGeneratedSchemas::IsGenerated(name);
}

}  // namespace extensions::api::cef
