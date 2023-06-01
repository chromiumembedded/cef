// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/test/test_server_impl.h"

#include "libcef/common/net/http_header_utils.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"

using namespace net::test_server;

namespace {

class CefTestServerConnectionImpl : public CefTestServerConnection {
 public:
  explicit CefTestServerConnectionImpl(
      base::WeakPtr<HttpResponseDelegate> delegate)
      : delegate_(delegate),
        task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
    DCHECK(delegate_);
    DCHECK(task_runner_);
  }

  void SendHttp200Response(const CefString& content_type,
                           const void* data,
                           size_t data_size) override {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type(base::StringPiece(content_type.ToString()));
    response->set_content(
        base::StringPiece(reinterpret_cast<const char*>(data), data_size));
    SendBasicHttpResponse(std::move(response));
  }

  void SendHttp404Response() override {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_NOT_FOUND);
    SendBasicHttpResponse(std::move(response));
  }

  void SendHttp500Response(const CefString& error_message) override {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    response->set_content_type(base::StringPiece("text/html"));
    response->set_content(base::StringPiece(error_message.ToString()));
    SendBasicHttpResponse(std::move(response));
  }

  void SendHttpResponse(int response_code,
                        const CefString& content_type,
                        const void* data,
                        size_t data_size,
                        const HeaderMap& extra_headers) override {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(static_cast<net::HttpStatusCode>(response_code));
    response->set_content_type(base::StringPiece(content_type.ToString()));
    response->set_content(
        base::StringPiece(reinterpret_cast<const char*>(data), data_size));
    for (const auto& [key, value] : extra_headers) {
      response->AddCustomHeader(key.ToString(), value.ToString());
    }
    SendBasicHttpResponse(std::move(response));
  }

 private:
  void SendBasicHttpResponse(std::unique_ptr<BasicHttpResponse> response) {
    if (!task_runner_->BelongsToCurrentThread()) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CefTestServerConnectionImpl::SendBasicHttpResponse,
                         this, std::move(response)));
      return;
    }

    if (delegate_) {
      response->SendResponse(delegate_);
    }
  }

  base::WeakPtr<HttpResponseDelegate> delegate_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  IMPLEMENT_REFCOUNTING(CefTestServerConnectionImpl);
};

class CefHttpResponse : public HttpResponse {
 public:
  CefHttpResponse(CefRefPtr<CefTestServer> server,
                  CefRefPtr<CefTestServerHandler> handler,
                  CefRefPtr<CefRequest> request)
      : server_(server), handler_(handler), request_(request) {
    DCHECK(server_);
    DCHECK(handler_);
    DCHECK(request_);
  }

  CefHttpResponse(const CefHttpResponse&) = delete;
  CefHttpResponse& operator=(const CefHttpResponse&) = delete;

  void SendResponse(base::WeakPtr<HttpResponseDelegate> delegate) override {
    CefRefPtr<CefTestServerConnectionImpl> connection(
        new CefTestServerConnectionImpl(delegate));
    const bool handled =
        handler_->OnTestServerRequest(server_, request_, connection.get());
    if (handled) {
      return;
    }

    LOG(WARNING) << "Request not handled. Returning 404: "
                 << request_->GetURL().ToString();
    connection->SendHttp404Response();
  }

 private:
  CefRefPtr<CefTestServer> server_;
  CefRefPtr<CefTestServerHandler> handler_;
  CefRefPtr<CefRequest> request_;
};

CefRefPtr<CefRequest> CreateCefRequest(const HttpRequest& request) {
  CefRefPtr<CefPostData> post_data;
  if (!request.content.empty()) {
    post_data = CefPostData::Create();
    auto element = CefPostDataElement::Create();
    element->SetToBytes(request.content.size(), request.content.c_str());
    post_data->AddElement(element);
  }

  CefRequest::HeaderMap header_map;
  CefString referer;

  HttpHeaderUtils::ParseHeaders(request.all_headers, header_map);

  // CefRequest will strip the Referer header from the map, so we don't need to
  // do that here.
  for (const auto& [key, value] : header_map) {
    if (base::EqualsCaseInsensitiveASCII(key.ToString(),
                                         net::HttpRequestHeaders::kReferer)) {
      referer = value;
    }
  }

  auto cef_request = CefRequest::Create();
  cef_request->Set(request.GetURL().spec(), request.method_string, post_data,
                   header_map);
  if (!referer.empty()) {
    cef_request->SetReferrer(referer, REFERRER_POLICY_DEFAULT);
  }
  return cef_request;
}

}  // namespace

