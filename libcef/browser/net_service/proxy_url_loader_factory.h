// Copyright (c) 2019 The Chromium Embedded Framework Authors. Portions
// Copyright (c) 2018 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_PROXY_URL_LOADER_FACTORY_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_PROXY_URL_LOADER_FACTORY_H_

#include "libcef/browser/net_service/stream_reader_url_loader.h"

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/strings/string_piece.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class ResourceContext;
}

namespace net_service {

class InterceptedRequest;
class ResourceContextData;

// Implement this interface to to evaluate requests. All methods are called on
// the IO thread, and all callbacks must be executed on the IO thread.
class InterceptedRequestHandler {
 public:
  InterceptedRequestHandler();

  InterceptedRequestHandler(const InterceptedRequestHandler&) = delete;
  InterceptedRequestHandler& operator=(const InterceptedRequestHandler&) =
      delete;

  virtual ~InterceptedRequestHandler();

  // Optionally modify |request| and execute |callback| to continue the request.
  // Set |intercept_request| to false if the request will not be intercepted.
  // Set |intercept_only| to true if the loader should not proceed unless the
  // request is intercepted. Keep a reference to |cancel_callback| and execute
  // at any time to cancel the request.
  using OnBeforeRequestResultCallback =
      base::OnceCallback<void(bool /* intercept_request */,
                              bool /* intercept_only */)>;
  using CancelRequestCallback = base::OnceCallback<void(int /* error_code */)>;
  virtual void OnBeforeRequest(int32_t request_id,
                               network::ResourceRequest* request,
                               bool request_was_redirected,
                               OnBeforeRequestResultCallback callback,
                               CancelRequestCallback cancel_callback);

  // Optionally modify |request| and execute |callback| after determining if the
  // request hould be intercepted.
  using ShouldInterceptRequestResultCallback =
      base::OnceCallback<void(std::unique_ptr<ResourceResponse>)>;
  virtual void ShouldInterceptRequest(
      int32_t request_id,
      network::ResourceRequest* request,
      ShouldInterceptRequestResultCallback callback);

  // Called to evaluate and optionally modify request headers. |request| is the
  // current request information. |redirect_url| will be non-empty if this
  // method is called in response to a redirect. The |modified_headers| and
  // |removed_headers| may be modified. If non-empty the |modified_headers|
  // values will be merged first, and then any |removed_headers| values will be
  // removed. This comparison is case sensitive.
  virtual void ProcessRequestHeaders(
      int32_t request_id,
      const network::ResourceRequest& request,
      const GURL& redirect_url,
      net::HttpRequestHeaders* modified_headers,
      std::vector<std::string>* removed_headers) {}

  // Called to evaluate and optionally modify response headers. |request| is the
  // current request information. |redirect_url| will be non-empty if this
  // method is called in response to a redirect. Even though |head| is const the
  // |head.headers| value is non-const and any changes will be passed to the
  // client.
  virtual void ProcessResponseHeaders(int32_t request_id,
                                      const network::ResourceRequest& request,
                                      const GURL& redirect_url,
                                      net::HttpResponseHeaders* headers) {}

  enum class ResponseMode {
    // Continue the request.
    CONTINUE,
    // Restart the request.
    RESTART,
    // Cancel the request.
    CANCEL
  };

  // Called on response. |request| is the current request information.
  // |redirect_info| will be non-empty for redirect responses.
  // Optionally modify |request| and execute the callback as appropriate.
  using OnRequestResponseResultCallback = base::OnceCallback<void(
      ResponseMode /* response_mode */,
      scoped_refptr<net::HttpResponseHeaders> /* override_headers */,
      const GURL& /* redirect_url */)>;
  virtual void OnRequestResponse(
      int32_t request_id,
      network::ResourceRequest* request,
      net::HttpResponseHeaders* headers,
      absl::optional<net::RedirectInfo> redirect_info,
      OnRequestResponseResultCallback callback);

  // Called to optionally filter the response body.
  virtual mojo::ScopedDataPipeConsumerHandle OnFilterResponseBody(
      int32_t request_id,
      const network::ResourceRequest& request,
      mojo::ScopedDataPipeConsumerHandle body);

  // Called on completion notification from the loader (successful or not).
  virtual void OnRequestComplete(
      int32_t request_id,
      const network::ResourceRequest& request,
      const network::URLLoaderCompletionStatus& status) {}

  // Called on error.
  virtual void OnRequestError(int32_t request_id,
                              const network::ResourceRequest& request,
                              int error_code,
                              bool safebrowsing_hit) {}
};

// URL Loader Factory that supports request/response interception, processing
// and callback invocation.
// Based on android_webview/browser/network_service/
// aw_proxying_url_loader_factory.cc
class ProxyURLLoaderFactory
    : public network::mojom::URLLoaderFactory,
      public network::mojom::TrustedURLLoaderHeaderClient {
 public:
  ProxyURLLoaderFactory(const ProxyURLLoaderFactory&) = delete;
  ProxyURLLoaderFactory& operator=(const ProxyURLLoaderFactory&) = delete;

  ~ProxyURLLoaderFactory() override;

  // Create a proxy object on the UI thread.
  static void CreateProxy(
      content::BrowserContext* browser_context,
      network::URLLoaderFactoryBuilder& factory_builder,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      std::unique_ptr<InterceptedRequestHandler> request_handler);

  // Create a proxy object on the IO thread.
  static void CreateProxy(
      content::WebContents::Getter web_contents_getter,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_request,
      std::unique_ptr<InterceptedRequestHandler> request_handler);

  // mojom::URLLoaderFactory methods:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory) override;

  // network::mojom::TrustedURLLoaderHeaderClient:
  void OnLoaderCreated(
      int32_t request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override;
  void OnLoaderForCorsPreflightCreated(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override;

 private:
  friend class InterceptedRequest;
  friend class ResourceContextData;

  ProxyURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          target_factory_remote,
      mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
          header_client_receiver,
      std::unique_ptr<InterceptedRequestHandler> request_handler);

  static void CreateOnIOThread(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          target_factory_remote,
      mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
          header_client_receiver,
      content::ResourceContext* resource_context,
      std::unique_ptr<InterceptedRequestHandler> request_handler);

  using DisconnectCallback = base::OnceCallback<void(ProxyURLLoaderFactory*)>;
  void SetDisconnectCallback(DisconnectCallback on_disconnect);

  void OnTargetFactoryError();
  void OnProxyBindingError();
  void RemoveRequest(InterceptedRequest* request);
  void MaybeDestroySelf();

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> proxy_receivers_;
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  mojo::Receiver<network::mojom::TrustedURLLoaderHeaderClient>
      url_loader_header_client_receiver_{this};

  std::unique_ptr<InterceptedRequestHandler> request_handler_;

  bool destroyed_ = false;
  DisconnectCallback on_disconnect_;

  // Map of request ID to request object.
  std::map<int32_t, std::unique_ptr<InterceptedRequest>> requests_;

  base::WeakPtrFactory<ProxyURLLoaderFactory> weak_factory_;
};

}  // namespace net_service

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_PROXY_URL_LOADER_FACTORY_H_
