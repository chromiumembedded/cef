// Copyright (c) 2019 The Chromium Embedded Framework Authors. Portions
// Copyright (c) 2018 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "libcef/browser/net_service/proxy_url_loader_factory.h"

#include <tuple>

#include "libcef/browser/context.h"
#include "libcef/browser/origin_whitelist_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/net/scheme_registration.h"
#include "libcef/common/net_service/net_service_util.h"

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/http/http_status_code.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"

namespace net_service {

namespace {

// User data key for ResourceContextData.
const void* const kResourceContextUserDataKey = &kResourceContextUserDataKey;

absl::optional<std::string> GetHeaderString(
    const net::HttpResponseHeaders* headers,
    const std::string& header_name) {
  std::string header_value;
  if (!headers || !headers->GetNormalizedHeader(header_name, &header_value)) {
    return absl::nullopt;
  }
  return header_value;
}

void CreateProxyHelper(
    content::WebContents::Getter web_contents_getter,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
    std::unique_ptr<InterceptedRequestHandler> request_handler) {
  ProxyURLLoaderFactory::CreateProxy(web_contents_getter,
                                     std::move(loader_receiver),
                                     std::move(request_handler));
}

bool DisableRequestHandlingForTesting() {
  static bool disabled([]() -> bool {
    return base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableRequestHandlingForTesting);
  }());
  return disabled;
}

// Match logic in devtools_url_loader_interceptor.cc
// InterceptionJob::CalculateResponseTainting.
network::mojom::FetchResponseType CalculateResponseTainting(
    bool should_check_cors,
    network::mojom::RequestMode mode,
    bool tainted_origin) {
  if (should_check_cors) {
    return network::mojom::FetchResponseType::kCors;
  }
  if (mode == network::mojom::RequestMode::kNoCors && tainted_origin) {
    return network::mojom::FetchResponseType::kOpaque;
  }
  return network::mojom::FetchResponseType::kBasic;
}

}  // namespace

// Owns all of the ProxyURLLoaderFactorys for a given BrowserContext. Since
// these live on the IO thread this is done indirectly through the
// ResourceContext.
class ResourceContextData : public base::SupportsUserData::Data {
 public:
  ResourceContextData(const ResourceContextData&) = delete;
  ResourceContextData& operator=(const ResourceContextData&) = delete;

  ~ResourceContextData() override = default;

  static void AddProxyOnUIThread(
      ProxyURLLoaderFactory* proxy,
      content::WebContents::Getter web_contents_getter) {
    CEF_REQUIRE_UIT();

    content::WebContents* web_contents = web_contents_getter.Run();

    // Maybe the browser was destroyed while AddProxyOnUIThread was pending.
    if (!web_contents) {
      // Delete on the IO thread as expected by mojo bindings.
      content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                         proxy);
      return;
    }

    content::BrowserContext* browser_context =
        web_contents->GetBrowserContext();
    DCHECK(browser_context);

    content::ResourceContext* resource_context =
        browser_context->GetResourceContext();
    DCHECK(resource_context);

    CEF_POST_TASK(CEF_IOT, base::BindOnce(ResourceContextData::AddProxy,
                                          base::Unretained(proxy),
                                          base::Unretained(resource_context)));
  }

  static void AddProxy(ProxyURLLoaderFactory* proxy,
                       content::ResourceContext* resource_context) {
    CEF_REQUIRE_IOT();

    // Maybe the proxy was destroyed while AddProxyOnUIThread was pending.
    if (proxy->destroyed_) {
      delete proxy;
      return;
    }

    auto* self = static_cast<ResourceContextData*>(
        resource_context->GetUserData(kResourceContextUserDataKey));
    if (!self) {
      self = new ResourceContextData();
      resource_context->SetUserData(kResourceContextUserDataKey,
                                    base::WrapUnique(self));
    }

    proxy->SetDisconnectCallback(base::BindOnce(
        &ResourceContextData::RemoveProxy, self->weak_factory_.GetWeakPtr()));
    self->proxies_.emplace(base::WrapUnique(proxy));
  }

 private:
  void RemoveProxy(ProxyURLLoaderFactory* proxy) {
    CEF_REQUIRE_IOT();

    auto it = proxies_.find(proxy);
    DCHECK(it != proxies_.end());
    proxies_.erase(it);
  }

  ResourceContextData() : weak_factory_(this) {}

  std::set<std::unique_ptr<ProxyURLLoaderFactory>, base::UniquePtrComparator>
      proxies_;

  base::WeakPtrFactory<ResourceContextData> weak_factory_;
};

