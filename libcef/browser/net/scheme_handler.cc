// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/net/scheme_handler.h"

#include <string>

#include "libcef/browser/net/chrome_scheme_handler.h"
#include "libcef/browser/net/devtools_scheme_handler.h"
#include "libcef/common/net/scheme_registration.h"

#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "net/net_features.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/ftp_protocol_handler.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "url/url_constants.h"

namespace scheme {

void InstallInternalProtectedHandlers(
    net::URLRequestJobFactoryImpl* job_factory,
    CefURLRequestManager* request_manager,
    content::ProtocolHandlerMap* protocol_handlers,
    net::HostResolver* host_resolver) {
  protocol_handlers->insert(
      std::make_pair(url::kDataScheme,
          linked_ptr<net::URLRequestJobFactory::ProtocolHandler>(
              new net::DataProtocolHandler)));
  protocol_handlers->insert(
      std::make_pair(url::kFileScheme,
          linked_ptr<net::URLRequestJobFactory::ProtocolHandler>(
              new net::FileProtocolHandler(
                  content::BrowserThread::GetBlockingPool()->
                      GetTaskRunnerWithShutdownBehavior(
                          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)))));
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  protocol_handlers->insert(
      std::make_pair(url::kFtpScheme,
          linked_ptr<net::URLRequestJobFactory::ProtocolHandler>(
              net::FtpProtocolHandler::Create(host_resolver).release())));
#endif

  for (content::ProtocolHandlerMap::iterator it =
           protocol_handlers->begin();
       it != protocol_handlers->end();
       ++it) {
    const std::string& scheme = it->first;
    std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler> protocol_handler;

    if (scheme == content::kChromeDevToolsScheme) {
      // Don't use the default "chrome-devtools" handler.
      continue;
    } else if (scheme == content::kChromeUIScheme) {
      // Filter the URLs that are passed to the default "chrome" handler so as
      // not to interfere with CEF's "chrome" handler.
      protocol_handler.reset(
          scheme::WrapChromeProtocolHandler(
              request_manager,
              base::WrapUnique(it->second.release())).release());
    } else {
      protocol_handler.reset(it->second.release());
    }

    // Make sure IsInternalProtectedScheme() stays synchronized with what
    // Chromium is actually giving us.
    DCHECK(IsInternalProtectedScheme(scheme));

    bool set_protocol = job_factory->SetProtocolHandler(
        scheme, base::WrapUnique(protocol_handler.release()));
    DCHECK(set_protocol);
  }
}

void RegisterInternalHandlers(CefURLRequestManager* request_manager) {
  scheme::RegisterChromeHandler(request_manager);
  scheme::RegisterChromeDevToolsHandler(request_manager);
}

void DidFinishLoad(CefRefPtr<CefFrame> frame, const GURL& validated_url) {
  if (validated_url.scheme() == content::kChromeUIScheme)
    scheme::DidFinishChromeLoad(frame, validated_url);
}

}  // namespace scheme
