// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/wrapper/cef_stream_resource_handler.h"

#include <algorithm>

#include "include/base/cef_logging.h"
#include "include/cef_task.h"
#include "include/wrapper/cef_helpers.h"

CefStreamResourceHandler::CefStreamResourceHandler(
    const CefString& mime_type,
    CefRefPtr<CefStreamReader> stream)
    : status_code_(200),
      status_text_("OK"),
      mime_type_(mime_type),
      stream_(stream) {
  DCHECK(!mime_type_.empty());
}

CefStreamResourceHandler::CefStreamResourceHandler(
    int status_code,
    const CefString& status_text,
    const CefString& mime_type,
    CefResponse::HeaderMap header_map,
    CefRefPtr<CefStreamReader> stream)
    : status_code_(status_code),
      status_text_(status_text),
      mime_type_(mime_type),
      header_map_(header_map),
      stream_(stream) {
  DCHECK(!mime_type_.empty());
}

bool CefStreamResourceHandler::Open(CefRefPtr<CefRequest> request,
                                    bool& handle_request,
                                    CefRefPtr<CefCallback> callback) {
  DCHECK(!CefCurrentlyOn(TID_UI) && !CefCurrentlyOn(TID_IO));

  // Continue the request immediately.
  handle_request = true;
  return true;
}

void CefStreamResourceHandler::GetResponseHeaders(
    CefRefPtr<CefResponse> response,
    int64& response_length,
    CefString& redirectUrl) {
  CEF_REQUIRE_IO_THREAD();

  response->SetStatus(status_code_);
  response->SetStatusText(status_text_);
  response->SetMimeType(mime_type_);

  if (!header_map_.empty())
    response->SetHeaderMap(header_map_);

  response_length = stream_ ? -1 : 0;
}

bool CefStreamResourceHandler::Read(
    void* data_out,
    int bytes_to_read,
    int& bytes_read,
    CefRefPtr<CefResourceReadCallback> callback) {
  DCHECK(!CefCurrentlyOn(TID_UI) && !CefCurrentlyOn(TID_IO));
  DCHECK_GT(bytes_to_read, 0);
  DCHECK(stream_);

  // Read until the buffer is full or until Read() returns 0 to indicate no
  // more data.
  bytes_read = 0;
  int read = 0;
  do {
    read = static_cast<int>(
        stream_->Read(static_cast<char*>(data_out) + bytes_read, 1,
                      bytes_to_read - bytes_read));
    bytes_read += read;
  } while (read != 0 && bytes_read < bytes_to_read);

  return (bytes_read > 0);
}

void CefStreamResourceHandler::Cancel() {}
