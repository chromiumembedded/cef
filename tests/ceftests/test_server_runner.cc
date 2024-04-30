// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/test_server_runner.h"

#include "include/base/cef_logging.h"
#include "include/cef_command_line.h"
#include "tests/shared/common/client_switches.h"

namespace test_server {

Runner::Runner(Delegate* delegate) : delegate_(delegate) {
  DCHECK(delegate_);
}

// static
std::unique_ptr<Runner> Runner::Create(Runner::Delegate* delegate,
                                       bool https_server) {
  static bool use_test_http_server = [] {
    auto command_line = CefCommandLine::GetGlobalCommandLine();
    return command_line->HasSwitch(client::switches::kUseTestHttpServer);
  }();

  if (https_server || use_test_http_server) {
    return CreateTest(delegate, https_server);
  }

  return CreateNormal(delegate);
}

}  // namespace test_server
