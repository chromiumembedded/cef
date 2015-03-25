// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/util_mac.h"

#include "base/mac/foundation_util.h"

namespace util_mac {

bool GetLocalLibraryDirectory(base::FilePath* result) {
  return base::mac::GetLocalDirectory(NSLibraryDirectory, result);
}

}  // namespace util_mac
