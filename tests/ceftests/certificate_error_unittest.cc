// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/test/cef_test_server.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_server_observer.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/common/string_util.h"

namespace {

// Used to observe HTTP and HTTPS server requests.
class OtherServerObserver : public test_server::ObserverHelper {
 public:
  using ReadyCallback = base::OnceCallback<void(const std::string& url)>;
  using RequestCallback =
      base::RepeatingCallback<bool(CefRefPtr<CefRequest> request,
                                   const ResponseCallback& response_callback)>;

  OtherServerObserver(bool https_server,
                      ReadyCallback ready_callback,
                      base::OnceClosure done_callback,
                      const RequestCallback& request_callback)
      : ready_callback_(std::move(ready_callback)),
        done_callback_(std::move(done_callback)),
        request_callback_(request_callback) {
    EXPECT_TRUE(ready_callback_);
    EXPECT_TRUE(done_callback_);
    EXPECT_TRUE(request_callback_);
    Initialize(https_server);
  }

  void OnInitialized(const std::string& server_origin) override {
    EXPECT_UI_THREAD();
    std::move(ready_callback_).Run(server_origin);
  }

  void OnShutdown() override {
    EXPECT_UI_THREAD();
    std::move(done_callback_).Run();
    delete this;
  }

  bool OnTestServerRequest(CefRefPtr<CefRequest> request,
                           const ResponseCallback& response_callback) override {
    EXPECT_UI_THREAD();
    return request_callback_.Run(request, response_callback);
  }

 private:
  ReadyCallback ready_callback_;
  base::OnceClosure done_callback_;
  RequestCallback request_callback_;

  DISALLOW_COPY_AND_ASSIGN(OtherServerObserver);
};

class CertificateErrorTest : public TestHandler, public CefTestServerHandler {
 public:
  using ResponseCallback = test_server::ObserverHelper::ResponseCallback;

  enum class OtherServerType {
    NONE,
    HTTP,
    HTTPS,
  };

  CertificateErrorTest(
      cef_test_cert_type_t cert_type,
      bool expect_load_success,
      bool expect_certificate_error,
      OtherServerType other_server_type = OtherServerType::NONE)
      : cert_type_(cert_type),
        expect_load_success_(expect_load_success),
        expect_certificate_error_(expect_certificate_error),
        other_server_type_(other_server_type) {}

  void RunTest() override {
    SetTestTimeout();
    CefPostTask(TID_UI,
                base::BindOnce(&CertificateErrorTest::StartHttpsServer, this));
  }

  bool OnTestServerRequest(
      CefRefPtr<CefTestServer> server,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefTestServerConnection> connection) override {
    CefPostTask(TID_UI,
                base::BindOnce(&CertificateErrorTest::HandleHttpsRequest, this,
                               request, connection));
    return true;
  }

  bool OnOtherServerRequest(CefRefPtr<CefRequest> request,
                            const ResponseCallback& response_callback) {
    EXPECT_UI_THREAD();
    HandleOtherServerRequest(request, response_callback);
    return true;
  }

  bool OnCertificateError(CefRefPtr<CefBrowser> browser,
                          cef_errorcode_t cert_error,
                          const CefString& request_url,
                          CefRefPtr<CefSSLInfo> ssl_info,
                          CefRefPtr<CefCallback> callback) override {
    EXPECT_UI_THREAD();
    got_certificate_error_.yes();

    EXPECT_STREQ(GetEndURL().c_str(), request_url.ToString().c_str());

    if (expect_load_success_) {
      // Continue the load.
      callback->Continue();
      return true;
    }

    // Cancel the load.
    return false;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_UI_THREAD();

    TestHandler::OnLoadEnd(browser, frame, httpStatusCode);

    const std::string& url = frame->GetURL();
    if (url == GetEndURL()) {
      got_load_end_.yes();
      MaybeEndTest();
    }
  }

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override {
    EXPECT_UI_THREAD();

    const std::string& url = frame->GetURL();
    if (url == GetEndURL()) {
      got_load_error_.yes();
      MaybeEndTest();
    }
  }

  void DestroyTest() override {
    EXPECT_FALSE(server_);
    EXPECT_FALSE(other_server_);

    EXPECT_TRUE(got_load_end_);

    if (expect_load_success_) {
      EXPECT_TRUE(got_request_);
      EXPECT_FALSE(got_load_error_);
    } else {
      EXPECT_FALSE(got_request_);
      EXPECT_TRUE(got_load_error_);
    }

    EXPECT_EQ(expect_certificate_error_, got_certificate_error_);

    TestHandler::DestroyTest();
  }

 protected:
  virtual std::string GetStartURL() const {
    return server_origin() + "/index.html";
  }
  virtual std::string GetEndURL() const { return GetStartURL(); }

  const std::string& server_origin() const { return server_origin_; }
  const std::string& other_origin() const { return other_origin_; }