// CORS preflight requests are handled in the network process, so we just need
// to continue all of the callbacks and then delete ourself.
class CorsPreflightRequest : public network::mojom::TrustedHeaderClient {
 public:
  explicit CorsPreflightRequest(
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      : weak_factory_(this) {
    header_client_receiver_.Bind(std::move(receiver));

    header_client_receiver_.set_disconnect_handler(base::BindOnce(
        &CorsPreflightRequest::OnDestroy, weak_factory_.GetWeakPtr()));
  }

  CorsPreflightRequest(const CorsPreflightRequest&) = delete;
  CorsPreflightRequest& operator=(const CorsPreflightRequest&) = delete;

  // mojom::TrustedHeaderClient methods:
  void OnBeforeSendHeaders(const net::HttpRequestHeaders& headers,
                           OnBeforeSendHeadersCallback callback) override {
    std::move(callback).Run(net::OK, headers);
  }

  void OnHeadersReceived(const std::string& headers,
                         const net::IPEndPoint& remote_endpoint,
                         OnHeadersReceivedCallback callback) override {
    std::move(callback).Run(net::OK, headers, /*redirect_url=*/GURL());
    OnDestroy();
  }

 private:
  void OnDestroy() { delete this; }

  mojo::Receiver<network::mojom::TrustedHeaderClient> header_client_receiver_{
      this};

  base::WeakPtrFactory<CorsPreflightRequest> weak_factory_;
};

//==============================
// InterceptedRequest
//=============================

// Handles intercepted, in-progress requests/responses, so that they can be
// controlled and modified accordingly.
class InterceptedRequest : public network::mojom::URLLoader,
                           public network::mojom::URLLoaderClient,
                           public network::mojom::TrustedHeaderClient {
 public:
  InterceptedRequest(
      ProxyURLLoaderFactory* factory,
      int32_t id,
      uint32_t options,
      const network::ResourceRequest& request,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory);

  InterceptedRequest(const InterceptedRequest&) = delete;
  InterceptedRequest& operator=(const InterceptedRequest&) = delete;

  ~InterceptedRequest() override;

  // Restart the request. This happens on initial start and after redirect.
  void Restart();

  // Called from ProxyURLLoaderFactory::OnLoaderCreated.
  void OnLoaderCreated(
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver);

  // Called from InterceptDelegate::OnInputStreamOpenFailed.
  void InputStreamFailed(bool restart_needed);

  // mojom::TrustedHeaderClient methods:
  void OnBeforeSendHeaders(const net::HttpRequestHeaders& headers,
                           OnBeforeSendHeadersCallback callback) override;
  void OnHeadersReceived(const std::string& headers,
                         const net::IPEndPoint& remote_endpoint,
                         OnHeadersReceivedCallback callback) override;

  // mojom::URLLoaderClient methods:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // mojom::URLLoader methods:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  int32_t id() const { return id_; }

 private:
  // Helpers for determining the request handler.
  void BeforeRequestReceived(const GURL& original_url,
                             bool intercept_request,
                             bool intercept_only);
  void InterceptResponseReceived(const GURL& original_url,
                                 std::unique_ptr<ResourceResponse> response);
  void ContinueAfterIntercept();
  void ContinueAfterInterceptWithOverride(
      std::unique_ptr<ResourceResponse> response);

  // Helpers for optionally overriding headers.
  void HandleResponseOrRedirectHeaders(
      absl::optional<net::RedirectInfo> redirect_info,
      net::CompletionOnceCallback continuation);
  void ContinueResponseOrRedirect(
      net::CompletionOnceCallback continuation,
      InterceptedRequestHandler::ResponseMode response_mode,
      scoped_refptr<net::HttpResponseHeaders> override_headers,
      const GURL& redirect_url);
  void ContinueToHandleOverrideHeaders(int error_code);
  net::RedirectInfo MakeRedirectResponseAndInfo(const GURL& new_location);

  // Helpers for redirect handling.
  void ContinueToBeforeRedirect(const net::RedirectInfo& redirect_info,
                                int error_code);

  // Helpers for response handling.
  void ContinueToResponseStarted(int error_code);

  void OnDestroy();

  void OnProcessRequestHeaders(const GURL& redirect_url,
                               net::HttpRequestHeaders* modified_headers,
                               std::vector<std::string>* removed_headers);

  // This is called when the original URLLoaderClient has a connection error.
  void OnURLLoaderClientError();

  // This is called when the original URLLoader has a connection error.
  void OnURLLoaderError(uint32_t custom_reason, const std::string& description);

  // Call OnComplete on |target_client_|. If |wait_for_loader_error| is true
  // then this object will wait for |proxied_loader_receiver_| to have a
  // connection error before destructing.
  void CallOnComplete(const network::URLLoaderCompletionStatus& status,
                      bool wait_for_loader_error);

  void SendErrorAndCompleteImmediately(int error_code);
  void SendErrorStatusAndCompleteImmediately(
      const network::URLLoaderCompletionStatus& status);

  void SendErrorCallback(int error_code, bool safebrowsing_hit);

  void OnUploadProgressACK();

  ProxyURLLoaderFactory* const factory_;
  const int32_t id_;
  const uint32_t options_;
  bool input_stream_previously_failed_ = false;
  bool request_was_redirected_ = false;
  int redirect_limit_ = net::URLRequest::kMaxRedirects;
  bool redirect_in_progress_ = false;

  // To avoid sending multiple OnReceivedError callbacks.
  bool sent_error_callback_ = false;

  // When true, the loader will provide the option to intercept the request.
  bool intercept_request_ = true;

  // When true, the loader will not proceed unless the intercept request
  // callback provided a non-null response.
  bool intercept_only_ = false;

  network::URLLoaderCompletionStatus status_;
  bool got_loader_error_ = false;

  // Used for rate limiting OnUploadProgress callbacks.
  bool waiting_for_upload_progress_ack_ = false;

  network::ResourceRequest request_;
  network::mojom::URLResponseHeadPtr current_response_;
  mojo::ScopedDataPipeConsumerHandle current_body_;
  absl::optional<mojo_base::BigBuffer> current_cached_metadata_;
  scoped_refptr<net::HttpResponseHeaders> current_headers_;
  scoped_refptr<net::HttpResponseHeaders> override_headers_;
  GURL original_url_;
  GURL redirect_url_;
  GURL header_client_redirect_url_;
  const net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  mojo::Receiver<network::mojom::URLLoader> proxied_loader_receiver_;
  mojo::Remote<network::mojom::URLLoaderClient> target_client_;

  mojo::Receiver<network::mojom::URLLoaderClient> proxied_client_receiver_{
      this};
  mojo::Remote<network::mojom::URLLoader> target_loader_;
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;

  bool current_request_uses_header_client_ = false;
  OnHeadersReceivedCallback on_headers_received_callback_;
  mojo::Receiver<network::mojom::TrustedHeaderClient> header_client_receiver_{
      this};

  StreamReaderURLLoader* stream_loader_ = nullptr;

  base::WeakPtrFactory<InterceptedRequest> weak_factory_;
};

class InterceptDelegate : public StreamReaderURLLoader::Delegate {
 public:
  explicit InterceptDelegate(std::unique_ptr<ResourceResponse> response,
                             base::WeakPtr<InterceptedRequest> request)
      : response_(std::move(response)), request_(request) {}

  bool OpenInputStream(int32_t request_id,
                       const network::ResourceRequest& request,
                       OpenCallback callback) override {
    return response_->OpenInputStream(request_id, request, std::move(callback));
  }

  void OnInputStreamOpenFailed(int32_t request_id, bool* restarted) override {
    request_->InputStreamFailed(false /* restart_needed */);
    *restarted = false;
  }

  void GetResponseHeaders(int32_t request_id,
                          int* status_code,
                          std::string* reason_phrase,
                          std::string* mime_type,
                          std::string* charset,
                          int64_t* content_length,
                          HeaderMap* extra_headers) override {
    response_->GetResponseHeaders(request_id, status_code, reason_phrase,
                                  mime_type, charset, content_length,
                                  extra_headers);
  }

