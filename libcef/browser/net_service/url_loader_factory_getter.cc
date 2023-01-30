// Copyright (c) 2019 The Chromium Embedded Framework Authors. Portions
// Copyright (c) 2018 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "libcef/browser/net_service/url_loader_factory_getter.h"

#include "libcef/browser/thread_util.h"
#include "libcef/common/app_manager.h"

#include "base/task/single_thread_task_runner.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"

namespace net_service {

// Based on CreatePendingSharedURLLoaderFactory from
// content/browser/download/download_manager_impl.cc.
// static
scoped_refptr<URLLoaderFactoryGetter> URLLoaderFactoryGetter::Create(
    content::RenderFrameHost* render_frame_host,
    content::BrowserContext* browser_context) {
  CEF_REQUIRE_UIT();
  DCHECK(browser_context);

  // Call this early because newly created BrowserContexts may need to
  // initialize additional state, and that should be done on the UI thread
  // instead of potentially racing with the WillCreateURLLoaderFactory
  // implementation.
  auto loader_factory = browser_context->GetDefaultStoragePartition()
                            ->GetURLLoaderFactoryForBrowserProcess();

  mojo::PendingRemote<network::mojom::URLLoaderFactory> proxy_factory_remote;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      proxy_factory_receiver;

  // Create an intermediate pipe that can be used to proxy the request's
  // URLLoaderFactory.
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      maybe_proxy_factory_remote;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      maybe_proxy_factory_receiver =
          maybe_proxy_factory_remote.InitWithNewPipeAndPassReceiver();

  bool should_proxy = false;
  int render_process_id = -1;

  if (render_frame_host) {
    render_process_id = render_frame_host->GetProcess()->GetID();

    // Allow DevTools to potentially inject itself into the proxy pipe.
    should_proxy =
        content::devtools_instrumentation::WillCreateURLLoaderFactory(
            static_cast<content::RenderFrameHostImpl*>(render_frame_host),
            false /* is_navigation */, false /* is_download */,
            &maybe_proxy_factory_receiver, nullptr /* factory_override */);
  }

  auto browser_client = CefAppManager::Get()->GetContentClient()->browser();

  // Allow the Content embedder to inject itself if it wants to.
  should_proxy |= browser_client->WillCreateURLLoaderFactory(
      browser_context, render_frame_host, render_process_id,
      content::ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      url::Origin(), absl::nullopt /* navigation_id */, ukm::SourceIdObj(),
      &maybe_proxy_factory_receiver, nullptr /* header_client */,
      nullptr /* bypass_redirect_checks */, nullptr /* disable_secure_dns */,
      nullptr /* factory_override */);

  // If anyone above indicated that they care about proxying, pass the
  // intermediate pipe along to the URLLoaderFactoryGetter.
  if (should_proxy) {
    proxy_factory_remote = std::move(maybe_proxy_factory_remote);
    proxy_factory_receiver = std::move(maybe_proxy_factory_receiver);
  }

  return base::WrapRefCounted(new URLLoaderFactoryGetter(
      loader_factory->Clone(), std::move(proxy_factory_remote),
      std::move(proxy_factory_receiver)));
}

// Based on CreateFactory from
// content/browser/download/network_download_pending_url_loader_factory.cc.
scoped_refptr<network::SharedURLLoaderFactory>
URLLoaderFactoryGetter::GetURLLoaderFactory() {
  // On first call we associate with the current thread.
  if (!task_runner_) {
    task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  } else {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
  }

  if (lazy_factory_) {
    return lazy_factory_;
  }

  // Bind on the current thread.
  auto loader_factory =
      network::SharedURLLoaderFactory::Create(std::move(loader_factory_info_));

  if (proxy_factory_receiver_.is_valid()) {
    loader_factory->Clone(std::move(proxy_factory_receiver_));
    lazy_factory_ =
        base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(proxy_factory_remote_));
  } else {
    lazy_factory_ = loader_factory;
  }
  return lazy_factory_;
}

URLLoaderFactoryGetter::URLLoaderFactoryGetter(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> loader_factory_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> proxy_factory_remote,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        proxy_factory_receiver)
    : loader_factory_info_(std::move(loader_factory_info)),
      proxy_factory_remote_(std::move(proxy_factory_remote)),
      proxy_factory_receiver_(std::move(proxy_factory_receiver)) {}

URLLoaderFactoryGetter::~URLLoaderFactoryGetter() = default;

void URLLoaderFactoryGetter::DeleteOnCorrectThread() const {
  if (task_runner_ && !task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->DeleteSoon(FROM_HERE, this);
    return;
  }
  delete this;
}

}  // namespace net_service