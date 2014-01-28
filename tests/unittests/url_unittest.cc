// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_url.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(URLTest, CreateURL) {
  // Create the URL using the spec.
  {
    CefURLParts parts;
    CefString url;
    CefString(&parts.spec).FromASCII(
        "http://user:pass@www.example.com:88/path/to.html?foo=test&bar=test2");
    EXPECT_TRUE(CefCreateURL(parts, url));
    EXPECT_EQ(url,
        "http://user:pass@www.example.com:88/path/to.html?foo=test&bar=test2");
  }

  // Test that scheme and host are required.
  {
    CefURLParts parts;
    CefString url;
    CefString(&parts.scheme).FromASCII("http");
    EXPECT_FALSE(CefCreateURL(parts, url));
  }
  {
    CefURLParts parts;
    CefString url;
    CefString(&parts.host).FromASCII("www.example.com");
    EXPECT_FALSE(CefCreateURL(parts, url));
  }

  // Create the URL using scheme and host.
  {
    CefURLParts parts;
    CefString url;
    CefString(&parts.scheme).FromASCII("http");
    CefString(&parts.host).FromASCII("www.example.com");
    EXPECT_TRUE(CefCreateURL(parts, url));
    EXPECT_EQ(url, "http://www.example.com/");
  }

  // Create the URL using scheme, host and path.
  {
    CefURLParts parts;
    CefString url;
    CefString(&parts.scheme).FromASCII("http");
    CefString(&parts.host).FromASCII("www.example.com");
    CefString(&parts.path).FromASCII("/path/to.html");
    EXPECT_TRUE(CefCreateURL(parts, url));
    EXPECT_EQ(url, "http://www.example.com/path/to.html");
  }

  // Create the URL using scheme, host, path and query.
  {
    CefURLParts parts;
    CefString url;
    CefString(&parts.scheme).FromASCII("http");
    CefString(&parts.host).FromASCII("www.example.com");
    CefString(&parts.path).FromASCII("/path/to.html");
    CefString(&parts.query).FromASCII("foo=test&bar=test2");
    EXPECT_TRUE(CefCreateURL(parts, url));
    EXPECT_EQ(url, "http://www.example.com/path/to.html?foo=test&bar=test2");
  }

  // Create the URL using all the various components.
  {
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
    EXPECT_EQ(url,
        "http://user:pass@www.example.com:88/path/to.html?foo=test&bar=test2");
  }
}

TEST(URLTest, ParseURL) {
  // Parse the URL using scheme and host.
  {
    CefURLParts parts;
    CefString url;
    url.FromASCII("http://www.example.com");
    EXPECT_TRUE(CefParseURL(url, parts));

    CefString spec(&parts.spec);
    EXPECT_EQ(spec, "http://www.example.com/");
    EXPECT_EQ(parts.username.length, (size_t)0);
    EXPECT_EQ(parts.password.length, (size_t)0);
    CefString scheme(&parts.scheme);
    EXPECT_EQ(scheme, "http");
    CefString host(&parts.host);
    EXPECT_EQ(host, "www.example.com");
    EXPECT_EQ(parts.port.length, (size_t)0);
    CefString path(&parts.path);
    EXPECT_EQ(path, "/");
    EXPECT_EQ(parts.query.length, (size_t)0);
  }

  // Parse the URL using scheme, host and path.
  {
    CefURLParts parts;
    CefString url;
    url.FromASCII("http://www.example.com/path/to.html");
    EXPECT_TRUE(CefParseURL(url, parts));

    CefString spec(&parts.spec);
    EXPECT_EQ(spec, "http://www.example.com/path/to.html");
    EXPECT_EQ(parts.username.length, (size_t)0);
    EXPECT_EQ(parts.password.length, (size_t)0);
    CefString scheme(&parts.scheme);
    EXPECT_EQ(scheme, "http");
    CefString host(&parts.host);
    EXPECT_EQ(host, "www.example.com");
    EXPECT_EQ(parts.port.length, (size_t)0);
    CefString path(&parts.path);
    EXPECT_EQ(path, "/path/to.html");
    EXPECT_EQ(parts.query.length, (size_t)0);
  }

  // Parse the URL using scheme, host, path and query.
  {
    CefURLParts parts;
    CefString url;
    url.FromASCII("http://www.example.com/path/to.html?foo=test&bar=test2");
    EXPECT_TRUE(CefParseURL(url, parts));

    CefString spec(&parts.spec);
    EXPECT_EQ(spec, "http://www.example.com/path/to.html?foo=test&bar=test2");
    EXPECT_EQ(parts.username.length, (size_t)0);
    EXPECT_EQ(parts.password.length, (size_t)0);
    CefString scheme(&parts.scheme);
    EXPECT_EQ(scheme, "http");
    CefString host(&parts.host);
    EXPECT_EQ(host, "www.example.com");
    EXPECT_EQ(parts.port.length, (size_t)0);
    CefString path(&parts.path);
    EXPECT_EQ(path, "/path/to.html");
    CefString query(&parts.query);
    EXPECT_EQ(query, "foo=test&bar=test2");
  }

  // Parse the URL using all the various components.
  {
    CefURLParts parts;
    CefString url;
    url.FromASCII(
        "http://user:pass@www.example.com:88/path/to.html?foo=test&bar=test2");
    EXPECT_TRUE(CefParseURL(url, parts));

    CefString spec(&parts.spec);
    EXPECT_EQ(spec,
        "http://user:pass@www.example.com:88/path/to.html?foo=test&bar=test2");
    CefString scheme(&parts.scheme);
    EXPECT_EQ(scheme, "http");
    CefString username(&parts.username);
    EXPECT_EQ(username, "user");
    CefString password(&parts.password);
    EXPECT_EQ(password, "pass");
    CefString host(&parts.host);
    EXPECT_EQ(host, "www.example.com");
    CefString port(&parts.port);
    EXPECT_EQ(port, "88");
    CefString path(&parts.path);
    EXPECT_EQ(path, "/path/to.html");
    CefString query(&parts.query);
    EXPECT_EQ(query, "foo=test&bar=test2");
  }

  // Parse an invalid URL.
  {
    CefURLParts parts;
    CefString url;
    url.FromASCII("www.example.com");
    EXPECT_FALSE(CefParseURL(url, parts));
  }
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