 private:
  std::unique_ptr<ResourceResponse> response_;
  base::WeakPtr<InterceptedRequest> request_;
};

InterceptedRequest::InterceptedRequest(
    ProxyURLLoaderFactory* factory,
    int32_t id,
    uint32_t options,
    const network::ResourceRequest& request,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory)
    : factory_(factory),
      id_(id),
      options_(options),
      request_(request),
      traffic_annotation_(traffic_annotation),
      proxied_loader_receiver_(this, std::move(loader_receiver)),
      target_client_(std::move(client)),
      target_factory_(std::move(target_factory)),
      weak_factory_(this) {
  status_ = network::URLLoaderCompletionStatus(net::OK);

  net::HttpRequestHeaders modified_headers;
  std::vector<std::string> removed_headers;
  OnProcessRequestHeaders(GURL() /* redirect_url */, &modified_headers,
                          &removed_headers);

  // If there is a client error, clean up the request.
  target_client_.set_disconnect_handler(base::BindOnce(
      &InterceptedRequest::OnURLLoaderClientError, base::Unretained(this)));
  proxied_loader_receiver_.set_disconnect_with_reason_handler(base::BindOnce(
      &InterceptedRequest::OnURLLoaderError, base::Unretained(this)));
}

InterceptedRequest::~InterceptedRequest() {
  if (status_.error_code != net::OK) {
    SendErrorCallback(status_.error_code, false);
  }
  if (on_headers_received_callback_) {
    std::move(on_headers_received_callback_)
        .Run(net::ERR_ABORTED, absl::nullopt, GURL());
  }
}

void InterceptedRequest::Restart() {
  stream_loader_ = nullptr;
  if (proxied_client_receiver_.is_bound()) {
    proxied_client_receiver_.reset();
    target_loader_.reset();
  }

  if (header_client_receiver_.is_bound()) {
    std::ignore = header_client_receiver_.Unbind();
  }

  current_request_uses_header_client_ =
      factory_->url_loader_header_client_receiver_.is_bound();

  const bool is_cross_origin =
      request_.request_initiator &&
      !request_.request_initiator->IsSameOriginWith(request_.url);
  const bool is_cors_enabled_scheme =
      scheme::IsCorsEnabledScheme(request_.url.scheme());

  // Match logic in network::cors::ShouldCheckCors.
  bool should_check_cors =
      is_cross_origin &&
      request_.mode != network::mojom::RequestMode::kNavigate &&
      request_.mode != network::mojom::RequestMode::kNoCors;

  if (should_check_cors && !is_cors_enabled_scheme &&
      !HasCrossOriginWhitelistEntry(*request_.request_initiator,
                                    url::Origin::Create(request_.url))) {
    // Fail requests if a CORS check is required and the scheme is not CORS
    // enabled. This matches the error condition that would be generated by
    // CorsURLLoader::StartRequest in the network process.
    SendErrorStatusAndCompleteImmediately(
        network::URLLoaderCompletionStatus(network::CorsErrorStatus(
            network::mojom::CorsError::kCorsDisabledScheme)));
    return;
  }

  // Maybe update |credentials_mode| for fetch requests.
  if (request_.credentials_mode ==
      network::mojom::CredentialsMode::kSameOrigin) {
    // Match logic in devtools_url_loader_interceptor.cc
    // InterceptionJob::FollowRedirect.
    bool tainted_origin = false;
    if (redirect_in_progress_ && request_.request_initiator &&
        !url::IsSameOriginWith(request_.url, original_url_) &&
        !request_.request_initiator->IsSameOriginWith(original_url_)) {
      tainted_origin = true;
    }

    // Match logic in CorsURLLoader::StartNetworkRequest.
    const auto response_tainting = CalculateResponseTainting(
        should_check_cors, request_.mode, tainted_origin);
    request_.credentials_mode =
        network::cors::CalculateCredentialsFlag(request_.credentials_mode,
                                                response_tainting)
            ? network::mojom::CredentialsMode::kInclude
            : network::mojom::CredentialsMode::kOmit;
  }

  const bool should_add_origin_header =
      // Cross-origin requests that are not kNavigate nor kNoCors.
      should_check_cors ||
      // Same-origin requests except for GET and HEAD.
      (!is_cross_origin &&
       request_.method != net::HttpRequestHeaders::kGetMethod &&
       request_.method != net::HttpRequestHeaders::kHeadMethod);

  if (should_add_origin_header) {
    // Match logic in navigation_request.cc AddAdditionalRequestHeaders.
    url::Origin origin_header_value =
        request_.request_initiator.value_or(url::Origin());
    origin_header_value = content::Referrer::SanitizeOriginForRequest(
        request_.url, origin_header_value,
        blink::ReferrerUtils::NetToMojoReferrerPolicy(
            request_.referrer_policy));

    request_.headers.SetHeaderIfMissing(net::HttpRequestHeaders::kOrigin,
                                        origin_header_value.Serialize());
  }

  const GURL original_url = request_.url;

  factory_->request_handler_->OnBeforeRequest(
      id_, &request_, request_was_redirected_,
      base::BindOnce(&InterceptedRequest::BeforeRequestReceived,
                     weak_factory_.GetWeakPtr(), original_url),
      base::BindOnce(&InterceptedRequest::SendErrorAndCompleteImmediately,
                     weak_factory_.GetWeakPtr()));
}

void InterceptedRequest::OnLoaderCreated(
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  DCHECK(current_request_uses_header_client_);

  // Only called if we're using the default loader.
  header_client_receiver_.Bind(std::move(receiver));
}

void InterceptedRequest::InputStreamFailed(bool restart_needed) {
  DCHECK(!input_stream_previously_failed_);

  if (intercept_only_) {
    // This can happen for unsupported schemes, when no proper
    // response from the intercept handler is received, i.e.
    // the provided input stream in response failed to load. In
    // this case we send and error and stop loading.
    SendErrorAndCompleteImmediately(net::ERR_UNKNOWN_URL_SCHEME);
    return;
  }

  if (!restart_needed) {
    return;
  }

  input_stream_previously_failed_ = true;
  Restart();
}

// TrustedHeaderClient methods.

void InterceptedRequest::OnBeforeSendHeaders(
    const net::HttpRequestHeaders& headers,
    OnBeforeSendHeadersCallback callback) {
  if (!current_request_uses_header_client_) {
    std::move(callback).Run(net::OK, absl::nullopt);
    return;
  }

  request_.headers = headers;
  std::move(callback).Run(net::OK, absl::nullopt);

  // Resume handling of client messages after continuing from an async callback.
  if (proxied_client_receiver_.is_bound()) {
    proxied_client_receiver_.Resume();
  }
}

void InterceptedRequest::OnHeadersReceived(
    const std::string& headers,
    const net::IPEndPoint& remote_endpoint,
    OnHeadersReceivedCallback callback) {
  if (!current_request_uses_header_client_) {
    std::move(callback).Run(net::OK, absl::nullopt, GURL());
    return;
  }

  current_headers_ = base::MakeRefCounted<net::HttpResponseHeaders>(headers);
  on_headers_received_callback_ = std::move(callback);

  absl::optional<net::RedirectInfo> redirect_info;
  std::string location;
  if (current_headers_->IsRedirect(&location)) {
    const GURL new_url = request_.url.Resolve(location);
    redirect_info =
        MakeRedirectInfo(request_, current_headers_.get(), new_url, 0);
  }

  HandleResponseOrRedirectHeaders(
      redirect_info,
      base::BindOnce(&InterceptedRequest::ContinueToHandleOverrideHeaders,
                     weak_factory_.GetWeakPtr()));
}

// URLLoaderClient methods.

void InterceptedRequest::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  target_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void InterceptedRequest::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  current_response_ = std::move(head);
  current_body_ = std::move(body);
  current_cached_metadata_ = std::move(cached_metadata);

