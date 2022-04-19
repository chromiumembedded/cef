// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/devtools/devtools_util.h"

#include "testing/gtest/include/gtest/gtest.h"

using namespace devtools_util;

TEST(DevToolsUtil, ProtocolParser_IsValidMessage) {
  // Empty dictionary is not valid.
  EXPECT_FALSE(ProtocolParser::IsValidMessage(""));
  EXPECT_FALSE(ProtocolParser::IsValidMessage("{}"));

  // Incorrectly formatted dictionary is not valid.
  EXPECT_FALSE(ProtocolParser::IsValidMessage("{ ]"));

  // Everything else is valid (we don't verify JSON structure).
  EXPECT_TRUE(ProtocolParser::IsValidMessage("{ }"));
  EXPECT_TRUE(ProtocolParser::IsValidMessage("{blah blah}"));
  EXPECT_TRUE(ProtocolParser::IsValidMessage("{method:\"foobar\"}"));
}

TEST(DevToolsUtil, ProtocolParser_Initialize_IsFailure_Unknown) {
  ProtocolParser parser;
  EXPECT_FALSE(parser.IsInitialized());

  // Empty message is invalid.
  EXPECT_TRUE(parser.Initialize(""));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsFailure());

  parser.Reset();
  EXPECT_FALSE(parser.IsInitialized());

  // Empty dictionary is invalid.
  EXPECT_TRUE(parser.Initialize("{}"));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsFailure());

  parser.Reset();
  EXPECT_FALSE(parser.IsInitialized());

  // Unrecognized dictionary type is invalid.
  EXPECT_TRUE(parser.Initialize("{blah blah}"));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsFailure());
}

TEST(DevToolsUtil, ProtocolParser_Initialize_IsFailure_EventMalformed) {
  ProtocolParser parser;
  EXPECT_FALSE(parser.IsInitialized());

  // Empty method is invalid.
  EXPECT_TRUE(parser.Initialize("{\"method\":\"\"}"));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsFailure());

  parser.Reset();
  EXPECT_FALSE(parser.IsInitialized());

  // Unrecognized value is invalid.
  EXPECT_TRUE(parser.Initialize("{\"method\":\"foo\",oops:false}"));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsFailure());

  parser.Reset();
  EXPECT_FALSE(parser.IsInitialized());

  // Params must be a dictionary.
  EXPECT_TRUE(parser.Initialize("{\"method\":\",params:[]}"));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsFailure());
}

TEST(DevToolsUtil, ProtocolParser_Initialize_IsEvent) {
  ProtocolParser parser;
  EXPECT_FALSE(parser.IsInitialized());

  // Method without params is valid.
  std::string message = "{\"method\":\"Test.myMethod\"}";
  EXPECT_TRUE(parser.Initialize(message));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsEvent());
  EXPECT_EQ("Test.myMethod", parser.method_);
  EXPECT_TRUE(parser.params_.empty());

  parser.Reset();
  EXPECT_FALSE(parser.IsInitialized());

  // Method with empty params dictionary is valid.
  message = "{\"method\":\"Test.myMethod2\",\"params\":{}}";
  EXPECT_TRUE(parser.Initialize(message));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsEvent());
  EXPECT_EQ("Test.myMethod2", parser.method_);
  EXPECT_EQ("{}", parser.params_);

  parser.Reset();
  EXPECT_FALSE(parser.IsInitialized());

  // Method with non-empty params dictionary is valid.
  message = "{\"method\":\"Test.myMethod3\",\"params\":{\"foo\":\"bar\"}}";
  EXPECT_TRUE(parser.Initialize(message));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsEvent());
  EXPECT_EQ("Test.myMethod3", parser.method_);
  EXPECT_EQ("{\"foo\":\"bar\"}", parser.params_);
}

TEST(DevToolsUtil, ProtocolParser_Initialize_IsFailure_ResultMalformed) {
  ProtocolParser parser;
  EXPECT_FALSE(parser.IsInitialized());

  // Empty ID is invalid.
  EXPECT_TRUE(parser.Initialize("{\"id\":,result:{}}"));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsFailure());

  parser.Reset();
  EXPECT_FALSE(parser.IsInitialized());

  // Missing result or error value is invalid.
  EXPECT_TRUE(parser.Initialize("{\"id\":1}"));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsFailure());

  parser.Reset();
  EXPECT_FALSE(parser.IsInitialized());

  // Unrecognized value is invalid.
  EXPECT_TRUE(parser.Initialize("{\"id\":1,oops:false}"));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsFailure());

  parser.Reset();
  EXPECT_FALSE(parser.IsInitialized());

  // Result must be a dictionary.
  EXPECT_TRUE(parser.Initialize("{\"id\":1,\"result\":[]}"));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsFailure());

  parser.Reset();
  EXPECT_FALSE(parser.IsInitialized());

  // Error must be a dictionary.
  EXPECT_TRUE(parser.Initialize("{\"id\":1,\"error\":[]}"));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsFailure());
}

TEST(DevToolsUtil, ProtocolParser_Initialize_IsResult_Result) {
  ProtocolParser parser;
  EXPECT_FALSE(parser.IsInitialized());

  // Id with empty result dictionary is valid.
  std::string message = "{\"id\":1,\"result\":{}}";
  EXPECT_TRUE(parser.Initialize(message));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsResult());
  EXPECT_EQ(1, parser.message_id_);
  EXPECT_TRUE(parser.success_);
  EXPECT_EQ("{}", parser.params_);

  parser.Reset();
  EXPECT_FALSE(parser.IsInitialized());

  // Id with non-empty result dictionary is valid.
  message = "{\"id\":2,\"result\":{\"foo\":\"bar\"}}";
  EXPECT_TRUE(parser.Initialize(message));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsResult());
  EXPECT_EQ(2, parser.message_id_);
  EXPECT_TRUE(parser.success_);
  EXPECT_EQ("{\"foo\":\"bar\"}", parser.params_);
}

TEST(DevToolsUtil, ProtocolParser_Initialize_IsResult_Error) {
  ProtocolParser parser;
  EXPECT_FALSE(parser.IsInitialized());

  // Id with empty error dictionary is valid.
  std::string message = "{\"id\":1,\"error\":{}}";
  EXPECT_TRUE(parser.Initialize(message));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsResult());
  EXPECT_EQ(1, parser.message_id_);
  EXPECT_FALSE(parser.success_);
  EXPECT_EQ("{}", parser.params_);

  parser.Reset();
  EXPECT_FALSE(parser.IsInitialized());

  // Id with non-empty error dictionary is valid.
  message = "{\"id\":2,\"error\":{\"foo\":\"bar\"}}";
  EXPECT_TRUE(parser.Initialize(message));
  EXPECT_TRUE(parser.IsInitialized());
  EXPECT_TRUE(parser.IsResult());
  EXPECT_EQ(2, parser.message_id_);
  EXPECT_FALSE(parser.success_);
  EXPECT_EQ("{\"foo\":\"bar\"}", parser.params_);
}

TEST(DevToolsUtil, ProtocolParser_Can_Handle_MissingQuote) {
  ProtocolParser parser;

  const auto message = "{\"method\":\"Test.myMethod}";
  EXPECT_TRUE(parser.Initialize(message));
  EXPECT_TRUE(parser.IsFailure());
}

TEST(DevToolsUtil, ProtocolParser_Can_Handle_MissingComma) {
  ProtocolParser parser;

  const auto message = "{\"id\":1\"error\":{}}";
  EXPECT_TRUE(parser.Initialize(message));
  EXPECT_TRUE(parser.IsFailure());
}
