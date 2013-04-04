// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_SCHEME_TEST_H_
#define CEF_TESTS_CEFCLIENT_SCHEME_TEST_H_
#pragma once

#include "include/cef_browser.h"
#include "include/cef_scheme.h"

namespace scheme_test {

void AddSchemes(CefRefPtr<CefSchemeRegistrar> registrar);

// Register the scheme handler.
void InitTest();

}  // namespace scheme_test

#endif  // CEF_TESTS_CEFCLIENT_SCHEME_TEST_H_
