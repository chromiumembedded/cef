// Copyright (c) 2014 the Chromium Embedded Framework authors.
// Portions Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/utility/content_utility_client.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/utility/utility_message_handler.h"
#include "components/printing/service/public/cpp/pdf_compositor_service_factory.h"
#include "components/printing/service/public/interfaces/pdf_compositor.mojom.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/proxy/mojo_proxy_resolver_factory_impl.h"
#include "services/service_manager/public/cpp/binder_registry.h"

#if defined(OS_WIN)
#include "chrome/utility/printing_handler.h"
#endif

namespace {

void CreateProxyResolverFactory(
    net::interfaces::ProxyResolverFactoryRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<net::MojoProxyResolverFactoryImpl>(),
                          std::move(request));
}

}  // namespace

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

  registry->AddInterface<net::interfaces::ProxyResolverFactory>(
      base::Bind(CreateProxyResolverFactory),
      base::ThreadTaskRunnerHandle::Get());

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
}
