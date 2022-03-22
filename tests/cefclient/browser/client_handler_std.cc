// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/client_handler_std.h"

namespace client {

ClientHandlerStd::ClientHandlerStd(Delegate* delegate,
                                   bool with_controls,
                                   const std::string& startup_url)
    : ClientHandler(delegate, /*is_osr=*/false, with_controls, startup_url) {}

}  // namespace client
