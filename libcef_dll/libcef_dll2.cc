// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
//

#include "include/cef_version.h"
#include <cstddef>

CEF_EXPORT int cef_build_revision() {
  return CEF_REVISION;
}

CEF_EXPORT int cef_version_info(int entry) {
  switch (entry) {
    case 0: return CEF_VERSION_MAJOR;
    case 1: return CEF_REVISION;
    case 2: return CHROME_VERSION_MAJOR;
    case 3: return CHROME_VERSION_MINOR;
    case 4: return CHROME_VERSION_BUILD;
    case 5: return CHROME_VERSION_PATCH;
    default: return 0;
  }
}

CEF_EXPORT const char* cef_api_hash(int entry) {
  switch (entry) {
    case 0: return CEF_API_HASH_PLATFORM;
    case 1: return CEF_API_HASH_UNIVERSAL;
    default: return NULL;
  }
}
