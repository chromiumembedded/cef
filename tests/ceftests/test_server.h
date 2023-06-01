// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_TEST_SERVER_H_
#define CEF_TESTS_CEFTESTS_TEST_SERVER_H_
#pragma once

#include <string>

#include "include/base/cef_callback.h"
#include "include/cef_registration.h"
#include "include/cef_request.h"
#include "include/cef_response.h"

namespace test_server {

// Use of these values is deprecated. Instead use GetOrigin/GetHost in the
// browser process after server initialization.
extern const char kHttpServerAddress[];
extern const uint16_t kHttpServerPort;

// Used with incomplete tests for data that should not be sent.
extern const char kIncompleteDoNotSendData[];

// Create a 404 response for passing to ResponseCallback.
CefRefPtr<CefResponse> Create404Response();

using ResponseCallback =
    base::RepeatingCallback<void(CefRefPtr<CefResponse> response,
                                 const std::string& response_data)>;

// Stops all servers that are currently running and executes |callback| on the
// UI thread. This method will be called by the test framework on shutdown.
void Stop(base::OnceClosure callback);

// Returns the origin for the currently running server (e.g.
// "[http|https]://127.0.0.1:<port>").
std::string GetOrigin(bool https_server);

// Returns the scheme for the currently running server (e.g. "http" or "https").
std::string GetScheme(bool https_server);

// Returns the host for the currently running server (e.g.
// "127.0.0.1[:<port>]").
std::string GetHost(bool https_server, bool include_port);

}  // namespace test_server

#endif  // CEF_TESTS_CEFTESTS_TEST_SERVER_H_
