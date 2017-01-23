// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include <sstream>

#include "include/cef_parser.h"

#include "base/base64.h"
#include "base/threading/thread_restrictions.h"
#include "components/url_formatter/elide_url.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "url/gurl.h"

bool CefParseURL(const CefString& url,
                 CefURLParts& parts) {
  GURL gurl(url.ToString());
  if (!gurl.is_valid())
    return false;

  CefString(&parts.spec).FromString(gurl.spec());
  CefString(&parts.scheme).FromString(gurl.scheme());
  CefString(&parts.username).FromString(gurl.username());
  CefString(&parts.password).FromString(gurl.password());
  CefString(&parts.host).FromString(gurl.host());
  CefString(&parts.origin).FromString(gurl.GetOrigin().spec());
  CefString(&parts.port).FromString(gurl.port());
  CefString(&parts.path).FromString(gurl.path());
  CefString(&parts.query).FromString(gurl.query());

  return true;
}

bool CefCreateURL(const CefURLParts& parts,
                  CefString& url) {
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

  GURL gurl;
  if (!spec.empty()) {
    gurl = GURL(spec);
  } else if (!scheme.empty() && !host.empty()) {
    std::stringstream ss;
    ss << scheme << "://";
    if (!username.empty()) {
      ss << username;
      if (!password.empty())
        ss << ":" << password;
      ss << "@";
    }
    ss << host;
    if (!port.empty())
      ss << ":" << port;
    if (!path.empty())
      ss << path;
    if (!query.empty())
      ss << "?" << query;
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
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  std::string mime_type;
  net::GetMimeTypeFromExtension(extension, &mime_type);
  return mime_type;
}

void CefGetExtensionsForMimeType(const CefString& mime_type,
                                 std::vector<CefString>& extensions) {
  typedef std::vector<base::FilePath::StringType> VectorType;
  VectorType ext;
  net::GetExtensionsForMimeType(mime_type, &ext);
  VectorType::const_iterator it = ext.begin();
  for (; it != ext.end(); ++it)
    extensions.push_back(*it);
}

CefString CefBase64Encode(const void* data, size_t data_size) {
  if (data_size == 0)
    return CefString();

  base::StringPiece input;
  input.set(static_cast<const char*>(data), data_size);
  std::string output;
  base::Base64Encode(input, &output);
  return output;
}

CefRefPtr<CefBinaryValue> CefBase64Decode(const CefString& data) {
  if (data.size() == 0)
    return NULL;

  const std::string& input = data;
  std::string output;
  if (base::Base64Decode(input, &output))
    return CefBinaryValue::Create(output.data(), output.size());
  return NULL;
}

CefString CefURIEncode(const CefString& text, bool use_plus) {
  return net::EscapeQueryParamValue(text.ToString(), use_plus);
}

CefString CefURIDecode(const CefString& text,
                       bool convert_to_utf8,
                       cef_uri_unescape_rule_t unescape_rule) {
  const net::UnescapeRule::Type type =
      static_cast<net::UnescapeRule::Type>(unescape_rule);
  if (convert_to_utf8)
    return net::UnescapeAndDecodeUTF8URLComponent(text.ToString(), type);
  else
    return net::UnescapeURLComponent(text.ToString(), type);
}
