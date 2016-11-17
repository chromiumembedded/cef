// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/resource_util.h"

#include "include/base/cef_logging.h"
#include "include/cef_stream.h"
#include "include/wrapper/cef_byte_read_handler.h"
#include "include/wrapper/cef_stream_resource_handler.h"

namespace client {

namespace {

bool LoadBinaryResource(int binaryId, DWORD &dwSize, LPBYTE &pBytes) {
  HINSTANCE hInst = GetModuleHandle(NULL);
  HRSRC hRes = FindResource(hInst, MAKEINTRESOURCE(binaryId),
                            MAKEINTRESOURCE(256));
  if (hRes) {
    HGLOBAL hGlob = LoadResource(hInst, hRes);
    if (hGlob) {
      dwSize = SizeofResource(hInst, hRes);
      pBytes = (LPBYTE)LockResource(hGlob);
      if (dwSize > 0 && pBytes)
        return true;
    }
  }

  return false;
}

// Provider of binary resources.
class BinaryResourceProvider : public CefResourceManager::Provider {
 public:
  explicit BinaryResourceProvider(const std::string& url_path)
    : url_path_(url_path) {
    DCHECK(!url_path.empty());
  }

  bool OnRequest(scoped_refptr<CefResourceManager::Request> request) OVERRIDE {
    CEF_REQUIRE_IO_THREAD();

    const std::string& url = request->url();
    if (url.find(url_path_) != 0L) {
      // Not handled by this provider.
      return false;
    }

    CefRefPtr<CefResourceHandler> handler;

    const std::string& relative_path = url.substr(url_path_.length());
    if (!relative_path.empty()) {
      CefRefPtr<CefStreamReader> stream =
          GetBinaryResourceReader(relative_path.data());
      if (stream.get()) {
        handler = new CefStreamResourceHandler(
            request->mime_type_resolver().Run(url),
            stream);
      }
    }

    request->Continue(handler);
    return true;
  }

 private:
  std::string url_path_;

  DISALLOW_COPY_AND_ASSIGN(BinaryResourceProvider);
};

}  // namespace

// Implemented in resource_util_win_idmap.cc.
extern int GetResourceId(const char* resource_name);

bool LoadBinaryResource(const char* resource_name, std::string& resource_data) {
  int resource_id = GetResourceId(resource_name);
  if (resource_id == 0)
    return false;

  DWORD dwSize;
  LPBYTE pBytes;

  if (LoadBinaryResource(resource_id, dwSize, pBytes)) {
    resource_data = std::string(reinterpret_cast<char*>(pBytes), dwSize);
    return true;
  }

  NOTREACHED();  // The resource should be found.
  return false;
}

CefRefPtr<CefStreamReader> GetBinaryResourceReader(const char* resource_name) {
  int resource_id = GetResourceId(resource_name);
  if (resource_id == 0)
    return NULL;

  DWORD dwSize;
  LPBYTE pBytes;

  if (LoadBinaryResource(resource_id, dwSize, pBytes)) {
    return CefStreamReader::CreateForHandler(
        new CefByteReadHandler(pBytes, dwSize, NULL));
  }

  NOTREACHED();  // The resource should be found.
  return NULL;
}

CefResourceManager::Provider* CreateBinaryResourceProvider(
    const std::string& url_path) {
  return new BinaryResourceProvider(url_path);
}

}  // namespace client
