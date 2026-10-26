// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <cstring>
#include <string>

#include "include/cef_parser.h"
#include "include/wrapper/cef_scoped_temp_dir.h"
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
// |kResultUrl|: 1 = loaded, 0 = failed.
constexpr char kContentScript[] = R"(
if (location.pathname === '/page.html') {
  const load = url => new Promise(resolve => {
    const img = document.createElement('img');
    img.onload = () => resolve(1);
    img.onerror = () => resolve(0);
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

class ExtensionResourcesTestHandler : public TestHandler {
 public:
  ExtensionResourcesTestHandler() = default;

  void RunTest() override {
    AddResource(kPageUrl, "<html><body>Page</body></html>", "text/html");
    AddResource(kResultUrl, "<html><body>Result</body></html>", "text/html");
    CreateBrowser(kPageUrl);
    SetTestTimeout(10000);
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
    static_loaded_ = query.find("static=1") != std::string::npos;
    dynamic_loaded_ = query.find("dynamic=1") != std::string::npos;
    dynamic_static_id_loaded_ =
        query.find("dynamic_static_id=1") != std::string::npos;
    DestroyTest();
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_result_);
    // Static resources load through their static-ID URL.
    EXPECT_TRUE(static_loaded_);
    // Dynamic resources load through their session GUID URL.
    EXPECT_TRUE(dynamic_loaded_);
    // A dynamic-only resource stays denied through the static-ID URL.
    EXPECT_FALSE(dynamic_static_id_loaded_);
    TestHandler::DestroyTest();
  }

 private:
  TrackCallback got_result_;
  bool static_loaded_ = false;
  bool dynamic_loaded_ = false;
  bool dynamic_static_id_loaded_ = true;

  IMPLEMENT_REFCOUNTING(ExtensionResourcesTestHandler);
};

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
  if (UseAlloyStyleBrowserGlobal()) {
    // Extensions are only supported with Chrome style browsers.
    return;
  }
  CefRefPtr<ExtensionResourcesTestHandler> handler =
      new ExtensionResourcesTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
