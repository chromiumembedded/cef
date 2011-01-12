// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _EXTERNAL_PROTOCOL_HANDLER_H
#define _EXTERNAL_PROTOCOL_HANDLER_H

#include <string>

class GURL;

namespace ExternalProtocolHandler {

// Returns true if the OS provides external support for the specified |scheme|.
bool HasExternalHandler(const std::string& scheme);

// Pass handling of the specified |gurl| to the OS.
bool HandleExternalProtocol(const GURL& gurl);

} // namespace ExternalProtocolHandler

#endif // _EXTERNAL_PROTOCOL_HANDLER_H