  virtual void HandleOtherServerRequest(
      CefRefPtr<CefRequest> request,
      const ResponseCallback& response_callback) {}

 private:
  void HandleHttpsRequest(CefRefPtr<CefRequest> request,
                          CefRefPtr<CefTestServerConnection> connection) {
    EXPECT_UI_THREAD();
    got_request_.yes();

    const std::string response = "<html><body>Certificate Test</body></html>";
    connection->SendHttp200Response("text/html", response.c_str(),
                                    response.size());

    MaybeEndTest();
  }

  void MaybeEndTest() {
    EXPECT_UI_THREAD();

    bool end_test;
    if (expect_load_success_) {
      end_test = got_load_end_ && got_request_;
    } else {
      end_test = got_load_end_ && got_load_error_;
    }

    if (end_test) {
      StopHttpsServer();
    }
  }

  void StartHttpsServer() {
    EXPECT_UI_THREAD();

    server_ = CefTestServer::CreateAndStart(/*port=*/0, /*https_server=*/true,
                                            cert_type_, this);
    server_origin_ = server_->GetOrigin();

    if (other_server_type_ != OtherServerType::NONE) {
      StartOtherServer();
    } else {
      DoCreateBrowser();
    }
  }

  void StartOtherServer() {
    EXPECT_UI_THREAD();
    EXPECT_NE(other_server_type_, OtherServerType::NONE);

    // Will delete itself after the server stops.
    other_server_ = new OtherServerObserver(
        other_server_type_ == OtherServerType::HTTPS,
        base::BindOnce(&CertificateErrorTest::StartedOtherServer, this),
        base::BindOnce(&CertificateErrorTest::StoppedOtherServer, this),
        base::BindRepeating(&CertificateErrorTest::OnOtherServerRequest, this));
  }

  void StartedOtherServer(const std::string& other_origin) {
    EXPECT_UI_THREAD();
    EXPECT_NE(other_server_type_, OtherServerType::NONE);

    other_origin_ = other_origin;
    DoCreateBrowser();
  }

  void DoCreateBrowser() {
    EXPECT_UI_THREAD();

    // Create a new in-memory context so certificate decisions aren't cached.
    CreateTestRequestContext(
        TEST_RC_MODE_CUSTOM_WITH_HANDLER, /*cache_path=*/std::string(),
        base::BindOnce(&CertificateErrorTest::DoCreateBrowserContinue, this));
  }

  void DoCreateBrowserContinue(CefRefPtr<CefRequestContext> request_context) {
    EXPECT_UI_THREAD();

    CreateBrowser(GetStartURL(), request_context);
  }

  void StopHttpsServer() {
    EXPECT_UI_THREAD();

    server_->Stop();
    server_ = nullptr;

    if (other_server_type_ != OtherServerType::NONE) {
      // Stop the other server. Results in a call to StoppedOtherServer().
      other_server_->Shutdown();
    } else {
      DestroyTest();
    }
  }

  void StoppedOtherServer() {
    EXPECT_UI_THREAD();
    EXPECT_NE(other_server_type_, OtherServerType::NONE);

    other_server_ = nullptr;
    DestroyTest();
  }

  const cef_test_cert_type_t cert_type_;
  const bool expect_load_success_;
  const bool expect_certificate_error_;
  const OtherServerType other_server_type_;

  CefRefPtr<CefTestServer> server_;
  std::string server_origin_;

  OtherServerObserver* other_server_ = nullptr;
  std::string other_origin_;

  TrackCallback got_request_;
  TrackCallback got_certificate_error_;
  TrackCallback got_load_end_;
  TrackCallback got_load_error_;

  IMPLEMENT_REFCOUNTING(CertificateErrorTest);
};

}  // namespace