  // |current_headers_| may be null for cached responses where OnHeadersReceived
  // is not called.
  if (current_request_uses_header_client_ && current_headers_) {
    // Use the headers we got from OnHeadersReceived as that'll contain
    // Set-Cookie if it existed.
    current_response_->headers = current_headers_;
    current_headers_ = nullptr;
    ContinueToResponseStarted(net::OK);
  } else {
    HandleResponseOrRedirectHeaders(
        absl::nullopt,
        base::BindOnce(&InterceptedRequest::ContinueToResponseStarted,
                       weak_factory_.GetWeakPtr()));
  }
}

void InterceptedRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  // Whether to notify the client. True by default so that we always notify for
  // internal redirects that originate from the network process (for HSTS, etc).
  // False while a redirect is in-progress to avoid duplicate notifications.
  bool notify_client = !redirect_in_progress_;

  current_response_ = std::move(head);
  current_body_.reset();
  current_cached_metadata_.reset();

  // |current_headers_| may be null for synthetic redirects where
  // OnHeadersReceived is not called.
  if (current_request_uses_header_client_ && current_headers_) {
    // Use the headers we got from OnHeadersReceived as that'll contain
    // Set-Cookie if it existed.
    current_response_->headers = current_headers_;
    current_headers_ = nullptr;
  }

  if (--redirect_limit_ == 0) {
    SendErrorAndCompleteImmediately(net::ERR_TOO_MANY_REDIRECTS);
    return;
  }

  net::RedirectInfo new_redirect_info;

  // When we redirect via ContinueToHandleOverrideHeaders the |redirect_info|
  // value is sometimes nonsense (HTTP_OK). Also, we won't get another call to
  // OnHeadersReceived for the new URL so we need to notify the client here.
  if (header_client_redirect_url_.is_valid() &&
      redirect_info.status_code == net::HTTP_OK) {
    DCHECK(current_request_uses_header_client_);
    notify_client = true;
    new_redirect_info =
        MakeRedirectResponseAndInfo(header_client_redirect_url_);
  } else {
    new_redirect_info = redirect_info;
  }

  if (notify_client) {
    HandleResponseOrRedirectHeaders(
        new_redirect_info,
        base::BindOnce(&InterceptedRequest::ContinueToBeforeRedirect,
                       weak_factory_.GetWeakPtr(), new_redirect_info));
  } else {
    ContinueToBeforeRedirect(new_redirect_info, net::OK);
  }
}

void InterceptedRequest::OnUploadProgress(int64_t current_position,
                                          int64_t total_size,
                                          OnUploadProgressCallback callback) {
  // Implement our own rate limiting for OnUploadProgress calls.
  if (!waiting_for_upload_progress_ack_) {
    waiting_for_upload_progress_ack_ = true;
    target_client_->OnUploadProgress(
        current_position, total_size,
        base::BindOnce(&InterceptedRequest::OnUploadProgressACK,
                       weak_factory_.GetWeakPtr()));
  }

  // Always execute the callback immediately to avoid a race between
  // URLLoaderClient_OnUploadProgress_ProxyToResponder::Run() (which would
  // otherwise be blocked on the target client executing the callback) and
  // CallOnComplete(). If CallOnComplete() is executed first the interface pipe
  // will be closed and the callback destructor will generate an assertion like:
  // "URLLoaderClient::OnUploadProgressCallback was destroyed without first
  // either being run or its corresponding binding being closed. It is an error
  // to drop response callbacks which still correspond to an open interface
  // pipe."
  std::move(callback).Run();
}

void InterceptedRequest::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  target_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void InterceptedRequest::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  // Only wait for the original loader to possibly have a custom error if the
  // target loader exists and succeeded. If the target loader failed, then it
  // was a race as to whether that error or the safe browsing error would be
  // reported.
  CallOnComplete(status, !stream_loader_ && status.error_code == net::OK);
}

// URLLoader methods.

void InterceptedRequest::FollowRedirect(
    const std::vector<std::string>& removed_headers_ext,
    const net::HttpRequestHeaders& modified_headers_ext,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  std::vector<std::string> removed_headers = removed_headers_ext;
  net::HttpRequestHeaders modified_headers = modified_headers_ext;
  OnProcessRequestHeaders(new_url.value_or(GURL()), &modified_headers,
                          &removed_headers);

  // If |OnURLLoaderClientError| was called then we're just waiting for the
  // connection error handler of |proxied_loader_receiver_|. Don't restart the
  // job since that'll create another URLLoader.
  if (!target_client_) {
    return;
  }

  // Normally we would call FollowRedirect on the target loader and it would
  // begin loading the redirected request. However, the client might want to
  // intercept that request so restart the job instead.
  Restart();
}

void InterceptedRequest::SetPriority(net::RequestPriority priority,
                                     int32_t intra_priority_value) {
  if (target_loader_) {
    target_loader_->SetPriority(priority, intra_priority_value);
  }
}

