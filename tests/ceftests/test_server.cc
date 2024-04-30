// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/test_server.h"

#include <vector>

#include "tests/ceftests/test_server_manager.h"

namespace test_server {

// Must use a different port than server_unittest.cc.
const char kHttpServerAddress[] = "127.0.0.1";
const uint16_t kHttpServerPort = 8098;

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

std::string GetOrigin(bool https_server) {
  return Manager::GetOrigin(https_server);
}

std::string GetScheme(bool https_server) {
  return https_server ? "https" : "http";
}

std::string GetHost(bool https_server, bool include_port) {
  const auto& origin = GetOrigin(https_server);

  const auto scheme_offset = origin.find("//");
  const auto& origin_without_scheme = origin.substr(scheme_offset + 2);
  if (include_port) {
    return origin_without_scheme;
  }

  const auto port_offset = origin_without_scheme.find(':');
  return origin_without_scheme.substr(0, port_offset);
}

}  // namespace test_server
