// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/extensions/chrome_generated_schemas.h"

#include "libcef/browser/extensions/chrome_api_registration.h"

#include "base/macros.h"
#include "chrome/common/extensions/api/generated_schemas.h"

namespace extensions {
namespace api {
namespace cef {

// static
base::StringPiece ChromeGeneratedSchemas::Get(const std::string& name) {
  if (!ChromeFunctionRegistry::IsSupported(name))
    return base::StringPiece();
  return extensions::api::ChromeGeneratedSchemas::Get(name);
}

// static
bool ChromeGeneratedSchemas::IsGenerated(std::string name) {
  if (!ChromeFunctionRegistry::IsSupported(name))
    return false;
  return extensions::api::ChromeGeneratedSchemas::IsGenerated(name);
}

}  // namespace cef
}  // namespace api
}  // namespace extensions
