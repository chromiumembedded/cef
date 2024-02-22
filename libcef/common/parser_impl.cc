// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include <sstream>

#include "include/cef_parser.h"

#include "base/base64.h"
#include "base/strings/escape.h"
#include "base/threading/thread_restrictions.h"
#include "components/url_formatter/elide_url.h"
#include "net/base/mime_util.h"
#include "url/gurl.h"

bool CefResolveURL(const CefString& base_url,
                   const CefString& relative_url,
                   CefString& resolved_url) {
  GURL base_gurl(base_url.ToString());
  if (!base_gurl.is_valid()) {
    return false;
  }

  GURL combined_gurl = base_gurl.Resolve(relative_url.ToString());
  if (!combined_gurl.is_valid()) {
    return false;
  }

  resolved_url = combined_gurl.spec();
  return true;
}

bool CefParseURL(const CefString& url, CefURLParts& parts) {
  GURL gurl(url.ToString());
  if (!gurl.is_valid()) {
    return false;
  }

  CefString(&parts.spec).FromString(gurl.spec());
  CefString(&parts.scheme).FromString(gurl.scheme());
  CefString(&parts.username).FromString(gurl.username());
  CefString(&parts.password).FromString(gurl.password());
  CefString(&parts.host).FromString(gurl.host());
  CefString(&parts.origin).FromString(gurl.DeprecatedGetOriginAsURL().spec());
  CefString(&parts.port).FromString(gurl.port());
  CefString(&parts.path).FromString(gurl.path());
  CefString(&parts.query).FromString(gurl.query());
  CefString(&parts.fragment).FromString(gurl.ref());

  return true;
}

bool CefCreateURL(const CefURLParts& parts, CefString& url) {
  std::string spec = CefString(parts.spec.str, parts.spec.length, false);
  std::string scheme = CefString(parts.scheme.str, parts.scheme.length, false);
  std::string username =
      CefString(parts.username.str, parts.username.length, false);
  std::string password =
      CefString(parts.password.str, parts.password.length, false);
  std::string host = CefString(parts.host.str, parts.host.length, false);
  std::string port = CefString(parts.port.str, parts.port.length, false);
  std::string path = CefString(parts.path.str, parts.path.length, false);
  std::string query = CefString(parts.query.str, parts.query.length, false);
  std::string fragment =
      CefString(parts.fragment.str, parts.fragment.length, false);

  GURL gurl;
  if (!spec.empty()) {
    gurl = GURL(spec);
  } else if (!scheme.empty() && !host.empty()) {
    std::stringstream ss;
    ss << scheme << "://";
    if (!username.empty()) {
      ss << username;
      if (!password.empty()) {
        ss << ":" << password;
      }
      ss << "@";
    }
    ss << host;
    if (!port.empty()) {
      ss << ":" << port;
    }
    if (!path.empty()) {
      ss << path;
    }
    if (!query.empty()) {
      ss << "?" << query;
    }
    if (!fragment.empty()) {
      ss << "#" << fragment;
    }
    gurl = GURL(ss.str());
  }

  if (gurl.is_valid()) {
    url = gurl.spec();
    return true;
  }

  return false;
}

CefString CefFormatUrlForSecurityDisplay(const CefString& origin_url) {
  return url_formatter::FormatUrlForSecurityDisplay(
      GURL(origin_url.ToString()));
}

CefString CefGetMimeType(const CefString& extension) {
  // Requests should not block on the disk!  On POSIX this goes to disk.
  // http://code.google.com/p/chromium/issues/detail?id=59849
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::string mime_type;
  net::GetMimeTypeFromExtension(extension, &mime_type);
  return mime_type;
}

void CefGetExtensionsForMimeType(const CefString& mime_type,
                                 std::vector<CefString>& extensions) {
  using VectorType = std::vector<base::FilePath::StringType>;
  VectorType ext;
  net::GetExtensionsForMimeType(mime_type, &ext);
  VectorType::const_iterator it = ext.begin();
  for (; it != ext.end(); ++it) {
    extensions.push_back(*it);
  }
}

CefString CefBase64Encode(const void* data, size_t data_size) {
  if (data_size == 0) {
    return CefString();
  }

  base::StringPiece input(static_cast<const char*>(data), data_size);
  return base::Base64Encode(input);
}

CefRefPtr<CefBinaryValue> CefBase64Decode(const CefString& data) {
  if (data.size() == 0) {
    return nullptr;
  }

  const std::string& input = data;
  std::string output;
  if (base::Base64Decode(input, &output)) {
    return CefBinaryValue::Create(output.data(), output.size());
  }
  return nullptr;
}

CefString CefURIEncode(const CefString& text, bool use_plus) {
  return base::EscapeQueryParamValue(text.ToString(), use_plus);
}

CefString CefURIDecode(const CefString& text,
                       bool convert_to_utf8,
                       cef_uri_unescape_rule_t unescape_rule) {
  const base::UnescapeRule::Type type =
      static_cast<base::UnescapeRule::Type>(unescape_rule);
  if (convert_to_utf8) {
    return base::UnescapeAndDecodeUTF8URLComponentWithAdjustments(
        text.ToString(), type, nullptr);
  } else {
    return base::UnescapeURLComponent(text.ToString(), type);
  }
}
