// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/scheme_registration.h"
#include "libcef/browser/chrome_scheme_handler.h"
#include "libcef/browser/devtools_scheme_handler.h"
#include "libcef/renderer/content_renderer_client.h"

#include "content/public/common/url_constants.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace scheme {

void AddInternalStandardSchemes(std::vector<std::string>* standard_schemes) {
  static struct {
    const char* name;
    bool is_local;
    bool is_display_isolated;
  } schemes[] = {
    { chrome::kChromeUIScheme,        true,  true  },
    { chrome::kChromeDevToolsScheme,  true,  false }
  };

  for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); ++i)
    standard_schemes->push_back(schemes[i].name);

  if (CefContentRendererClient::Get()) {
    // Running in single-process mode. Register the schemes with WebKit.
    for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); ++i) {
      CefContentRendererClient::Get()->AddCustomScheme(
          schemes[i].name, schemes[i].is_local, schemes[i].is_display_isolated);
    }
  }
}

bool IsInternalProtectedScheme(const std::string& scheme) {
  // These values originate from StoragePartitionImplMap::Get() in
  // content/browser/storage_partition_impl_map.cc and are modified by
  // InstallInternalHandlers().
  static const char* schemes[] = {
    chrome::kBlobScheme,
    chrome::kChromeUIScheme,
    chrome::kFileSystemScheme,
  };

  for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); ++i) {
    if (scheme == schemes[i])
      return true;
  }

  return false;
}

void InstallInternalProtectedHandlers(
    net::URLRequestJobFactoryImpl* job_factory,
    content::ProtocolHandlerMap* protocol_handlers) {
  for (content::ProtocolHandlerMap::iterator it =
           protocol_handlers->begin();
       it != protocol_handlers->end();
       ++it) {
    const std::string& scheme = it->first;
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler> protocol_handler;

    if (scheme == chrome::kChromeDevToolsScheme) {
      // Don't use the default "chrome-devtools" handler.
      continue;
    } else if (scheme == chrome::kChromeUIScheme) {
      // Filter the URLs that are passed to the default "chrome" handler so as
      // not to interfere with CEF's "chrome" handler.
      protocol_handler.reset(
          scheme::WrapChromeProtocolHandler(
              make_scoped_ptr(it->second.release())).release());
    } else {
      protocol_handler.reset(it->second.release());
    }

    // Make sure IsInternalScheme() stays synchronized with what Chromium is
    // actually giving us.
    DCHECK(IsInternalProtectedScheme(scheme));

    bool set_protocol = job_factory->SetProtocolHandler(
        scheme, protocol_handler.release());
    DCHECK(set_protocol);
  }
}

void RegisterInternalHandlers() {
  scheme::RegisterChromeHandler();
  scheme::RegisterChromeDevToolsHandler();
}

void DidFinishLoad(CefRefPtr<CefFrame> frame, const GURL& validated_url) {
  if (validated_url.scheme() == chrome::kChromeUIScheme)
    scheme::DidFinishChromeLoad(frame, validated_url);
}

}  // namespace scheme
