// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <set>
#include <vector>

#include "include/base/cef_callback.h"
#include "include/cef_callback.h"
#include "include/cef_origin_whitelist.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_request.h"
#include "tests/ceftests/test_server.h"
#include "tests/ceftests/test_util.h"
#include "tests/shared/browser/client_app_browser.h"

namespace {

// Browser-side app delegate.
class CorsBrowserTest : public client::ClientAppBrowser::Delegate {
 public:
  CorsBrowserTest() {}

  void OnContextInitialized(CefRefPtr<client::ClientAppBrowser> app) override {
    if (IsChromeRuntimeEnabled()) {
      // Disable InsecureFormNavigationThrottle which blocks 307 redirect of
      // POST requests from HTTPS to custom non-standard scheme causing the
      // CorsTest.RedirectPost307HttpSchemeToCustomNonStandardScheme test to
      // fail.
      CefRefPtr<CefValue> value = CefValue::Create();
      value->SetBool(false);
      CefString error;
      bool result = CefRequestContext::GetGlobalContext()->SetPreference(
          "profile.mixed_forms_warnings", value, error);
      CHECK(result) << error.ToString();
    }
  }

 private:
  IMPLEMENT_REFCOUNTING(CorsBrowserTest);
};

const char kMimeTypeHtml[] = "text/html";
const char kMimeTypeText[] = "text/plain";

const char kDefaultHtml[] = "<html><body>TEST</body></html>";
const char kDefaultText[] = "TEST";
const char kDefaultCookie[] = "testCookie=testVal";

const char kSuccessMsg[] = "CorsTestHandler.Success";
const char kFailureMsg[] = "CorsTestHandler.Failure";

// Source that will handle the request.
enum class HandlerType {
  SERVER,
  HTTP_SCHEME,
  CUSTOM_STANDARD_SCHEME,
  CUSTOM_NONSTANDARD_SCHEME,
  CUSTOM_UNREGISTERED_SCHEME,
};

std::string GetOrigin(HandlerType handler) {
  switch (handler) {
    case HandlerType::SERVER:
      return test_server::kServerOrigin;
    case HandlerType::HTTP_SCHEME:
      // Use HTTPS because requests from HTTP to the loopback address will be
      // blocked by https://chromestatus.com/feature/5436853517811712.
      return "https://corstest.com";
    case HandlerType::CUSTOM_STANDARD_SCHEME:
      // Standard scheme that's registered as CORS and fetch enabled.
      // Registered in scheme_handler_unittest.cc.
      return "customstdfetch://corstest";
    case HandlerType::CUSTOM_NONSTANDARD_SCHEME:
      // Non-standard schemes are not CORS or fetch enabled.
      // Registered in scheme_handler_unittest.cc.
      return "customnonstd:corstest";
    case HandlerType::CUSTOM_UNREGISTERED_SCHEME:
      // A scheme that isn't registered anywhere is treated as a non-standard
      // scheme.
      return "customstdunregistered://corstest";
  }
  NOTREACHED();
  return std::string();
}

std::string GetScheme(HandlerType handler) {
  switch (handler) {
    case HandlerType::SERVER:
      return test_server::kServerScheme;
    case HandlerType::HTTP_SCHEME:
      return "https";
    case HandlerType::CUSTOM_STANDARD_SCHEME:
      return "customstdfetch";
    case HandlerType::CUSTOM_NONSTANDARD_SCHEME:
      return "customnonstd";
    case HandlerType::CUSTOM_UNREGISTERED_SCHEME:
      return "customstdunregistered";
  }
  NOTREACHED();
  return std::string();
}

bool IsNonStandardType(HandlerType handler) {
  return handler == HandlerType::CUSTOM_NONSTANDARD_SCHEME ||
         handler == HandlerType::CUSTOM_UNREGISTERED_SCHEME;
}

bool IsStandardType(HandlerType handler) {
  return !IsNonStandardType(handler);
}

std::string GetPathURL(HandlerType handler, const std::string& path) {
  return GetOrigin(handler) + path;
}

struct Resource {
  // Uniquely identifies the resource.
  HandlerType handler = HandlerType::SERVER;
  std::string path;
  // If non-empty the method value must match.
  std::string method;

  // Response information that will be returned.
  CefRefPtr<CefResponse> response;
  std::string response_data;

  // Expected error code in OnLoadError.
  cef_errorcode_t expected_error_code = ERR_NONE;

  // Expected number of responses.
  int expected_response_ct = 1;

  // Expected number of OnQuery calls.
  int expected_success_query_ct = 0;
  int expected_failure_query_ct = 0;

  // Actual number of responses.
  int response_ct = 0;

  // Actual number of OnQuery calls.
  int success_query_ct = 0;
  int failure_query_ct = 0;

  Resource() {}
  Resource(HandlerType request_handler,
           const std::string& request_path,
           const std::string& mime_type = kMimeTypeHtml,
           const std::string& data = kDefaultHtml,
           int status = 200) {
    Init(request_handler, request_path, mime_type, data, status);
  }

  // Perform basic initialization.
  void Init(HandlerType request_handler,
            const std::string& request_path,
            const std::string& mime_type = kMimeTypeHtml,
            const std::string& data = kDefaultHtml,
            int status = 200) {
    handler = request_handler;
    path = request_path;
    response_data = data;
    response = CefResponse::Create();
    response->SetMimeType(mime_type);
    response->SetStatus(status);
  }

  // Validate expected initial state.
  void Validate() const {
    DCHECK(!path.empty());
    DCHECK(response);
    DCHECK(!response->GetMimeType().empty());
    DCHECK_EQ(0, response_ct);
    DCHECK_GE(expected_response_ct, 0);
  }

  std::string GetPathURL() const { return ::GetPathURL(handler, path); }

  // Returns true if all expectations have been met.
  bool IsDone() const {
    return response_ct == expected_response_ct &&
           success_query_ct == expected_success_query_ct &&
           failure_query_ct == expected_failure_query_ct;
  }

  void AssertDone() const {
    EXPECT_EQ(expected_response_ct, response_ct) << GetPathURL();
    EXPECT_EQ(expected_success_query_ct, success_query_ct) << GetPathURL();
    EXPECT_EQ(expected_failure_query_ct, failure_query_ct) << GetPathURL();
  }

  // Optionally override to verify request contents.
  virtual bool VerifyRequest(CefRefPtr<CefRequest> request) const {
    return true;
  }
};

struct TestSetup {
  // Available resources.
  typedef std::vector<Resource*> ResourceList;
  ResourceList resources;

  // Used for testing received console messages.
  std::vector<std::string> console_messages;

  // If true cookies will be cleared after every test run.
  bool clear_cookies = false;

  void AddResource(Resource* resource) {
    DCHECK(resource);
    resource->Validate();
    resources.push_back(resource);
  }

  void AddConsoleMessage(const std::string& message) {
    DCHECK(!message.empty());
    console_messages.push_back(message);
  }

  Resource* GetResource(const std::string& url,
                        const std::string& method = std::string()) const {
    if (resources.empty())
      return nullptr;

    std::set<std::string> matching_methods;
    if (method.empty()) {
      // Match standard HTTP methods.
      matching_methods.insert("GET");
      matching_methods.insert("POST");
    } else {
      matching_methods.insert(method);
    }

    const std::string& path_url = test_request::GetPathURL(url);
    ResourceList::const_iterator it = resources.begin();
    for (; it != resources.end(); ++it) {
      Resource* resource = *it;
      if (resource->GetPathURL() == path_url &&
          (resource->method.empty() ||
           matching_methods.find(resource->method) != matching_methods.end())) {
        return resource;
      }
    }
    return nullptr;
  }

  Resource* GetResource(CefRefPtr<CefRequest> request) const {
    return GetResource(request->GetURL(), request->GetMethod());
  }

  // Validate expected initial state.
  void Validate() const { DCHECK(!resources.empty()); }

  std::string GetMainURL() const { return resources.front()->GetPathURL(); }

  // Returns true if the server will be used.
  bool NeedsServer() const {
    ResourceList::const_iterator it = resources.begin();
    for (; it != resources.end(); ++it) {
      Resource* resource = *it;
      if (resource->handler == HandlerType::SERVER)
        return true;
    }
    return false;
  }

  // Returns true if all expectations have been met.
  bool IsDone() const {
    ResourceList::const_iterator it = resources.begin();
    for (; it != resources.end(); ++it) {
      Resource* resource = *it;
      if (!resource->IsDone())
        return false;
    }
    return true;
  }

  void AssertDone() const {
    ResourceList::const_iterator it = resources.begin();
    for (; it != resources.end(); ++it) {
      (*it)->AssertDone();
    }
  }

  // Optionally override to verify cleared cookie contents.
  virtual bool VerifyClearedCookies(
      const test_request::CookieVector& cookies) const {
    return true;
  }
};

class TestServerObserver : public test_server::ObserverHelper {
 public:
  TestServerObserver(TestSetup* setup,
                     base::OnceClosure ready_callback,
                     base::OnceClosure done_callback)
      : setup_(setup),
        ready_callback_(std::move(ready_callback)),
        done_callback_(std::move(done_callback)),
        weak_ptr_factory_(this) {
    DCHECK(setup);
    Initialize();
  }

  ~TestServerObserver() override { std::move(done_callback_).Run(); }

  void OnInitialized(const std::string& server_origin) override {
    CEF_REQUIRE_UI_THREAD();
    std::move(ready_callback_).Run();
  }

  bool OnHttpRequest(CefRefPtr<CefServer> server,
                     int connection_id,
                     const CefString& client_address,
                     CefRefPtr<CefRequest> request) override {
    CEF_REQUIRE_UI_THREAD();
    Resource* resource = setup_->GetResource(request);
    if (!resource) {
      // Not a request we handle.
      return false;
    }

    resource->response_ct++;
    EXPECT_TRUE(resource->VerifyRequest(request))
        << request->GetURL().ToString();
    test_server::SendResponse(server, connection_id, resource->response,
                              resource->response_data);

    // Stop propagating the callback.
    return true;
  }

