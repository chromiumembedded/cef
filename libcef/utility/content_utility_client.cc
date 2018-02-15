// Copyright (c) 2014 the Chromium Embedded Framework authors.
// Portions Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/utility/content_utility_client.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/services/printing/printing_service.h"
#include "chrome/services/printing/public/interfaces/constants.mojom.h"
#include "chrome/utility/utility_message_handler.h"
#include "components/printing/service/public/cpp/pdf_compositor_service_factory.h"
#include "components/printing/service/public/interfaces/pdf_compositor.mojom.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/network/url_request_context_builder_mojo.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/proxy_resolver/proxy_resolver_service.h"  // nogncheck
#include "services/proxy_resolver/public/interfaces/proxy_resolver.mojom.h"  // nogncheck
#include "services/service_manager/public/cpp/binder_registry.h"

#if defined(OS_WIN)
#include "chrome/utility/printing_handler.h"
#endif

CefContentUtilityClient::CefContentUtilityClient() {
#if defined(OS_WIN)
  handlers_.push_back(base::MakeUnique<printing::PrintingHandler>());
#endif
}

CefContentUtilityClient::~CefContentUtilityClient() {}

void CefContentUtilityClient::UtilityThreadStarted() {
  content::ServiceManagerConnection* connection =
      content::ChildThread::Get()->GetServiceManagerConnection();

  // NOTE: Some utility process instances are not connected to the Service
  // Manager. Nothing left to do in that case.
  if (!connection)
    return;

  auto registry = base::MakeUnique<service_manager::BinderRegistry>();

  connection->AddConnectionFilter(
      base::MakeUnique<content::SimpleConnectionFilter>(std::move(registry)));
}

bool CefContentUtilityClient::OnMessageReceived(const IPC::Message& message) {
  bool handled = false;

  for (Handlers::iterator it = handlers_.begin();
       !handled && it != handlers_.end(); ++it) {
    handled = (*it)->OnMessageReceived(message);
  }

  return handled;
}

void CefContentUtilityClient::RegisterServices(StaticServiceMap* services) {
  service_manager::EmbeddedServiceInfo pdf_compositor_info;
  pdf_compositor_info.factory =
      base::Bind(&printing::CreatePdfCompositorService, std::string());
  services->emplace(printing::mojom::kServiceName, pdf_compositor_info);

  {
    service_manager::EmbeddedServiceInfo printing_info;
    printing_info.factory =
        base::Bind(&printing::PrintingService::CreateService);
    services->emplace(printing::mojom::kChromePrintingServiceName,
                      printing_info);
  }

  service_manager::EmbeddedServiceInfo proxy_resolver_info;
  proxy_resolver_info.task_runner =
      content::ChildThread::Get()->GetIOTaskRunner();
  proxy_resolver_info.factory =
      base::Bind(&proxy_resolver::ProxyResolverService::CreateService);
  services->emplace(proxy_resolver::mojom::kProxyResolverServiceName,
                    proxy_resolver_info);
}