class CefTestServerImpl::Context {
 public:
  Context(CefRefPtr<CefTestServer> server,
          CefRefPtr<CefTestServerHandler> handler)
      : server_(server), handler_(handler) {
    DCHECK(server_);
    DCHECK(handler_);
  }

  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  ~Context() {
    // The server should not be running.
    DCHECK(!test_server_);
  }

  bool Start(uint16_t port,
             bool https_server,
             cef_test_cert_type_t https_cert_type) {
    DCHECK(thread_checker_.CalledOnValidThread());

    DCHECK(!test_server_);
    test_server_ = std::make_unique<EmbeddedTestServer>(
        https_server ? EmbeddedTestServer::TYPE_HTTPS
                     : EmbeddedTestServer::TYPE_HTTP);

    // Unretained is safe because Stop is called before |this| is destroyed.
    test_server_->RegisterRequestHandler(
        base::BindRepeating(&Context::HandleRequest, base::Unretained(this)));

    if (https_server) {
      switch (https_cert_type) {
        case CEF_TEST_CERT_OK_IP:
          // Default value.
          break;
        case CEF_TEST_CERT_OK_DOMAIN:
          test_server_->SetSSLConfig(
              EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
          break;
        case CEF_TEST_CERT_EXPIRED:
          test_server_->SetSSLConfig(EmbeddedTestServer::CERT_EXPIRED);
          break;
      }
    }

    test_server_handle_ =
        test_server_->StartAndReturnHandle(static_cast<int>(port));
    if (!test_server_handle_) {
      test_server_.reset();
      return false;
    }

    origin_ = test_server_->base_url();
    return true;
  }

  void Stop() {
    // Should be called on the creation thread.
    DCHECK(thread_checker_.CalledOnValidThread());

    DCHECK(test_server_);

    // Destruction of |test_server_handle_| will stop the server and block until
    // the dedicated server thread has shut down.
    test_server_handle_ = EmbeddedTestServerHandle();
    test_server_.reset();

    server_ = nullptr;
    handler_ = nullptr;
  }

  const GURL& origin() const { return origin_; }

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    // Should be on the dedicated server thread.
    DCHECK(!thread_checker_.CalledOnValidThread());
    return std::make_unique<CefHttpResponse>(server_, handler_,
                                             CreateCefRequest(request));
  }

  // Safe to access on any thread.
  CefRefPtr<CefTestServer> server_;
  CefRefPtr<CefTestServerHandler> handler_;
  GURL origin_;

  base::ThreadChecker thread_checker_;

  // Only accessed on the creation thread.
  std::unique_ptr<EmbeddedTestServer> test_server_;
  EmbeddedTestServerHandle test_server_handle_;
};

bool CefTestServerImpl::Start(uint16_t port,
                              bool https_server,
                              cef_test_cert_type_t https_cert_type,
                              CefRefPtr<CefTestServerHandler> handler) {
  DCHECK(!context_);
  context_ = std::make_unique<CefTestServerImpl::Context>(this, handler);
  if (context_->Start(port, https_server, https_cert_type)) {
    const auto& origin = context_->origin().spec();
    // Remove the trailing '/'
    origin_ = origin.substr(0, origin.length() - 1);
    return true;
  }

  context_.reset();
  return false;
}

void CefTestServerImpl::Stop() {
  DCHECK(context_);
  context_->Stop();
  context_.reset();
}

CefString CefTestServerImpl::GetOrigin() {
  return origin_;
}

// static
CefRefPtr<CefTestServer> CefTestServer::CreateAndStart(
    uint16_t port,
    bool https_server,
    cef_test_cert_type_t https_cert_type,
    CefRefPtr<CefTestServerHandler> handler) {
  CefRefPtr<CefTestServerImpl> server(new CefTestServerImpl());
  if (server->Start(port, https_server, https_cert_type, handler)) {
    return server;
  }
  return nullptr;
}
