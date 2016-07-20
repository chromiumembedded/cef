// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_CHROME_API_REGISTRATION_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_CHROME_API_REGISTRATION_H_

#include <string>

class ExtensionFunctionRegistry;

namespace extensions {
namespace api {
namespace cef {

class ChromeFunctionRegistry {
 public:
  static bool IsSupported(const std::string& name);
  static void RegisterAll(ExtensionFunctionRegistry* registry);
};

}  // namespace cef
}  // namespace api
}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_CHROME_API_REGISTRATION_H_
