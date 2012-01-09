// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/external_protocol_handler.h"

namespace ExternalProtocolHandler {

bool HasExternalHandler(const std::string& scheme) {
  return false;
}

bool HandleExternalProtocol(const GURL& gurl) {
  return false;
}

}  // namespace ExternalProtocolHandler
