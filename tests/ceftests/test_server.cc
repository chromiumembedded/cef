// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/test_server.h"

#include "tests/ceftests/test_server_manager.h"

#include <vector>

namespace test_server {

// Must use a different port than server_unittest.cc.
const char kServerAddress[] = "127.0.0.1";
const uint16 kServerPort = 8098;
const char kServerScheme[] = "http";
const char kServerOrigin[] = "http://127.0.0.1:8098";
const char kIncompleteDoNotSendData[] = "DO NOT SEND";

CefRefPtr<CefResponse> Create404Response() {
  auto response = CefResponse::Create();
  response->SetStatus(404);
  response->SetMimeType("text/html");
  return response;
}

void Stop(base::OnceClosure callback) {
  // Stop both HTTPS and HTTP servers in a chain.
  Manager::Stop(base::BindOnce(
                    [](base::OnceClosure callback) {
                      Manager::Stop(std::move(callback),
                                    /*https_server=*/false);
                    },
                    std::move(callback)),
                /*https_server=*/true);
}

}  // namespace test_server
