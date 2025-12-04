// Copyright 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_TASK_UTIL_H_
#define CEF_LIBCEF_COMMON_TASK_UTIL_H_
#pragma once

#include "cef/include/cef_task.h"

namespace cef {

// Internal variant of CefCurrentlyOn that does not log when called before
// CefInitialize (when task runners are not yet initialized). Should only be
// used by libcef internal code that may be called before initialization.
// Returns false when task runners are not initialized.
bool CurrentlyOnThread(CefThreadId thread_id);

}  // namespace cef

#endif  // CEF_LIBCEF_COMMON_TASK_UTIL_H_