void InterceptedRequest::PauseReadingBodyFromNet() {
  if (target_loader_) {
    target_loader_->PauseReadingBodyFromNet();
  }
}

void InterceptedRequest::ResumeReadingBodyFromNet() {
  if (target_loader_) {
    target_loader_->ResumeReadingBodyFromNet();
  }
}

// Helper methods.

void InterceptedRequest::BeforeRequestReceived(const GURL& original_url,
                                               bool intercept_request,
                                               bool intercept_only) {
  intercept_request_ = intercept_request;
  intercept_only_ = intercept_only;

  if (input_stream_previously_failed_ || !intercept_request_) {
    // Equivalent to no interception.
    InterceptResponseReceived(original_url, nullptr);
  } else {
    // TODO(network): Verify the case when WebContents::RenderFrameDeleted is
    // called before network request is intercepted (i.e. if that's possible
    // and whether it can result in any issues).
    factory_->request_handler_->ShouldInterceptRequest(
        id_, &request_,
        base::BindOnce(&InterceptedRequest::InterceptResponseReceived,
                       weak_factory_.GetWeakPtr(), original_url));
  }
}

void InterceptedRequest::InterceptResponseReceived(
    const GURL& original_url,
    std::unique_ptr<ResourceResponse> response) {
  if (request_.url != original_url) {
    // A response object shouldn't be created if we're redirecting.
    DCHECK(!response);

    // Perform the redirect.
    current_response_ = network::mojom::URLResponseHead::New();
    current_response_->request_start = base::TimeTicks::Now();
    current_response_->response_start = base::TimeTicks::Now();
    current_body_.reset();
    current_cached_metadata_.reset();

    auto headers = MakeResponseHeaders(
        net::HTTP_TEMPORARY_REDIRECT, std::string(), std::string(),
        std::string(), -1, {}, false /* allow_existing_header_override */);
    current_response_->headers = headers;

    current_response_->encoded_data_length = headers->raw_headers().length();
    current_response_->content_length = 0;
    // Avoid incorrect replacement of 0 with nullptr. NOLINTNEXTLINE
    current_response_->encoded_body_length = 0;

    std::string origin;
    if (request_.headers.GetHeader(net::HttpRequestHeaders::kOrigin, &origin) &&
        origin != url::Origin().Serialize()) {
      // Allow redirects of cross-origin resource loads.
      headers->AddHeader(network::cors::header_names::kAccessControlAllowOrigin,
                         origin);
    }

    if (request_.credentials_mode ==
        network::mojom::CredentialsMode::kInclude) {
      headers->AddHeader(
          network::cors::header_names::kAccessControlAllowCredentials, "true");
    }

    const net::RedirectInfo& redirect_info =
        MakeRedirectInfo(request_, headers.get(), request_.url, 0);
    HandleResponseOrRedirectHeaders(
        redirect_info,
        base::BindOnce(&InterceptedRequest::ContinueToBeforeRedirect,
                       weak_factory_.GetWeakPtr(), redirect_info));
    return;
  }

  if (response) {
    // Non-null response: make sure to use it as an override for the
    // normal network data.
    ContinueAfterInterceptWithOverride(std::move(response));
  } else {
    // Request was not intercepted/overridden. Proceed with loading
    // from network, unless this is a special |intercept_only_| loader,
    // which happens for external schemes (e.g. unsupported schemes).
    if (intercept_only_) {
      SendErrorAndCompleteImmediately(net::ERR_UNKNOWN_URL_SCHEME);
      return;
    }
    ContinueAfterIntercept();
  }
}

void InterceptedRequest::ContinueAfterIntercept() {
  if (!target_loader_ && target_factory_) {
    // Even if this request does not use the header client, future redirects
    // might, so we need to set the option on the loader.
    uint32_t options = options_ | network::mojom::kURLLoadOptionUseHeaderClient;
    target_factory_->CreateLoaderAndStart(
        target_loader_.BindNewPipeAndPassReceiver(), id_, options, request_,
        proxied_client_receiver_.BindNewPipeAndPassRemote(),
        traffic_annotation_);
  }
}

void InterceptedRequest::ContinueAfterInterceptWithOverride(
    std::unique_ptr<ResourceResponse> response) {
  // StreamReaderURLLoader will synthesize TrustedHeaderClient callbacks to
  // avoid having Set-Cookie headers stripped by the IPC layer.
  current_request_uses_header_client_ = true;

  stream_loader_ = new StreamReaderURLLoader(
      id_, request_, proxied_client_receiver_.BindNewPipeAndPassRemote(),
      header_client_receiver_.BindNewPipeAndPassRemote(), traffic_annotation_,
      std::move(current_cached_metadata_),
      std::make_unique<InterceptDelegate>(std::move(response),
                                          weak_factory_.GetWeakPtr()));
  stream_loader_->Start();
}

void InterceptedRequest::HandleResponseOrRedirectHeaders(
    absl::optional<net::RedirectInfo> redirect_info,
    net::CompletionOnceCallback continuation) {
  override_headers_ = nullptr;
  redirect_url_ = redirect_info.has_value() ? redirect_info->new_url : GURL();
  original_url_ = request_.url;

  if (!redirect_url_.is_empty()) {
    redirect_in_progress_ = true;
  }

  // |current_response_| may be nullptr when called from OnHeadersReceived.
  auto headers =
      current_response_ ? current_response_->headers : current_headers_;

  // Even though |head| is const we can get a non-const pointer to the headers
  // and modifications we make are passed to the target client.
  factory_->request_handler_->ProcessResponseHeaders(
      id_, request_, redirect_url_, headers.get());

  // Pause handling of client messages before waiting on an async callback.
  if (proxied_client_receiver_.is_bound()) {
    proxied_client_receiver_.Pause();
  }

  factory_->request_handler_->OnRequestResponse(
      id_, &request_, headers.get(), redirect_info,
      base::BindOnce(&InterceptedRequest::ContinueResponseOrRedirect,
                     weak_factory_.GetWeakPtr(), std::move(continuation)));
}

