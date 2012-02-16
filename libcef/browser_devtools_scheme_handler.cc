// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser_devtools_scheme_handler.h"

#include <string>

#include "include/cef_browser.h"
#include "include/cef_request.h"
#include "include/cef_response.h"
#include "include/cef_scheme.h"
#include "include/cef_stream.h"
#include "include/cef_url.h"
#include "libcef/browser_webkit_glue.h"
#include "libcef/cef_context.h"

#include "base/file_util.h"
#include "base/string_util.h"
#include "grit/devtools_resources_map.h"
#include "net/base/mime_util.h"
#include "ui/base/resource/resource_bundle.h"

const char kChromeDevToolsScheme[] = "chrome-devtools";
const char kChromeDevToolsHost[] = "devtools";
const char kChromeDevToolsURL[] = "chrome-devtools://devtools/";

namespace {

class DevToolsSchemeHandler : public CefSchemeHandler {
 public:
  DevToolsSchemeHandler(const std::string& path,
                        CefRefPtr<CefStreamReader> reader,
                        int size)
    : path_(path), reader_(reader), size_(size) {
  }

  virtual bool ProcessRequest(CefRefPtr<CefRequest> request,
                              CefRefPtr<CefSchemeHandlerCallback> callback)
                              OVERRIDE {
    callback->HeadersAvailable();
    return true;
  }

  virtual void GetResponseHeaders(CefRefPtr<CefResponse> response,
                                  int64& response_length,
                                  CefString& redirectUrl) OVERRIDE {
    response_length = size_;

    std::string mime_type = "text/plain";
    if (net::GetMimeTypeFromFile(FilePath(CefString(path_)), &mime_type))
      response->SetMimeType(mime_type);

    response->SetStatus(200);
  }

  virtual bool ReadResponse(void* data_out,
                            int bytes_to_read,
                            int& bytes_read,
                            CefRefPtr<CefSchemeHandlerCallback> callback)
                            OVERRIDE {
    bytes_read = reader_->Read(data_out, 1, bytes_to_read);
    return (bytes_read > 0);
  }

  virtual void Cancel() OVERRIDE {
  }

 private:
  std::string path_;
  CefRefPtr<CefStreamReader> reader_;
  int size_;

  IMPLEMENT_REFCOUNTING(DevToolSSchemeHandler);
};

class DevToolsSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  DevToolsSchemeHandlerFactory() {}

  virtual CefRefPtr<CefSchemeHandler> Create(CefRefPtr<CefBrowser> browser,
                                             const CefString& scheme_name,
                                             CefRefPtr<CefRequest> request)
                                             OVERRIDE {
    // Remove the query component of the URL, if any.
    CefURLParts parts;
    CefParseURL(request->GetURL(), parts);
    cef_string_clear(&parts.spec);
    cef_string_clear(&parts.query);
    CefString urlStr;
    CefCreateURL(parts, urlStr);

    std::string url = urlStr;
    const char* path = &url.c_str()[strlen(kChromeDevToolsURL)];

    int size = -1;
    CefRefPtr<CefStreamReader> reader = GetStreamReader(path, size);
    if (!reader.get())
      return NULL;

    return new DevToolsSchemeHandler(path, reader, size);
  }

  CefRefPtr<CefStreamReader> GetStreamReader(const char* path, int& size) {
    // Create a stream for the grit resource.
    for (size_t i = 0; i < kDevtoolsResourcesSize; ++i) {
      if (base::strcasecmp(kDevtoolsResources[i].name, path) == 0) {
        base::StringPiece piece =
            _Context->GetDataResource(kDevtoolsResources[i].value);
        if (!piece.empty()) {
          size = piece.size();
          return CefStreamReader::CreateForData(const_cast<char*>(piece.data()),
                                                size);
        }
      }
    }

    NOTREACHED() << "Missing DevTools resource: " << path;
    return NULL;
  }

  IMPLEMENT_REFCOUNTING(DevToolSSchemeHandlerFactory);
};

}  // namespace

// Register the DevTools scheme handler.
void RegisterDevToolsSchemeHandler(bool firstTime) {
  if (firstTime)
    CefRegisterCustomScheme(kChromeDevToolsScheme, true, false, true);
  CefRegisterSchemeHandlerFactory(kChromeDevToolsScheme, kChromeDevToolsHost,
                                  new DevToolsSchemeHandlerFactory());
}
