// Copyright (c) 2024 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/base/cef_dump_without_crashing.h"

#include "include/internal/cef_dump_without_crashing_internal.h"

bool CefDumpWithoutCrashing(long long mseconds_between_dumps,
                            const char* function_name,
                            const char* file_name,
                            int line_number) {
  return cef_dump_without_crashing(mseconds_between_dumps, function_name,
                                   file_name, line_number);
}

#if CEF_API_REMOVED(13500)
bool CefDumpWithoutCrashingUnthrottled() {
  return cef_dump_without_crashing_unthrottled();
}
#endif
