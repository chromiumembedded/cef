// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <cstring>
#include <string>

#include "include/cef_parser.h"
#include "include/cef_stream.h"
#include "include/wrapper/cef_scoped_temp_dir.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/browser/file_util.h"

namespace {

// Extension web_accessible_resources loaded by a web page. Resources declared
// with "use_dynamic_url" are requested through a per-session GUID URL that
// Chromium's extension loader internally redirects to the static-ID URL, and
// admits only when the same loader follows that redirect.
//
// The extension is loaded for the whole test run with --load-extension (there
// is no other way to install one with the Chrome runtime). Its content script
// only matches the test origin, so other tests are not affected.

constexpr char kPageUrl[] = "https://extension-resources-test/page.html";
constexpr char kResultUrl[] = "https://extension-resources-test/result.html";

constexpr char kManifest[] = R"({
  "manifest_version": 3,
  "name": "CEF web accessible resources test",
  "version": "1.0",
  "content_scripts": [{
    "matches": ["https://extension-resources-test/*"],
    "js": ["content.js"],
    "run_at": "document_idle"
  }],
  "web_accessible_resources": [
    {"resources": ["static.svg"], "matches": ["<all_urls>"]},
    {"resources": ["dynamic.svg"], "matches": ["<all_urls>"],
     "use_dynamic_url": true}
  ]
})";

// Loads each image into the page and reports the outcomes by navigating to
// |kResultUrl|: the image size ("<width>x<height>") or "error".
constexpr char kContentScript[] = R"(
if (location.pathname === '/page.html') {
  const load = url => new Promise(resolve => {
    const img = document.createElement('img');
    img.onload = () => resolve(img.naturalWidth + 'x' + img.naturalHeight);
    img.onerror = () => resolve('error');
    img.src = url;
    document.body.append(img);
  });
  (async () => {
    const id = chrome.runtime.id;
    const results = [
      await load(chrome.runtime.getURL('static.svg')),
      await load(chrome.runtime.getURL('dynamic.svg')),
      await load(`chrome-extension://${id}/dynamic.svg`),
    ];
    location.href = 'https://extension-resources-test/result.html?static=' +
        results[0] + '&dynamic=' + results[1] + '&dynamic_static_id=' +
        results[2];
  })();
}
)";

constexpr char kImage[] =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"8\" height=\"8\">"
    "<rect width=\"8\" height=\"8\"/></svg>";
constexpr char kImageSize[] = "8x8";

// Returned by the client for the redirected dynamic resource.
constexpr char kCustomImage[] =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"37\" height=\"23\">"
    "<rect width=\"37\" height=\"23\"/></svg>";
constexpr char kCustomImageSize[] = "37x23";

constexpr char kImageError[] = "error";

constexpr char kDynamicPath[] = "/dynamic.svg";

// Writes the test extension to |directory|. Returns its path or empty.
std::string WriteExtension(CefScopedTempDir& directory) {
  if (!directory.CreateUniqueTempDir()) {
    return std::string();
  }
  const std::string path = directory.GetPath();
  const auto write = [&path](const char* name, const char* content) {
    const std::string file = client::file_util::JoinPath(path, name);
    const int size = static_cast<int>(strlen(content));
    return client::file_util::WriteFile(file, content, size) == size;
  };
  if (write("manifest.json", kManifest) &&
      write("content.js", kContentScript) && write("static.svg", kImage) &&
      write("dynamic.svg", kImage)) {
    return path;
  }
  [[maybe_unused]] const bool deleted = directory.Delete();
  return std::string();
}

// Loads the test extension for the whole run. The delegate lives as long as
// the browser process; the extension directory is deleted with it.
class ExtensionResourcesBrowserTest
    : public client::ClientAppBrowser::Delegate {
 public:
  ExtensionResourcesBrowserTest() = default;

  void OnBeforeCommandLineProcessing(
      CefRefPtr<client::ClientAppBrowser> app,
      CefRefPtr<CefCommandLine> command_line) override {
    if (command_line->HasSwitch("load-extension") || directory_.IsValid()) {
      return;
    }
    const std::string path = WriteExtension(directory_);
    if (!path.empty()) {
      command_line->AppendSwitchWithValue("load-extension", path);
    }
  }

 private:
  CefScopedTempDir directory_;

  IMPLEMENT_REFCOUNTING(ExtensionResourcesBrowserTest);
};

