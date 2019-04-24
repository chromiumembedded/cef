// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/net_service/resource_handler_wrapper.h"

#include "libcef/browser/net_service/proxy_url_loader_factory.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/net_service/net_service_util.h"
#include "libcef/common/request_impl.h"

#include "base/strings/string_number_conversions.h"
#include "net/http/http_status_code.h"

namespace net_service {

namespace {

class SkipCallbackWrapper : public CefResourceSkipCallback {
 public:
  explicit SkipCallbackWrapper(InputStream::SkipCallback callback)
      : callback_(std::move(callback)),
        work_thread_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

  ~SkipCallbackWrapper() override {
    if (!callback_.is_null()) {
      std::move(callback_).Run(net::ERR_FAILED);
    }
  }

  void Continue(int64 bytes_skipped) override {
    if (!work_thread_task_runner_->RunsTasksInCurrentSequence()) {
      work_thread_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&SkipCallbackWrapper::Continue, this, bytes_skipped));
      return;
    }
    if (!callback_.is_null()) {
      std::move(callback_).Run(bytes_skipped);
    }
  }

  void Disconnect() { callback_.Reset(); }

 private:
  InputStream::SkipCallback callback_;

  scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner_;

  IMPLEMENT_REFCOUNTING(SkipCallbackWrapper);
  DISALLOW_COPY_AND_ASSIGN(SkipCallbackWrapper);
};

class ReadCallbackWrapper : public CefResourceReadCallback {
 public:
  explicit ReadCallbackWrapper(InputStream::ReadCallback callback)
      : callback_(std::move(callback)),
        work_thread_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

  ~ReadCallbackWrapper() override {
    if (!callback_.is_null()) {
      std::move(callback_).Run(net::ERR_FAILED);
    }
  }

  void Continue(int bytes_read) override {
    if (!work_thread_task_runner_->RunsTasksInCurrentSequence()) {
      work_thread_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&ReadCallbackWrapper::Continue, this, bytes_read));
      return;
    }
    if (!callback_.is_null()) {
      std::move(callback_).Run(bytes_read);
    }
  }

  void Disconnect() { callback_.Reset(); }

 private:
  InputStream::ReadCallback callback_;

  scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner_;

  IMPLEMENT_REFCOUNTING(ReadCallbackWrapper);
  DISALLOW_COPY_AND_ASSIGN(ReadCallbackWrapper);
};

class ReadResponseCallbackWrapper : public CefCallback {
 public:
  ~ReadResponseCallbackWrapper() override {
    if (callback_) {
      callback_->Continue(net::ERR_FAILED);
    }
  }

  void Continue() override {
    CEF_POST_TASK(CEF_IOT,
                  base::Bind(&ReadResponseCallbackWrapper::DoRead, this));
  }

  void Cancel() override {
    CEF_POST_TASK(CEF_IOT,
                  base::Bind(&ReadResponseCallbackWrapper::DoCancel, this));
  }

  static void ReadResponse(CefRefPtr<CefResourceHandler> handler,
                           net::IOBuffer* dest,
                           int length,
                           CefRefPtr<ReadCallbackWrapper> callback) {
    CEF_POST_TASK(
        CEF_IOT, base::Bind(ReadResponseCallbackWrapper::ReadResponseOnIOThread,
                            handler, base::Unretained(dest), length, callback));
  }

 private:
  ReadResponseCallbackWrapper(CefRefPtr<CefResourceHandler> handler,
                              net::IOBuffer* dest,
                              int length,
                              CefRefPtr<ReadCallbackWrapper> callback)
      : handler_(handler), dest_(dest), length_(length), callback_(callback) {}

  static void ReadResponseOnIOThread(CefRefPtr<CefResourceHandler> handler,
                                     net::IOBuffer* dest,
                                     int length,
                                     CefRefPtr<ReadCallbackWrapper> callback) {
    CEF_REQUIRE_IOT();
    CefRefPtr<ReadResponseCallbackWrapper> callbackWrapper =
        new ReadResponseCallbackWrapper(handler, dest, length, callback);
    callbackWrapper->DoRead();
  }

