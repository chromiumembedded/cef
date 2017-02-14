// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_urlrequest_impl.h"

#include <string>
#include <utility>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/net/url_request_user_data.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/response_impl.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;

namespace {

class CefURLFetcherDelegate : public net::URLFetcherDelegate {
 public:
  CefURLFetcherDelegate(CefBrowserURLRequest::Context* context,
                        int request_flags);
  ~CefURLFetcherDelegate() override;

  // net::URLFetcherDelegate methods.
  void OnURLFetchComplete(const net::URLFetcher* source) override;
  void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                  int64 current,
                                  int64 total,
                                  int64_t current_network_bytes) override;
  void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                int64 current, int64 total) override;

 private:
  // The context_ pointer will outlive this object.
  CefBrowserURLRequest::Context* context_;
  int request_flags_;
};

class NET_EXPORT CefURLFetcherResponseWriter :
    public net::URLFetcherResponseWriter {
 public:
  CefURLFetcherResponseWriter(
      CefRefPtr<CefBrowserURLRequest> url_request,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : url_request_(url_request),
        task_runner_(task_runner) {
  }

  // net::URLFetcherResponseWriter methods.
  int Initialize(const net::CompletionCallback& callback) override {
    return net::OK;
  }

  int Write(net::IOBuffer* buffer,
            int num_bytes,
            const net::CompletionCallback& callback) override {
    if (url_request_.get()) {
      task_runner_->PostTask(FROM_HERE,
          base::Bind(&CefURLFetcherResponseWriter::WriteOnClientThread,
                     url_request_, scoped_refptr<net::IOBuffer>(buffer),
                     num_bytes, callback,
                     base::MessageLoop::current()->task_runner()));
      return net::ERR_IO_PENDING;
    }
    return num_bytes;
  }

  int Finish(int net_error, const net::CompletionCallback& callback) override {
    if (url_request_.get())
      url_request_ = NULL;
    return net::OK;
  }

 private:
  static void WriteOnClientThread(
      CefRefPtr<CefBrowserURLRequest> url_request,
      scoped_refptr<net::IOBuffer> buffer,
      int num_bytes,
      const net::CompletionCallback& callback,
      scoped_refptr<base::SequencedTaskRunner> source_message_loop_proxy) {
    CefRefPtr<CefURLRequestClient> client = url_request->GetClient();
    if (client.get())
      client->OnDownloadData(url_request.get(), buffer->data(), num_bytes);

    source_message_loop_proxy->PostTask(FROM_HERE,
        base::Bind(&CefURLFetcherResponseWriter::ContinueOnSourceThread,
                   num_bytes, callback));
  }

  static void ContinueOnSourceThread(
      int num_bytes,
      const net::CompletionCallback& callback) {
    callback.Run(num_bytes);
  }

  CefRefPtr<CefBrowserURLRequest> url_request_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CefURLFetcherResponseWriter);
};

base::SupportsUserData::Data* CreateURLRequestUserData(
    CefRefPtr<CefURLRequestClient> client) {
  return new CefURLRequestUserData(client);
}

}  // namespace


// CefBrowserURLRequest::Context ----------------------------------------------

