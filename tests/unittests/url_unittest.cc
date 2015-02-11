// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

// Include this first to avoid type conflicts with CEF headers.
#include "tests/unittests/chromium_includes.h"

#include "include/cef_url.h"
#include "testing/gtest/include/gtest/gtest.h"

// Create the URL using the spec.
TEST(URLTest, CreateURLSpec) {
  CefURLParts parts;
  CefString url;
  CefString(&parts.spec).FromASCII(
      "http://user:pass@www.example.com:88/path/to.html?foo=test&bar=test2");
  EXPECT_TRUE(CefCreateURL(parts, url));
  EXPECT_STREQ(
      "http://user:pass@www.example.com:88/path/to.html?foo=test&bar=test2",
      url.ToString().c_str());
}

// Test that host is required.
TEST(URLTest, CreateURLHostRequired) {
  CefURLParts parts;
  CefString url;
  CefString(&parts.scheme).FromASCII("http");
  EXPECT_FALSE(CefCreateURL(parts, url));
}

// Test that scheme is required.
TEST(URLTest, CreateURLSchemeRequired) {
  CefURLParts parts;
  CefString url;
  CefString(&parts.host).FromASCII("www.example.com");
  EXPECT_FALSE(CefCreateURL(parts, url));
}

// Create the URL using scheme and host.
TEST(URLTest, CreateURLSchemeHost) {
  CefURLParts parts;
  CefString url;
  CefString(&parts.scheme).FromASCII("http");
  CefString(&parts.host).FromASCII("www.example.com");
  EXPECT_TRUE(CefCreateURL(parts, url));
  EXPECT_STREQ("http://www.example.com/", url.ToString().c_str());
}

// Create the URL using scheme, host and path.
TEST(URLTest, CreateURLSchemeHostPath) {
  CefURLParts parts;
  CefString url;
  CefString(&parts.scheme).FromASCII("http");
  CefString(&parts.host).FromASCII("www.example.com");
  CefString(&parts.path).FromASCII("/path/to.html");
  EXPECT_TRUE(CefCreateURL(parts, url));
  EXPECT_STREQ("http://www.example.com/path/to.html", url.ToString().c_str());
}

// Create the URL using scheme, host, path and query.
TEST(URLTest, CreateURLSchemeHostPathQuery) {
  CefURLParts parts;
  CefString url;
  CefString(&parts.scheme).FromASCII("http");
  CefString(&parts.host).FromASCII("www.example.com");
  CefString(&parts.path).FromASCII("/path/to.html");
  CefString(&parts.query).FromASCII("foo=test&bar=test2");
  EXPECT_TRUE(CefCreateURL(parts, url));
  EXPECT_STREQ("http://www.example.com/path/to.html?foo=test&bar=test2",
                url.ToString().c_str());
}

// Create the URL using all the various components.
TEST(URLTest, CreateURLAll) {
  CefURLParts parts;
  CefString url;
  CefString(&parts.scheme).FromASCII("http");
  CefString(&parts.username).FromASCII("user");
  CefString(&parts.password).FromASCII("pass");
  CefString(&parts.host).FromASCII("www.example.com");
  CefString(&parts.port).FromASCII("88");
  CefString(&parts.path).FromASCII("/path/to.html");
  CefString(&parts.query).FromASCII("foo=test&bar=test2");
  EXPECT_TRUE(CefCreateURL(parts, url));
  EXPECT_STREQ(
      "http://user:pass@www.example.com:88/path/to.html?foo=test&bar=test2",
      url.ToString().c_str());
}