  void DoRead() {
    CEF_REQUIRE_IOT();
    if (!callback_)
      return;

    int bytes_read = 0;
    bool result =
        handler_->ReadResponse(dest_->data(), length_, bytes_read, this);
    if (result) {
      if (bytes_read > 0) {
        // Continue immediately.
        callback_->Continue(bytes_read);
        callback_ = nullptr;
      }
      return;
    }

    // Signal response completion immediately.
    callback_->Continue(0);
    callback_ = nullptr;
  }

  void DoCancel() {
    CEF_REQUIRE_IOT();
    if (callback_) {
      callback_->Continue(net::ERR_FAILED);
      callback_ = nullptr;
    }
  }

  CefRefPtr<CefResourceHandler> handler_;
  net::IOBuffer* const dest_;
  int length_;
  CefRefPtr<ReadCallbackWrapper> callback_;

  IMPLEMENT_REFCOUNTING(ReadResponseCallbackWrapper);
  DISALLOW_COPY_AND_ASSIGN(ReadResponseCallbackWrapper);
};

class InputStreamWrapper : public InputStream {
 public:
  explicit InputStreamWrapper(CefRefPtr<CefResourceHandler> handler)
      : handler_(handler) {}
  ~InputStreamWrapper() override {}

  // InputStream methods:
  bool Skip(int64_t n, int64_t* bytes_skipped, SkipCallback callback) override {
    CefRefPtr<SkipCallbackWrapper> callbackWrapper =
        new SkipCallbackWrapper(std::move(callback));
    bool result = handler_->Skip(n, *bytes_skipped, callbackWrapper.get());
    if (result) {
      if (*bytes_skipped > 0) {
        // Continue immediately.
        callbackWrapper->Disconnect();
      }
      return true;
    }

    // Cancel immediately.
    return false;
  }

  bool Read(net::IOBuffer* dest,
            int length,
            int* bytes_read,
            ReadCallback callback) override {
    CefRefPtr<ReadCallbackWrapper> callbackWrapper =
        new ReadCallbackWrapper(std::move(callback));
    bool result = handler_->Read(dest->data(), length, *bytes_read,
                                 callbackWrapper.get());
    if (result) {
      if (*bytes_read > 0) {
        // Continue immediately.
        callbackWrapper->Disconnect();
      }
      return true;
    }

    if (*bytes_read == -1) {
      // Call ReadResponse on the IO thread.
      ReadResponseCallbackWrapper::ReadResponse(handler_, dest, length,
                                                callbackWrapper);
      *bytes_read = 0;
      return true;
    }

    // Complete or cancel immediately.
    return false;
  }

 private:
  CefRefPtr<CefResourceHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(InputStreamWrapper);
};

class OpenCallbackWrapper : public CefCallback {
 public:
  OpenCallbackWrapper(ResourceResponse::OpenCallback callback,
                      std::unique_ptr<InputStreamWrapper> stream)
      : callback_(std::move(callback)),
        stream_(std::move(stream)),
        work_thread_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

  ~OpenCallbackWrapper() override {
    if (!callback_.is_null()) {
      Execute(std::move(callback_), std::move(stream_), false);
    }
  }

  void Continue() override {
    if (!work_thread_task_runner_->RunsTasksInCurrentSequence()) {
      work_thread_task_runner_->PostTask(
          FROM_HERE, base::Bind(&OpenCallbackWrapper::Continue, this));
      return;
    }
    if (!callback_.is_null()) {
      Execute(std::move(callback_), std::move(stream_), true);
    }
  }

  void Cancel() override {
    if (!work_thread_task_runner_->RunsTasksInCurrentSequence()) {
      work_thread_task_runner_->PostTask(
          FROM_HERE, base::Bind(&OpenCallbackWrapper::Cancel, this));
      return;
    }
    if (!callback_.is_null()) {
      Execute(std::move(callback_), std::move(stream_), false);
    }
  }

 private:
  static void Execute(ResourceResponse::OpenCallback callback,
                      std::unique_ptr<InputStreamWrapper> stream,
                      bool cont) {
    std::move(callback).Run(cont ? std::move(stream) : nullptr);
  }

  ResourceResponse::OpenCallback callback_;
  std::unique_ptr<InputStreamWrapper> stream_;

  scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner_;