// Returns the value of |name| in |query| ("a=1&b=2"), or empty.
std::string GetQueryValue(const std::string& query, const std::string& name) {
  const std::string prefix = name + "=";
  size_t start = 0;
  while (start <= query.size()) {
    size_t end = query.find('&', start);
    if (end == std::string::npos) {
      end = query.size();
    }
    if (query.compare(start, prefix.size(), prefix) == 0) {
      return query.substr(start + prefix.size(), end - start - prefix.size());
    }
    start = end + 1;
  }
  return std::string();
}

// What the client does with the static-ID destination of the dynamic
// resource's internal GUID -> ID redirect.
enum class DestinationMode {
  // Default handling: the extension loader serves the resource.
  kDefault,
  // Return RV_CANCEL from OnBeforeResourceLoad.
  kCancel,
  // Return a custom image from GetResourceHandler.
  kCustomResponse,
  // Modify the request in OnBeforeResourceLoad. The extension loader can't
  // apply the change when following its redirect, so the modified request is
  // loaded by a new loader that lacks the GUID URL and denies the resource.
  kModifyRequest,
};

class ExtensionResourcesTestHandler : public TestHandler {
 public:
  explicit ExtensionResourcesTestHandler(DestinationMode mode) : mode_(mode) {}

  void RunTest() override {
    AddResource(kPageUrl, "<html><body>Page</body></html>", "text/html");
    AddResource(kResultUrl, "<html><body>Result</body></html>", "text/html");
    CreateBrowser(kPageUrl);
    SetTestTimeout(10000);
  }

  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      bool is_navigation,
      bool is_download,
      const CefString& request_initiator,
      bool& disable_default_handling) override {
    EXPECT_IO_THREAD();
    if (IsRedirectedDestination(request)) {
      got_destination_handler_.yes();
    }
    return this;
  }

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();
    if (IsRedirectedDestination(request)) {
      got_destination_before_resource_load_.yes();
      if (mode_ == DestinationMode::kCancel) {
        return RV_CANCEL;
      }
      if (mode_ == DestinationMode::kModifyRequest) {
        request->SetHeaderByName("X-Extension-Resources-Test", "modified",
                                 /*overwrite=*/true);
      }
    }
    return RV_CONTINUE;
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    EXPECT_IO_THREAD();
    if (IsRedirectedDestination(request)) {
      got_destination_resource_handler_.yes();
      if (mode_ == DestinationMode::kCustomResponse) {
        CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
            const_cast<char*>(kCustomImage), sizeof(kCustomImage) - 1);
        return new CefStreamResourceHandler("image/svg+xml", stream);
      }
    }
    return TestHandler::GetResourceHandler(browser, frame, request);
  }

  void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefResponse> response,
                          CefString& new_url) override {
    EXPECT_IO_THREAD();
    CefURLParts old_parts, new_parts;
    if (!CefParseURL(request->GetURL(), old_parts) ||
        !CefParseURL(new_url, new_parts)) {
      return;
    }
    const std::string old_scheme = CefString(&old_parts.scheme);
    const std::string new_scheme = CefString(&new_parts.scheme);
    const std::string old_host = CefString(&old_parts.host);
    const std::string new_host = CefString(&new_parts.host);
    const std::string old_path = CefString(&old_parts.path);
    const std::string new_path = CefString(&new_parts.path);
    if (old_scheme == "chrome-extension" && new_scheme == "chrome-extension" &&
        old_path == kDynamicPath && new_path == kDynamicPath &&
        old_host != new_host) {
      // The internal redirect from the GUID URL to the static-ID URL.
      EXPECT_FALSE(got_redirect_);
      got_redirect_.yes();
      redirected_request_id_ = request->GetIdentifier();
      destination_url_ = new_url;
    }
  }

  void OnResourceLoadComplete(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              CefRefPtr<CefResponse> response,
                              URLRequestStatus status,
                              int64_t received_content_length) override {
    EXPECT_IO_THREAD();
    if (IsRedirectedDestination(request)) {
      got_destination_complete_.yes();
      destination_status_ = status;
    }
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    const std::string url = frame->GetURL();
    if (!frame->IsMain() || url.rfind(kResultUrl, 0) != 0) {
      return;
    }
    got_result_.yes();

    CefURLParts parts;
    EXPECT_TRUE(CefParseURL(url, parts));
    const std::string query = CefString(&parts.query);
    static_result_ = GetQueryValue(query, "static");
    dynamic_result_ = GetQueryValue(query, "dynamic");
    dynamic_static_id_result_ = GetQueryValue(query, "dynamic_static_id");
    DestroyTest();
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_result_);

    // Static resources load through their static-ID URL.
    EXPECT_STREQ(kImageSize, static_result_.c_str());
    // A dynamic-only resource stays denied through the static-ID URL. The
    // renderer denies this direct request (ResourceRequestPolicy) before it
    // reaches the client, and the destination checks below only match the
    // redirected request ID.
    EXPECT_STREQ(kImageError, dynamic_static_id_result_.c_str());

    // The dynamic resource was requested through its session GUID URL and
    // redirected to its static-ID URL. The client selected a handler for that
    // destination and intercepted the same request.
    EXPECT_TRUE(got_redirect_);
    EXPECT_TRUE(got_destination_handler_);
    EXPECT_TRUE(got_destination_before_resource_load_);
    EXPECT_TRUE(got_destination_complete_);

    switch (mode_) {
      case DestinationMode::kDefault:
        EXPECT_TRUE(got_destination_resource_handler_);
        EXPECT_EQ(UR_SUCCESS, destination_status_);
        EXPECT_STREQ(kImageSize, dynamic_result_.c_str());
        break;
      case DestinationMode::kCancel:
        // Canceled before GetResourceHandler.
        EXPECT_FALSE(got_destination_resource_handler_);
        EXPECT_NE(UR_SUCCESS, destination_status_);
        EXPECT_STREQ(kImageError, dynamic_result_.c_str());
        break;
      case DestinationMode::kCustomResponse:
        EXPECT_TRUE(got_destination_resource_handler_);
        EXPECT_EQ(UR_SUCCESS, destination_status_);
        EXPECT_STREQ(kCustomImageSize, dynamic_result_.c_str());
        break;
      case DestinationMode::kModifyRequest:
        EXPECT_TRUE(got_destination_resource_handler_);
        EXPECT_NE(UR_SUCCESS, destination_status_);
        EXPECT_STREQ(kImageError, dynamic_result_.c_str());
        break;
    }

    TestHandler::DestroyTest();
  }

 private:
  // True for the static-ID destination of the GUID -> ID redirect, on the
  // request that was redirected.
  bool IsRedirectedDestination(CefRefPtr<CefRequest> request) const {
    return got_redirect_ &&
           request->GetIdentifier() == redirected_request_id_ &&
           request->GetURL() == destination_url_;
  }

  const DestinationMode mode_;

  // Accessed on the IO thread.
  uint64_t redirected_request_id_ = 0;
  std::string destination_url_;

  TrackCallback got_redirect_;
  TrackCallback got_destination_handler_;
  TrackCallback got_destination_before_resource_load_;
  TrackCallback got_destination_resource_handler_;
  TrackCallback got_destination_complete_;
  URLRequestStatus destination_status_ = UR_UNKNOWN;

  TrackCallback got_result_;
  std::string static_result_;
  std::string dynamic_result_;
  std::string dynamic_static_id_result_;

  IMPLEMENT_REFCOUNTING(ExtensionResourcesTestHandler);
};

