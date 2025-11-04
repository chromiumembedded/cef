// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_NET_SCHEME_REGISTRATION_H_
#define CEF_LIBCEF_COMMON_NET_SCHEME_REGISTRATION_H_
#pragma once

#include <string_view>

namespace scheme {

// Returns true if the specified |scheme| is handled internally.
bool IsInternalHandledScheme(std::string_view scheme);

// Returns true if the specified |scheme| is a registered standard scheme.
//
// NOTE: This method is a convenience for implicitly converting std::string
// values to the std::optional<std::string_view> argument expected by the
// exported variant of url::IsStandardScheme(). Callers that already have a
// std::string_view value can call url::IsStandardScheme() directly.
bool IsStandardScheme(std::string_view scheme);

// Returns true if the specified |scheme| is a registered CORS enabled scheme.
bool IsCorsEnabledScheme(std::string_view scheme);

}  // namespace scheme

#endif  // CEF_LIBCEF_COMMON_NET_SCHEME_REGISTRATION_H_