  IMPLEMENT_REFCOUNTING(OpenCallbackWrapper);
  DISALLOW_COPY_AND_ASSIGN(OpenCallbackWrapper);
};

void CallProcessRequestOnIOThread(
    CefRefPtr<CefResourceHandler> handler,
    CefRefPtr<CefRequestImpl> request,
    CefRefPtr<OpenCallbackWrapper> callbackWrapper) {
  CEF_REQUIRE_IOT();
  if (!handler->ProcessRequest(request.get(), callbackWrapper.get())) {
    callbackWrapper->Cancel();
  }
}

class ResourceResponseWrapper : public ResourceResponse {
 public:
  ResourceResponseWrapper(const RequestId request_id,
                          CefRefPtr<CefResourceHandler> handler)
      : request_id_(request_id), handler_(handler) {
    DCHECK(handler_);
  }
  ~ResourceResponseWrapper() override {}

  // ResourceResponse methods:
  bool OpenInputStream(const RequestId& request_id,
                       const network::ResourceRequest& request,
                       OpenCallback callback) override {
    DCHECK_EQ(request_id, request_id_);

    // May be recreated on redirect.
    request_ = new CefRequestImpl();
    request_->Set(&request, request_id.hash());
    request_->SetReadOnly(true);

    CefRefPtr<OpenCallbackWrapper> callbackWrapper = new OpenCallbackWrapper(
        std::move(callback), std::make_unique<InputStreamWrapper>(handler_));
    bool handle_request = false;
    bool result =
        handler_->Open(request_.get(), handle_request, callbackWrapper.get());
    if (result) {
      if (handle_request) {
        // Continue immediately.
        callbackWrapper->Continue();
      }
      return true;
    }

    if (handle_request) {
      // Cancel immediately.
      callbackWrapper->Cancel();
      return true;
    }

    // Call ProcessRequest on the IO thread.
    CEF_POST_TASK(CEF_IOT, base::Bind(CallProcessRequestOnIOThread, handler_,
                                      request_, callbackWrapper));
    return true;
  }

  void GetResponseHeaders(const RequestId& request_id,
                          int* status_code,
                          std::string* reason_phrase,
                          std::string* mime_type,
                          std::string* charset,
                          int64_t* content_length,
                          HeaderMap* extra_headers) override {
    DCHECK_EQ(request_id, request_id_);
    CEF_REQUIRE_IOT();

    CefRefPtr<CefResponse> response = CefResponse::Create();
    int64_t response_length = -1;
    CefString redirect_url;
    handler_->GetResponseHeaders(response, response_length, redirect_url);

    const auto error_code = response->GetError();
    if (error_code != ERR_NONE) {
      // Early exit if the handler reported an error.
      *status_code = error_code;
      return;
    }

    if (!redirect_url.empty()) {
      // Perform a redirect.
      *status_code = net::HTTP_TEMPORARY_REDIRECT;
      *reason_phrase = std::string();
      extra_headers->insert(
          std::make_pair(kHTTPLocationHeaderName, redirect_url));
    } else {
      *status_code = response->GetStatus();
      *reason_phrase = response->GetStatusText();
    }

    if (reason_phrase->empty() && *status_code > 0) {
      *reason_phrase = net::GetHttpReasonPhrase(
          static_cast<net::HttpStatusCode>(*status_code));
    }

    *mime_type = response->GetMimeType();
    *charset = response->GetCharset();

    // A |content_length| value may already be specified if the request included
    // a Range header.
    if (response_length >= 0 && *content_length == -1)
      *content_length = response_length;

    CefResponse::HeaderMap headerMap;
    response->GetHeaderMap(headerMap);
    for (const auto& value : headerMap) {
      extra_headers->insert(std::make_pair(value.first, value.second));
    }
  }

 private:
  const RequestId request_id_;

  CefRefPtr<CefRequestImpl> request_;
  CefRefPtr<CefResourceHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(ResourceResponseWrapper);
};

}  // namespace

std::unique_ptr<ResourceResponse> CreateResourceResponse(
    const RequestId& request_id,
    CefRefPtr<CefResourceHandler> handler) {
  return std::make_unique<ResourceResponseWrapper>(request_id, handler);
}

}  // namespace net_service
