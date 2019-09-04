// Copyright (c) 2019 The Chromium Embedded Framework Authors. Portions
// Copyright (c) 2018 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_PROXY_URL_LOADER_FACTORY_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_PROXY_URL_LOADER_FACTORY_H_

#include "libcef/browser/net_service/stream_reader_url_loader.h"

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/hash/hash.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_request_info.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace net_service {

class InterceptedRequest;
class ResourceContextData;

// Implement this interface to to evaluate requests. All methods are called on
// the IO thread, and all callbacks must be executed on the IO thread.
class InterceptedRequestHandler {
 public:
  InterceptedRequestHandler();
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
  virtual void OnBeforeRequest(const RequestId& id,
                               network::ResourceRequest* request,
                               bool request_was_redirected,
                               OnBeforeRequestResultCallback callback,
                               CancelRequestCallback cancel_callback);

  // Optionally modify |request| and execute |callback| after determining if the
  // request hould be intercepted.
  using ShouldInterceptRequestResultCallback =
      base::OnceCallback<void(std::unique_ptr<ResourceResponse>)>;
  virtual void ShouldInterceptRequest(
      const RequestId& id,
      network::ResourceRequest* request,
      ShouldInterceptRequestResultCallback callback);

  // Called to evaluate and optionally modify request headers. |request| is the
  // current request information. |redirect_url| will be non-empty if this
  // method is called in response to a redirect. The |modified_headers| and
  // |removed_headers| may be modified. If non-empty the |modified_headers|
  // values will be merged first, and then any |removed_headers| values will be
  // removed. This comparison is case sensitive.
  virtual void ProcessRequestHeaders(
      const RequestId& id,
      const network::ResourceRequest& request,
      const GURL& redirect_url,
      net::HttpRequestHeaders* modified_headers,
      std::vector<std::string>* removed_headers) {}

  // Called to evaluate and optionally modify response headers. |request| is the
  // current request information. |redirect_url| will be non-empty if this
  // method is called in response to a redirect. Even though |head| is const the
  // |head.headers| value is non-const and any changes will be passed to the
  // client.
  virtual void ProcessResponseHeaders(
      const RequestId& id,
      const network::ResourceRequest& request,
      const GURL& redirect_url,
      const network::ResourceResponseHead& head) {}

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
      const RequestId& id,
      network::ResourceRequest* request,
      const network::ResourceResponseHead& head,
      base::Optional<net::RedirectInfo> redirect_info,
      OnRequestResponseResultCallback callback);

  // Called to optionally filter the response body.
  virtual mojo::ScopedDataPipeConsumerHandle OnFilterResponseBody(
      const RequestId& id,
      const network::ResourceRequest& request,
      mojo::ScopedDataPipeConsumerHandle body);

  // Called on completion notification from the loader (successful or not).
  virtual void OnRequestComplete(
      const RequestId& id,
      const network::ResourceRequest& request,
      const network::URLLoaderCompletionStatus& status) {}

  // Called on error.
  virtual void OnRequestError(const RequestId& id,
                              const network::ResourceRequest& request,
                              int error_code,
                              bool safebrowsing_hit) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InterceptedRequestHandler);
};

// URL Loader Factory that supports request/response interception, processing
// and callback invocation.
// Based on android_webview/browser/network_service/
// aw_proxying_url_loader_factory.cc
class ProxyURLLoaderFactory
    : public network::mojom::URLLoaderFactory,
      public network::mojom::TrustedURLLoaderHeaderClient {
 public:
  ~ProxyURLLoaderFactory() override;

  // Create a proxy object on the UI thread.
  static void CreateProxy(
      content::BrowserContext* browser_context,
      network::mojom::URLLoaderFactoryRequest loader_request,
      network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
      network::mojom::TrustedURLLoaderHeaderClientPtrInfo* header_client,
      std::unique_ptr<InterceptedRequestHandler> request_handler);

  // Create a proxy object on the IO thread.
  static ProxyURLLoaderFactory* CreateProxy(
      content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
      network::mojom::URLLoaderFactoryRequest loader_request,
      std::unique_ptr<InterceptedRequestHandler> request_handler);

  // mojom::URLLoaderFactory methods:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest loader_request) override;

  // network::mojom::TrustedURLLoaderHeaderClient:
  void OnLoaderCreated(
      int32_t request_id,
      network::mojom::TrustedHeaderClientRequest request) override;

 private:
  friend class InterceptedRequest;
  friend class ResourceContextData;

  ProxyURLLoaderFactory(
      network::mojom::URLLoaderFactoryRequest loader_request,
      network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
      network::mojom::TrustedURLLoaderHeaderClientRequest header_client_request,
      std::unique_ptr<InterceptedRequestHandler> request_handler);

  static void CreateOnIOThread(
      network::mojom::URLLoaderFactoryRequest loader_request,
      network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
      network::mojom::TrustedURLLoaderHeaderClientRequest header_client_request,
      content::ResourceContext* resource_context,
      std::unique_ptr<InterceptedRequestHandler> request_handler);

  using DisconnectCallback = base::OnceCallback<void(ProxyURLLoaderFactory*)>;
  void SetDisconnectCallback(DisconnectCallback on_disconnect);

  void OnTargetFactoryError();
  void OnProxyBindingError();
  void RemoveRequest(InterceptedRequest* request);
  void MaybeDestroySelf();

  mojo::BindingSet<network::mojom::URLLoaderFactory> proxy_bindings_;
  network::mojom::URLLoaderFactoryPtr target_factory_;
  mojo::Binding<network::mojom::TrustedURLLoaderHeaderClient>
      url_loader_header_client_binding_;

  std::unique_ptr<InterceptedRequestHandler> request_handler_;

  bool destroyed_ = false;
  DisconnectCallback on_disconnect_;

  // Map of request ID to request object.
  std::map<int32_t, std::unique_ptr<InterceptedRequest>> requests_;

  base::WeakPtrFactory<ProxyURLLoaderFactory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyURLLoaderFactory);
};

}  // namespace net_service

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_PROXY_URL_LOADER_FACTORY_H_
