// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_TEST_RUNNER_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_TEST_RUNNER_H_
#pragma once

#include <set>
#include <string>

#include "include/cef_browser.h"
#include "include/cef_request.h"
#include "include/wrapper/cef_message_router.h"
#include "include/wrapper/cef_resource_manager.h"

namespace client::test_runner {

// Run a test.
void RunTest(CefRefPtr<CefBrowser> browser, int id);

// Returns the contents of the CefRequest as a string.
std::string DumpRequestContents(CefRefPtr<CefRequest> request);

// Returns the dump response as a stream. |request| is the request.
// |response_headers| will be populated with extra response headers, if any.
CefRefPtr<CefStreamReader> GetDumpResponse(
    CefRefPtr<CefRequest> request,
    CefResponse::HeaderMap& response_headers);

// Returns a data: URI with the specified contents.
std::string GetDataURI(const std::string& data, const std::string& mime_type);

// Returns the string representation of the specified error code.
std::string GetErrorString(cef_errorcode_t code);
std::string GetErrorString(cef_termination_status_t status);

using StringResourceMap = std::map<std::string, std::string>;

// Set up the resource manager for tests.
void SetupResourceManager(CefRefPtr<CefResourceManager> resource_manager,
                          StringResourceMap* string_resource_map);

// Show a JS alert message.
void Alert(CefRefPtr<CefBrowser> browser, const std::string& message);

// Returns "https://tests/<path>".
std::string GetTestURL(const std::string& path);

// Returns true if |url| is a test URL with the specified |path|. This matches
// both "https://tests/<path>" and "http://localhost:xxxx/<path>".
bool IsTestURL(const std::string& url, const std::string& path);

// Create all CefMessageRouterBrowserSide::Handler objects. They will be
// deleted when the ClientHandler is destroyed.
using MessageHandlerSet = std::set<CefMessageRouterBrowserSide::Handler*>;
void CreateMessageHandlers(MessageHandlerSet& handlers);

// Register scheme handlers for tests.
void RegisterSchemeHandlers();

// Create a resource response filter for tests.
CefRefPtr<CefResponseFilter> GetResourceResponseFilter(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefResponse> response);

}  // namespace client::test_runner

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_TEST_RUNNER_H_
