// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/net/internal_scheme_handler.h"

#include <string>
#include <utility>

#include "libcef/common/content_client.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/mime_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace scheme {

namespace {

base::FilePath FilePathFromASCII(const std::string& str) {
#if defined(OS_WIN)
  return base::FilePath(base::ASCIIToUTF16(str));
#else
  return base::FilePath(str);
#endif
}

std::string GetMimeType(const std::string& filename) {
  // Requests should not block on the disk!  On POSIX this goes to disk.
  // http://code.google.com/p/chromium/issues/detail?id=59849
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  std::string mime_type;
  const base::FilePath& file_path = FilePathFromASCII(filename);
  if (net::GetMimeTypeFromFile(file_path, &mime_type))
    return mime_type;

  // Check for newer extensions used by internal resources but not yet
  // recognized by the mime type detector.
  const std::string& extension = CefString(file_path.FinalExtension());
  if (extension == ".woff2")
    return "application/font-woff2";

  NOTREACHED() << "No known mime type for file: " << filename.c_str();
  return "text/plain";
}

class RedirectHandler : public CefResourceHandler {
 public:
  explicit RedirectHandler(const GURL& url)
      : url_(url) {
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    callback->Continue();
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
    response_length = 0;
    redirectUrl = url_.spec();
  }

  bool ReadResponse(void* data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    return false;
  }

  void Cancel() override {
  }

 private:
  GURL url_;

  IMPLEMENT_REFCOUNTING(RedirectHandler);
};

class InternalHandler : public CefResourceHandler {
 public:
  InternalHandler(const std::string& mime_type,
                  CefRefPtr<CefStreamReader> reader,
                  int size)
      : mime_type_(mime_type),
        reader_(reader),
        size_(size) {
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    callback->Continue();
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
    response_length = size_;

    response->SetMimeType(mime_type_);
    response->SetStatus(200);
  }

  bool ReadResponse(void* data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    bytes_read = reader_->Read(data_out, 1, bytes_to_read);
    return (bytes_read > 0);
  }

  void Cancel() override {
  }

 private:
  std::string mime_type_;
  CefRefPtr<CefStreamReader> reader_;
  int size_;

  IMPLEMENT_REFCOUNTING(InternalHandler);
};

class InternalHandlerFactory : public CefSchemeHandlerFactory {
 public:
  explicit InternalHandlerFactory(
      std::unique_ptr<InternalHandlerDelegate> delegate)
      : delegate_(std::move(delegate)) {
  }

  CefRefPtr<CefResourceHandler> Create(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      const CefString& scheme_name,
      CefRefPtr<CefRequest> request) override {
    GURL url = GURL(request->GetURL().ToString());

    InternalHandlerDelegate::Action action;
    if (delegate_->OnRequest(browser, request, &action)) {
      if (!action.redirect_url.is_empty() && action.redirect_url.is_valid())
        return new RedirectHandler(action.redirect_url);

      if (action.mime_type.empty())
        action.mime_type = GetMimeType(url.path());

      if (action.resource_id >= 0) {
        base::StringPiece piece = CefContentClient::Get()->GetDataResource(
            action.resource_id, ui::SCALE_FACTOR_NONE);
        if (!piece.empty()) {
          action.stream =
              CefStreamReader::CreateForData(const_cast<char*>(piece.data()),
                                             piece.size());
          action.stream_size = piece.size();
        } else {
          NOTREACHED() << "Failed to load internal resource for id: " <<
              action.resource_id << " URL: " << url.spec().c_str();
          return NULL;
        }
      }

      if (action.stream.get()) {
        return new InternalHandler(action.mime_type, action.stream,
            action.stream_size);
      }
    }

    return NULL;
  }

 private:
  std::unique_ptr<InternalHandlerDelegate> delegate_;

  IMPLEMENT_REFCOUNTING(InternalHandlerFactory);
};

}  // namespace

InternalHandlerDelegate::Action::Action()
    : stream_size(-1),
      resource_id(-1) {
}

CefRefPtr<CefSchemeHandlerFactory> CreateInternalHandlerFactory(
    std::unique_ptr<InternalHandlerDelegate> delegate) {
  DCHECK(delegate.get());
  return new InternalHandlerFactory(std::move(delegate));
}

}  // namespace scheme
