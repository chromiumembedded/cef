// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/test_server_runner.h"

#include "include/base/cef_logging.h"

namespace test_server {

std::unique_ptr<Runner> Runner::CreateTest(Delegate* delegate,
                                           bool https_server) {
  NOTREACHED();
  return nullptr;
}

}  // namespace test_server
