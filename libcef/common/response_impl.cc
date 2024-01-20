// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/response_impl.h"

#include <string>

#include "libcef/common/net/http_header_utils.h"
#include "libcef/common/net_service/net_service_util.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"

#define CHECK_READONLY_RETURN_VOID()        \
  if (read_only_) {                         \
    DCHECK(false) << "object is read only"; \
    return;                                 \
  }

// CefResponse ----------------------------------------------------------------

// static
CefRefPtr<CefResponse> CefResponse::Create() {
  CefRefPtr<CefResponse> response(new CefResponseImpl());
  return response;
}

// CefResponseImpl ------------------------------------------------------------

CefResponseImpl::CefResponseImpl() = default;

bool CefResponseImpl::IsReadOnly() {
  base::AutoLock lock_scope(lock_);
  return read_only_;
}

cef_errorcode_t CefResponseImpl::GetError() {
  base::AutoLock lock_scope(lock_);
  return error_code_;
}

void CefResponseImpl::SetError(cef_errorcode_t error) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  error_code_ = error;
}

int CefResponseImpl::GetStatus() {
  base::AutoLock lock_scope(lock_);
  return status_code_;
}

void CefResponseImpl::SetStatus(int status) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  status_code_ = status;
}

CefString CefResponseImpl::GetStatusText() {
  base::AutoLock lock_scope(lock_);
  return status_text_;
}

void CefResponseImpl::SetStatusText(const CefString& statusText) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  status_text_ = statusText;
}

CefString CefResponseImpl::GetMimeType() {
  base::AutoLock lock_scope(lock_);
  return mime_type_;
}

void CefResponseImpl::SetMimeType(const CefString& mimeType) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  mime_type_ = mimeType;
}

CefString CefResponseImpl::GetCharset() {
  base::AutoLock lock_scope(lock_);
  return charset_;
}

void CefResponseImpl::SetCharset(const CefString& charset) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  charset_ = charset;
}

CefString CefResponseImpl::GetHeaderByName(const CefString& name) {
  base::AutoLock lock_scope(lock_);

  std::string nameLower = name;
  HttpHeaderUtils::MakeASCIILower(&nameLower);

  auto it = HttpHeaderUtils::FindHeaderInMap(nameLower, header_map_);
  if (it != header_map_.end()) {
    return it->second;
  }

  return CefString();
}

void CefResponseImpl::SetHeaderByName(const CefString& name,
                                      const CefString& value,
                                      bool overwrite) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  std::string nameLower = name;
  HttpHeaderUtils::MakeASCIILower(&nameLower);

  // There may be multiple values, so remove any first.
  for (auto it = header_map_.begin(); it != header_map_.end();) {
    if (base::EqualsCaseInsensitiveASCII(it->first.ToString(), nameLower)) {
      if (!overwrite) {
        return;
      }
      it = header_map_.erase(it);
    } else {
      ++it;
    }
  }

  header_map_.insert(std::make_pair(name, value));
}

CefString CefResponseImpl::GetURL() {
  base::AutoLock lock_scope(lock_);
  return url_;
}

void CefResponseImpl::SetURL(const CefString& url) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  url_ = url;
}

void CefResponseImpl::GetHeaderMap(HeaderMap& map) {
  base::AutoLock lock_scope(lock_);
  map = header_map_;
}

void CefResponseImpl::SetHeaderMap(const HeaderMap& headerMap) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  header_map_ = headerMap;
}

scoped_refptr<net::HttpResponseHeaders> CefResponseImpl::GetResponseHeaders() {
  base::AutoLock lock_scope(lock_);

  std::string mime_type = mime_type_;
  if (mime_type.empty()) {
    mime_type = "text/html";
  }

  std::multimap<std::string, std::string> extra_headers;
  for (const auto& pair : header_map_) {
    extra_headers.insert(std::make_pair(pair.first, pair.second));
  }

  return net_service::MakeResponseHeaders(
      status_code_, status_text_, mime_type, charset_, -1, extra_headers,
      true /* allow_existing_header_override */);
}

void CefResponseImpl::SetResponseHeaders(
    const net::HttpResponseHeaders& headers) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  header_map_.clear();

  size_t iter = 0;
  std::string name, value;
  while (headers.EnumerateHeaderLines(&iter, &name, &value)) {
    header_map_.insert(std::make_pair(name, value));
  }

  status_code_ = headers.response_code();
  status_text_ = headers.GetStatusText();

  if (headers.IsRedirect(nullptr)) {
    // Don't report Content-Type header values for redirects.
    mime_type_.clear();
    charset_.clear();
  } else {
    std::string mime_type, charset;
    headers.GetMimeTypeAndCharset(&mime_type, &charset);
    mime_type_ = mime_type;
    charset_ = charset;
  }
}

void CefResponseImpl::Set(const blink::WebURLResponse& response) {
  DCHECK(!response.IsNull());

  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  blink::WebString str;
  status_code_ = response.HttpStatusCode();
  str = response.HttpStatusText();
  status_text_ = str.Utf16();
  str = response.MimeType();
  mime_type_ = str.Utf16();
  str = response.CurrentRequestUrl().GetString();
  url_ = str.Utf16();

  class HeaderVisitor : public blink::WebHTTPHeaderVisitor {
   public:
    explicit HeaderVisitor(HeaderMap* map) : map_(map) {}

    void VisitHeader(const blink::WebString& name,
                     const blink::WebString& value) override {
      map_->insert(std::make_pair(name.Utf16(), value.Utf16()));
    }

   private:
    HeaderMap* map_;
  };

  HeaderVisitor visitor(&header_map_);
  response.VisitHttpHeaderFields(&visitor);
}

void CefResponseImpl::SetReadOnly(bool read_only) {
  base::AutoLock lock_scope(lock_);
  read_only_ = read_only;
}
