// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_SHUTDOWN_CHECKER_H_
#define CEF_LIBCEF_DLL_SHUTDOWN_CHECKER_H_
#pragma once

namespace shutdown_checker {

// Check that CEF objects are not held at CefShutdown.
void AssertNotShutdown();

// Called from libcef_dll.cc and libcef_dll_wrapper.cc.
void SetIsShutdown();

}  // namespace shutdown_checker

#endif  // CEF_LIBCEF_DLL_SHUTDOWN_CHECKER_H_
