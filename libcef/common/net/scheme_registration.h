// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_NET_SCHEME_REGISTRATION_H_
#define CEF_LIBCEF_COMMON_NET_SCHEME_REGISTRATION_H_
#pragma once

#include <string>
#include <vector>

#include "content/public/common/content_client.h"

namespace scheme {

// Add internal schemes.
void AddInternalSchemes(content::ContentClient::Schemes* schemes);

// Returns true if the specified |scheme| is handled internally.
bool IsInternalHandledScheme(const std::string& scheme);

// Returns true if the specified |scheme| is handled internally and should not
// be explicitly registered or unregistered with the URLRequestJobFactory. A
// registered handler for one of these schemes (like "chrome") may still be
// triggered via chaining from an existing ProtocolHandler. |scheme| should
// always be a lower-case string.
bool IsInternalProtectedScheme(const std::string& scheme);

}  // namespace scheme

#endif  // CEF_LIBCEF_COMMON_NET_SCHEME_REGISTRATION_H_
