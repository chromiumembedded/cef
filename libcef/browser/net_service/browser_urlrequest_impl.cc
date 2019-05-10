// Copyright (c) 2019 The Chromium Embedded Framework Authors. Portions
// Copyright (c) 2018 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "libcef/browser/net_service/browser_urlrequest_impl.h"

#include <string>
#include <utility>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/frame_host_impl.h"
#include "libcef/browser/net_service/url_loader_factory_getter.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/net_service/net_service_util.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/response_impl.h"
#include "libcef/common/task_runner_impl.h"

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"

// CefBrowserURLRequest::Context ----------------------------------------------

class CefBrowserURLRequest::Context
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  Context(CefRefPtr<CefBrowserURLRequest> url_request,
          CefRefPtr<CefFrame> frame,
          CefRefPtr<CefRequest> request,
          CefRefPtr<CefURLRequestClient> client,
          CefRefPtr<CefRequestContext> request_context)
      : url_request_(url_request),
        frame_(frame),
        request_(static_cast<CefRequestImpl*>(request.get())),
        client_(client),
        request_context_(request_context),
        task_runner_(CefTaskRunnerImpl::GetCurrentTaskRunner()),
        response_(new CefResponseImpl()),
        weak_ptr_factory_(this) {
    // Mark the request/response objects as read-only.
    request_->SetReadOnly(true);
    response_->SetReadOnly(true);
  }
  ~Context() override = default;

  bool Start() {
    DCHECK(CalledOnValidThread());

    const GURL& url = GURL(request_->GetURL().ToString());
    if (!url.is_valid())
      return false;

    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(
            &CefBrowserURLRequest::Context::GetURLLoaderFactoryGetterOnUIThread,
            frame_, request_context_, weak_ptr_factory_.GetWeakPtr(),
            task_runner_));

    return true;
  }

  void Cancel() {
    DCHECK(CalledOnValidThread());

    // The request may already be complete or canceled.
    if (!url_request_)
      return;

    DCHECK_EQ(status_, UR_IO_PENDING);
    status_ = UR_CANCELED;

    response_->SetReadOnly(false);
    response_->SetError(ERR_ABORTED);
    response_->SetReadOnly(true);

    cleanup_immediately_ = true;
    OnComplete(false);
  }

  CefRefPtr<CefRequest> request() const { return request_.get(); }
  CefRefPtr<CefURLRequestClient> client() const { return client_; }
  CefURLRequest::Status status() const { return status_; }
  CefRefPtr<CefResponse> response() const { return response_.get(); }
  bool response_was_cached() const { return response_was_cached_; }

  inline bool CalledOnValidThread() {
    return task_runner_->RunsTasksInCurrentSequence();
  }

 private:
  static void GetURLLoaderFactoryGetterOnUIThread(
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequestContext> request_context,
      base::WeakPtr<CefBrowserURLRequest::Context> self,
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    CEF_REQUIRE_UIT();

    // Get or create the request context and browser context.
    CefRefPtr<CefRequestContextImpl> request_context_impl =
        CefRequestContextImpl::GetOrCreateForRequestContext(request_context);
    DCHECK(request_context_impl);
    CefBrowserContext* browser_context =
        request_context_impl->GetBrowserContext();
    DCHECK(browser_context);

    content::RenderFrameHost* rfh = nullptr;
    if (frame) {
      // The request will be associated with this frame/browser.
      rfh = static_cast<CefFrameHostImpl*>(frame.get())->GetRenderFrameHost();
    }

    auto loader_factory_getter =
        net_service::URLLoaderFactoryGetter::Create(rfh, browser_context);

    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CefBrowserURLRequest::Context::ContinueOnOriginatingThread, self,
            loader_factory_getter));
  }

  void ContinueOnOriginatingThread(
      scoped_refptr<net_service::URLLoaderFactoryGetter>
          loader_factory_getter) {
    DCHECK(CalledOnValidThread());

    // The request may have been canceled.
    if (!url_request_)
      return;

    DCHECK_EQ(status_, UR_IO_PENDING);

    loader_factory_getter_ = loader_factory_getter;

    const int request_flags = request_->GetFlags();

    // Create the URLLoaderFactory and bind to this thread.
    auto loader_factory = loader_factory_getter_->GetURLLoaderFactory();

    auto resource_request = std::make_unique<network::ResourceRequest>();
    static_cast<CefRequestImpl*>(request_.get())
        ->Get(resource_request.get(), false);

    // SimpleURLLoader is picky about the body contents. Try to populate them
    // correctly below.
    auto request_body = resource_request->request_body;
    resource_request->request_body = nullptr;

    std::string content_type;
    std::string method = resource_request->method;
    if (request_body) {
      if (method == "GET" || method == "HEAD") {
        // Fix the method value to allow a request body.
        method = "POST";
        resource_request->method = method;

        request_->SetReadOnly(false);
        request_->SetMethod(method);
        request_->SetReadOnly(true);
      }
      resource_request->headers.GetHeader(net::HttpRequestHeaders::kContentType,
                                          &content_type);
    }

    loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                               NO_TRAFFIC_ANNOTATION_YET);

    if (request_body) {
      if (request_body->elements()->size() == 1) {
        const auto& element = (*request_body->elements())[0];
        if (element.type() == network::mojom::DataElementType::kFile) {
          if (content_type.empty()) {
            const auto& extension = element.path().Extension();
            if (!extension.empty()) {
              // Requests should not block on the disk! On POSIX this goes to
              // disk. http://code.google.com/p/chromium/issues/detail?id=59849
              base::ThreadRestrictions::ScopedAllowIO allow_io;
              // Also remove the leading period.
              net::GetMimeTypeFromExtension(extension.substr(1), &content_type);
            }
          }
          loader_->AttachFileForUpload(element.path(), content_type);
        } else if (element.type() == network::mojom::DataElementType::kBytes) {
          if (content_type.empty()) {
            content_type = net_service::kContentTypeApplicationFormURLEncoded;
          }
          loader_->AttachStringForUpload(
              std::string(element.bytes() + element.offset(),
                          element.length() - element.offset()),
              content_type);

          if (request_flags & UR_FLAG_REPORT_UPLOAD_PROGRESS) {
            // Report the expected upload data size.
            upload_data_size_ = element.length() - element.offset();
          }
        } else {
          NOTIMPLEMENTED() << "Unsupported element type: " << element.type();
        }
      } else if (request_body->elements()->size() > 1) {
        NOTIMPLEMENTED() << "Multi-part form data is not supported";
      }
    }

    if (request_flags & UR_FLAG_NO_RETRY_ON_5XX) {
      // No retries is the default setting, so we don't need to configure that.
      // Allow delivery of non-2xx response bodies.
      loader_->SetAllowHttpErrorResults(true);
    } else {
      // Allow 2 retries on 5xx response or network change.
      // TODO(network): Consider exposing configuration of max retries and/or
      // RETRY_ON_NETWORK_CHANGE as a separate flag.
      loader_->SetRetryOptions(
          2, network::SimpleURLLoader::RETRY_ON_5XX |
                 network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
    }

    if (request_flags & UR_FLAG_STOP_ON_REDIRECT) {
      // The request will be canceled in OnRedirect.
      loader_->SetOnRedirectCallback(
          base::Bind(&CefBrowserURLRequest::Context::OnRedirect,
                     weak_ptr_factory_.GetWeakPtr()));
    }

    if (request_flags & UR_FLAG_REPORT_UPLOAD_PROGRESS) {
      loader_->SetOnUploadProgressCallback(
          base::Bind(&CefBrowserURLRequest::Context::OnUploadProgress,
                     weak_ptr_factory_.GetWeakPtr()));
    }

    if ((request_flags & UR_FLAG_NO_DOWNLOAD_DATA) || method == "HEAD") {
      loader_->DownloadHeadersOnly(
          loader_factory.get(),
          base::BindOnce(&CefBrowserURLRequest::Context::OnHeadersOnly,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      loader_->SetOnResponseStartedCallback(
          base::BindOnce(&CefBrowserURLRequest::Context::OnResponseStarted,
                         weak_ptr_factory_.GetWeakPtr()));
      loader_->SetOnDownloadProgressCallback(
          base::Bind(&CefBrowserURLRequest::Context::OnDownloadProgress,
                     weak_ptr_factory_.GetWeakPtr()));

      loader_->DownloadAsStream(loader_factory.get(), this);
    }
  }

  void OnHeadersOnly(scoped_refptr<net::HttpResponseHeaders> headers) {
    DCHECK(CalledOnValidThread());
    DCHECK_EQ(status_, UR_IO_PENDING);

    response_->SetReadOnly(false);
    response_->SetResponseHeaders(*headers);
    response_->SetReadOnly(true);

    // Match the previous behavior of sending download progress notifications
    // for UR_FLAG_NO_DOWNLOAD_DATA requests but not HEAD requests.
    if (request_->GetMethod().ToString() != "HEAD") {
      download_data_size_ = headers->GetContentLength();
      OnDownloadProgress(0);
    }

    cleanup_immediately_ = true;
    OnComplete(true);
  }

  void OnRedirect(const net::RedirectInfo& redirect_info,
                  const network::ResourceResponseHead& response_head,
                  std::vector<std::string>* removed_headers) {
    DCHECK(CalledOnValidThread());
    DCHECK_EQ(status_, UR_IO_PENDING);

    // This method is only called if we intend to stop on redirects.
    DCHECK(request_->GetFlags() | UR_FLAG_STOP_ON_REDIRECT);

    response_->SetReadOnly(false);
    response_->SetResponseHeaders(*response_head.headers);
    response_->SetReadOnly(true);

    Cancel();
  }

  void OnResponseStarted(const GURL& final_url,
                         const network::ResourceResponseHead& response_head) {
    DCHECK(CalledOnValidThread());
    DCHECK_EQ(status_, UR_IO_PENDING);

    response_->SetReadOnly(false);
    response_->SetURL(final_url.spec());
    response_->SetResponseHeaders(*response_head.headers);
    response_->SetReadOnly(true);

    download_data_size_ = response_head.content_length;
  }

  void OnUploadProgress(uint64_t position, uint64_t total) {
    DCHECK(CalledOnValidThread());
    DCHECK_EQ(status_, UR_IO_PENDING);

    upload_data_size_ = total;
    if (position == total)
      got_upload_progress_complete_ = true;

    client_->OnUploadProgress(url_request_.get(), position, total);
  }

  void OnDownloadProgress(uint64_t current) {
    DCHECK(CalledOnValidThread());
    DCHECK_EQ(status_, UR_IO_PENDING);

    if (response_->GetStatus() == 0) {
      // With failed requests this callback may arrive without a proceeding
      // OnHeadersOnly or OnResponseStarted.
      return;
    }

    NotifyUploadProgressIfNecessary();

    client_->OnDownloadProgress(url_request_.get(), current,
                                download_data_size_);
  }

  void NotifyUploadProgressIfNecessary() {
    if (!got_upload_progress_complete_ && upload_data_size_ > 0) {
      // URLLoader sends upload notifications using a timer and will not send
      // a notification if the request completes too quickly. We therefore
      // send the notification here if necessary.
      client_->OnUploadProgress(url_request_.get(), upload_data_size_,
                                upload_data_size_);
      got_upload_progress_complete_ = true;
    }
  }

  // SimpleURLLoaderStreamConsumer methods:
  void OnDataReceived(base::StringPiece string_piece,
                      base::OnceClosure resume) override {
    DCHECK(CalledOnValidThread());
    DCHECK_EQ(status_, UR_IO_PENDING);

    client_->OnDownloadData(url_request_.get(), string_piece.data(),
                            string_piece.length());
    std::move(resume).Run();
  }

  void OnComplete(bool success) override {
    DCHECK(CalledOnValidThread());

    // The request may already be complete or canceled.
    if (!url_request_)
      return;

    // Status will be UR_IO_PENDING if we're called when the request is complete
    // (via SimpleURLLoaderStreamConsumer or OnHeadersOnly). We can only call
    // these SimpleURLLoader methods if the request is complete.
    if (status_ == UR_IO_PENDING) {
      status_ = success ? UR_SUCCESS : UR_FAILED;

      response_->SetReadOnly(false);
      response_->SetURL(loader_->GetFinalURL().spec());
      response_->SetError(static_cast<cef_errorcode_t>(loader_->NetError()));
      response_->SetReadOnly(true);

      response_was_cached_ = loader_->LoadedFromCache();
    }

    if (success)
      NotifyUploadProgressIfNecessary();

    client_->OnRequestComplete(url_request_.get());

    // When called via SimpleURLLoaderStreamConsumer we need to cleanup
    // asynchronously. If the load is still pending this will also cancel it.
    Cleanup();
  }

  void OnRetry(base::OnceClosure start_retry) override {
    DCHECK(CalledOnValidThread());
    DCHECK_EQ(status_, UR_IO_PENDING);
    std::move(start_retry).Run();
  }

  void Cleanup() {
    DCHECK(CalledOnValidThread());
    DCHECK(url_request_);

    client_ = nullptr;
    request_context_ = nullptr;

    // We may be canceled before the loader is created.
    if (loader_) {
      // Must delete the loader before the factory.
      if (cleanup_immediately_) {
        // Most SimpleURLLoader callbacks let us delete the URLLoader objects
        // immediately.
        loader_.reset();
        loader_factory_getter_ = nullptr;
      } else {
        // Delete the URLLoader objects asynchronously on the correct thread.
        task_runner_->DeleteSoon(FROM_HERE, std::move(loader_));
        task_runner_->ReleaseSoon(FROM_HERE, std::move(loader_factory_getter_));
      }
    }

    // We may be holding the last reference to |url_request_|, destruction of
    // which will delete |this|. Use a local variable to keep |url_request_|
    // alive until this method returns.
    auto url_request = url_request_;
    url_request_ = nullptr;
  }

  // Members only accessed on the initialization thread.
  CefRefPtr<CefBrowserURLRequest> url_request_;
  CefRefPtr<CefFrame> frame_;
  CefRefPtr<CefRequestImpl> request_;
  CefRefPtr<CefURLRequestClient> client_;
  CefRefPtr<CefRequestContext> request_context_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  scoped_refptr<net_service::URLLoaderFactoryGetter> loader_factory_getter_;
  std::unique_ptr<network::SimpleURLLoader> loader_;

  CefURLRequest::Status status_ = UR_IO_PENDING;
  CefRefPtr<CefResponseImpl> response_;
  bool response_was_cached_ = false;
  int64 upload_data_size_ = 0;
  int64 download_data_size_ = -1;
  bool got_upload_progress_complete_ = false;
  bool cleanup_immediately_ = false;

  // Must be the last member.
  base::WeakPtrFactory<CefBrowserURLRequest::Context> weak_ptr_factory_;
};

// CefBrowserURLRequest -------------------------------------------------------

CefBrowserURLRequest::CefBrowserURLRequest(
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefURLRequestClient> client,
    CefRefPtr<CefRequestContext> request_context) {
  context_.reset(new Context(this, frame, request, client, request_context));
}

CefBrowserURLRequest::~CefBrowserURLRequest() {}

bool CefBrowserURLRequest::Start() {
  if (!VerifyContext())
    return false;
  return context_->Start();
}

CefRefPtr<CefRequest> CefBrowserURLRequest::GetRequest() {
  if (!VerifyContext())
    return NULL;
  return context_->request();
}

CefRefPtr<CefURLRequestClient> CefBrowserURLRequest::GetClient() {
  if (!VerifyContext())
    return NULL;
  return context_->client();
}

CefURLRequest::Status CefBrowserURLRequest::GetRequestStatus() {
  if (!VerifyContext())
    return UR_UNKNOWN;
  return context_->status();
}

CefURLRequest::ErrorCode CefBrowserURLRequest::GetRequestError() {
  if (!VerifyContext())
    return ERR_NONE;
  return context_->response()->GetError();
}

CefRefPtr<CefResponse> CefBrowserURLRequest::GetResponse() {
  if (!VerifyContext())
    return NULL;
  return context_->response();
}

bool CefBrowserURLRequest::ResponseWasCached() {
  if (!VerifyContext())
    return false;
  return context_->response_was_cached();
}

void CefBrowserURLRequest::Cancel() {
  if (!VerifyContext())
    return;
  return context_->Cancel();
}

bool CefBrowserURLRequest::VerifyContext() {
  if (!context_->CalledOnValidThread()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  return true;
}