void InterceptedRequest::ContinueResponseOrRedirect(
    net::CompletionOnceCallback continuation,
    InterceptedRequestHandler::ResponseMode response_mode,
    scoped_refptr<net::HttpResponseHeaders> override_headers,
    const GURL& redirect_url) {
  if (response_mode == InterceptedRequestHandler::ResponseMode::CANCEL) {
    std::move(continuation).Run(net::ERR_ABORTED);
    return;
  } else if (response_mode ==
             InterceptedRequestHandler::ResponseMode::RESTART) {
    Restart();
    return;
  }

  override_headers_ = override_headers;
  if (override_headers_) {
    // Make sure to update current_response_, since when OnReceiveResponse
    // is called we will not use its headers as it might be missing the
    // Set-Cookie line (which gets stripped by the IPC layer).
    current_response_->headers = override_headers_;
  }
  redirect_url_ = redirect_url;

  std::move(continuation).Run(net::OK);
}

void InterceptedRequest::ContinueToHandleOverrideHeaders(int error_code) {
  if (error_code != net::OK) {
    SendErrorAndCompleteImmediately(error_code);
    return;
  }

  DCHECK(on_headers_received_callback_);
  absl::optional<std::string> headers;
  if (override_headers_) {
    headers = override_headers_->raw_headers();
  }
  header_client_redirect_url_ = redirect_url_;
  std::move(on_headers_received_callback_).Run(net::OK, headers, redirect_url_);

  override_headers_ = nullptr;
  redirect_url_ = GURL();

  // Resume handling of client messages after continuing from an async callback.
  if (proxied_client_receiver_.is_bound()) {
    proxied_client_receiver_.Resume();
  }
}

net::RedirectInfo InterceptedRequest::MakeRedirectResponseAndInfo(
    const GURL& new_location) {
  // Clear the Content-Type values.
  current_response_->mime_type = current_response_->charset = std::string();
  current_response_->headers->RemoveHeader(
      net::HttpRequestHeaders::kContentType);

  // Clear the Content-Length values.
  current_response_->content_length = 0;
  // Avoid incorrect replacement of 0 with nullptr. NOLINTNEXTLINE
  current_response_->encoded_body_length = 0;
  current_response_->headers->RemoveHeader(
      net::HttpRequestHeaders::kContentLength);

  current_response_->encoded_data_length =
      current_response_->headers->raw_headers().size();

  const net::RedirectInfo& redirect_info = MakeRedirectInfo(
      request_, current_response_->headers.get(), new_location, 0);
  current_response_->headers->ReplaceStatusLine(
      MakeStatusLine(redirect_info.status_code, std::string(), true));

  return redirect_info;
}

void InterceptedRequest::ContinueToBeforeRedirect(
    const net::RedirectInfo& redirect_info,
    int error_code) {
  if (error_code != net::OK) {
    SendErrorAndCompleteImmediately(error_code);
    return;
  }

  request_was_redirected_ = true;
  redirect_in_progress_ = false;

  if (header_client_redirect_url_.is_valid()) {
    header_client_redirect_url_ = GURL();
  }

  const GURL redirect_url = redirect_url_;
  override_headers_ = nullptr;
  redirect_url_ = GURL();

  // Resume handling of client messages after continuing from an async callback.
  if (proxied_client_receiver_.is_bound()) {
    proxied_client_receiver_.Resume();
  }

  const auto original_url = request_.url;
  const auto original_method = request_.method;

  net::RedirectInfo new_redirect_info = redirect_info;
  if (redirect_url.is_valid()) {
    new_redirect_info.new_url = redirect_url;
    new_redirect_info.new_site_for_cookies =
        net::SiteForCookies::FromUrl(redirect_url);
  }

  target_client_->OnReceiveRedirect(new_redirect_info,
                                    std::move(current_response_));

  request_.url = new_redirect_info.new_url;
  request_.method = new_redirect_info.new_method;
  request_.site_for_cookies = new_redirect_info.new_site_for_cookies;
  request_.referrer = GURL(new_redirect_info.new_referrer);
  request_.referrer_policy = new_redirect_info.new_referrer_policy;

  if (request_.trusted_params) {
    request_.trusted_params->isolation_info =
        request_.trusted_params->isolation_info.CreateForRedirect(
            url::Origin::Create(request_.url));
  }

  // Remove existing Cookie headers. They may be re-added after Restart().
  const std::vector<std::string> remove_headers{
      net::HttpRequestHeaders::kCookie};

  // Use common logic for sanitizing request headers including Origin and
  // Content-*.
  bool should_clear_upload;
  net::RedirectUtil::UpdateHttpRequest(original_url, original_method,
                                       new_redirect_info,
                                       absl::make_optional(remove_headers),
                                       /*modified_headers=*/absl::nullopt,
                                       &request_.headers, &should_clear_upload);

  if (should_clear_upload) {
    request_.request_body = nullptr;
  }
}

void InterceptedRequest::ContinueToResponseStarted(int error_code) {
  if (error_code != net::OK) {
    SendErrorAndCompleteImmediately(error_code);
    return;
  }

  const GURL redirect_url = redirect_url_;
  override_headers_ = nullptr;
  redirect_url_ = GURL();

  scoped_refptr<net::HttpResponseHeaders> headers =
      current_response_ ? current_response_->headers : nullptr;

  std::string location;
  const bool is_redirect =
      redirect_url.is_valid() || (headers && headers->IsRedirect(&location));
  if (stream_loader_ && is_redirect) {
    // Redirecting from OnReceiveResponse generally isn't supported by the
    // NetworkService, so we can only support it when using a custom loader.
    // TODO(network): Remove this special case.
    const GURL new_location = redirect_url.is_valid()
                                  ? redirect_url
                                  : original_url_.Resolve(location);
    const net::RedirectInfo& redirect_info =
        MakeRedirectResponseAndInfo(new_location);

    HandleResponseOrRedirectHeaders(
        redirect_info,
        base::BindOnce(&InterceptedRequest::ContinueToBeforeRedirect,
                       weak_factory_.GetWeakPtr(), redirect_info));
  } else {
    LOG_IF(WARNING, is_redirect) << "Redirect at this time is not supported by "
                                    "the default network loader.";

    // CORS check for requests that are handled by the client. Requests handled
    // by the network process will be checked there.
    if (stream_loader_ && !is_redirect && request_.request_initiator &&
        network::cors::ShouldCheckCors(request_.url, request_.request_initiator,
                                       request_.mode)) {
      const auto result = network::cors::CheckAccess(
          request_.url,
          GetHeaderString(
              headers.get(),
              network::cors::header_names::kAccessControlAllowOrigin),
          GetHeaderString(
              headers.get(),
              network::cors::header_names::kAccessControlAllowCredentials),
          request_.credentials_mode, *request_.request_initiator);
      if (!result.has_value() &&
          !HasCrossOriginWhitelistEntry(*request_.request_initiator,
                                        url::Origin::Create(request_.url))) {
        SendErrorStatusAndCompleteImmediately(
            network::URLLoaderCompletionStatus(result.error()));
        return;
      }
    }

    // Resume handling of client messages after continuing from an async
    // callback.
    if (proxied_client_receiver_.is_bound()) {
      proxied_client_receiver_.Resume();
    }

    target_client_->OnReceiveResponse(
        std::move(current_response_),
        factory_->request_handler_->OnFilterResponseBody(
            id_, request_, std::move(current_body_)),
        std::move(current_cached_metadata_));
  }
}

