// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_TEST_HELPERS_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_TEST_HELPERS_H_

#include <string>

#include "base/files/file_path.h"

namespace cef_installer {
namespace test {

// Returns the path to the installer test data directory.
base::FilePath GetTestDataPath();

// Returns the SHA-1 thumbprint of the test signing certificate.
std::string GetTestCertificateThumbprint();

}  // namespace test
}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_TEST_HELPERS_H_
