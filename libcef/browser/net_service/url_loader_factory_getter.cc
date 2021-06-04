// Copyright (c) 2019 The Chromium Embedded Framework Authors. Portions
// Copyright (c) 2018 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "libcef/browser/net_service/url_loader_factory_getter.h"

#include "libcef/browser/thread_util.h"
#include "libcef/common/app_manager.h"

#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"

namespace net_service {

// Based on CreateDownloadURLLoaderFactoryGetter from
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

  network::mojom::URLLoaderFactoryPtrInfo proxy_factory_ptr_info;
  network::mojom::URLLoaderFactoryRequest proxy_factory_request;

  // Create an intermediate pipe that can be used to proxy the request's
  // URLLoaderFactory.
  network::mojom::URLLoaderFactoryPtrInfo maybe_proxy_factory_ptr_info;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      maybe_proxy_factory_request = MakeRequest(&maybe_proxy_factory_ptr_info);

  bool should_proxy = false;
  int render_process_id = -1;

  if (render_frame_host) {
    render_process_id = render_frame_host->GetProcess()->GetID();

    // Allow DevTools to potentially inject itself into the proxy pipe.
    should_proxy =
        content::devtools_instrumentation::WillCreateURLLoaderFactory(
            static_cast<content::RenderFrameHostImpl*>(render_frame_host),
            false /* is_navigation */, false /* is_download */,
            &maybe_proxy_factory_request, nullptr /* factory_override */);
  }

  auto browser_client = CefAppManager::Get()->GetContentClient()->browser();

  // Allow the Content embedder to inject itself if it wants to.
  should_proxy |= browser_client->WillCreateURLLoaderFactory(
      browser_context, render_frame_host, render_process_id,
      content::ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      url::Origin(), absl::nullopt /* navigation_id */, ukm::SourceIdObj(),
      &maybe_proxy_factory_request, nullptr /* header_client */,
      nullptr /* bypass_redirect_checks */, nullptr /* disable_secure_dns */,
      nullptr /* factory_override */);

  // If anyone above indicated that they care about proxying, pass the
  // intermediate pipe along to the URLLoaderFactoryGetter.
  if (should_proxy) {
    proxy_factory_ptr_info = std::move(maybe_proxy_factory_ptr_info);
    proxy_factory_request = std::move(maybe_proxy_factory_request);
  }

  return base::WrapRefCounted(new URLLoaderFactoryGetter(
      loader_factory->Clone(), std::move(proxy_factory_ptr_info),
      std::move(proxy_factory_request)));
}

// Based on NetworkDownloadURLLoaderFactoryGetter from
// content/browser/download/network_download_url_loader_factory_getter.cc.
scoped_refptr<network::SharedURLLoaderFactory>
URLLoaderFactoryGetter::GetURLLoaderFactory() {
  // On first call we associate with the current thread.
  if (!task_runner_) {
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
  } else {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
  }

  if (lazy_factory_)
    return lazy_factory_;

  // Bind on the current thread.
  auto loader_factory =
      network::SharedURLLoaderFactory::Create(std::move(loader_factory_info_));

  if (proxy_factory_request_.is_pending()) {
    loader_factory->Clone(std::move(proxy_factory_request_));
    lazy_factory_ =
        base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(proxy_factory_ptr_info_));
  } else {
    lazy_factory_ = loader_factory;
  }
  return lazy_factory_;
}

URLLoaderFactoryGetter::URLLoaderFactoryGetter(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> loader_factory_info,
    network::mojom::URLLoaderFactoryPtrInfo proxy_factory_ptr_info,
    network::mojom::URLLoaderFactoryRequest proxy_factory_request)
    : loader_factory_info_(std::move(loader_factory_info)),
      proxy_factory_ptr_info_(std::move(proxy_factory_ptr_info)),
      proxy_factory_request_(std::move(proxy_factory_request)) {}

URLLoaderFactoryGetter::~URLLoaderFactoryGetter() = default;

void URLLoaderFactoryGetter::DeleteOnCorrectThread() const {
  if (task_runner_ && !task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->DeleteSoon(FROM_HERE, this);
    return;
  }
  delete this;
}

}  // namespace net_service