void InterceptedRequest::OnDestroy() {
  // We don't want any callbacks after this point.
  weak_factory_.InvalidateWeakPtrs();

  factory_->request_handler_->OnRequestComplete(id_, request_, status_);

  // Destroys |this|.
  factory_->RemoveRequest(this);
}

void InterceptedRequest::OnProcessRequestHeaders(
    const GURL& redirect_url,
    net::HttpRequestHeaders* modified_headers,
    std::vector<std::string>* removed_headers) {
  factory_->request_handler_->ProcessRequestHeaders(
      id_, request_, redirect_url, modified_headers, removed_headers);

  if (!modified_headers->IsEmpty() || !removed_headers->empty()) {
    request_.headers.MergeFrom(*modified_headers);
    for (const std::string& name : *removed_headers) {
      request_.headers.RemoveHeader(name);
    }
  }
}

void InterceptedRequest::OnURLLoaderClientError() {
  // We set |wait_for_loader_error| to true because if the loader did have a
  // custom_reason error then the client would be reset as well and it would be
  // a race as to which connection error we saw first.
  CallOnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED),
                 true /* wait_for_loader_error */);
}

void InterceptedRequest::OnURLLoaderError(uint32_t custom_reason,
                                          const std::string& description) {
  if (custom_reason == network::mojom::URLLoader::kClientDisconnectReason &&
      description == safe_browsing::kCustomCancelReasonForURLLoader) {
    SendErrorCallback(safe_browsing::kNetErrorCodeForSafeBrowsing, true);
  }

  got_loader_error_ = true;

  // If CallOnComplete was already called, then this object is ready to be
  // deleted.
  if (!target_client_) {
    OnDestroy();
  }
}

void InterceptedRequest::CallOnComplete(
    const network::URLLoaderCompletionStatus& status,
    bool wait_for_loader_error) {
  status_ = status;

  if (target_client_) {
    target_client_->OnComplete(status);
  }

  if (proxied_loader_receiver_.is_bound() &&
      (wait_for_loader_error && !got_loader_error_)) {
    // Don't delete |this| yet, in case the |proxied_loader_receiver_|'s
    // error_handler is called with a reason to indicate an error which we want
    // to send to the client bridge. Also reset |target_client_| so we don't
    // get its error_handler called and then delete |this|.
    target_client_.reset();

    // Since the original client is gone no need to continue loading the
    // request.
    proxied_client_receiver_.reset();
    header_client_receiver_.reset();
    target_loader_.reset();

    // In case there are pending checks as to whether this request should be
    // intercepted, we don't want that causing |target_client_| to be used
    // later.
    weak_factory_.InvalidateWeakPtrs();
  } else {
    OnDestroy();
  }
}

void InterceptedRequest::SendErrorAndCompleteImmediately(int error_code) {
  SendErrorStatusAndCompleteImmediately(
      network::URLLoaderCompletionStatus(error_code));
}

void InterceptedRequest::SendErrorStatusAndCompleteImmediately(
    const network::URLLoaderCompletionStatus& status) {
  status_ = status;
  SendErrorCallback(status_.error_code, false);
  target_client_->OnComplete(status_);
  OnDestroy();
}

void InterceptedRequest::SendErrorCallback(int error_code,
                                           bool safebrowsing_hit) {
  // Ensure we only send one error callback, e.g. to avoid sending two if
  // there's both a networking error and safe browsing blocked the request.
  if (sent_error_callback_) {
    return;
  }

  sent_error_callback_ = true;
  factory_->request_handler_->OnRequestError(id_, request_, error_code,
                                             safebrowsing_hit);
}

void InterceptedRequest::OnUploadProgressACK() {
  DCHECK(waiting_for_upload_progress_ack_);
  waiting_for_upload_progress_ack_ = false;
}

//==============================
// InterceptedRequestHandler
//==============================

InterceptedRequestHandler::InterceptedRequestHandler() = default;
InterceptedRequestHandler::~InterceptedRequestHandler() = default;

void InterceptedRequestHandler::OnBeforeRequest(
    int32_t request_id,
    network::ResourceRequest* request,
    bool request_was_redirected,
    OnBeforeRequestResultCallback callback,
    CancelRequestCallback cancel_callback) {
  std::move(callback).Run(false, false);
}

void InterceptedRequestHandler::ShouldInterceptRequest(
    int32_t request_id,
    network::ResourceRequest* request,
    ShouldInterceptRequestResultCallback callback) {
  std::move(callback).Run(nullptr);
}

void InterceptedRequestHandler::OnRequestResponse(
    int32_t request_id,
    network::ResourceRequest* request,
    net::HttpResponseHeaders* headers,
    absl::optional<net::RedirectInfo> redirect_info,
    OnRequestResponseResultCallback callback) {
  std::move(callback).Run(
      ResponseMode::CONTINUE, nullptr,
      redirect_info.has_value() ? redirect_info->new_url : GURL());
}

mojo::ScopedDataPipeConsumerHandle
InterceptedRequestHandler::OnFilterResponseBody(
    int32_t request_id,
    const network::ResourceRequest& request,
    mojo::ScopedDataPipeConsumerHandle body) {
  return body;
}

//==============================
// ProxyURLLoaderFactory
//==============================

