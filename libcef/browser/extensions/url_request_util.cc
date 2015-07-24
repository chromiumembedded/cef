// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/url_request_util.h"

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/file_util.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_simple_job.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;
using content::ResourceType;
using extensions::ExtensionsBrowserClient;

namespace {

// A request for an extension resource in a Chrome .pak file. These are used
// by component extensions.
class URLRequestResourceBundleJob : public net::URLRequestSimpleJob {
 public:
  URLRequestResourceBundleJob(net::URLRequest* request,
                              net::NetworkDelegate* network_delegate,
                              const base::FilePath& filename,
                              int resource_id,
                              const std::string& content_security_policy,
                              bool send_cors_header)
      : net::URLRequestSimpleJob(request, network_delegate),
        filename_(filename),
        resource_id_(resource_id),
        weak_factory_(this) {
    // Leave cache headers out of resource bundle requests.
    response_info_.headers = extensions::BuildHttpHeaders(
        content_security_policy, send_cors_header, base::Time());
  }

  // Overridden from URLRequestSimpleJob:
  int GetRefCountedData(
      std::string* mime_type,
      std::string* charset,
      scoped_refptr<base::RefCountedMemory>* data,
      const net::CompletionCallback& callback) const override {
    const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    *data = rb.LoadDataResourceBytes(resource_id_);

    // Add the Content-Length header now that we know the resource length.
    response_info_.headers->AddHeader(
        base::StringPrintf("%s: %s", net::HttpRequestHeaders::kContentLength,
                           base::UintToString((*data)->size()).c_str()));

    std::string* read_mime_type = new std::string;
    bool posted = base::PostTaskAndReplyWithResult(
        BrowserThread::GetBlockingPool(), FROM_HERE,
        base::Bind(&net::GetMimeTypeFromFile, filename_,
                   base::Unretained(read_mime_type)),
        base::Bind(&URLRequestResourceBundleJob::OnMimeTypeRead,
                   weak_factory_.GetWeakPtr(), mime_type, charset, *data,
                   base::Owned(read_mime_type), callback));
    DCHECK(posted);

    return net::ERR_IO_PENDING;
  }

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    *info = response_info_;
  }

 private:
  ~URLRequestResourceBundleJob() override {}

  void OnMimeTypeRead(std::string* out_mime_type,
                      std::string* charset,
                      scoped_refptr<base::RefCountedMemory> data,
                      std::string* read_mime_type,
                      const net::CompletionCallback& callback,
                      bool read_result) {
    *out_mime_type = *read_mime_type;
    if (base::StartsWith(*read_mime_type, "text/",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      // All of our HTML files should be UTF-8 and for other resource types
      // (like images), charset doesn't matter.
      DCHECK(base::IsStringUTF8(base::StringPiece(
          reinterpret_cast<const char*>(data->front()), data->size())));
      *charset = "utf-8";
    }
    int result = read_result ? net::OK : net::ERR_INVALID_URL;
    callback.Run(result);
  }

  // We need the filename of the resource to determine the mime type.
  base::FilePath filename_;

  // The resource bundle id to load.
  int resource_id_;

  net::HttpResponseInfo response_info_;

  mutable base::WeakPtrFactory<URLRequestResourceBundleJob> weak_factory_;
};

}  // namespace

namespace extensions {
namespace url_request_util {

net::URLRequestJob* MaybeCreateURLRequestResourceBundleJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& directory_path,
    const std::string& content_security_policy,
    bool send_cors_header) {
  base::FilePath resources_path;
  base::FilePath relative_path;
  // Try to load extension resources from chrome resource file if
  // directory_path is a descendant of resources_path. This path structure is
  // set up by CefExtensionSystem::ComponentExtensionInfo.
  if (PathService::Get(chrome::DIR_RESOURCES, &resources_path) &&
    // Since component extension resources are included in
    // component_extension_resources.pak file in resources_path, calculate
    // extension relative path against resources_path.
    resources_path.AppendRelativePath(directory_path, &relative_path)) {
    base::FilePath request_path =
        extensions::file_util::ExtensionURLToRelativeFilePath(request->url());
    int resource_id = 0;
    if (ExtensionsBrowserClient::Get()
            ->GetComponentExtensionResourceManager()
            ->IsComponentExtensionResource(
                directory_path, request_path, &resource_id)) {
      relative_path = relative_path.Append(request_path);
      relative_path = relative_path.NormalizePathSeparators();
      return new URLRequestResourceBundleJob(request,
                                             network_delegate,
                                             relative_path,
                                             resource_id,
                                             content_security_policy,
                                             send_cors_header);
    }
  }
  return NULL;
}

}  // namespace url_request_util
}  // namespace extensions
