// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/renderer/render_urlrequest_impl.h"

#include <stdint.h>

#include "libcef/common/request_impl.h"
#include "libcef/common/response_impl.h"
#include "libcef/renderer/blink_glue.h"
#include "libcef/renderer/frame_impl.h"
#include "libcef/renderer/thread_util.h"

#include "base/logging.h"
#include "net/base/request_priority.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/public/platform/web_url_response.h"

using blink::WebString;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLLoader;
using blink::WebURLRequest;
using blink::WebURLResponse;

namespace {

class CefWebURLLoaderClient : public blink::WebURLLoaderClient {
 public:
  CefWebURLLoaderClient(CefRenderURLRequest::Context* context,
                        int request_flags);
  ~CefWebURLLoaderClient() override;

  // blink::WebURLLoaderClient methods.
  void DidSendData(uint64_t bytes_sent,
                   uint64_t total_bytes_to_be_sent) override;
  void DidReceiveResponse(const WebURLResponse& response) override;
  void DidReceiveData(const char* data, int dataLength) override;
  void DidFinishLoading(base::TimeTicks finish_time,
                        int64_t total_encoded_data_length,
                        int64_t total_encoded_body_length,
                        int64_t total_decoded_body_length,
                        bool should_report_corb_blocking) override;
  void DidFail(const WebURLError&,
               base::TimeTicks finish_time,
               int64_t total_encoded_data_length,
               int64_t total_encoded_body_length,
               int64_t total_decoded_body_length) override;
  void DidStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle response_body) override;
  bool WillFollowRedirect(const WebURL& new_url,
                          const net::SiteForCookies& new_site_for_cookies,
                          const WebString& new_referrer,
                          network::mojom::ReferrerPolicy new_referrer_policy,
                          const WebString& new_method,
                          const WebURLResponse& passed_redirect_response,
                          bool& report_raw_headers,
                          std::vector<std::string>* removed_headers,
                          bool insecure_scheme_was_upgraded) override;

 protected:
  // The context_ pointer will outlive this object.
  CefRenderURLRequest::Context* context_;
  int request_flags_;
};

}  // namespace

// CefRenderURLRequest::Context -----------------------------------------------