ProxyURLLoaderFactory::ProxyURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote,
    mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
        header_client_receiver,
    std::unique_ptr<InterceptedRequestHandler> request_handler)
    : request_handler_(std::move(request_handler)), weak_factory_(this) {
  CEF_REQUIRE_IOT();
  DCHECK(request_handler_);

  // Actual creation of the factory.
  if (target_factory_remote) {
    target_factory_.Bind(std::move(target_factory_remote));
    target_factory_.set_disconnect_handler(base::BindOnce(
        &ProxyURLLoaderFactory::OnTargetFactoryError, base::Unretained(this)));
  }
  proxy_receivers_.Add(this, std::move(factory_receiver));
  proxy_receivers_.set_disconnect_handler(base::BindRepeating(
      &ProxyURLLoaderFactory::OnProxyBindingError, base::Unretained(this)));

  if (header_client_receiver) {
    url_loader_header_client_receiver_.Bind(std::move(header_client_receiver));
  }
}

ProxyURLLoaderFactory::~ProxyURLLoaderFactory() {
  CEF_REQUIRE_IOT();
}

// static
void ProxyURLLoaderFactory::CreateOnIOThread(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
    mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
        header_client_receiver,
    content::ResourceContext* resource_context,
    std::unique_ptr<InterceptedRequestHandler> request_handler) {
  CEF_REQUIRE_IOT();
  auto proxy = new ProxyURLLoaderFactory(
      std::move(factory_receiver), std::move(target_factory),
      std::move(header_client_receiver), std::move(request_handler));
  ResourceContextData::AddProxy(proxy, resource_context);
}

void ProxyURLLoaderFactory::SetDisconnectCallback(
    DisconnectCallback on_disconnect) {
  CEF_REQUIRE_IOT();
  DCHECK(!destroyed_);
  DCHECK(!on_disconnect_);
  on_disconnect_ = std::move(on_disconnect);
}

// static
void ProxyURLLoaderFactory::CreateProxy(
    content::BrowserContext* browser_context,
    network::URLLoaderFactoryBuilder& factory_builder,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    std::unique_ptr<InterceptedRequestHandler> request_handler) {
  CEF_REQUIRE_UIT();
  DCHECK(request_handler);

  auto [factory_receiver, target_factory_remote] = factory_builder.Append();

  mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
      header_client_receiver;
  if (header_client) {
    header_client_receiver = header_client->InitWithNewPipeAndPassReceiver();
  }

  content::ResourceContext* resource_context =
      browser_context->GetResourceContext();
  DCHECK(resource_context);

  CEF_POST_TASK(
      CEF_IOT,
      base::BindOnce(
          &ProxyURLLoaderFactory::CreateOnIOThread, std::move(factory_receiver),
          std::move(target_factory_remote), std::move(header_client_receiver),
          base::Unretained(resource_context), std::move(request_handler)));
}

// static
void ProxyURLLoaderFactory::CreateProxy(
    content::WebContents::Getter web_contents_getter,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
    std::unique_ptr<InterceptedRequestHandler> request_handler) {
  DCHECK(request_handler);

  if (!CEF_CURRENTLY_ON_IOT()) {
    CEF_POST_TASK(
        CEF_IOT,
        base::BindOnce(CreateProxyHelper, web_contents_getter,
                       std::move(loader_receiver), std::move(request_handler)));
    return;
  }

  auto proxy = new ProxyURLLoaderFactory(
      std::move(loader_receiver),
      mojo::PendingRemote<network::mojom::URLLoaderFactory>(),
      mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>(),
      std::move(request_handler));
  CEF_POST_TASK(CEF_UIT,
                base::BindOnce(ResourceContextData::AddProxyOnUIThread,
                               base::Unretained(proxy), web_contents_getter));
}

void ProxyURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  CEF_REQUIRE_IOT();
  if (!CONTEXT_STATE_VALID()) {
    // Don't start a request while we're shutting down.
    return;
  }

  if (DisableRequestHandlingForTesting() && request.url.SchemeIsHTTPOrHTTPS()) {
    // This is the so-called pass-through, no-op option.
    if (target_factory_) {
      target_factory_->CreateLoaderAndStart(std::move(receiver), request_id,
                                            options, request, std::move(client),
                                            traffic_annotation);
    }
    return;
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_clone;
  if (target_factory_) {
    target_factory_->Clone(
        target_factory_clone.InitWithNewPipeAndPassReceiver());
  }

  InterceptedRequest* req = new InterceptedRequest(
      this, request_id, options, request, traffic_annotation,
      std::move(receiver), std::move(client), std::move(target_factory_clone));
  requests_.insert(std::make_pair(request_id, base::WrapUnique(req)));
  req->Restart();
}

void ProxyURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory) {
  CEF_REQUIRE_IOT();
  proxy_receivers_.Add(this, std::move(factory));
}

void ProxyURLLoaderFactory::OnLoaderCreated(
    int32_t request_id,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  CEF_REQUIRE_IOT();
  auto request_it = requests_.find(request_id);
  if (request_it != requests_.end()) {
    request_it->second->OnLoaderCreated(std::move(receiver));
  }
}

void ProxyURLLoaderFactory::OnLoaderForCorsPreflightCreated(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  CEF_REQUIRE_IOT();
  new CorsPreflightRequest(std::move(receiver));
}

void ProxyURLLoaderFactory::OnTargetFactoryError() {
  // Stop calls to CreateLoaderAndStart() when |target_factory_| is invalid.
  target_factory_.reset();
  proxy_receivers_.Clear();

  MaybeDestroySelf();
}

void ProxyURLLoaderFactory::OnProxyBindingError() {
  if (proxy_receivers_.empty()) {
    target_factory_.reset();
  }

  MaybeDestroySelf();
}

void ProxyURLLoaderFactory::RemoveRequest(InterceptedRequest* request) {
  auto it = requests_.find(request->id());
  DCHECK(it != requests_.end());
  requests_.erase(it);

  MaybeDestroySelf();
}

void ProxyURLLoaderFactory::MaybeDestroySelf() {
  // Even if all URLLoaderFactory pipes connected to this object have been
  // closed it has to stay alive until all active requests have completed.
  if (target_factory_.is_bound() || !requests_.empty()) {
    return;
  }

  destroyed_ = true;

  // In some cases we may be destroyed before SetDisconnectCallback is called.
  if (on_disconnect_) {
    // Deletes |this|.
    std::move(on_disconnect_).Run(this);
  }
}

}  // namespace net_service