  void OnShutdown() override {
    CEF_REQUIRE_UI_THREAD();
    delete this;
  }

 private:
  TestSetup* const setup_;
  base::OnceClosure ready_callback_;
  base::OnceClosure done_callback_;

  base::WeakPtrFactory<TestServerObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestServerObserver);
};

class CorsTestHandler : public RoutingTestHandler {
 public:
  explicit CorsTestHandler(TestSetup* setup) : setup_(setup) {
    setup_->Validate();
  }

  void RunTest() override {
    StartServer(base::BindOnce(&CorsTestHandler::TriggerCreateBrowser, this));

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  // Necessary to make the method public in order to destroy the test from
  // ClientSchemeHandlerType::ProcessRequest().
  void DestroyTest() override {
    EXPECT_TRUE(shutting_down_);

    if (setup_->NeedsServer()) {
      EXPECT_TRUE(got_stopped_server_);
    } else {
      EXPECT_FALSE(got_stopped_server_);
    }

    if (setup_->clear_cookies) {
      EXPECT_TRUE(got_cleared_cookies_);
    } else {
      EXPECT_FALSE(got_cleared_cookies_);
    }

    setup_->AssertDone();
    EXPECT_TRUE(setup_->console_messages.empty())
        << "Did not receive expected console message: "
        << setup_->console_messages.front();

    RoutingTestHandler::DestroyTest();
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    CEF_REQUIRE_IO_THREAD();
    const std::string& url = request->GetURL();
    const std::string& method = request->GetMethod();
    if (method == "OPTIONS") {
      // We should never see the CORS preflight request.
      ADD_FAILURE() << "Unexpected CORS preflight for " << url;
    }

    Resource* resource = setup_->GetResource(request);
    if (resource && resource->handler != HandlerType::SERVER) {
      resource->response_ct++;
      EXPECT_TRUE(resource->VerifyRequest(request)) << url;
      return test_request::CreateResourceHandler(resource->response,
                                                 resource->response_data);
    }
    return RoutingTestHandler::GetResourceHandler(browser, frame, request);
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    const std::string& url = frame->GetURL();
    Resource* resource = GetResource(url);
    if (!resource)
      return;

    const int expected_status = resource->response->GetStatus();
    if (url == main_url_ || expected_status != 200) {
      // Test that the status code is correct.
      EXPECT_EQ(expected_status, httpStatusCode) << url;
    }

    TriggerDestroyTestIfDone();
  }

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override {
    Resource* resource = GetResource(failedUrl);
    if (!resource)
      return;

    const cef_errorcode_t expected_error = resource->response->GetError();

    // Tests sometimes also fail with ERR_ABORTED.
    if (!(expected_error == ERR_NONE && errorCode == ERR_ABORTED)) {
      EXPECT_EQ(expected_error, errorCode) << failedUrl.ToString();
    }

    TriggerDestroyTestIfDone();
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    Resource* resource = GetResource(frame->GetURL());
    if (!resource)
      return false;

    if (request.ToString() == kSuccessMsg ||
        request.ToString() == kFailureMsg) {
      callback->Success("");
      if (request.ToString() == kSuccessMsg)
        resource->success_query_ct++;
      else
        resource->failure_query_ct++;
      TriggerDestroyTestIfDone();
      return true;
    }
    return false;
  }

  bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                        cef_log_severity_t level,
                        const CefString& message,
                        const CefString& source,
                        int line) override {
    bool expected = false;
    if (!setup_->console_messages.empty()) {
      std::vector<std::string>::iterator it = setup_->console_messages.begin();
      for (; it != setup_->console_messages.end(); ++it) {
        const std::string& possible = *it;
        const std::string& actual = message.ToString();
        if (actual.find(possible) == 0U) {
          expected = true;
          setup_->console_messages.erase(it);
          break;
        }
      }
    }

    EXPECT_TRUE(expected) << "Unexpected console message: "
                          << message.ToString();
    return false;
  }

 protected:
  void TriggerCreateBrowser() {
    main_url_ = setup_->GetMainURL();
    CreateBrowser(main_url_);
  }

  void TriggerDestroyTestIfDone() {
    CefPostTask(TID_UI,
                base::BindOnce(&CorsTestHandler::DestroyTestIfDone, this));
  }

  void DestroyTestIfDone() {
    CEF_REQUIRE_UI_THREAD();
    if (shutting_down_)
      return;

    if (setup_->IsDone()) {
      shutting_down_ = true;
      StopServer();
    }
  }

  void StartServer(base::OnceClosure next_step) {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI, base::BindOnce(&CorsTestHandler::StartServer, this,
                                         std::move(next_step)));
      return;
    }

    if (!setup_->NeedsServer()) {
      std::move(next_step).Run();
      return;
    }

    // Will delete itself after the server stops.
    server_ = new TestServerObserver(
        setup_, std::move(next_step),
        base::BindOnce(&CorsTestHandler::StoppedServer, this));
  }

  void StopServer() {
    CEF_REQUIRE_UI_THREAD();
    if (!server_) {
      DCHECK(!setup_->NeedsServer());
      AfterStoppedServer();
      return;
    }

    // Results in a call to StoppedServer().
    server_->Shutdown();
  }

  void StoppedServer() {
    CEF_REQUIRE_UI_THREAD();
    got_stopped_server_.yes();
    server_ = nullptr;
    AfterStoppedServer();
  }

  void AfterStoppedServer() {
    CEF_REQUIRE_UI_THREAD();
    if (setup_->clear_cookies) {
      ClearCookies();
    } else {
      DestroyTest();
    }
  }

  void ClearCookies() {
    CEF_REQUIRE_UI_THREAD();
    DCHECK(setup_->clear_cookies);
    test_request::GetAllCookies(
        CefCookieManager::GetGlobalManager(nullptr), /*delete_cookies=*/true,
        base::BindOnce(&CorsTestHandler::ClearedCookies, this));
  }

  void ClearedCookies(const test_request::CookieVector& cookies) {
    CEF_REQUIRE_UI_THREAD();
    got_cleared_cookies_.yes();
    EXPECT_TRUE(setup_->VerifyClearedCookies(cookies));
    DestroyTest();
  }

  Resource* GetResource(const std::string& url) const {
    Resource* resource = setup_->GetResource(url);
    EXPECT_TRUE(resource) << url;
    return resource;
  }

  TestSetup* setup_;
  std::string main_url_;
  TestServerObserver* server_ = nullptr;
  bool shutting_down_ = false;

  TrackCallback got_stopped_server_;
  TrackCallback got_cleared_cookies_;

  IMPLEMENT_REFCOUNTING(CorsTestHandler);
  DISALLOW_COPY_AND_ASSIGN(CorsTestHandler);
};

// JS that results in a call to CorsTestHandler::OnQuery.
std::string GetMsgJS(const std::string& msg) {
  return "window.testQuery({request:'" + msg + "'});";
}

std::string GetSuccessMsgJS() {
  return GetMsgJS(kSuccessMsg);
}
std::string GetFailureMsgJS() {
  return GetMsgJS(kFailureMsg);
}

std::string GetDefaultSuccessMsgHtml() {
  return "<html><body>TEST<script>" + GetSuccessMsgJS() +
         "</script></body></html>";
}

}  // namespace

