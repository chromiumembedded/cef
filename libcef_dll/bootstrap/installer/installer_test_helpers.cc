// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_test_helpers.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"

namespace cef_installer {
namespace test {

base::FilePath GetTestDataPath() {
  base::FilePath src_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root);
  return src_root.Append(L"cef")
      .Append(L"libcef_dll")
      .Append(L"bootstrap")
      .Append(L"installer")
      .Append(L"testdata");
}

std::string GetTestCertificateThumbprint() {
  base::FilePath path = GetTestDataPath().Append(L"test_thumbprint.txt");
  std::string thumbprint;
  if (!base::ReadFileToString(path, &thumbprint)) {
    return std::string();
  }
  base::TrimWhitespaceASCII(thumbprint, base::TRIM_ALL, &thumbprint);
  return thumbprint;
}

}  // namespace test
}  // namespace cef_installer
