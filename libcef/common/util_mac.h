// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_UTIL_MAC_H_
#define CEF_LIBCEF_COMMON_UTIL_MAC_H_
#pragma once

namespace base {
class FilePath;
}

namespace util_mac {

bool GetLocalLibraryDirectory(base::FilePath* result);

}  // namespace util_mac

#endif  // CEF_LIBCEF_COMMON_UTIL_MAC_H_
