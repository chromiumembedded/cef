// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/test/cef_test_helpers.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_server.h"
#include "tests/ceftests/test_server_observer.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/common/string_util.h"

namespace {

// Set the "Strict-Transport-Security" header on an HTTPS response to enable
// HSTS redirects for follow-up HTTP requests to the same origin. See
// https://www.chromium.org/hsts/.
//
// HSTS is implemented in the network service so real servers are required to
// test the redirect behavior. It also requires a "localhost" domain certificate
// instead of IP address (see https://crbug.com/456712). See additional comments
// in OnResourceRedirect about redirect behavior with non-standard port numbers.
//
// The test works as follows:
// 1. Start HTTP and HTTPS servers.
// 2. Load an HTTP URL that redirects to an HTTPS URL.
// 3. Set the "Strict-Transport-Security" header in response to the first HTTPS
//    request.
// 4. Load the same HTTP URL additional times to trigger the internal HTTP to
//    HTTPS redirect.

// Number of times to load the same HTTP URL. Must be > 1.
constexpr size_t kHSTSLoadCount = 3;

constexpr char kHSTSURLPath[] = "/index.html";

// Used to observe HTTP and HTTPS server requests.
class HSTSTestServerObserver : public test_server::ObserverHelper {
 public:
  using ReadyCallback = base::OnceCallback<void(const std::string& url)>;

  HSTSTestServerObserver(bool https_server,
                         size_t& nav_ct,
                         TrackCallback (&got_request)[kHSTSLoadCount],
                         ReadyCallback ready_callback,
                         base::OnceClosure done_callback)
      : https_server_(https_server),
        nav_ct_(nav_ct),
        got_request_(got_request),
        ready_callback_(std::move(ready_callback)),
        done_callback_(std::move(done_callback)) {
    Initialize(https_server);
  }

  void OnInitialized(const std::string& server_origin) override {
    EXPECT_UI_THREAD();

    origin_ = ToLocalhostOrigin(server_origin);
    url_ = origin_ + kHSTSURLPath;

    std::move(ready_callback_).Run(url_);
  }

  void OnShutdown() override {
    EXPECT_UI_THREAD();
    std::move(done_callback_).Run();
    delete this;
  }

  bool OnTestServerRequest(CefRefPtr<CefRequest> request,
                           const ResponseCallback& response_callback) override {
    EXPECT_UI_THREAD();

    // At most 1 request per load.
    EXPECT_FALSE(got_request_[nav_ct_]) << nav_ct_;
    got_request_[nav_ct_].yes();

    const std::string& url = ToLocalhostOrigin(request->GetURL());

    auto response = CefResponse::Create();
    response->SetMimeType("text/html");
    std::string response_body;

    if (!https_server_) {
      // Redirect to the HTTPS URL.
      EXPECT_STREQ(url_.c_str(), url.c_str()) << nav_ct_;
      response->SetStatus(301);  // Permanent Redirect
      response->SetHeaderByName("Location",
                                GetLocalhostURL(/*https_server=*/true),
                                /*overwrite=*/true);
    } else {
      // Normal response after an HTTP to HTTPS redirect.
      EXPECT_STREQ(url_.c_str(), url.c_str()) << nav_ct_;
      response->SetStatus(200);

      if (nav_ct_ == 0) {
        // Set the "Strict-Transport-Security" header in response to the first
        // HTTPS request.
        response->SetHeaderByName("Strict-Transport-Security",
                                  "max-age=16070400",
                                  /*overwrite=*/true);
      }

      // Don't cache the HTTPS response (so we see all the requests).
      response->SetHeaderByName("Cache-Control", "no-cache",
                                /*overwrite=*/true);

      response_body = "<html><body>Test1</body></html>";
    }

    response_callback.Run(response, response_body);

    // Stop propagating the callback.
    return true;
  }

 private:
  static std::string ToLocalhostOrigin(const std::string& origin) {
    // Need to explicitly use the "localhost" domain instead of the IP address.
    // HTTPS URLs will already be using "localhost".
    return client::AsciiStrReplace(origin, "127.0.0.1", "localhost");
  }

  static std::string GetLocalhostOrigin(bool https_server) {
    return ToLocalhostOrigin(test_server::GetOrigin(https_server));
  }

  static std::string GetLocalhostURL(bool https_server) {
    return GetLocalhostOrigin(/*https_server=*/true) + kHSTSURLPath;
  }

  const bool https_server_;

  size_t& nav_ct_;
  TrackCallback (&got_request_)[kHSTSLoadCount];
  ReadyCallback ready_callback_;
  base::OnceClosure done_callback_;

  std::string origin_;
  std::string url_;

  DISALLOW_COPY_AND_ASSIGN(HSTSTestServerObserver);
};

class HSTSRedirectTest : public TestHandler {
 public:
  HSTSRedirectTest() = default;

  void RunTest() override {
    SetTestTimeout();
    CefPostTask(TID_UI,
                base::BindOnce(&HSTSRedirectTest::StartHttpServer, this));
  }