class CefRenderURLRequest::Context
    : public base::RefCountedThreadSafe<CefRenderURLRequest::Context> {
 public:
  Context(CefRefPtr<CefRenderURLRequest> url_request,
          CefRefPtr<CefFrame> frame,
          CefRefPtr<CefRequest> request,
          CefRefPtr<CefURLRequestClient> client)
      : url_request_(url_request),
        frame_(frame),
        request_(request),
        client_(client),
        status_(UR_IO_PENDING),
        error_code_(ERR_NONE),
        body_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
        response_was_cached_(false),
        upload_data_size_(0),
        got_upload_progress_complete_(false),
        download_data_received_(0),
        download_data_total_(-1) {
    // Mark the request as read-only.
    static_cast<CefRequestImpl*>(request_.get())->SetReadOnly(true);
  }

  bool Start() {
    GURL url = GURL(request_->GetURL().ToString());
    if (!url.is_valid())
      return false;

    url_client_.reset(new CefWebURLLoaderClient(this, request_->GetFlags()));

    std::unique_ptr<network::ResourceRequest> resource_request =
        std::make_unique<network::ResourceRequest>();
    static_cast<CefRequestImpl*>(request_.get())
        ->Get(resource_request.get(), false);
    resource_request->priority = net::MEDIUM;

    // Behave the same as a subresource load.
    resource_request->resource_type =
        static_cast<int>(blink::mojom::ResourceType::kSubResource);

    // Need load timing info for WebURLLoaderImpl::PopulateURLResponse to
    // properly set cached status.
    resource_request->enable_load_timing = true;

    // Set the origin to match the request. The requirement for an origin is
    // DCHECK'd in ResourceDispatcherHostImpl::ContinuePendingBeginRequest.
    resource_request->request_initiator = url::Origin::Create(url);

    if (request_->GetFlags() & UR_FLAG_ALLOW_STORED_CREDENTIALS) {
      // Include SameSite cookies.
      resource_request->site_for_cookies =
          net::SiteForCookies::FromOrigin(*resource_request->request_initiator);
    }

    if (resource_request->request_body) {
      const auto& elements = *resource_request->request_body->elements();
      if (elements.size() > 0) {
        const auto& element = elements[0];
        if (element.type() == network::DataElement::Tag::kBytes) {
          const auto& bytes_element = element.As<network::DataElementBytes>();
          upload_data_size_ = bytes_element.bytes().size();
        }
      }
    }

    auto frame_impl = static_cast<CefFrameImpl*>(frame_.get());
    loader_ = frame_impl->CreateURLLoader();
    loader_->LoadAsynchronously(
        std::move(resource_request), /*extra_data=*/nullptr,
        /*no_mime_sniffing=*/false,
        frame_impl->CreateResourceLoadInfoNotifierWrapper(), url_client_.get());
    return true;
  }

  void Cancel() {
    // The request may already be complete.
    if (!loader_.get() || status_ != UR_IO_PENDING)
      return;

    status_ = UR_CANCELED;
    error_code_ = ERR_ABORTED;

    // Will result in a call to OnError().
    loader_->Cancel();
  }

  void OnStopRedirect(const WebURL& redirect_url,
                      const WebURLResponse& response) {
    response_was_cached_ = blink_glue::ResponseWasCached(response);
    response_ = CefResponse::Create();
    CefResponseImpl* responseImpl =
        static_cast<CefResponseImpl*>(response_.get());

    // In case of StopOnRedirect we only set these fields. Everything else is
    // left blank. This also replicates the behaviour of the browser urlrequest
    // fetcher.
    responseImpl->SetStatus(response.HttpStatusCode());
    responseImpl->SetURL(redirect_url.GetString().Utf16());
    responseImpl->SetReadOnly(true);

    status_ = UR_CANCELED;
    error_code_ = ERR_ABORTED;

    OnComplete();
  }

  void OnResponse(const WebURLResponse& response) {
    response_was_cached_ = blink_glue::ResponseWasCached(response);
    response_ = CefResponse::Create();
    CefResponseImpl* responseImpl =
        static_cast<CefResponseImpl*>(response_.get());
    responseImpl->Set(response);
    responseImpl->SetReadOnly(true);

    download_data_total_ = response.ExpectedContentLength();
  }

  void OnError(const WebURLError& error) {
    if (status_ == UR_IO_PENDING) {
      status_ = UR_FAILED;
      error_code_ = static_cast<cef_errorcode_t>(error.reason());
    }

    OnComplete();
  }

  void OnComplete() {
    if (body_handle_.is_valid()) {
      return;
    }

    if (status_ == UR_IO_PENDING) {
      status_ = UR_SUCCESS;
      NotifyUploadProgressIfNecessary();
    }

    if (loader_.get())
      loader_.reset(nullptr);

    DCHECK(url_request_.get());
    client_->OnRequestComplete(url_request_.get());

    // This may result in the Context object being deleted.
    url_request_ = nullptr;
  }

  void OnBodyReadable(MojoResult, const mojo::HandleSignalsState&) {
    const void* buffer = nullptr;
    uint32_t read_bytes = 0;
    MojoResult result = body_handle_->BeginReadData(&buffer, &read_bytes,
                                                    MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      body_watcher_.ArmOrNotify();
      return;
    }

    if (result == MOJO_RESULT_FAILED_PRECONDITION) {
      // Whole body has been read.
      body_handle_.reset();
      body_watcher_.Cancel();
      OnComplete();
      return;
    }

    if (result != MOJO_RESULT_OK) {
      // Something went wrong.
      body_handle_.reset();
      body_watcher_.Cancel();
      OnComplete();
      return;
    }

    download_data_received_ += read_bytes;

    client_->OnDownloadProgress(url_request_.get(), download_data_received_,
                                download_data_total_);

    if (!(request_->GetFlags() & UR_FLAG_NO_DOWNLOAD_DATA)) {
      client_->OnDownloadData(url_request_.get(), buffer, read_bytes);
    }

    body_handle_->EndReadData(read_bytes);
    body_watcher_.ArmOrNotify();
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle response_body) {
    DCHECK(response_body);
    DCHECK(!body_handle_);
    body_handle_ = std::move(response_body);

    body_watcher_.Watch(
        body_handle_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&CefRenderURLRequest::Context::OnBodyReadable,
                            base::Unretained(this)));
    body_watcher_.ArmOrNotify();
  }

  void OnDownloadProgress(int64_t current) {
    DCHECK(url_request_.get());

    NotifyUploadProgressIfNecessary();

    download_data_received_ += current;
    client_->OnDownloadProgress(url_request_.get(), download_data_received_,
                                download_data_total_);
  }

  void OnDownloadData(const char* data, int dataLength) {
    DCHECK(url_request_.get());
    client_->OnDownloadData(url_request_.get(), data, dataLength);
  }

  void OnUploadProgress(int64_t current, int64_t total) {
    DCHECK(url_request_.get());
    if (current == total)
      got_upload_progress_complete_ = true;
    client_->OnUploadProgress(url_request_.get(), current, total);
  }

  CefRefPtr<CefRequest> request() const { return request_; }
  CefRefPtr<CefURLRequestClient> client() const { return client_; }
  CefURLRequest::Status status() const { return status_; }
  CefURLRequest::ErrorCode error_code() const { return error_code_; }
  CefRefPtr<CefResponse> response() const { return response_; }
  bool response_was_cached() const { return response_was_cached_; }

 private:
  friend class base::RefCountedThreadSafe<CefRenderURLRequest::Context>;

  virtual ~Context() {}

  void NotifyUploadProgressIfNecessary() {
    if (!got_upload_progress_complete_ && upload_data_size_ > 0) {
      // Upload notifications are sent using a timer and may not occur if the
      // request completes too quickly. We therefore send the notification here
      // if necessary.
      url_client_->DidSendData(upload_data_size_, upload_data_size_);
      got_upload_progress_complete_ = true;
    }
  }

  // Members only accessed on the initialization thread.
  CefRefPtr<CefRenderURLRequest> url_request_;
  CefRefPtr<CefFrame> frame_;
  CefRefPtr<CefRequest> request_;
  CefRefPtr<CefURLRequestClient> client_;
  CefURLRequest::Status status_;
  CefURLRequest::ErrorCode error_code_;
  CefRefPtr<CefResponse> response_;
  mojo::ScopedDataPipeConsumerHandle body_handle_;
  mojo::SimpleWatcher body_watcher_;
  bool response_was_cached_;
  std::unique_ptr<blink::WebURLLoader> loader_;
  std::unique_ptr<CefWebURLLoaderClient> url_client_;
  int64_t upload_data_size_;
  bool got_upload_progress_complete_;
  int64_t download_data_received_;
  int64_t download_data_total_;
};