void RunExtensionResourcesTest(DestinationMode mode) {
  if (UseAlloyStyleBrowserGlobal()) {
    // Extensions are only supported with Chrome style browsers.
    return;
  }
  CefRefPtr<ExtensionResourcesTestHandler> handler =
      new ExtensionResourcesTestHandler(mode);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

}  // namespace

// Entry point for creating extension resources browser test objects.
// Called from client_app_delegates.cc.
void CreateExtensionResourcesBrowserTests(
    client::ClientAppBrowser::DelegateSet& delegates) {
  delegates.insert(new ExtensionResourcesBrowserTest);
}

// Test that a web page can load web_accessible_resources of an extension,
// including resources declared with "use_dynamic_url".
TEST(ExtensionResourcesTest, WebAccessibleResources) {
  RunExtensionResourcesTest(DestinationMode::kDefault);
}

// Test that the client can cancel the static-ID destination of a dynamic
// resource's internal redirect from OnBeforeResourceLoad.
TEST(ExtensionResourcesTest, DynamicResourceDestinationCancel) {
  RunExtensionResourcesTest(DestinationMode::kCancel);
}

// Test that the client can return a custom response for the static-ID
// destination of a dynamic resource's internal redirect.
TEST(ExtensionResourcesTest, DynamicResourceDestinationCustomResponse) {
  RunExtensionResourcesTest(DestinationMode::kCustomResponse);
}

// Test that a request modified for the static-ID destination of a dynamic
// resource's internal redirect is loaded with the modification, which loses
// the GUID URL and so is denied.
TEST(ExtensionResourcesTest, DynamicResourceDestinationModified) {
  RunExtensionResourcesTest(DestinationMode::kModifyRequest);
}
