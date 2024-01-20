// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/net/internal_scheme_handler.h"

#include <string>
#include <utility>

#include "libcef/common/app_manager.h"

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/mime_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace scheme {

namespace {

base::FilePath FilePathFromASCII(const std::string& str) {
#if BUILDFLAG(IS_WIN)
  return base::FilePath(base::ASCIIToWide(str));
#else
  return base::FilePath(str);
#endif
}

std::string GetMimeType(const std::string& filename) {
  // Requests should not block on the disk!  On POSIX this goes to disk.
  // http://code.google.com/p/chromium/issues/detail?id=59849
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::string mime_type;
  const base::FilePath& file_path = FilePathFromASCII(filename);
  if (net::GetMimeTypeFromFile(file_path, &mime_type)) {
    return mime_type;
  }

  // Check for newer extensions used by internal resources but not yet
  // recognized by the mime type detector.
  const std::string& extension = CefString(file_path.FinalExtension());
  if (extension == ".md") {
    return "text/markdown";
  }
  if (extension == ".woff2") {
    return "application/font-woff2";
  }

  DCHECK(false) << "No known mime type for file: " << filename.c_str();
  return "text/plain";
}

class RedirectHandler : public CefResourceHandler {
 public:
  explicit RedirectHandler(const GURL& url) : url_(url) {}

  RedirectHandler(const RedirectHandler&) = delete;
  RedirectHandler& operator=(const RedirectHandler&) = delete;

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    // Continue immediately.
    handle_request = true;
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override {
    response_length = 0;
    redirectUrl = url_.spec();
  }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    DCHECK(false);
    return false;
  }

  void Cancel() override {}

 private:
  GURL url_;

  IMPLEMENT_REFCOUNTING(RedirectHandler);
};

class InternalHandler : public CefResourceHandler {
 public:
  InternalHandler(const std::string& mime_type,
                  CefRefPtr<CefStreamReader> reader,
                  int size)
      : mime_type_(mime_type), reader_(reader), size_(size) {}

  InternalHandler(const InternalHandler&) = delete;
  InternalHandler& operator=(const InternalHandler&) = delete;

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    // Continue immediately.
    handle_request = true;
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override {
    response_length = size_;

    response->SetMimeType(mime_type_);
    response->SetStatus(200);
  }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    // Read until the buffer is full or until Read() returns 0 to indicate no
    // more data.
    bytes_read = 0;
    int read = 0;
    do {
      read = static_cast<int>(
          reader_->Read(static_cast<char*>(data_out) + bytes_read, 1,
                        bytes_to_read - bytes_read));
      bytes_read += read;
    } while (read != 0 && bytes_read < bytes_to_read);

    return (bytes_read > 0);
  }

  void Cancel() override {}

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
      : delegate_(std::move(delegate)) {}

  InternalHandlerFactory(const InternalHandlerFactory&) = delete;
  InternalHandlerFactory& operator=(const InternalHandlerFactory&) = delete;

  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override {
    GURL url = GURL(request->GetURL().ToString());

    InternalHandlerDelegate::Action action;
    if (delegate_->OnRequest(browser, request, &action)) {
      if (!action.redirect_url.is_empty() && action.redirect_url.is_valid()) {
        return new RedirectHandler(action.redirect_url);
      }

      if (action.mime_type.empty()) {
        action.mime_type = GetMimeType(url.path());
      }

      if (!action.bytes && action.resource_id >= 0) {
        std::string str =
            ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                action.resource_id);
        if (str.empty()) {
          DCHECK(false) << "Failed to load internal resource for id: "
                        << action.resource_id << " URL: " << url.spec().c_str();
          return nullptr;
        }
        action.bytes =
            base::MakeRefCounted<base::RefCountedString>(std::move(str));
      }

      if (action.bytes) {
        action.stream = CefStreamReader::CreateForData(
            const_cast<unsigned char*>(action.bytes->data()),
            action.bytes->size());
        action.stream_size = action.bytes->size();
      }

      if (action.stream.get()) {
        return new InternalHandler(action.mime_type, action.stream,
                                   action.stream_size);
      }
    }

    return nullptr;
  }

 private:
  std::unique_ptr<InternalHandlerDelegate> delegate_;

  IMPLEMENT_REFCOUNTING(InternalHandlerFactory);
};

}  // namespace

InternalHandlerDelegate::Action::Action() = default;

CefRefPtr<CefSchemeHandlerFactory> CreateInternalHandlerFactory(
    std::unique_ptr<InternalHandlerDelegate> delegate) {
  DCHECK(delegate.get());
  return new InternalHandlerFactory(std::move(delegate));
}

}  // namespace scheme