  void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefResponse> response,
                          CefString& new_url) override {
    EXPECT_IO_THREAD();

    EXPECT_FALSE(got_redirect_[nav_ct_]) << nav_ct_;
    got_redirect_[nav_ct_].yes();

    EXPECT_STREQ(http_url_.c_str(), request->GetURL().ToString().c_str())
        << nav_ct_;

    if (nav_ct_ == 0) {
      // Initial HTTP to HTTPS redirect.
      EXPECT_STREQ(https_url_.c_str(), new_url.ToString().c_str()) << nav_ct_;
    } else {
      // HSTS HTTP to HTTPS redirect. This will use the wrong "localhost" port
      // number, per spec. From RFC 6797:
      //   The UA MUST replace the URI scheme with "https" [RFC2818], and if the
      //   URI contains an explicit port component of "80", then the UA MUST
      //   convert the port component to be "443", or if the URI contains an
      //   explicit port component that is not equal to "80", the port component
      //   value MUST be preserved; otherwise, if the URI does not contain an
      //   explicit port component, the UA MUST NOT add one.
      //
      // This behavior is changed in M132 with
      // https://issues.chromium.org/issues/41251622.
      if (!CefIsFeatureEnabledForTests("IgnoreHSTSForLocalhost")) {
        const std::string& expected_https_url =
            client::AsciiStrReplace(http_url_, "http:", "https:");
        EXPECT_STREQ(expected_https_url.c_str(), new_url.ToString().c_str())
            << nav_ct_;

        // Redirect to the correct HTTPS URL instead.
        new_url = https_url_;
      }
    }
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_UI_THREAD();

    TestHandler::OnLoadEnd(browser, frame, httpStatusCode);

    EXPECT_FALSE(got_load_end_[nav_ct_]) << nav_ct_;
    got_load_end_[nav_ct_].yes();

    // Expect only the HTTPS URL to load.
    EXPECT_STREQ(https_url_.c_str(), frame->GetURL().ToString().c_str())
        << nav_ct_;

    if (++nav_ct_ == kHSTSLoadCount) {
      StopHttpServer();
    } else {
      // Load the same HTTP URL again.
      browser->GetMainFrame()->LoadURL(http_url_);
    }
  }

  void DestroyTest() override {
    EXPECT_FALSE(http_server_);
    EXPECT_FALSE(https_server_);

    EXPECT_EQ(kHSTSLoadCount, nav_ct_);
    for (size_t i = 0; i < kHSTSLoadCount; ++i) {
      EXPECT_TRUE(got_redirect_[i]) << i;
      EXPECT_TRUE(got_load_end_[i]) << i;
    }

    for (size_t i = 0; i < kHSTSLoadCount; ++i) {
      // Should only see the 1st HTTP request due to the internal HSTS redirect
      // for the 2nd+ requests.
      EXPECT_EQ(i == 0, got_http_request_[i]) << i;

      // Should see all HTTPS requests.
      EXPECT_TRUE(got_https_request_[i]) << i;
    }

    TestHandler::DestroyTest();
  }

 private:
  void StartHttpServer() {
    EXPECT_UI_THREAD();

    // Will delete itself after the server stops.
    http_server_ = new HSTSTestServerObserver(
        /*https_server=*/false, nav_ct_, got_http_request_,
        base::BindOnce(&HSTSRedirectTest::StartedHttpServer, this),
        base::BindOnce(&HSTSRedirectTest::StoppedHttpServer, this));
  }

  void StartedHttpServer(const std::string& url) {
    EXPECT_UI_THREAD();

    http_url_ = url;
    EXPECT_TRUE(http_url_.find("http://localhost:") == 0);

    // Start the HTTPS server. Will delete itself after the server stops.
    https_server_ = new HSTSTestServerObserver(
        /*https_server=*/true, nav_ct_, got_https_request_,
        base::BindOnce(&HSTSRedirectTest::StartedHttpsServer, this),
        base::BindOnce(&HSTSRedirectTest::StoppedHttpsServer, this));
  }

  void StartedHttpsServer(const std::string& url) {
    EXPECT_UI_THREAD();

    https_url_ = url;
    EXPECT_TRUE(https_url_.find("https://localhost:") == 0);

    // Create a new in-memory context so HSTS decisions aren't cached.
    CreateTestRequestContext(
        TEST_RC_MODE_CUSTOM_WITH_HANDLER, /*cache_path=*/std::string(),
        base::BindOnce(&HSTSRedirectTest::StartedHttpsServerContinue, this));
  }

  void StartedHttpsServerContinue(
      CefRefPtr<CefRequestContext> request_context) {
    EXPECT_UI_THREAD();

    CreateBrowser(http_url_, request_context);
  }

  void StopHttpServer() {
    EXPECT_UI_THREAD();

    // Results in a call to StoppedHttpServer().
    http_server_->Shutdown();
  }

  void StoppedHttpServer() {
    EXPECT_UI_THREAD();

    http_server_ = nullptr;

    // Stop the HTTPS server. Results in a call to StoppedHttpsServer().
    https_server_->Shutdown();
  }

  void StoppedHttpsServer() {
    EXPECT_UI_THREAD();

    https_server_ = nullptr;

    DestroyTest();
  }

  HSTSTestServerObserver* http_server_ = nullptr;
  std::string http_url_;

  HSTSTestServerObserver* https_server_ = nullptr;
  std::string https_url_;

  size_t nav_ct_ = 0U;
  TrackCallback got_http_request_[kHSTSLoadCount];
  TrackCallback got_https_request_[kHSTSLoadCount];
  TrackCallback got_load_end_[kHSTSLoadCount];
  TrackCallback got_redirect_[kHSTSLoadCount];

  IMPLEMENT_REFCOUNTING(HSTSRedirectTest);
};

}  // namespace

TEST(HSTSRedirectTest, Redirect) {
  CefRefPtr<HSTSRedirectTest> handler = new HSTSRedirectTest();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