// Parse the URL using scheme and host.
TEST(URLTest, ParseURLSchemeHost) {
  CefURLParts parts;
  CefString url;
  url.FromASCII("http://www.example.com");
  EXPECT_TRUE(CefParseURL(url, parts));

  CefString spec(&parts.spec);
  EXPECT_STREQ("http://www.example.com/", spec.ToString().c_str());
  EXPECT_EQ(0U, parts.username.length);
  EXPECT_EQ(0U, parts.password.length);
  CefString scheme(&parts.scheme);
  EXPECT_STREQ("http", scheme.ToString().c_str());
  CefString host(&parts.host);
  EXPECT_STREQ("www.example.com", host.ToString().c_str());
  EXPECT_EQ(0U, parts.port.length);
  CefString origin(&parts.origin);
  EXPECT_STREQ(origin.ToString().c_str(), "http://www.example.com/");
  CefString path(&parts.path);
  EXPECT_STREQ("/", path.ToString().c_str());
  EXPECT_EQ(0U, parts.query.length);
}

// Parse the URL using scheme, host and path.
TEST(URLTest, ParseURLSchemeHostPath) {
  CefURLParts parts;
  CefString url;
  url.FromASCII("http://www.example.com/path/to.html");
  EXPECT_TRUE(CefParseURL(url, parts));

  CefString spec(&parts.spec);
  EXPECT_STREQ("http://www.example.com/path/to.html",
                spec.ToString().c_str());
  EXPECT_EQ(0U, parts.username.length);
  EXPECT_EQ(0U, parts.password.length);
  CefString scheme(&parts.scheme);
  EXPECT_STREQ("http", scheme.ToString().c_str());
  CefString host(&parts.host);
  EXPECT_STREQ("www.example.com", host.ToString().c_str());
  EXPECT_EQ(0U, parts.port.length);
  CefString origin(&parts.origin);
  EXPECT_STREQ(origin.ToString().c_str(), "http://www.example.com/");
  CefString path(&parts.path);
  EXPECT_STREQ("/path/to.html", path.ToString().c_str());
  EXPECT_EQ(0U, parts.query.length);
}

// Parse the URL using scheme, host, path and query.
TEST(URLTest, ParseURLSchemeHostPathQuery) {
  CefURLParts parts;
  CefString url;
  url.FromASCII("http://www.example.com/path/to.html?foo=test&bar=test2");
  EXPECT_TRUE(CefParseURL(url, parts));

  CefString spec(&parts.spec);
  EXPECT_STREQ("http://www.example.com/path/to.html?foo=test&bar=test2",
                spec.ToString().c_str());
  EXPECT_EQ(0U, parts.username.length);
  EXPECT_EQ(0U, parts.password.length);
  CefString scheme(&parts.scheme);
  EXPECT_STREQ("http", scheme.ToString().c_str());
  CefString host(&parts.host);
  EXPECT_STREQ("www.example.com", host.ToString().c_str());
  EXPECT_EQ(0U, parts.port.length);
  CefString origin(&parts.origin);
  EXPECT_STREQ(origin.ToString().c_str(), "http://www.example.com/");
  CefString path(&parts.path);
  EXPECT_STREQ("/path/to.html", path.ToString().c_str());
  CefString query(&parts.query);
  EXPECT_STREQ("foo=test&bar=test2", query.ToString().c_str());
}

// Parse the URL using all the various components.
TEST(URLTest, ParseURLAll) {
  CefURLParts parts;
  CefString url;
  url.FromASCII(
      "http://user:pass@www.example.com:88/path/to.html?foo=test&bar=test2");
  EXPECT_TRUE(CefParseURL(url, parts));

  CefString spec(&parts.spec);
  EXPECT_STREQ(
      "http://user:pass@www.example.com:88/path/to.html?foo=test&bar=test2",
      spec.ToString().c_str());
  CefString scheme(&parts.scheme);
  EXPECT_STREQ("http", scheme.ToString().c_str());
  CefString username(&parts.username);
  EXPECT_STREQ("user", username.ToString().c_str());
  CefString password(&parts.password);
  EXPECT_STREQ("pass", password.ToString().c_str());
  CefString host(&parts.host);
  EXPECT_STREQ("www.example.com", host.ToString().c_str());
  CefString port(&parts.port);
  EXPECT_STREQ("88", port.ToString().c_str());
  CefString origin(&parts.origin);
  EXPECT_STREQ(origin.ToString().c_str(), "http://www.example.com:88/");
  CefString path(&parts.path);
  EXPECT_STREQ("/path/to.html", path.ToString().c_str());
  CefString query(&parts.query);
  EXPECT_STREQ("foo=test&bar=test2", query.ToString().c_str());
}