class CefBrowserURLRequest::Context
    : public base::RefCountedThreadSafe<CefBrowserURLRequest::Context> {
 public:
  Context(CefRefPtr<CefBrowserURLRequest> url_request,
          CefRefPtr<CefRequest> request,
          CefRefPtr<CefURLRequestClient> client,
          CefRefPtr<CefRequestContext> request_context)
    : url_request_(url_request),
      request_(request),
      client_(client),
      request_context_(request_context),
      task_runner_(base::MessageLoop::current()->task_runner()),
    status_(UR_IO_PENDING),
    error_code_(ERR_NONE),
    upload_data_size_(0),
    got_upload_progress_complete_(false) {
    // Mark the request as read-only.
    static_cast<CefRequestImpl*>(request_.get())->SetReadOnly(true);
  }

  inline bool CalledOnValidThread() {
    return task_runner_->RunsTasksOnCurrentThread();
  }

  bool Start() {
    DCHECK(CalledOnValidThread());

    const GURL& url = GURL(request_->GetURL().ToString());
    if (!url.is_valid())
      return false;

    const std::string& method =
        base::ToLowerASCII(request_->GetMethod().ToString());
    net::URLFetcher::RequestType request_type = net::URLFetcher::GET;
    if (base::LowerCaseEqualsASCII(method, "get")) {
    } else if (base::LowerCaseEqualsASCII(method, "post")) {
      request_type = net::URLFetcher::POST;
    } else if (base::LowerCaseEqualsASCII(method, "head")) {
      request_type = net::URLFetcher::HEAD;
    } else if (base::LowerCaseEqualsASCII(method, "delete")) {
      request_type = net::URLFetcher::DELETE_REQUEST;
    } else if (base::LowerCaseEqualsASCII(method, "put")) {
      request_type = net::URLFetcher::PUT;
    } else {
      NOTREACHED() << "invalid request type";
      return false;
    }

    BrowserThread::PostTaskAndReply(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CefBrowserURLRequest::Context::GetRequestContextOnUIThread,
                 this),
      base::Bind(&CefBrowserURLRequest::Context::ContinueOnOriginatingThread,
                 this, url, request_type));

    return true;
  }

  void GetRequestContextOnUIThread() {
    CEF_REQUIRE_UIT();

    // Get or create the request context and browser context.
    CefRefPtr<CefRequestContextImpl> request_context_impl =
        CefRequestContextImpl::GetOrCreateForRequestContext(request_context_);
    DCHECK(request_context_impl.get());
    CefBrowserContext* browser_context =
        request_context_impl->GetBrowserContext();
    DCHECK(browser_context);

    if (!request_context_.get())
      request_context_ = request_context_impl.get();

    // The request context is created on the UI thread but accessed and
    // destroyed on the IO thread.
    url_request_getter_ = browser_context->GetRequestContext();
  }

  void ContinueOnOriginatingThread(const GURL& url,
                                   net::URLFetcher::RequestType request_type) {
    DCHECK(CalledOnValidThread());

    int request_flags = request_->GetFlags();

    fetcher_delegate_.reset(new CefURLFetcherDelegate(this, request_flags));
    fetcher_ = net::URLFetcher::Create(url, request_type,
                                       fetcher_delegate_.get());

    DCHECK(url_request_getter_.get());
    fetcher_->SetRequestContext(url_request_getter_.get());

    static_cast<CefRequestImpl*>(request_.get())->Get(*fetcher_,
                                                      upload_data_size_);

    fetcher_->SetURLRequestUserData(
        CefURLRequestUserData::kUserDataKey,
        base::Bind(&CreateURLRequestUserData, client_));

    std::unique_ptr<net::URLFetcherResponseWriter> response_writer;
    if (request_flags & UR_FLAG_NO_DOWNLOAD_DATA) {
      response_writer.reset(new CefURLFetcherResponseWriter(NULL, NULL));
    } else {
      response_writer.reset(
          new CefURLFetcherResponseWriter(url_request_, task_runner_));
    }
    fetcher_->SaveResponseWithWriter(std::move(response_writer));

    fetcher_->Start();
  }

  void Cancel() {
    DCHECK(CalledOnValidThread());

    // The request may already be complete.
    if (!fetcher_.get())
      return;

    // Cancel the fetch by deleting the fetcher.
    fetcher_.reset(NULL);

    status_ = UR_CANCELED;
    error_code_ = ERR_ABORTED;
    OnComplete();
  }

  void OnComplete() {
    DCHECK(CalledOnValidThread());

    if (fetcher_.get()) {
      const net::URLRequestStatus& status = fetcher_->GetStatus();

      if (status.is_success())
        NotifyUploadProgressIfNecessary();

      switch (status.status()) {
        case net::URLRequestStatus::SUCCESS:
          status_ = UR_SUCCESS;
          break;
        case net::URLRequestStatus::IO_PENDING:
          status_ = UR_IO_PENDING;
          break;
        case net::URLRequestStatus::CANCELED:
          status_ = UR_CANCELED;
          break;
        case net::URLRequestStatus::FAILED:
          status_ = UR_FAILED;
          break;
      }

      error_code_ = static_cast<CefURLRequest::ErrorCode>(status.error());

      if(!response_.get())
        OnResponse();
    }

    DCHECK(url_request_.get());
    client_->OnRequestComplete(url_request_.get());

    if (fetcher_.get())
      fetcher_.reset(NULL);

    // This may result in the Context object being deleted.
    url_request_ = NULL;
  }

  void OnDownloadProgress(int64 current, int64 total) {
    DCHECK(CalledOnValidThread());
    DCHECK(url_request_.get());

    if(!response_.get())
      OnResponse();

    NotifyUploadProgressIfNecessary();

    client_->OnDownloadProgress(url_request_.get(), current, total);
  }

  void OnDownloadData(std::unique_ptr<std::string> download_data) {
    DCHECK(CalledOnValidThread());
    DCHECK(url_request_.get());

    if(!response_.get())
      OnResponse();

    client_->OnDownloadData(url_request_.get(), download_data->c_str(),
        download_data->length());
  }

  void OnUploadProgress(int64 current, int64 total) {
    DCHECK(CalledOnValidThread());
    DCHECK(url_request_.get());
    if (current == total)
      got_upload_progress_complete_ = true;
    client_->OnUploadProgress(url_request_.get(), current, total);
  }

  CefRefPtr<CefRequest> request() { return request_; }
  CefRefPtr<CefURLRequestClient> client() { return client_; }
  CefURLRequest::Status status() { return status_; }
  CefURLRequest::ErrorCode error_code() { return error_code_; }
  CefRefPtr<CefResponse> response() { return response_; }

 private:
  friend class base::RefCountedThreadSafe<CefBrowserURLRequest::Context>;

  ~Context() {
    if (fetcher_.get()) {
      // Delete the fetcher object on the thread that created it.
      task_runner_->DeleteSoon(FROM_HERE, fetcher_.release());
    }
  }

  void NotifyUploadProgressIfNecessary() {
    if (!got_upload_progress_complete_ && upload_data_size_ > 0) {
      // URLFetcher sends upload notifications using a timer and will not send
      // a notification if the request completes too quickly. We therefore
      // send the notification here if necessary.
      client_->OnUploadProgress(url_request_.get(), upload_data_size_,
                                upload_data_size_);
      got_upload_progress_complete_ = true;
    }
  }

  void OnResponse() {
    if (fetcher_.get()) {  
      response_ = new CefResponseImpl();
      CefResponseImpl* responseImpl =
          static_cast<CefResponseImpl*>(response_.get());

      net::HttpResponseHeaders* headers = fetcher_->GetResponseHeaders();
      if (headers)
        responseImpl->SetResponseHeaders(*headers);

      responseImpl->SetReadOnly(true);
    }
  }

  // Members only accessed on the initialization thread.
  CefRefPtr<CefBrowserURLRequest> url_request_;
  CefRefPtr<CefRequest> request_;
  CefRefPtr<CefURLRequestClient> client_;
  CefRefPtr<CefRequestContext> request_context_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<net::URLFetcher> fetcher_;
  std::unique_ptr<CefURLFetcherDelegate> fetcher_delegate_;
  CefURLRequest::Status status_;
  CefURLRequest::ErrorCode error_code_;
  CefRefPtr<CefResponse> response_;
  int64 upload_data_size_;
  bool got_upload_progress_complete_;

  scoped_refptr<net::URLRequestContextGetter> url_request_getter_;
};


