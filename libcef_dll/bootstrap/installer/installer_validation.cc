// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_validation.h"

#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"

namespace cef_installer {

bool IsValidAbiHash(const std::string& hash) {
  if (hash.empty() || hash.size() > kMaxAbiHashLength) {
    return false;
  }
  for (char c : hash) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F'))) {
      return false;
    }
  }
  return true;
}

}  // namespace cef_installer
