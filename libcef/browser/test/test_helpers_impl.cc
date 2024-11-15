// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "base/feature_list.h"
#include "base/features.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "cef/include/test/cef_test_helpers.h"
#include "net/base/features.h"
#include "services/network/public/cpp/features.h"

void CefSetDataDirectoryForTests(const CefString& dir) {
  base::PathService::OverrideAndCreateIfNeeded(
      base::DIR_SRC_TEST_DATA_ROOT, base::FilePath(dir), /*is_absolute=*/true,
      /*create=*/false);
}

bool CefIsFeatureEnabledForTests(const CefString& feature_name) {
  // Only includes values that are queried by unit tests.
  const base::Feature* features[] = {
      &net::features::kIgnoreHSTSForLocalhost,
      &base::features::kUseRustJsonParser,
      &network::features::kReduceAcceptLanguage,
  };

  const std::string& name = feature_name;
  for (auto* feature : features) {
    if (feature->name == name) {
      return base::FeatureList::IsEnabled(*feature);
    }
  }

  LOG(FATAL) << "Feature " << name << " is not supported";
}