// CefWebURLLoaderClient --------------------------------------------------

namespace {

CefWebURLLoaderClient::CefWebURLLoaderClient(
    CefRenderURLRequest::Context* context,
    int request_flags)
    : context_(context), request_flags_(request_flags) {}

CefWebURLLoaderClient::~CefWebURLLoaderClient() {}

void CefWebURLLoaderClient::DidSendData(uint64_t bytes_sent,
                                        uint64_t total_bytes_to_be_sent) {
  if (request_flags_ & UR_FLAG_REPORT_UPLOAD_PROGRESS)
    context_->OnUploadProgress(bytes_sent, total_bytes_to_be_sent);
}

void CefWebURLLoaderClient::DidReceiveResponse(const WebURLResponse& response) {
  context_->OnResponse(response);
}

void CefWebURLLoaderClient::DidReceiveData(const char* data, int dataLength) {
  context_->OnDownloadProgress(dataLength);

  if (!(request_flags_ & UR_FLAG_NO_DOWNLOAD_DATA))
    context_->OnDownloadData(data, dataLength);
}

void CefWebURLLoaderClient::DidFinishLoading(base::TimeTicks finish_time,
                                             int64_t total_encoded_data_length,
                                             int64_t total_encoded_body_length,
                                             int64_t total_decoded_body_length,
                                             bool should_report_corb_blocking) {
  context_->OnComplete();
}

void CefWebURLLoaderClient::DidFail(const WebURLError& error,
                                    base::TimeTicks finish_time,
                                    int64_t total_encoded_data_length,
                                    int64_t total_encoded_body_length,
                                    int64_t total_decoded_body_length) {
  context_->OnError(error);
}

void CefWebURLLoaderClient::DidStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle response_body) {
  context_->OnStartLoadingResponseBody(std::move(response_body));
}

bool CefWebURLLoaderClient::WillFollowRedirect(
    const WebURL& new_url,
    const net::SiteForCookies& new_site_for_cookies,
    const WebString& new_referrer,
    network::mojom::ReferrerPolicy new_referrer_policy,
    const WebString& new_method,
    const WebURLResponse& passed_redirect_response,
    bool& report_raw_headers,
    std::vector<std::string>* removed_headers,
    bool insecure_scheme_was_upgraded) {
  if (request_flags_ & UR_FLAG_STOP_ON_REDIRECT) {
    context_->OnStopRedirect(new_url, passed_redirect_response);
    return false;
  }
  return true;
}

}  // namespace

// CefRenderURLRequest --------------------------------------------------------

CefRenderURLRequest::CefRenderURLRequest(
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefURLRequestClient> client) {
  DCHECK(frame);
  DCHECK(request);
  DCHECK(client);
  context_ = new Context(this, frame, request, client);
}

CefRenderURLRequest::~CefRenderURLRequest() {}

bool CefRenderURLRequest::Start() {
  if (!VerifyContext())
    return false;
  return context_->Start();
}

CefRefPtr<CefRequest> CefRenderURLRequest::GetRequest() {
  if (!VerifyContext())
    return nullptr;
  return context_->request();
}

CefRefPtr<CefURLRequestClient> CefRenderURLRequest::GetClient() {
  if (!VerifyContext())
    return nullptr;
  return context_->client();
}

CefURLRequest::Status CefRenderURLRequest::GetRequestStatus() {
  if (!VerifyContext())
    return UR_UNKNOWN;
  return context_->status();
}

CefURLRequest::ErrorCode CefRenderURLRequest::GetRequestError() {
  if (!VerifyContext())
    return ERR_NONE;
  return context_->error_code();
}

CefRefPtr<CefResponse> CefRenderURLRequest::GetResponse() {
  if (!VerifyContext())
    return nullptr;
  return context_->response();
}

bool CefRenderURLRequest::ResponseWasCached() {
  if (!VerifyContext())
    return false;
  return context_->response_was_cached();
}

void CefRenderURLRequest::Cancel() {
  if (!VerifyContext())
    return;
  return context_->Cancel();
}

bool CefRenderURLRequest::VerifyContext() {
  DCHECK(context_.get());
  if (!CEF_CURRENTLY_ON_RT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  return true;
}