// Parse an invalid URL.
TEST(URLTest, ParseURLInvalid) {
  CefURLParts parts;
  CefString url;
  url.FromASCII("www.example.com");
  EXPECT_FALSE(CefParseURL(url, parts));
}

// Parse a non-standard scheme.
TEST(URLTest, ParseURLNonStandard) {
  CefURLParts parts;
  CefString url;
  url.FromASCII("custom:something%20else?foo");
  EXPECT_TRUE(CefParseURL(url, parts));

  CefString spec(&parts.spec);
  EXPECT_STREQ("custom:something%20else?foo", spec.ToString().c_str());
  EXPECT_EQ(0U, parts.username.length);
  EXPECT_EQ(0U, parts.password.length);
  CefString scheme(&parts.scheme);
  EXPECT_STREQ("custom", scheme.ToString().c_str());
  EXPECT_EQ(0U, parts.host.length);
  EXPECT_EQ(0U, parts.port.length);
  EXPECT_EQ(0U, parts.origin.length);
  CefString path(&parts.path);
  EXPECT_STREQ("something%20else", path.ToString().c_str());
  CefString query(&parts.query);
  EXPECT_STREQ("foo", query.ToString().c_str());
}

TEST(URLTest, GetMimeType) {
  CefString mime_type;

  mime_type = CefGetMimeType("html");
  EXPECT_STREQ("text/html", mime_type.ToString().c_str());

  mime_type = CefGetMimeType("txt");
  EXPECT_STREQ("text/plain", mime_type.ToString().c_str());

  mime_type = CefGetMimeType("gif");
  EXPECT_STREQ("image/gif", mime_type.ToString().c_str());
}

TEST(URLTest, Base64Encode) {
  const std::string& test_str_decoded = "A test string";
  const std::string& test_str_encoded = "QSB0ZXN0IHN0cmluZw==";
  const CefString& encoded_value =
      CefBase64Encode(test_str_decoded.data(), test_str_decoded.size());
  EXPECT_STREQ(test_str_encoded.c_str(), encoded_value.ToString().c_str());
}

TEST(URLTest, Base64Decode) {
  const std::string& test_str_decoded = "A test string";
  const std::string& test_str_encoded = "QSB0ZXN0IHN0cmluZw==";
  CefRefPtr<CefBinaryValue> decoded_value = CefBase64Decode(test_str_encoded);
  EXPECT_TRUE(decoded_value.get());

  const size_t decoded_size = decoded_value->GetSize();
  EXPECT_EQ(test_str_decoded.size(), decoded_size);

  std::string decoded_str;
  decoded_str.resize(decoded_size + 1);  // Include space for NUL-terminator.
  const size_t get_data_result =
      decoded_value->GetData(const_cast<char*>(decoded_str.data()),
                             decoded_size, 0);
  EXPECT_EQ(decoded_size, get_data_result);
  EXPECT_STREQ(test_str_decoded.c_str(), decoded_str.c_str());
}

TEST(URLTest, URIEncode) {
  const std::string& test_str_decoded = "A test string=";
  const std::string& test_str_encoded = "A%20test%20string%3D";
  const CefString& encoded_value = CefURIEncode(test_str_decoded, false);
  EXPECT_STREQ(test_str_encoded.c_str(), encoded_value.ToString().c_str());
}

TEST(URLTest, URIDecode) {
  const std::string& test_str_decoded = "A test string=";
  const std::string& test_str_encoded = "A%20test%20string%3D";
  const CefString& decoded_value =
      CefURIDecode(test_str_encoded, false,
                   static_cast<cef_uri_unescape_rule_t>(
                      UU_SPACES | UU_URL_SPECIAL_CHARS));
  EXPECT_STREQ(test_str_decoded.c_str(), decoded_value.ToString().c_str());
}
