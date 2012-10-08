// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/internal_scheme_handler.h"
#include <string>
#include "base/string_util.h"
#include "content/public/common/content_client.h"
#include "ui/base/resource/resource_bundle.h"

namespace scheme {

namespace {

static std::string GetMimeType(const std::string& filename) {
  if (EndsWith(filename, ".html", false)) {
    return "text/html";
  } else if (EndsWith(filename, ".css", false)) {
    return "text/css";
  } else if (EndsWith(filename, ".jpg", false)) {
    return "image/jpeg";
  } else if (EndsWith(filename, ".js", false)) {
    return "application/javascript";
  } else if (EndsWith(filename, ".png", false)) {
    return "image/png";
  } else if (EndsWith(filename, ".gif", false)) {
    return "image/gif";
  }
  NOTREACHED() << "No known mime type for file: " << filename.c_str();
  return "text/plain";
}

class RedirectHandler : public CefResourceHandler {
 public:
  explicit RedirectHandler(const GURL& url)
      : url_(url) {
  }

  virtual bool ProcessRequest(CefRefPtr<CefRequest> request,
                              CefRefPtr<CefCallback> callback) OVERRIDE {
    callback->Continue();
    return true;
  }

  virtual void GetResponseHeaders(CefRefPtr<CefResponse> response,
                                  int64& response_length,
                                  CefString& redirectUrl) OVERRIDE {
    response_length = 0;
    redirectUrl = url_.spec();
  }

  virtual bool ReadResponse(void* data_out,
                            int bytes_to_read,
                            int& bytes_read,
                            CefRefPtr<CefCallback> callback) OVERRIDE {
    return false;
  }

  virtual void Cancel() OVERRIDE {
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

  virtual bool ProcessRequest(CefRefPtr<CefRequest> request,
                              CefRefPtr<CefCallback> callback) OVERRIDE {
    callback->Continue();
    return true;
  }

  virtual void GetResponseHeaders(CefRefPtr<CefResponse> response,
                                  int64& response_length,
                                  CefString& redirectUrl) OVERRIDE {
    response_length = size_;

    response->SetMimeType(mime_type_);
    response->SetStatus(200);
  }

  virtual bool ReadResponse(void* data_out,
                            int bytes_to_read,
                            int& bytes_read,
                            CefRefPtr<CefCallback> callback) OVERRIDE {
    bytes_read = reader_->Read(data_out, 1, bytes_to_read);
    return (bytes_read > 0);
  }

  virtual void Cancel() OVERRIDE {
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
      scoped_ptr<InternalHandlerDelegate> delegate)
      : delegate_(delegate.Pass()) {
  }

  virtual CefRefPtr<CefResourceHandler> Create(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      const CefString& scheme_name,
      CefRefPtr<CefRequest> request) OVERRIDE {
    GURL url = GURL(request->GetURL().ToString());

    InternalHandlerDelegate::Action action;
    if (delegate_->OnRequest(request, &action)) {
      if (!action.redirect_url.is_empty() && action.redirect_url.is_valid())
        return new RedirectHandler(action.redirect_url);

      if (action.mime_type.empty())
        action.mime_type = GetMimeType(url.path());

      if (action.resource_id >= 0) {
        base::StringPiece piece = content::GetContentClient()->GetDataResource(
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
  scoped_ptr<InternalHandlerDelegate> delegate_;

  IMPLEMENT_REFCOUNTING(InternalHandlerFactory);
};

}  // namespace

InternalHandlerDelegate::Action::Action()
    : stream_size(-1),
      resource_id(-1) {
}

CefRefPtr<CefSchemeHandlerFactory> CreateInternalHandlerFactory(
    scoped_ptr<InternalHandlerDelegate> delegate) {
  DCHECK(delegate.get());
  return new InternalHandlerFactory(delegate.Pass());
}

}  // namespace scheme