// CefURLFetcherDelegate ------------------------------------------------------

namespace {

CefURLFetcherDelegate::CefURLFetcherDelegate(
    CefBrowserURLRequest::Context* context, int request_flags)
  : context_(context),
    request_flags_(request_flags) {
}

CefURLFetcherDelegate::~CefURLFetcherDelegate() {
}

void CefURLFetcherDelegate::OnURLFetchComplete(
    const net::URLFetcher* source) {
  // Complete asynchronously so as not to delete the URLFetcher while it's still
  // in the call stack.
  base::MessageLoop::current()->task_runner()->PostTask(FROM_HERE,
      base::Bind(&CefBrowserURLRequest::Context::OnComplete, context_));
}

void CefURLFetcherDelegate::OnURLFetchDownloadProgress(
    const net::URLFetcher* source,
    int64 current,
    int64 total,
    int64_t current_network_bytes) {
  context_->OnDownloadProgress(current, total);
}

void CefURLFetcherDelegate::OnURLFetchUploadProgress(
    const net::URLFetcher* source,
    int64 current, int64 total) {
  if (request_flags_ & UR_FLAG_REPORT_UPLOAD_PROGRESS)
    context_->OnUploadProgress(current, total);
}

}  // namespace


// CefBrowserURLRequest -------------------------------------------------------

CefBrowserURLRequest::CefBrowserURLRequest(
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefURLRequestClient> client,
    CefRefPtr<CefRequestContext> request_context) {
  context_ = new Context(this, request, client, request_context);
}

CefBrowserURLRequest::~CefBrowserURLRequest() {
}

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
  return context_->error_code();
}

CefRefPtr<CefResponse> CefBrowserURLRequest::GetResponse() {
  if (!VerifyContext())
    return NULL;
  return context_->response();
}

void CefBrowserURLRequest::Cancel() {
  if (!VerifyContext())
    return;
  return context_->Cancel();
}

bool CefBrowserURLRequest::VerifyContext() {
  DCHECK(context_.get());
  if (!context_->CalledOnValidThread()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  return true;
}
