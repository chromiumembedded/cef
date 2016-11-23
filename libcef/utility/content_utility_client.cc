// Copyright (c) 2014 the Chromium Embedded Framework authors.
// Portions Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/utility/content_utility_client.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/utility/utility_message_handler.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/proxy/mojo_proxy_resolver_factory_impl.h"
#include "services/service_manager/public/cpp/interface_registry.h"

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
  handlers_.push_back(new printing::PrintingHandler());
#endif
}

CefContentUtilityClient::~CefContentUtilityClient() {
}

bool CefContentUtilityClient::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = false;

  for (Handlers::iterator it = handlers_.begin();
       !handled && it != handlers_.end(); ++it) {
    handled = (*it)->OnMessageReceived(message);
  }

  return handled;
}

void CefContentUtilityClient::ExposeInterfacesToBrowser(
    service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<net::interfaces::ProxyResolverFactory>(
      base::Bind(CreateProxyResolverFactory));
}
