// Copyright (c) 2014 the Chromium Embedded Framework authors.
// Portions Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/utility/content_utility_client.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/services/printing/printing_service.h"
#include "chrome/services/printing/public/mojom/constants.mojom.h"
#include "components/services/pdf_compositor/public/cpp/pdf_compositor_service_factory.h"
#include "components/services/pdf_compositor/public/interfaces/pdf_compositor.mojom.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/network/url_request_context_builder_mojo.h"
#include "services/proxy_resolver/proxy_resolver_service.h"  // nogncheck
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"  // nogncheck
#include "services/service_manager/public/cpp/binder_registry.h"

#if defined(OS_WIN)
#include "chrome/utility/printing_handler.h"
#endif

namespace {
void RunServiceAsyncThenTerminateProcess(
    std::unique_ptr<service_manager::Service> service) {
  service_manager::Service::RunAsyncUntilTermination(
      std::move(service),
      base::BindOnce([] { content::UtilityThread::Get()->ReleaseProcess(); }));
}

#if !defined(OS_ANDROID)
std::unique_ptr<service_manager::Service> CreateProxyResolverService(
    service_manager::mojom::ServiceRequest request) {
  return std::make_unique<proxy_resolver::ProxyResolverService>(
      std::move(request));
}
#endif

using ServiceFactory =
    base::OnceCallback<std::unique_ptr<service_manager::Service>()>;
void RunServiceOnIOThread(ServiceFactory factory) {
  base::OnceClosure terminate_process = base::BindOnce(
      base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
      base::SequencedTaskRunnerHandle::Get(), FROM_HERE,
      base::BindOnce([] { content::UtilityThread::Get()->ReleaseProcess(); }));
  content::ChildThread::Get()->GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ServiceFactory factory, base::OnceClosure terminate_process) {
            service_manager::Service::RunAsyncUntilTermination(
                std::move(factory).Run(), std::move(terminate_process));
          },
          std::move(factory), std::move(terminate_process)));
}

}  // namespace

CefContentUtilityClient::CefContentUtilityClient() {
#if defined(OS_WIN)
  printing_handler_ = std::make_unique<printing::PrintingHandler>();
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

  auto registry = std::make_unique<service_manager::BinderRegistry>();

  connection->AddConnectionFilter(
      std::make_unique<content::SimpleConnectionFilter>(std::move(registry)));
}

bool CefContentUtilityClient::OnMessageReceived(const IPC::Message& message) {
  bool handled = false;

#if defined(OS_WIN)
  if (printing_handler_->OnMessageReceived(message))
    return true;
#endif

  return handled;
}

std::unique_ptr<service_manager::Service>
CefContentUtilityClient::MaybeCreateMainThreadService(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  if (service_name == printing::mojom::kServiceName) {
    return printing::CreatePdfCompositorService(std::move(request));
  }
  if (service_name == printing::mojom::kChromePrintingServiceName) {
    return std::make_unique<printing::PrintingService>(std::move(request));
  }
  return nullptr;
}

bool CefContentUtilityClient::HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  if (service_name == proxy_resolver::mojom::kProxyResolverServiceName) {
    RunServiceOnIOThread(
        base::BindOnce(&CreateProxyResolverService, std::move(request)));
    return true;
  }
  auto service = MaybeCreateMainThreadService(service_name, std::move(request));
  if (service) {
    RunServiceAsyncThenTerminateProcess(std::move(service));
    return true;
  }
  return false;
}
