// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_response.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

TEST(ResponseTest, SetGetHeaderByName) {
  CefRefPtr<CefResponse> response(CefResponse::Create());
  EXPECT_TRUE(response.get() != nullptr);

  CefResponse::HeaderMap headers, expectedHeaders;

  response->SetHeaderByName("HeaderA", "ValueA", false);
  response->SetHeaderByName("HeaderB", "ValueB", false);

  expectedHeaders.insert(std::make_pair("HeaderA", "ValueA"));
  expectedHeaders.insert(std::make_pair("HeaderB", "ValueB"));

  // Case insensitive retrieval.
  EXPECT_STREQ("ValueA",
               response->GetHeaderByName("headera").ToString().c_str());
  EXPECT_STREQ("ValueB",
               response->GetHeaderByName("headerb").ToString().c_str());
  EXPECT_STREQ("", response->GetHeaderByName("noexist").ToString().c_str());

  response->GetHeaderMap(headers);
  TestMapEqual(expectedHeaders, headers, false);

  // Replace an existing value.
  response->SetHeaderByName("HeaderA", "ValueANew", true);

  expectedHeaders.clear();
  expectedHeaders.insert(std::make_pair("HeaderA", "ValueANew"));
  expectedHeaders.insert(std::make_pair("HeaderB", "ValueB"));

  // Case insensitive retrieval.
  EXPECT_STREQ("ValueANew",
               response->GetHeaderByName("headerA").ToString().c_str());

  response->GetHeaderMap(headers);
  TestMapEqual(expectedHeaders, headers, false);

  // Header with multiple values.
  expectedHeaders.clear();
  expectedHeaders.insert(std::make_pair("HeaderA", "ValueA1"));
  expectedHeaders.insert(std::make_pair("HeaderA", "ValueA2"));
  expectedHeaders.insert(std::make_pair("HeaderB", "ValueB"));
  response->SetHeaderMap(expectedHeaders);

  // When there are multiple values only the first is returned.
  EXPECT_STREQ("ValueA1",
               response->GetHeaderByName("headera").ToString().c_str());

  // Don't overwrite the value.
  response->SetHeaderByName("HeaderA", "ValueANew", false);

  response->GetHeaderMap(headers);
  TestMapEqual(expectedHeaders, headers, false);

  // Overwrite the value (remove the duplicates).
  response->SetHeaderByName("HeaderA", "ValueANew", true);

  expectedHeaders.clear();
  expectedHeaders.insert(std::make_pair("HeaderA", "ValueANew"));
  expectedHeaders.insert(std::make_pair("HeaderB", "ValueB"));

  response->GetHeaderMap(headers);
  TestMapEqual(expectedHeaders, headers, false);
}
