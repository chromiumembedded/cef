// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/response_impl.h"

#include <string>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"


#define CHECK_READONLY_RETURN_VOID() \
  if (read_only_) { \
    NOTREACHED() << "object is read only"; \
    return; \
  }


// CefResponse ----------------------------------------------------------------

// static
CefRefPtr<CefResponse> CefResponse::Create() {
  CefRefPtr<CefResponse> response(new CefResponseImpl());
  return response;
}


// CefResponseImpl ------------------------------------------------------------

CefResponseImpl::CefResponseImpl()
  : error_code_(ERR_NONE),
    status_code_(0),
    read_only_(false) {
}

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

CefString CefResponseImpl::GetHeader(const CefString& name) {
  base::AutoLock lock_scope(lock_);

  CefString value;

  HeaderMap::const_iterator it = header_map_.find(name);
  if (it != header_map_.end())
    value = it->second;

  return value;
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

net::HttpResponseHeaders* CefResponseImpl::GetResponseHeaders() {
  base::AutoLock lock_scope(lock_);

  std::string response;
  std::string status_text;
  bool has_content_type_header = false;

  if (!status_text_.empty())
    status_text = status_text_;
  else
    status_text = (status_code_ == 200)?"OK":"ERROR";

  base::SStringPrintf(&response, "HTTP/1.1 %d %s", status_code_,
                      status_text.c_str());
  if (header_map_.size() > 0) {
    for (HeaderMap::const_iterator header = header_map_.begin();
        header != header_map_.end();
        ++header) {
      const CefString& key = header->first;
      const CefString& value = header->second;

      if (!key.empty()) {
        // Delimit with "\0" as required by net::HttpResponseHeaders.
        std::string key_str(key);
        std::string value_str(value);
        base::StringAppendF(&response, "%c%s: %s", '\0', key_str.c_str(),
                            value_str.c_str());

        if (!has_content_type_header &&
            key_str == net::HttpRequestHeaders::kContentType) {
          has_content_type_header = true;
        }
      }
    }
  }

  if (!has_content_type_header) {
    std::string mime_type;
    if (!mime_type_.empty())
      mime_type = mime_type_;
    else
      mime_type = "text/html";

    base::StringAppendF(&response, "%c%s: %s", '\0',
        net::HttpRequestHeaders::kContentType, mime_type.c_str());
  }

  return new net::HttpResponseHeaders(response);
}

void CefResponseImpl::SetResponseHeaders(
    const net::HttpResponseHeaders& headers) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  header_map_.empty();

  size_t iter = 0;
  std::string name, value;
  while (headers.EnumerateHeaderLines(&iter, &name, &value))
    header_map_.insert(std::make_pair(name, value));

  status_code_ = headers.response_code();
  status_text_ = headers.GetStatusText();

  std::string mime_type;
  if (headers.GetMimeType(&mime_type))
    mime_type_ = mime_type;
  else
    mime_type_.clear();
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
  response.VisitHTTPHeaderFields(&visitor);
}

void CefResponseImpl::Set(const net::URLRequest* request) {
  DCHECK(request);

  const net::HttpResponseHeaders* headers = request->response_headers();
  if (headers)
    SetResponseHeaders(*headers);
}

void CefResponseImpl::SetReadOnly(bool read_only) {
  base::AutoLock lock_scope(lock_);
  read_only_ = read_only;
}