// Verify the test harness for server requests.
TEST(CorsTest, BasicServer) {
  TestSetup setup;
  Resource resource(HandlerType::SERVER, "/CorsTest.BasicServer");
  setup.AddResource(&resource);

  CefRefPtr<CorsTestHandler> handler = new CorsTestHandler(&setup);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Like above, but also send a query JS message.
TEST(CorsTest, BasicServerWithQuery) {
  TestSetup setup;
  Resource resource(HandlerType::SERVER, "/CorsTest.BasicServerWithQuery",
                    kMimeTypeHtml, GetDefaultSuccessMsgHtml());
  resource.expected_success_query_ct = 1;
  setup.AddResource(&resource);

  CefRefPtr<CorsTestHandler> handler = new CorsTestHandler(&setup);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify the test harness for http scheme requests.
TEST(CorsTest, BasicHttpScheme) {
  TestSetup setup;
  Resource resource(HandlerType::HTTP_SCHEME, "/CorsTest.BasicHttpScheme");
  setup.AddResource(&resource);

  CefRefPtr<CorsTestHandler> handler = new CorsTestHandler(&setup);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Like above, but also send a query JS message.
TEST(CorsTest, BasicHttpSchemeWithQuery) {
  TestSetup setup;
  Resource resource(HandlerType::HTTP_SCHEME,
                    "/CorsTest.BasicHttpSchemeWithQuery", kMimeTypeHtml,
                    GetDefaultSuccessMsgHtml());
  resource.expected_success_query_ct = 1;
  setup.AddResource(&resource);

  CefRefPtr<CorsTestHandler> handler = new CorsTestHandler(&setup);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify the test harness for custom standard scheme requests.
TEST(CorsTest, BasicCustomStandardScheme) {
  TestSetup setup;
  Resource resource(HandlerType::CUSTOM_STANDARD_SCHEME,
                    "/CorsTest.BasicCustomStandardScheme");
  setup.AddResource(&resource);

  CefRefPtr<CorsTestHandler> handler = new CorsTestHandler(&setup);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Like above, but also send a query JS message.
TEST(CorsTest, BasicCustomStandardSchemeWithQuery) {
  TestSetup setup;
  Resource resource(HandlerType::CUSTOM_STANDARD_SCHEME,
                    "/CorsTest.BasicCustomStandardSchemeWithQuery",
                    kMimeTypeHtml, GetDefaultSuccessMsgHtml());
  resource.expected_success_query_ct = 1;
  setup.AddResource(&resource);

  CefRefPtr<CorsTestHandler> handler = new CorsTestHandler(&setup);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

struct CookieTestSetup : TestSetup {
  CookieTestSetup() {}

  bool expect_cookie = false;

  bool VerifyClearedCookies(
      const test_request::CookieVector& cookies) const override {
    if (!expect_cookie) {
      EXPECT_TRUE(cookies.empty());
      return cookies.empty();
    }

    EXPECT_EQ(1U, cookies.size());
    const std::string& cookie = CefString(&cookies[0].name).ToString() + "=" +
                                CefString(&cookies[0].value).ToString();
    EXPECT_STREQ(kDefaultCookie, cookie.c_str());
    return cookie == kDefaultCookie;
  }
};

struct CookieResource : Resource {
  CookieResource() {}

  bool expect_cookie = false;

  void InitSetCookie() {
    response->SetHeaderByName("Set-Cookie", kDefaultCookie,
                              /*override=*/true);
  }

  bool VerifyRequest(CefRefPtr<CefRequest> request) const override {
    const std::string& cookie = request->GetHeaderByName("Cookie");
    const std::string& expected_cookie =
        expect_cookie ? kDefaultCookie : std::string();
    EXPECT_STREQ(expected_cookie.c_str(), cookie.c_str()) << GetPathURL();
    return expected_cookie == cookie;
  }
};

void SetupCookieExpectations(CookieTestSetup* setup,
                             CookieResource* main_resource,
                             CookieResource* sub_resource) {
  // All schemes except custom non-standard support cookies.
  const bool supports_cookies = IsStandardType(main_resource->handler);

  // The main resource may set the cookie (if cookies are supported), but should
  // not receive one.
  main_resource->InitSetCookie();
  main_resource->expect_cookie = false;

  // A cookie will be set only for schemes that support cookies.
  setup->expect_cookie = supports_cookies;
  // Always clear cookies so we can verify that one wasn't set unexpectedly.
  setup->clear_cookies = true;

  // Expect the sub-resource to receive the cookie for same-origin requests
  // only.
  sub_resource->expect_cookie =
      supports_cookies && main_resource->handler == sub_resource->handler;
}

std::string GetIframeMainHtml(const std::string& iframe_url,
                              const std::string& sandbox_attribs) {
  return "<html><body>TEST<iframe src=\"" + iframe_url + "\" sandbox=\"" +
         sandbox_attribs + "\"></iframe></body></html>";
}

std::string GetIframeSubHtml() {
  // Try to script the parent frame, then send the SuccessMsg.
  return "<html><body>TEST<script>try { parent.document.body; } catch "
         "(exception) { console.log(exception.toString()); }" +
         GetSuccessMsgJS() + "</script></body></html>";
}

bool HasSandboxAttrib(const std::string& sandbox_attribs,
                      const std::string& attrib) {
  return sandbox_attribs.find(attrib) != std::string::npos;
}

void SetupIframeRequest(CookieTestSetup* setup,
                        const std::string& test_name,
                        HandlerType main_handler,
                        CookieResource* main_resource,
                        HandlerType iframe_handler,
                        CookieResource* iframe_resource,
                        const std::string& sandbox_attribs) {
  const std::string& base_path = "/" + test_name;

  // Expect a single iframe request.
  iframe_resource->Init(iframe_handler, base_path + ".iframe.html",
                        kMimeTypeHtml, GetIframeSubHtml());

  // Expect a single main frame request.
  const std::string& iframe_url = iframe_resource->GetPathURL();
  main_resource->Init(main_handler, base_path, kMimeTypeHtml,
                      GetIframeMainHtml(iframe_url, sandbox_attribs));

  SetupCookieExpectations(setup, main_resource, iframe_resource);

  if (HasSandboxAttrib(sandbox_attribs, "allow-scripts")) {
    // Expect the iframe to load successfully and send the SuccessMsg.
    iframe_resource->expected_success_query_ct = 1;

    const bool has_same_origin =
        HasSandboxAttrib(sandbox_attribs, "allow-same-origin");
    if (!has_same_origin ||
        (has_same_origin &&
         (IsNonStandardType(main_handler) || main_handler != iframe_handler))) {
      // Expect parent frame scripting to fail if:
      // - "allow-same-origin" is not specified;
      // - the main frame is a non-standard scheme (e.g. CORS disabled);
      // - the main frame and iframe origins don't match.
      // The reported origin will be "null" if "allow-same-origin" is not
      // specified, or if the iframe is hosted on a non-standard scheme.
      const std::string& origin =
          !has_same_origin || IsNonStandardType(iframe_handler)
              ? "null"
              : GetOrigin(iframe_handler);
      setup->AddConsoleMessage("SecurityError: Blocked a frame with origin \"" +
                               origin +
                               "\" from accessing a cross-origin frame.");
    }
  } else {
    // Expect JavaScript execution to fail.
    setup->AddConsoleMessage("Blocked script execution in '" + iframe_url +
                             "' because the document's frame is sandboxed and "
                             "the 'allow-scripts' permission is not set.");
  }

  setup->AddResource(main_resource);
  setup->AddResource(iframe_resource);
}

}  // namespace

// Test iframe sandbox attributes with different origin combinations.
#define CORS_TEST_IFRAME(test_name, handler_main, handler_iframe,     \
                         sandbox_attribs)                             \
  TEST(CorsTest, Iframe##test_name) {                                 \
    CookieTestSetup setup;                                            \
    CookieResource resource_main, resource_iframe;                    \
    SetupIframeRequest(&setup, "CorsTest.Iframe" #test_name,          \
                       HandlerType::handler_main, &resource_main,     \
                       HandlerType::handler_iframe, &resource_iframe, \
                       sandbox_attribs);                              \
    CefRefPtr<CorsTestHandler> handler = new CorsTestHandler(&setup); \
    handler->ExecuteTest();                                           \
    ReleaseAndWaitForDestructor(handler);                             \
  }

// Test all origin combinations (same and cross-origin).
#define CORS_TEST_IFRAME_ALL(name, sandbox_attribs)                            \
  CORS_TEST_IFRAME(name##ServerToServer, SERVER, SERVER, sandbox_attribs)      \
  CORS_TEST_IFRAME(name##ServerToHttpScheme, SERVER, HTTP_SCHEME,              \
                   sandbox_attribs)                                            \
  CORS_TEST_IFRAME(name##ServerToCustomStandardScheme, SERVER,                 \
                   CUSTOM_STANDARD_SCHEME, sandbox_attribs)                    \
  CORS_TEST_IFRAME(name##ServerToCustomNonStandardScheme, SERVER,              \
                   CUSTOM_NONSTANDARD_SCHEME, sandbox_attribs)                 \
  CORS_TEST_IFRAME(name##ServerToCustomUnregisteredScheme, SERVER,             \
                   CUSTOM_UNREGISTERED_SCHEME, sandbox_attribs)                \
  CORS_TEST_IFRAME(name##HttpSchemeToServer, HTTP_SCHEME, SERVER,              \
                   sandbox_attribs)                                            \
  CORS_TEST_IFRAME(name##HttpSchemeToHttpScheme, HTTP_SCHEME, HTTP_SCHEME,     \
                   sandbox_attribs)                                            \
  CORS_TEST_IFRAME(name##HttpSchemeToCustomStandardScheme, HTTP_SCHEME,        \
                   CUSTOM_STANDARD_SCHEME, sandbox_attribs)                    \
  CORS_TEST_IFRAME(name##HttpSchemeToCustomNonStandardScheme, HTTP_SCHEME,     \
                   CUSTOM_NONSTANDARD_SCHEME, sandbox_attribs)                 \
  CORS_TEST_IFRAME(name##HttpSchemeToCustomUnregisteredScheme, HTTP_SCHEME,    \
                   CUSTOM_UNREGISTERED_SCHEME, sandbox_attribs)                \
  CORS_TEST_IFRAME(name##CustomStandardSchemeToServer, CUSTOM_STANDARD_SCHEME, \
                   SERVER, sandbox_attribs)                                    \
  CORS_TEST_IFRAME(name##CustomStandardSchemeToHttpScheme,                     \
                   CUSTOM_STANDARD_SCHEME, HTTP_SCHEME, sandbox_attribs)       \
  CORS_TEST_IFRAME(name##CustomStandardSchemeToCustomStandardScheme,           \
                   CUSTOM_STANDARD_SCHEME, CUSTOM_STANDARD_SCHEME,             \
                   sandbox_attribs)                                            \
  CORS_TEST_IFRAME(name##CustomStandardSchemeToCustomNonStandardScheme,        \
                   CUSTOM_STANDARD_SCHEME, CUSTOM_NONSTANDARD_SCHEME,          \
                   sandbox_attribs)                                            \
  CORS_TEST_IFRAME(name##CustomStandardSchemeToCustomUnregisteredScheme,       \
                   CUSTOM_STANDARD_SCHEME, CUSTOM_UNREGISTERED_SCHEME,         \
                   sandbox_attribs)                                            \
  CORS_TEST_IFRAME(name##CustomNonStandardSchemeToServer,                      \
                   CUSTOM_NONSTANDARD_SCHEME, SERVER, sandbox_attribs)         \
  CORS_TEST_IFRAME(name##CustomNonStandardSchemeToHttpScheme,                  \
                   CUSTOM_NONSTANDARD_SCHEME, HTTP_SCHEME, sandbox_attribs)    \
  CORS_TEST_IFRAME(name##CustomNonStandardSchemeToCustomStandardScheme,        \
                   CUSTOM_NONSTANDARD_SCHEME, CUSTOM_STANDARD_SCHEME,          \
                   sandbox_attribs)                                            \
  CORS_TEST_IFRAME(name##CustomNonStandardSchemeToCustomNonStandardScheme,     \
                   CUSTOM_NONSTANDARD_SCHEME, CUSTOM_NONSTANDARD_SCHEME,       \
                   sandbox_attribs)                                            \
  CORS_TEST_IFRAME(name##CustomNonStandardSchemeToCustomUnregisteredScheme,    \
                   CUSTOM_NONSTANDARD_SCHEME, CUSTOM_UNREGISTERED_SCHEME,      \
                   sandbox_attribs)                                            \
  CORS_TEST_IFRAME(name##CustomUnregisteredSchemeToServer,                     \
                   CUSTOM_UNREGISTERED_SCHEME, SERVER, sandbox_attribs)        \
  CORS_TEST_IFRAME(name##CustomUnregisteredSchemeToHttpScheme,                 \
                   CUSTOM_UNREGISTERED_SCHEME, HTTP_SCHEME, sandbox_attribs)   \
  CORS_TEST_IFRAME(name##CustomUnregisteredSchemeToCustomStandardScheme,       \
                   CUSTOM_UNREGISTERED_SCHEME, CUSTOM_STANDARD_SCHEME,         \
                   sandbox_attribs)                                            \
  CORS_TEST_IFRAME(name##CustomUnregisteredSchemeToCustomNonStandardScheme,    \
                   CUSTOM_UNREGISTERED_SCHEME, CUSTOM_NONSTANDARD_SCHEME,      \
                   sandbox_attribs)                                            \
  CORS_TEST_IFRAME(name##CustomUnregisteredSchemeToCustomUnregisteredScheme,   \
                   CUSTOM_UNREGISTERED_SCHEME, CUSTOM_UNREGISTERED_SCHEME,     \
                   sandbox_attribs)

// Everything is blocked.
CORS_TEST_IFRAME_ALL(None, "")

// JavaScript execution is allowed.
CORS_TEST_IFRAME_ALL(AllowScripts, "allow-scripts")

// JavaScript execution is allowed and scripting the parent is allowed for
// same-origin only.
CORS_TEST_IFRAME_ALL(AllowScriptsAndSameOrigin,
                     "allow-scripts allow-same-origin")

namespace {

const char kSubRequestMethod[] = "GET";
const char kSubUnsafeHeaderName[] = "x-unsafe-header";
const char kSubUnsafeHeaderValue[] = "not-safe";

struct SubResource : CookieResource {
  SubResource() {}

  std::string main_origin;
  bool supports_cors = false;
  bool is_cross_origin = false;

  void InitCors(HandlerType main_handler, bool add_header) {
    // Must specify the method to differentiate from the preflight request.
    method = kSubRequestMethod;

    // Origin is always "null" for non-standard schemes.
    main_origin =
        IsNonStandardType(main_handler) ? "null" : GetOrigin(main_handler);

    // True if cross-origin requests are allowed. XHR requests to non-standard
    // schemes are not allowed (due to the "null" origin).
    supports_cors = IsStandardType(handler);
    if (!supports_cors) {
      // Don't expect the xhr request.
      expected_response_ct = 0;
    }

    // True if the request is considered cross-origin. Any requests between
    // non-standard schemes are considered cross-origin (due to the "null"
    // origin).
    is_cross_origin =
        main_handler != handler ||
        (IsNonStandardType(main_handler) && handler == main_handler);

    if (is_cross_origin && add_header) {
      response->SetHeaderByName("Access-Control-Allow-Origin", main_origin,
                                false);
    }
  }

  bool VerifyRequest(CefRefPtr<CefRequest> request) const override {
    if (!CookieResource::VerifyRequest(request))
      return false;

    const std::string& request_method = request->GetMethod();
    EXPECT_STREQ(method.c_str(), request_method.c_str()) << GetPathURL();
    if (request_method != method)
      return false;

    // Verify that the "Origin" header contains the expected value.
    const std::string& origin = request->GetHeaderByName("Origin");
    const std::string& expected_origin =
        is_cross_origin ? main_origin : std::string();
    EXPECT_STREQ(expected_origin.c_str(), origin.c_str()) << GetPathURL();
    if (expected_origin != origin)
      return false;

    // Verify that the "X-Unsafe-Header" header contains the expected value.
    const std::string& unsafe_header =
        request->GetHeaderByName(kSubUnsafeHeaderName);
    EXPECT_STREQ(kSubUnsafeHeaderValue, unsafe_header.c_str()) << GetPathURL();
    return unsafe_header == kSubUnsafeHeaderValue;
  }
};

// See https://developer.mozilla.org/en-US/docs/Glossary/Preflight_request
// for details of CORS preflight behavior.
struct PreflightResource : Resource {
  std::string main_origin;

  void InitPreflight(HandlerType main_handler) {
    // CORS preflight requests originate from PreflightController in the network
    // process, so we only expect them for server requests.
    EXPECT_EQ(HandlerType::SERVER, handler);

    // Origin is always "null" for non-standard schemes.
    main_origin =
        IsNonStandardType(main_handler) ? "null" : GetOrigin(main_handler);

    method = "OPTIONS";
    response->SetHeaderByName("Access-Control-Allow-Methods",
                              "GET,HEAD,OPTIONS,POST", false);
    response->SetHeaderByName("Access-Control-Allow-Headers",
                              kSubUnsafeHeaderName, false);
    response->SetHeaderByName("Access-Control-Allow-Origin", main_origin,
                              false);
  }

  bool VerifyRequest(CefRefPtr<CefRequest> request) const override {
    const std::string& request_method = request->GetMethod();
    EXPECT_STREQ(method.c_str(), request_method.c_str()) << GetPathURL();
    if (request_method != method)
      return false;

    const std::string& origin = request->GetHeaderByName("Origin");
    EXPECT_STREQ(main_origin.c_str(), origin.c_str()) << GetPathURL();
    if (main_origin != origin)
      return false;

    const std::string& ac_request_method =
        request->GetHeaderByName("Access-Control-Request-Method");
    EXPECT_STREQ(kSubRequestMethod, ac_request_method.c_str()) << GetPathURL();
    if (ac_request_method != kSubRequestMethod)
      return false;

    const std::string& ac_request_headers =
        request->GetHeaderByName("Access-Control-Request-Headers");
    EXPECT_STREQ(kSubUnsafeHeaderName, ac_request_headers.c_str())
        << GetPathURL();
    if (ac_request_headers != kSubUnsafeHeaderName)
      return false;

    return true;
  }
};

enum class ExecMode {
  XHR,
  FETCH,
};

std::string GetXhrExecJS(const std::string& sub_url) {
  // Inclusion of an unsafe header triggers CORS preflight for cross-origin
  // requests to the server.
  return "xhr = new XMLHttpRequest();\n"
         "xhr.open(\"GET\", \"" +
         sub_url +
         "\", true)\n;"
         "xhr.setRequestHeader('" +
         kSubUnsafeHeaderName + "', '" + kSubUnsafeHeaderValue +
         "');\n"
         "xhr.onload = function(e) {\n"
         "  if (xhr.readyState === 4) {\n"
         "    if (xhr.status === 200) {\n"
         "      onResult(xhr.responseText);\n"
         "    } else {\n"
         "      console.log('XMLHttpRequest failed with status ' + "
         "xhr.status);\n"
         "      onResult('FAILURE');\n"
         "    }\n"
         "  }\n"
         "};\n"
         "xhr.onerror = function(e) {\n"
         "  onResult('FAILURE');\n"
         "};\n"
         "xhr.send();\n";
}

std::string GetFetchExecJS(const std::string& sub_url) {
  // Inclusion of an unsafe header triggers CORS preflight for cross-origin
  // requests to the server.
  return std::string() +
         "let h = new Headers();\n"
         "h.append('" +
         kSubUnsafeHeaderName + "', '" + kSubUnsafeHeaderValue +
         "');\n"
         "fetch('" +
         sub_url +
         "', {headers: h})\n"
         ".then(function(response) {\n"
         "  if (response.status === 200) {\n"
         "      response.text().then(function(text) {\n"
         "          onResult(text);\n"
         "      }).catch(function(e) {\n"
         "          onResult('FAILURE')\n;        "
         "      })\n;"
         "  } else {\n"
         "      onResult('FAILURE');\n"
         "  }\n"
         "}).catch(function(e) {\n"
         "  onResult('FAILURE');\n"
         "});\n";
}

std::string GetExecMainHtml(ExecMode mode, const std::string& sub_url) {
  return std::string() +
         "<html><head>\n"
         "<script language=\"JavaScript\">\n" +
         "function onResult(val) {\n"
         "  if (val === '" +
         kDefaultText + "') {" + GetSuccessMsgJS() + "} else {" +
         GetFailureMsgJS() +
         "}\n}\n"
         "function execRequest() {\n" +
         (mode == ExecMode::XHR ? GetXhrExecJS(sub_url)
                                : GetFetchExecJS(sub_url)) +
         "}\n</script>\n"
         "</head><body onload=\"execRequest();\">"
         "Running execRequest..."
         "</body></html>";
}

// XHR and fetch requests behave the same, except for console message contents.
// In addition to basic CORS header behaviors and request blocking, this test
// verifies that CORS preflight requests are sent and received when expected.
// Since preflight behavior is implemented in the network process we expect it
// to already have substantial test coverage in Chromium.
void SetupExecRequest(ExecMode mode,
                      CookieTestSetup* setup,
                      const std::string& test_name,
                      HandlerType main_handler,
                      CookieResource* main_resource,
                      HandlerType sub_handler,
                      SubResource* sub_resource,
                      PreflightResource* preflight_resource,
                      bool add_header) {
  const std::string& base_path = "/" + test_name;

  // Expect a single xhr request.
  const std::string& sub_path = base_path + ".sub.txt";
  sub_resource->Init(sub_handler, sub_path, kMimeTypeText, kDefaultText);
  sub_resource->InitCors(main_handler, add_header);

  // Expect a single main frame request.
  const std::string& sub_url = sub_resource->GetPathURL();
  main_resource->Init(main_handler, base_path, kMimeTypeHtml,
                      GetExecMainHtml(mode, sub_url));

  SetupCookieExpectations(setup, main_resource, sub_resource);

  // Cross-origin requests to a server sub-resource will receive a CORS
  // preflight request because we add an unsafe header.
  const bool expect_cors_preflight =
      sub_resource->is_cross_origin && sub_handler == HandlerType::SERVER;

  if (sub_resource->is_cross_origin &&
      (!sub_resource->supports_cors || !add_header)) {
    // Expect the cross-origin XHR to be blocked.
    main_resource->expected_failure_query_ct = 1;

    if (sub_resource->supports_cors && !add_header) {
      // The request supports CORS, but we didn't add the
      // "Access-Control-Allow-Origin" header.
      if (!expect_cors_preflight || preflight_resource != nullptr) {
        // This is the error message when not expecting a CORS preflight
        // request, or when the preflight request is handled by the server.
        // Unhandled preflight requests will output a different error message
        // (see below).
        if (mode == ExecMode::XHR) {
          setup->AddConsoleMessage(
              "Access to XMLHttpRequest at '" + sub_url + "' from origin '" +
              sub_resource->main_origin +
              "' has been blocked by CORS policy: No "
              "'Access-Control-Allow-Origin' "
              "header is present on the requested resource.");
        } else {
          setup->AddConsoleMessage(
              "Access to fetch at '" + sub_url + "' from origin '" +
              sub_resource->main_origin +
              "' has been blocked by CORS policy: No "
              "'Access-Control-Allow-Origin' header is present on the "
              "requested "
              "resource. If an opaque response serves your needs, set the "
              "request's mode to 'no-cors' to fetch the resource with CORS "
              "disabled.");
        }
      }
    } else if (mode == ExecMode::XHR) {
      setup->AddConsoleMessage(
          "Access to XMLHttpRequest at '" + sub_url + "' from origin '" +
          sub_resource->main_origin +
          "' has been blocked by CORS policy: Cross origin requests are only "
          "supported for protocol schemes:");
    } else {
      setup->AddConsoleMessage("Fetch API cannot load " + sub_url +
                               ". URL scheme \"" + GetScheme(sub_handler) +
                               "\" is not supported.");
    }
  } else {
    // Expect the (possibly cross-origin) XHR to be allowed.
    main_resource->expected_success_query_ct = 1;
  }

  setup->AddResource(main_resource);
  setup->AddResource(sub_resource);

  if (expect_cors_preflight) {
    // Expect a CORS preflight request.
    if (preflight_resource) {
      // The server will handle the preflight request. The cross-origin XHR may
      // still be blocked if the "Access-Control-Allow-Origin" header is missing
      // (see above).
      preflight_resource->Init(sub_handler, sub_path, kMimeTypeText,
                               std::string());
      preflight_resource->InitPreflight(main_handler);
      setup->AddResource(preflight_resource);
    } else {
      // The server will not handle the preflight request. Expect the
      // cross-origin XHR to be blocked.
      main_resource->expected_failure_query_ct = 1;
      main_resource->expected_success_query_ct = 0;
      sub_resource->expected_response_ct = 0;

      if (mode == ExecMode::XHR) {
        setup->AddConsoleMessage(
            "Access to XMLHttpRequest at '" + sub_url + "' from origin '" +
            sub_resource->main_origin +
            "' has been blocked by CORS policy: Response to preflight request "
            "doesn't pass access control check: No "
            "'Access-Control-Allow-Origin' header is present on the requested "
            "resource.");
      } else {
        setup->AddConsoleMessage(
            "Access to fetch at '" + sub_url + "' from origin '" +
            sub_resource->main_origin +
            "' has been blocked by CORS policy: Response to preflight request "
            "doesn't pass access control check: No "
            "'Access-Control-Allow-Origin' header is present on the requested "
            "resource. If an opaque response serves your needs, set the "
            "request's mode to 'no-cors' to fetch the resource with CORS "
            "disabled.");
      }
    }
  }
}

}  // namespace

// Test XHR requests with different origin combinations.
#define CORS_TEST_XHR(test_name, handler_main, handler_sub, add_header) \
  TEST(CorsTest, Xhr##test_name) {                                      \
    CookieTestSetup setup;                                              \
    CookieResource resource_main;                                       \
    SubResource resource_sub;                                           \
    PreflightResource resource_preflight;                               \
    SetupExecRequest(ExecMode::XHR, &setup, "CorsTest.Xhr" #test_name,  \
                     HandlerType::handler_main, &resource_main,         \
                     HandlerType::handler_sub, &resource_sub,           \
                     &resource_preflight, add_header);                  \
    CefRefPtr<CorsTestHandler> handler = new CorsTestHandler(&setup);   \
    handler->ExecuteTest();                                             \
    ReleaseAndWaitForDestructor(handler);                               \
  }

// Test all origin combinations (same and cross-origin).
#define CORS_TEST_XHR_ALL(name, add_header)                                    \
  CORS_TEST_XHR(name##ServerToServer, SERVER, SERVER, add_header)              \
  CORS_TEST_XHR(name##ServerToHttpScheme, SERVER, HTTP_SCHEME, add_header)     \
  CORS_TEST_XHR(name##ServerToCustomStandardScheme, SERVER,                    \
                CUSTOM_STANDARD_SCHEME, add_header)                            \
  CORS_TEST_XHR(name##ServerToCustomNonStandardScheme, SERVER,                 \
                CUSTOM_NONSTANDARD_SCHEME, add_header)                         \
  CORS_TEST_XHR(name##ServerToCustomUnregisteredScheme, SERVER,                \
                CUSTOM_UNREGISTERED_SCHEME, add_header)                        \
  CORS_TEST_XHR(name##HttpSchemeToServer, HTTP_SCHEME, SERVER, add_header)     \
  CORS_TEST_XHR(name##HttpSchemeToHttpScheme, HTTP_SCHEME, HTTP_SCHEME,        \
                add_header)                                                    \
  CORS_TEST_XHR(name##HttpSchemeToCustomStandardScheme, HTTP_SCHEME,           \
                CUSTOM_STANDARD_SCHEME, add_header)                            \
  CORS_TEST_XHR(name##HttpSchemeToCustomNonStandardScheme, HTTP_SCHEME,        \
                CUSTOM_NONSTANDARD_SCHEME, add_header)                         \
  CORS_TEST_XHR(name##HttpSchemeToCustomUnregisteredScheme, HTTP_SCHEME,       \
                CUSTOM_UNREGISTERED_SCHEME, add_header)                        \
  CORS_TEST_XHR(name##CustomStandardSchemeToServer, CUSTOM_STANDARD_SCHEME,    \
                SERVER, add_header)                                            \
  CORS_TEST_XHR(name##CustomStandardSchemeToHttpScheme,                        \
                CUSTOM_STANDARD_SCHEME, HTTP_SCHEME, add_header)               \
  CORS_TEST_XHR(name##CustomStandardSchemeToCustomStandardScheme,              \
                CUSTOM_STANDARD_SCHEME, CUSTOM_STANDARD_SCHEME, add_header)    \
  CORS_TEST_XHR(name##CustomStandardSchemeToCustomNonStandardScheme,           \
                CUSTOM_STANDARD_SCHEME, CUSTOM_NONSTANDARD_SCHEME, add_header) \
  CORS_TEST_XHR(name##CustomStandardSchemeToCustomUnregisteredScheme,          \
                CUSTOM_STANDARD_SCHEME, CUSTOM_UNREGISTERED_SCHEME,            \
                add_header)                                                    \
  CORS_TEST_XHR(name##CustomNonStandardSchemeToServer,                         \
                CUSTOM_NONSTANDARD_SCHEME, SERVER, add_header)                 \
  CORS_TEST_XHR(name##CustomNonStandardSchemeToHttpScheme,                     \
                CUSTOM_NONSTANDARD_SCHEME, HTTP_SCHEME, add_header)            \
  CORS_TEST_XHR(name##CustomNonStandardSchemeToCustomStandardScheme,           \
                CUSTOM_NONSTANDARD_SCHEME, CUSTOM_STANDARD_SCHEME, add_header) \
  CORS_TEST_XHR(name##CustomNonStandardSchemeToCustomNonStandardScheme,        \
                CUSTOM_NONSTANDARD_SCHEME, CUSTOM_NONSTANDARD_SCHEME,          \
                add_header)                                                    \
  CORS_TEST_XHR(name##CustomNonStandardSchemeToCustomUnregisteredScheme,       \
                CUSTOM_NONSTANDARD_SCHEME, CUSTOM_UNREGISTERED_SCHEME,         \
                add_header)                                                    \
  CORS_TEST_XHR(name##CustomUnregisteredSchemeToServer,                        \
                CUSTOM_UNREGISTERED_SCHEME, SERVER, add_header)                \
  CORS_TEST_XHR(name##CustomUnregisteredSchemeToHttpScheme,                    \
                CUSTOM_UNREGISTERED_SCHEME, HTTP_SCHEME, add_header)           \
  CORS_TEST_XHR(name##CustomUnregisteredSchemeToCustomStandardScheme,          \
                CUSTOM_UNREGISTERED_SCHEME, CUSTOM_STANDARD_SCHEME,            \
                add_header)                                                    \
  CORS_TEST_XHR(name##CustomUnregisteredSchemeToCustomNonStandardScheme,       \
                CUSTOM_UNREGISTERED_SCHEME, CUSTOM_NONSTANDARD_SCHEME,         \
                add_header)                                                    \
  CORS_TEST_XHR(name##CustomUnregisteredSchemeToCustomUnregisteredScheme,      \
                CUSTOM_UNREGISTERED_SCHEME, CUSTOM_UNREGISTERED_SCHEME,        \
                add_header)

// XHR requests without the "Access-Control-Allow-Origin" header.
CORS_TEST_XHR_ALL(NoHeader, false)

// XHR requests with the "Access-Control-Allow-Origin" header.
CORS_TEST_XHR_ALL(WithHeader, true)

// Like above, but without handling CORS preflight requests.
#define CORS_TEST_XHR_NO_PREFLIGHT(test_name, handler_main, handler_sub, \
                                   add_header)                           \
  TEST(CorsTest, Xhr##test_name) {                                       \
    CookieTestSetup setup;                                               \
    CookieResource resource_main;                                        \
    SubResource resource_sub;                                            \
    SetupExecRequest(ExecMode::XHR, &setup, "CorsTest.Xhr" #test_name,   \
                     HandlerType::handler_main, &resource_main,          \
                     HandlerType::handler_sub, &resource_sub, nullptr,   \
                     add_header);                                        \
    CefRefPtr<CorsTestHandler> handler = new CorsTestHandler(&setup);    \
    handler->ExecuteTest();                                              \
    ReleaseAndWaitForDestructor(handler);                                \
  }

#define CORS_TEST_XHR_NO_PREFLIGHT_SERVER(name, add_header)                    \
  CORS_TEST_XHR_NO_PREFLIGHT(name##ServerToServer, SERVER, SERVER, add_header) \
  CORS_TEST_XHR_NO_PREFLIGHT(name##HttpSchemeToServer, HTTP_SCHEME, SERVER,    \
                             add_header)                                       \
  CORS_TEST_XHR_NO_PREFLIGHT(name##CustomStandardSchemeToServer,               \
                             CUSTOM_STANDARD_SCHEME, SERVER, add_header)       \
  CORS_TEST_XHR_NO_PREFLIGHT(name##CustomNonStandardSchemeToServer,            \
                             CUSTOM_NONSTANDARD_SCHEME, SERVER, add_header)

// XHR requests without the "Access-Control-Allow-Origin" header.
CORS_TEST_XHR_NO_PREFLIGHT_SERVER(NoHeaderNoPreflight, false)

// XHR requests with the "Access-Control-Allow-Origin" header.
CORS_TEST_XHR_NO_PREFLIGHT_SERVER(WithHeaderNoPreflight, true)

// Test fetch requests with different origin combinations.
#define CORS_TEST_FETCH(test_name, handler_main, handler_sub, add_header)  \
  TEST(CorsTest, Fetch##test_name) {                                       \
    CookieTestSetup setup;                                                 \
    CookieResource resource_main;                                          \
    SubResource resource_sub;                                              \
    PreflightResource resource_preflight;                                  \
    SetupExecRequest(ExecMode::FETCH, &setup, "CorsTest.Fetch" #test_name, \
                     HandlerType::handler_main, &resource_main,            \
                     HandlerType::handler_sub, &resource_sub,              \
                     &resource_preflight, add_header);                     \
    CefRefPtr<CorsTestHandler> handler = new CorsTestHandler(&setup);      \
    handler->ExecuteTest();                                                \
    ReleaseAndWaitForDestructor(handler);                                  \
  }

// Test all origin combinations (same and cross-origin).
#define CORS_TEST_FETCH_ALL(name, add_header)                                 \
  CORS_TEST_FETCH(name##ServerToServer, SERVER, SERVER, add_header)           \
  CORS_TEST_FETCH(name##ServerToHttpScheme, SERVER, HTTP_SCHEME, add_header)  \
  CORS_TEST_FETCH(name##ServerToCustomStandardScheme, SERVER,                 \
                  CUSTOM_STANDARD_SCHEME, add_header)                         \
  CORS_TEST_FETCH(name##ServerToCustomNonStandardScheme, SERVER,              \
                  CUSTOM_NONSTANDARD_SCHEME, add_header)                      \
  CORS_TEST_FETCH(name##ServerToCustomUnregisteredScheme, SERVER,             \
                  CUSTOM_UNREGISTERED_SCHEME, add_header)                     \
  CORS_TEST_FETCH(name##HttpSchemeToServer, HTTP_SCHEME, SERVER, add_header)  \
  CORS_TEST_FETCH(name##HttpSchemeToHttpScheme, HTTP_SCHEME, HTTP_SCHEME,     \
                  add_header)                                                 \
  CORS_TEST_FETCH(name##HttpSchemeToCustomStandardScheme, HTTP_SCHEME,        \
                  CUSTOM_STANDARD_SCHEME, add_header)                         \
  CORS_TEST_FETCH(name##HttpSchemeToCustomNonStandardScheme, HTTP_SCHEME,     \
                  CUSTOM_NONSTANDARD_SCHEME, add_header)                      \
  CORS_TEST_FETCH(name##HttpSchemeToCustomUnregisteredScheme, HTTP_SCHEME,    \
                  CUSTOM_UNREGISTERED_SCHEME, add_header)                     \
  CORS_TEST_FETCH(name##CustomStandardSchemeToServer, CUSTOM_STANDARD_SCHEME, \
                  SERVER, add_header)                                         \
  CORS_TEST_FETCH(name##CustomStandardSchemeToHttpScheme,                     \
                  CUSTOM_STANDARD_SCHEME, HTTP_SCHEME, add_header)            \
  CORS_TEST_FETCH(name##CustomStandardSchemeToCustomStandardScheme,           \
                  CUSTOM_STANDARD_SCHEME, CUSTOM_STANDARD_SCHEME, add_header) \
  CORS_TEST_FETCH(name##CustomStandardSchemeToCustomNonStandardScheme,        \
                  CUSTOM_STANDARD_SCHEME, CUSTOM_NONSTANDARD_SCHEME,          \
                  add_header)                                                 \
  CORS_TEST_FETCH(name##CustomStandardSchemeToCustomUnregisteredScheme,       \
                  CUSTOM_STANDARD_SCHEME, CUSTOM_UNREGISTERED_SCHEME,         \
                  add_header)                                                 \
  CORS_TEST_FETCH(name##CustomNonStandardSchemeToServer,                      \
                  CUSTOM_NONSTANDARD_SCHEME, SERVER, add_header)              \
  CORS_TEST_FETCH(name##CustomNonStandardSchemeToHttpScheme,                  \
                  CUSTOM_NONSTANDARD_SCHEME, HTTP_SCHEME, add_header)         \
  CORS_TEST_FETCH(name##CustomNonStandardSchemeToCustomStandardScheme,        \
                  CUSTOM_NONSTANDARD_SCHEME, CUSTOM_STANDARD_SCHEME,          \
                  add_header)                                                 \
  CORS_TEST_FETCH(name##CustomNonStandardSchemeToCustomNonStandardScheme,     \
                  CUSTOM_NONSTANDARD_SCHEME, CUSTOM_NONSTANDARD_SCHEME,       \
                  add_header)                                                 \
  CORS_TEST_FETCH(name##CustomNonStandardSchemeToCustomUnregisteredScheme,    \
                  CUSTOM_NONSTANDARD_SCHEME, CUSTOM_UNREGISTERED_SCHEME,      \
                  add_header)                                                 \
  CORS_TEST_FETCH(name##CustomUnregisteredSchemeToServer,                     \
                  CUSTOM_UNREGISTERED_SCHEME, SERVER, add_header)             \
  CORS_TEST_FETCH(name##CustomUnregisteredSchemeToHttpScheme,                 \
                  CUSTOM_UNREGISTERED_SCHEME, HTTP_SCHEME, add_header)        \
  CORS_TEST_FETCH(name##CustomUnregisteredSchemeToCustomStandardScheme,       \
                  CUSTOM_UNREGISTERED_SCHEME, CUSTOM_STANDARD_SCHEME,         \
                  add_header)                                                 \
  CORS_TEST_FETCH(name##CustomUnregisteredSchemeToCustomNonStandardScheme,    \
                  CUSTOM_UNREGISTERED_SCHEME, CUSTOM_NONSTANDARD_SCHEME,      \
                  add_header)                                                 \
  CORS_TEST_FETCH(name##CustomUnregisteredSchemeToCustomUnregisteredScheme,   \
                  CUSTOM_UNREGISTERED_SCHEME, CUSTOM_UNREGISTERED_SCHEME,     \
                  add_header)

// Fetch requests without the "Access-Control-Allow-Origin" header.
CORS_TEST_FETCH_ALL(NoHeader, false)

// Fetch requests with the "Access-Control-Allow-Origin" header.
CORS_TEST_FETCH_ALL(WithHeader, true)

// Like above, but without handling CORS preflight requests.
#define CORS_TEST_FETCH_NO_PREFLIGHT(test_name, handler_main, handler_sub, \
                                     add_header)                           \
  TEST(CorsTest, Fetch##test_name) {                                       \
    CookieTestSetup setup;                                                 \
    CookieResource resource_main;                                          \
    SubResource resource_sub;                                              \
    SetupExecRequest(ExecMode::FETCH, &setup, "CorsTest.Fetch" #test_name, \
                     HandlerType::handler_main, &resource_main,            \
                     HandlerType::handler_sub, &resource_sub, nullptr,     \
                     add_header);                                          \
    CefRefPtr<CorsTestHandler> handler = new CorsTestHandler(&setup);      \
    handler->ExecuteTest();                                                \
    ReleaseAndWaitForDestructor(handler);                                  \
  }

#define CORS_TEST_FETCH_NO_PREFLIGHT_SERVER(name, add_header)                 \
  CORS_TEST_FETCH_NO_PREFLIGHT(name##ServerToServer, SERVER, SERVER,          \
                               add_header)                                    \
  CORS_TEST_FETCH_NO_PREFLIGHT(name##HttpSchemeToServer, HTTP_SCHEME, SERVER, \
                               add_header)                                    \
  CORS_TEST_FETCH_NO_PREFLIGHT(name##CustomStandardSchemeToServer,            \
                               CUSTOM_STANDARD_SCHEME, SERVER, add_header)    \
  CORS_TEST_FETCH_NO_PREFLIGHT(name##CustomNonStandardSchemeToServer,         \
                               CUSTOM_NONSTANDARD_SCHEME, SERVER, add_header)

// Fetch requests without the "Access-Control-Allow-Origin" header.
CORS_TEST_FETCH_NO_PREFLIGHT_SERVER(NoHeaderNoPreflight, false)

// Fetch requests with the "Access-Control-Allow-Origin" header.
CORS_TEST_FETCH_NO_PREFLIGHT_SERVER(WithHeaderNoPreflight, true)

namespace {

enum class RedirectMode {
  MODE_302,
  MODE_307,
};

struct RedirectGetResource : CookieResource {
  RedirectGetResource() {}

  bool VerifyRequest(CefRefPtr<CefRequest> request) const override {
    if (!CookieResource::VerifyRequest(request))
      return false;

    // The "Origin" header should never be present for a redirect.
    const std::string& origin = request->GetHeaderByName("Origin");
    EXPECT_TRUE(origin.empty()) << GetPathURL();
    return origin.empty();
  }
};

void SetupRedirectResponse(RedirectMode mode,
                           const std::string& redirect_url,
                           CefRefPtr<CefResponse> response) {
  if (mode == RedirectMode::MODE_302)
    response->SetStatus(302);
  else if (mode == RedirectMode::MODE_307)
    response->SetStatus(307);
  else
    NOTREACHED();

  response->SetHeaderByName("Location", redirect_url,
                            /*override=*/false);
}

// Test redirect requests.
void SetupRedirectGetRequest(RedirectMode mode,
                             CookieTestSetup* setup,
                             const std::string& test_name,
                             HandlerType main_handler,
                             CookieResource* main_resource,
                             HandlerType redirect_handler,
                             RedirectGetResource* redirect_resource) {
  const std::string& base_path = "/" + test_name;

  // Expect a single redirect request that sends SuccessMsg.
  redirect_resource->Init(redirect_handler, base_path + ".redirect.html",
                          kMimeTypeHtml, GetDefaultSuccessMsgHtml());
  redirect_resource->expected_success_query_ct = 1;

  // Expect a single main request that results in a redirect.
  const std::string& redirect_url = redirect_resource->GetPathURL();
  main_resource->Init(main_handler, base_path, kMimeTypeHtml, std::string());
  SetupRedirectResponse(mode, redirect_url, main_resource->response);

  SetupCookieExpectations(setup, main_resource, redirect_resource);

  setup->AddResource(main_resource);
  setup->AddResource(redirect_resource);
}

}  // namespace

// Test redirect GET requests with different origin combinations.
#define CORS_TEST_REDIRECT_GET(test_name, mode, handler_main,          \
                               handler_redirect)                       \
  TEST(CorsTest, RedirectGet##test_name) {                             \
    CookieTestSetup setup;                                             \
    CookieResource resource_main;                                      \
    RedirectGetResource resource_redirect;                             \
    SetupRedirectGetRequest(                                           \
        RedirectMode::mode, &setup, "CorsTest.RedirectGet" #test_name, \
        HandlerType::handler_main, &resource_main,                     \
        HandlerType::handler_redirect, &resource_redirect);            \
    CefRefPtr<CorsTestHandler> handler = new CorsTestHandler(&setup);  \
    handler->ExecuteTest();                                            \
    ReleaseAndWaitForDestructor(handler);                              \
  }

// Test all redirect GET combinations (same and cross-origin).
#define CORS_TEST_REDIRECT_GET_ALL(name, mode)                                 \
  CORS_TEST_REDIRECT_GET(name##ServerToServer, mode, SERVER, SERVER)           \
  CORS_TEST_REDIRECT_GET(name##ServerToHttpScheme, mode, SERVER, HTTP_SCHEME)  \
  CORS_TEST_REDIRECT_GET(name##ServerToCustomStandardScheme, mode, SERVER,     \
                         CUSTOM_STANDARD_SCHEME)                               \
  CORS_TEST_REDIRECT_GET(name##ServerToCustomNonStandardScheme, mode, SERVER,  \
                         CUSTOM_NONSTANDARD_SCHEME)                            \
  CORS_TEST_REDIRECT_GET(name##ServerToCustomUnregisteredScheme, mode, SERVER, \
                         CUSTOM_UNREGISTERED_SCHEME)                           \
  CORS_TEST_REDIRECT_GET(name##HttpSchemeToServer, mode, HTTP_SCHEME, SERVER)  \
  CORS_TEST_REDIRECT_GET(name##HttpSchemeToHttpScheme, mode, HTTP_SCHEME,      \
                         HTTP_SCHEME)                                          \
  CORS_TEST_REDIRECT_GET(name##HttpSchemeToCustomStandardScheme, mode,         \
                         HTTP_SCHEME, CUSTOM_STANDARD_SCHEME)                  \
  CORS_TEST_REDIRECT_GET(name##HttpSchemeToCustomNonStandardScheme, mode,      \
                         HTTP_SCHEME, CUSTOM_NONSTANDARD_SCHEME)               \
  CORS_TEST_REDIRECT_GET(name##HttpSchemeToCustomUnregisteredScheme, mode,     \
                         HTTP_SCHEME, CUSTOM_UNREGISTERED_SCHEME)              \
  CORS_TEST_REDIRECT_GET(name##CustomStandardSchemeToServer, mode,             \
                         CUSTOM_STANDARD_SCHEME, SERVER)                       \
  CORS_TEST_REDIRECT_GET(name##CustomStandardSchemeToHttpScheme, mode,         \
                         CUSTOM_STANDARD_SCHEME, HTTP_SCHEME)                  \
  CORS_TEST_REDIRECT_GET(name##CustomStandardSchemeToCustomStandardScheme,     \
                         mode, CUSTOM_STANDARD_SCHEME, CUSTOM_STANDARD_SCHEME) \
  CORS_TEST_REDIRECT_GET(name##CustomStandardSchemeToCustomNonStandardScheme,  \
                         mode, CUSTOM_STANDARD_SCHEME,                         \
                         CUSTOM_NONSTANDARD_SCHEME)                            \
  CORS_TEST_REDIRECT_GET(name##CustomStandardSchemeToCustomUnregisteredScheme, \
                         mode, CUSTOM_STANDARD_SCHEME,                         \
                         CUSTOM_UNREGISTERED_SCHEME)                           \
  CORS_TEST_REDIRECT_GET(name##CustomNonStandardSchemeToServer, mode,          \
                         CUSTOM_NONSTANDARD_SCHEME, SERVER)                    \
  CORS_TEST_REDIRECT_GET(name##CustomNonStandardSchemeToHttpScheme, mode,      \
                         CUSTOM_NONSTANDARD_SCHEME, HTTP_SCHEME)               \
  CORS_TEST_REDIRECT_GET(name##CustomNonStandardSchemeToCustomStandardScheme,  \
                         mode, CUSTOM_NONSTANDARD_SCHEME,                      \
                         CUSTOM_STANDARD_SCHEME)                               \
  CORS_TEST_REDIRECT_GET(                                                      \
      name##CustomNonStandardSchemeToCustomNonStandardScheme, mode,            \
      CUSTOM_NONSTANDARD_SCHEME, CUSTOM_NONSTANDARD_SCHEME)                    \
  CORS_TEST_REDIRECT_GET(                                                      \
      name##CustomNonStandardSchemeToCustomUnregisteredScheme, mode,           \
      CUSTOM_NONSTANDARD_SCHEME, CUSTOM_UNREGISTERED_SCHEME)                   \
  CORS_TEST_REDIRECT_GET(name##CustomUnregisteredSchemeToServer, mode,         \
                         CUSTOM_UNREGISTERED_SCHEME, SERVER)                   \
  CORS_TEST_REDIRECT_GET(name##CustomUnregisteredSchemeToHttpScheme, mode,     \
                         CUSTOM_UNREGISTERED_SCHEME, HTTP_SCHEME)              \
  CORS_TEST_REDIRECT_GET(name##CustomUnregisteredSchemeToCustomStandardScheme, \
                         mode, CUSTOM_UNREGISTERED_SCHEME,                     \
                         CUSTOM_STANDARD_SCHEME)                               \
  CORS_TEST_REDIRECT_GET(                                                      \
      name##CustomUnregisteredSchemeToCustomNonStandardScheme, mode,           \
      CUSTOM_UNREGISTERED_SCHEME, CUSTOM_NONSTANDARD_SCHEME)                   \
  CORS_TEST_REDIRECT_GET(                                                      \
      name##CustomUnregisteredSchemeToCustomUnregisteredScheme, mode,          \
      CUSTOM_UNREGISTERED_SCHEME, CUSTOM_UNREGISTERED_SCHEME)

// Redirect GET requests.
CORS_TEST_REDIRECT_GET_ALL(302, MODE_302)
CORS_TEST_REDIRECT_GET_ALL(307, MODE_307)

namespace {

struct PostResource : CookieResource {
  PostResource() {}

  bool expect_downgrade_to_get = false;
  bool was_redirected = false;

  std::string main_origin;
  bool is_cross_origin;

  void InitOrigin(HandlerType main_handler) {
    // Origin is always "null" for non-HTTP(S) schemes.
    // This should only be "null" for non-standard schemes, but Blink is likely
    // using SchemeIsHTTPOrHTTPS() when submitting the form request.
    main_origin = IsNonStandardType(main_handler) ||
                          main_handler == HandlerType::CUSTOM_STANDARD_SCHEME
                      ? "null"
                      : GetOrigin(main_handler);

    // True if the request is considered cross-origin. Any requests between
    // non-standard schemes are considered cross-origin (due to the "null"
    // origin).
    is_cross_origin =
        main_handler != handler ||
        (IsNonStandardType(main_handler) && handler == main_handler);
  }

  bool VerifyRequest(CefRefPtr<CefRequest> request) const override {
    if (!CookieResource::VerifyRequest(request))
      return false;

    // The "Origin" header should be present if the request is POST, and was not
    // redirected cross-origin.
    std::string expected_origin;
    if (!expect_downgrade_to_get) {
      if (was_redirected && is_cross_origin) {
        // Always "null" for cross-origin redirects.
        expected_origin = "null";
      } else {
        expected_origin = main_origin;
      }
    }

    const std::string& origin = request->GetHeaderByName("Origin");
    EXPECT_STREQ(expected_origin.c_str(), origin.c_str()) << GetPathURL();
    if (expected_origin != origin)
      return false;

    const std::string& req_method = request->GetMethod();
    const bool has_post_data = request->GetPostData() != nullptr;
    if (expect_downgrade_to_get) {
      EXPECT_FALSE(has_post_data) << GetPathURL();
      EXPECT_STREQ("GET", req_method.c_str()) << GetPathURL();
      return !has_post_data && req_method == "GET";
    } else {
      EXPECT_TRUE(has_post_data) << GetPathURL();
      EXPECT_STREQ("POST", req_method.c_str()) << GetPathURL();
      return has_post_data && req_method == "POST";
    }
  }
};

std::string GetPostFormHtml(const std::string& submit_url) {
  return "<html><body>"
         "<form id=\"f\" action=\"" +
         submit_url +
         "\" method=\"post\">"
         "<input type=\"hidden\" name=\"n\" value=\"v\"></form>"
         "<script>document.getElementById('f').submit();</script>"
         "</body></html>";
}

// Test redirect requests.
void SetupRedirectPostRequest(RedirectMode mode,
                              CookieTestSetup* setup,
                              const std::string& test_name,
                              HandlerType main_handler,
                              CookieResource* main_resource,
                              PostResource* submit_resource,
                              HandlerType redirect_handler,
                              PostResource* redirect_resource) {
  const std::string& base_path = "/" + test_name;

  // Expect a single redirect request that sends SuccessMsg.
  redirect_resource->Init(redirect_handler, base_path + ".redirect.html",
                          kMimeTypeHtml, GetDefaultSuccessMsgHtml());
  redirect_resource->InitOrigin(main_handler);
  redirect_resource->expected_success_query_ct = 1;

  // 302 redirects will downgrade POST requests to GET.
  redirect_resource->expect_downgrade_to_get = mode == RedirectMode::MODE_302;
  redirect_resource->was_redirected = true;

  // Expect a single submit request that redirects the response.
  const std::string& redirect_url = redirect_resource->GetPathURL();
  submit_resource->Init(main_handler, base_path + ".submit.html", kMimeTypeHtml,
                        std::string());
  submit_resource->InitOrigin(main_handler);
  SetupRedirectResponse(mode, redirect_url, submit_resource->response);

  // Expect a single main request that submits the form.
  const std::string& submit_url = submit_resource->GetPathURL();
  main_resource->Init(main_handler, base_path, kMimeTypeHtml,
                      GetPostFormHtml(submit_url));

  SetupCookieExpectations(setup, main_resource, submit_resource);
  SetupCookieExpectations(setup, main_resource, redirect_resource);

  setup->AddResource(main_resource);
  setup->AddResource(submit_resource);
  setup->AddResource(redirect_resource);
}

}  // namespace

// Test redirect GET requests with different origin combinations.
#define CORS_TEST_REDIRECT_POST(test_name, mode, handler_main,          \
                                handler_redirect)                       \
  TEST(CorsTest, RedirectPost##test_name) {                             \
    CookieTestSetup setup;                                              \
    CookieResource resource_main;                                       \
    PostResource resource_submit, resource_redirect;                    \
    SetupRedirectPostRequest(                                           \
        RedirectMode::mode, &setup, "CorsTest.RedirectPost" #test_name, \
        HandlerType::handler_main, &resource_main, &resource_submit,    \
        HandlerType::handler_redirect, &resource_redirect);             \
    CefRefPtr<CorsTestHandler> handler = new CorsTestHandler(&setup);   \
    handler->ExecuteTest();                                             \
    ReleaseAndWaitForDestructor(handler);                               \
  }

// Test all redirect GET combinations (same and cross-origin).
#define CORS_TEST_REDIRECT_POST_ALL(name, mode)                                \
  CORS_TEST_REDIRECT_POST(name##ServerToServer, mode, SERVER, SERVER)          \
  CORS_TEST_REDIRECT_POST(name##ServerToHttpScheme, mode, SERVER, HTTP_SCHEME) \
  CORS_TEST_REDIRECT_POST(name##ServerToCustomStandardScheme, mode, SERVER,    \
                          CUSTOM_STANDARD_SCHEME)                              \
  CORS_TEST_REDIRECT_POST(name##ServerToCustomNonStandardScheme, mode, SERVER, \
                          CUSTOM_NONSTANDARD_SCHEME)                           \
  CORS_TEST_REDIRECT_POST(name##ServerToCustomUnregisteredScheme, mode,        \
                          SERVER, CUSTOM_UNREGISTERED_SCHEME)                  \
  CORS_TEST_REDIRECT_POST(name##HttpSchemeToServer, mode, HTTP_SCHEME, SERVER) \
  CORS_TEST_REDIRECT_POST(name##HttpSchemeToHttpScheme, mode, HTTP_SCHEME,     \
                          HTTP_SCHEME)                                         \
  CORS_TEST_REDIRECT_POST(name##HttpSchemeToCustomStandardScheme, mode,        \
                          HTTP_SCHEME, CUSTOM_STANDARD_SCHEME)                 \
  CORS_TEST_REDIRECT_POST(name##HttpSchemeToCustomNonStandardScheme, mode,     \
                          HTTP_SCHEME, CUSTOM_NONSTANDARD_SCHEME)              \
  CORS_TEST_REDIRECT_POST(name##HttpSchemeToCustomUnregisteredScheme, mode,    \
                          HTTP_SCHEME, CUSTOM_UNREGISTERED_SCHEME)             \
  CORS_TEST_REDIRECT_POST(name##CustomStandardSchemeToServer, mode,            \
                          CUSTOM_STANDARD_SCHEME, SERVER)                      \
  CORS_TEST_REDIRECT_POST(name##CustomStandardSchemeToHttpScheme, mode,        \
                          CUSTOM_STANDARD_SCHEME, HTTP_SCHEME)                 \
  CORS_TEST_REDIRECT_POST(name##CustomStandardSchemeToCustomStandardScheme,    \
                          mode, CUSTOM_STANDARD_SCHEME,                        \
                          CUSTOM_STANDARD_SCHEME)                              \
  CORS_TEST_REDIRECT_POST(name##CustomStandardSchemeToCustomNonStandardScheme, \
                          mode, CUSTOM_STANDARD_SCHEME,                        \
                          CUSTOM_NONSTANDARD_SCHEME)                           \
  CORS_TEST_REDIRECT_POST(                                                     \
      name##CustomStandardSchemeToCustomUnregisteredScheme, mode,              \
      CUSTOM_STANDARD_SCHEME, CUSTOM_UNREGISTERED_SCHEME)                      \
  CORS_TEST_REDIRECT_POST(name##CustomNonStandardSchemeToServer, mode,         \
                          CUSTOM_NONSTANDARD_SCHEME, SERVER)                   \
  CORS_TEST_REDIRECT_POST(name##CustomNonStandardSchemeToHttpScheme, mode,     \
                          CUSTOM_NONSTANDARD_SCHEME, HTTP_SCHEME)              \
  CORS_TEST_REDIRECT_POST(name##CustomNonStandardSchemeToCustomStandardScheme, \
                          mode, CUSTOM_NONSTANDARD_SCHEME,                     \
                          CUSTOM_STANDARD_SCHEME)                              \
  CORS_TEST_REDIRECT_POST(                                                     \
      name##CustomNonStandardSchemeToCustomNonStandardScheme, mode,            \
      CUSTOM_NONSTANDARD_SCHEME, CUSTOM_NONSTANDARD_SCHEME)                    \
  CORS_TEST_REDIRECT_POST(                                                     \
      name##CustomNonStandardSchemeToCustomUnregisteredScheme, mode,           \
      CUSTOM_NONSTANDARD_SCHEME, CUSTOM_UNREGISTERED_SCHEME)                   \
  CORS_TEST_REDIRECT_POST(name##CustomUnregisteredSchemeToServer, mode,        \
                          CUSTOM_UNREGISTERED_SCHEME, SERVER)                  \
  CORS_TEST_REDIRECT_POST(name##CustomUnregisteredSchemeToHttpScheme, mode,    \
                          CUSTOM_UNREGISTERED_SCHEME, HTTP_SCHEME)             \
  CORS_TEST_REDIRECT_POST(                                                     \
      name##CustomUnregisteredSchemeToCustomStandardScheme, mode,              \
      CUSTOM_UNREGISTERED_SCHEME, CUSTOM_STANDARD_SCHEME)                      \
  CORS_TEST_REDIRECT_POST(                                                     \
      name##CustomUnregisteredSchemeToCustomNonStandardScheme, mode,           \
      CUSTOM_UNREGISTERED_SCHEME, CUSTOM_NONSTANDARD_SCHEME)                   \
  CORS_TEST_REDIRECT_POST(                                                     \
      name##CustomUnregisteredSchemeToCustomUnregisteredScheme, mode,          \
      CUSTOM_UNREGISTERED_SCHEME, CUSTOM_UNREGISTERED_SCHEME)

// Redirect GET requests.
CORS_TEST_REDIRECT_POST_ALL(302, MODE_302)
CORS_TEST_REDIRECT_POST_ALL(307, MODE_307)

// Entry point for creating CORS browser test objects.
// Called from client_app_delegates.cc.
void CreateCorsBrowserTests(client::ClientAppBrowser::DelegateSet& delegates) {
  delegates.insert(new CorsBrowserTest);
}