TEST(CertificateErrorTest, DirectNoError) {
  CefRefPtr<CertificateErrorTest> handler = new CertificateErrorTest(
      /*cert_type=*/CEF_TEST_CERT_OK_DOMAIN, /*expect_load_success=*/true,
      /*expect_certificate_error=*/false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(CertificateErrorTest, DirectExpired) {
  CefRefPtr<CertificateErrorTest> handler = new CertificateErrorTest(
      /*cert_type=*/CEF_TEST_CERT_EXPIRED, /*expect_load_success=*/false,
      /*expect_certificate_error=*/true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

class DirectMismatchedTest : public CertificateErrorTest {
 public:
  explicit DirectMismatchedTest(bool continue_invalid_certificate)
      : CertificateErrorTest(
            /*cert_type=*/CEF_TEST_CERT_OK_DOMAIN,
            /*expect_load_success=*/continue_invalid_certificate,
            /*expect_certificate_error=*/true) {}

 protected:
  std::string GetStartURL() const override {
    // Load by IP address when the certificate expects a domain.
    return client::AsciiStrReplace(server_origin(), "localhost", "127.0.0.1") +
           "/index.html";
  }
};

}  // namespace

TEST(CertificateErrorTest, DirectMismatchedCancel) {
  CefRefPtr<DirectMismatchedTest> handler =
      new DirectMismatchedTest(/*continue_invalid_certificate=*/false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(CertificateErrorTest, DirectMismatchedContinue) {
  CefRefPtr<DirectMismatchedTest> handler =
      new DirectMismatchedTest(/*continue_invalid_certificate=*/true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

class RedirectMismatchedFromHandlerTest : public CertificateErrorTest {
 public:
  RedirectMismatchedFromHandlerTest(bool continue_invalid_certificate)
      : CertificateErrorTest(
            /*cert_type=*/CEF_TEST_CERT_OK_DOMAIN,
            /*expect_load_success=*/continue_invalid_certificate,
            /*expect_certificate_error=*/true) {}

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    EXPECT_IO_THREAD();

    const std::string& url = request->GetURL();
    if (url == GetStartURL()) {
      // Redirect to the end URL.
      return new CefStreamResourceHandler(
          301, "Permanent Redirect", "text/html", {{"Location", GetEndURL()}},
          /*stream=*/nullptr);
    }

    return TestHandler::GetResourceHandler(browser, frame, request);
  }

 protected:
  std::string GetStartURL() const override {
    return "https://certificate-test.com/index.html";
  }

  std::string GetEndURL() const override {
    // Load by IP address when the certificate expects a domain.
    return client::AsciiStrReplace(server_origin(), "localhost", "127.0.0.1") +
           "/index.html";
  }
};

}  // namespace

TEST(CertificateErrorTest, RedirectMismatchedFromHttpsResourceCancel) {
  CefRefPtr<RedirectMismatchedFromHandlerTest> handler =
      new RedirectMismatchedFromHandlerTest(
          /*continue_invalid_certificate=*/false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(CertificateErrorTest, RedirectMismatchedFromHttpsResourceContinue) {
  CefRefPtr<RedirectMismatchedFromHandlerTest> handler =
      new RedirectMismatchedFromHandlerTest(
          /*continue_invalid_certificate=*/true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

class RedirectMismatchedFromServerTest : public CertificateErrorTest {
 public:
  RedirectMismatchedFromServerTest(bool continue_invalid_certificate,
                                   bool redirect_from_https)
      : CertificateErrorTest(
            /*cert_type=*/CEF_TEST_CERT_OK_DOMAIN,
            /*expect_load_success=*/continue_invalid_certificate,
            /*expect_certificate_error=*/true,
            redirect_from_https ? OtherServerType::HTTPS
                                : OtherServerType::HTTP) {}

  void HandleOtherServerRequest(
      CefRefPtr<CefRequest> request,
      const ResponseCallback& response_callback) override {
    EXPECT_UI_THREAD();

    EXPECT_FALSE(got_other_request_);
    got_other_request_.yes();

    const std::string& url = request->GetURL();
    EXPECT_STREQ(GetStartURL().c_str(), url.c_str());

    // Redirect to the end URL.
    auto response = CefResponse::Create();
    response->SetMimeType("text/html");
    response->SetStatus(301);  // Permanent Redirect
    response->SetHeaderByName("Location", GetEndURL(),
                              /*overwrite=*/true);

    response_callback.Run(response, /*response_body=*/std::string());
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_other_request_);
    CertificateErrorTest::DestroyTest();
  }

 protected:
  std::string GetStartURL() const override {
    return other_origin() + "/index.html";
  }

  std::string GetEndURL() const override {
    // Load by IP address when the certificate expects a domain.
    return client::AsciiStrReplace(server_origin(), "localhost", "127.0.0.1") +
           "/index.html";
  }

 private:
  TrackCallback got_other_request_;
};

}  // namespace

TEST(CertificateErrorTest, RedirectMismatchedFromHttpsServerCancel) {
  CefRefPtr<RedirectMismatchedFromServerTest> handler =
      new RedirectMismatchedFromServerTest(
          /*continue_invalid_certificate=*/false, /*redirect_from_https=*/true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(CertificateErrorTest, RedirectMismatchedFromHttpsServerContinue) {
  CefRefPtr<RedirectMismatchedFromServerTest> handler =
      new RedirectMismatchedFromServerTest(
          /*continue_invalid_certificate=*/true, /*redirect_from_https=*/true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(CertificateErrorTest, RedirectMismatchedFromHttpServerCancel) {
  CefRefPtr<RedirectMismatchedFromServerTest> handler =
      new RedirectMismatchedFromServerTest(
          /*continue_invalid_certificate=*/false,
          /*redirect_from_https=*/false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(CertificateErrorTest, RedirectMismatchedFromHttpServerContinue) {
  CefRefPtr<RedirectMismatchedFromServerTest> handler =
      new RedirectMismatchedFromServerTest(
          /*continue_invalid_certificate=*/true, /*redirect_from_https=*/false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
