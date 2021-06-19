// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/extensions/extension_test_handler.h"

#include "tests/ceftests/test_util.h"
#include "tests/shared/browser/extension_util.h"

namespace {

const char kExtensionPath[] = "view-extension";

// Test extension load/unload.
class ViewLoadUnloadTestHandler : public ExtensionTestHandler {
 public:
  explicit ViewLoadUnloadTestHandler(RequestContextType request_context_type)
      : ExtensionTestHandler(request_context_type) {
    // Only creating the extension browser.
    set_create_main_browser(false);
  }

  // CefExtensionHandler methods:
  void OnExtensionLoaded(CefRefPtr<CefExtension> extension) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_TRUE(extension);
    EXPECT_TRUE(extension->IsLoaded());
    EXPECT_TRUE(extension->GetLoaderContext());
    EXPECT_TRUE(
        loader_request_context()->IsSame(extension->GetLoaderContext()));
    VerifyExtension(extension);

    EXPECT_FALSE(got_loaded_);
    got_loaded_.yes();

    EXPECT_FALSE(extension_);
    extension_ = extension;

    CreateBrowserForExtension();
  }

  void OnExtensionUnloaded(CefRefPtr<CefExtension> extension) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_TRUE(extension);
    EXPECT_FALSE(extension->IsLoaded());
    EXPECT_FALSE(extension->GetLoaderContext());

    EXPECT_FALSE(got_unloaded_);
    got_unloaded_.yes();

    EXPECT_TRUE(extension_);
    EXPECT_TRUE(extension_->IsSame(extension));

    // The extension should no longer be registered with the context.
    if (loader_request_context())
      VerifyExtensionInContext(extension, loader_request_context(), false,
                               true);
    if (request_context() && !request_context_same_loader())
      VerifyExtensionInContext(extension, request_context(), false, false);

    extension_ = nullptr;

    // Execute asynchronously so call stacks have a chance to unwind.
    // Will close the browser windows.
    CefPostTask(TID_UI,
                base::BindOnce(&ViewLoadUnloadTestHandler::DestroyTest, this));
  }

  // CefLoadHandler methods:
  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    EXPECT_FALSE(browser->GetHost()->IsBackgroundHost());

    CefRefPtr<CefExtension> extension = browser->GetHost()->GetExtension();
    EXPECT_TRUE(extension);
    EXPECT_TRUE(extension_->IsSame(extension));

    if (isLoading) {
      EXPECT_FALSE(extension_browser_);
      extension_browser_ = browser;
    } else {
      EXPECT_TRUE(browser->IsSame(extension_browser_));

      const std::string& url = browser->GetMainFrame()->GetURL();
      EXPECT_STREQ(extension_url_.c_str(), url.c_str());

      EXPECT_FALSE(got_load_done_);
      got_load_done_.yes();

      TriggerDestroyTestIfDone();
    }
  }

  // CefResourceRequestHandler methods:
  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    EXPECT_FALSE(browser->GetHost()->IsBackgroundHost());
    EXPECT_TRUE(browser->IsSame(extension_browser_));

    CefRefPtr<CefExtension> extension = browser->GetHost()->GetExtension();
    EXPECT_TRUE(extension);
    EXPECT_TRUE(extension_->IsSame(extension));

    const std::string& url = request->GetURL();
    EXPECT_STREQ(extension_url_.c_str(), url.c_str());

    EXPECT_FALSE(got_url_request_);
    got_url_request_.yes();

    // Handle the resource request.
    return RoutingTestHandler::GetResourceHandler(browser, frame, request);
  }

 protected:
  void OnLoadExtensions() override {
    LoadExtension(kExtensionPath, CreateManifest());
  }

  bool OnMessage(CefRefPtr<CefBrowser> browser,
                 const std::string& message) override {
    EXPECT_FALSE(browser->GetHost()->IsBackgroundHost());
    EXPECT_STREQ("extension_onload", message.c_str());

    CefRefPtr<CefExtension> extension = browser->GetHost()->GetExtension();
    EXPECT_TRUE(extension);
    EXPECT_TRUE(extension_->IsSame(extension));

    EXPECT_TRUE(browser->IsSame(extension_browser_));

    EXPECT_FALSE(got_body_onload_);
    got_body_onload_.yes();

    TriggerDestroyTestIfDone();
    return true;
  }

  void OnDestroyTest() override {
    extension_browser_ = nullptr;

    EXPECT_TRUE(got_loaded_);
    EXPECT_TRUE(got_url_request_);
    EXPECT_TRUE(got_body_onload_);
    EXPECT_TRUE(got_load_done_);
    EXPECT_TRUE(got_unloaded_);
  }

  // Create the default manifest.
  CefRefPtr<CefDictionaryValue> CreateManifest() const {
    return CreateDefaultManifest(ApiPermissionsList());
  }

  // Verify |extension| contents.
  void VerifyExtension(CefRefPtr<CefExtension> extension) const {
    EXPECT_STREQ(("extensions/" + std::string(kExtensionPath)).c_str(),
                 client::extension_util::GetInternalExtensionResourcePath(
                     extension->GetPath())
                     .c_str());

    CefRefPtr<CefDictionaryValue> expected_manifest = CreateManifest();
    TestDictionaryEqual(expected_manifest, extension->GetManifest());

    VerifyExtensionInContext(extension, loader_request_context(), true, true);
    if (!request_context_same_loader())
      VerifyExtensionInContext(extension, request_context(), true, false);
  }

  void CreateBrowserForExtension() {
    const std::string& identifier = extension_->GetIdentifier();
    EXPECT_FALSE(identifier.empty());
    const std::string& origin =
        client::extension_util::GetExtensionOrigin(identifier);
    EXPECT_FALSE(origin.empty());

    // Add extension resources.
    extension_url_ = origin + "extension.html";
    AddResource(extension_url_,
                "<html><body onLoad=" + GetMessageJS("extension_onload") +
                    ">Extension</body></html>",
                "text/html");

    // Create a browser to host the extension.
    CreateBrowser(extension_url_, request_context());
  }

  void TriggerDestroyTestIfDone() {
    if (got_body_onload_ && got_load_done_) {
      TriggerDestroyTest();
    }
  }

  virtual void TriggerDestroyTest() {
    // Execute asynchronously so call stacks have a chance to unwind.
    CefPostTask(TID_UI,
                base::BindOnce(&ViewLoadUnloadTestHandler::UnloadExtension,
                               this, extension_));
  }

  CefRefPtr<CefExtension> extension_;
  std::string extension_url_;
  CefRefPtr<CefBrowser> extension_browser_;

  TrackCallback got_loaded_;
  TrackCallback got_url_request_;
  TrackCallback got_body_onload_;
  TrackCallback got_load_done_;
  TrackCallback got_unloaded_;

  IMPLEMENT_REFCOUNTING(ViewLoadUnloadTestHandler);
  DISALLOW_COPY_AND_ASSIGN(ViewLoadUnloadTestHandler);
};

}  // namespace

EXTENSION_TEST_GROUP_ALL(ViewLoadUnload, ViewLoadUnloadTestHandler)

namespace {

// Same as above but without the unload. Only do this with a custom context to
// avoid poluting the global context.
class ViewLoadNoUnloadTestHandler : public ViewLoadUnloadTestHandler {
 public:
  explicit ViewLoadNoUnloadTestHandler(RequestContextType request_context_type)
      : ViewLoadUnloadTestHandler(request_context_type) {}

 protected:
  void TriggerDestroyTest() override {
    // Release everything that references the request context. This should
    // trigger unload of the extension.
    CloseBrowser(extension_browser_, false);
    extension_browser_ = nullptr;
    ReleaseRequestContexts();
  }
};

}  // namespace

EXTENSION_TEST_GROUP_MINIMAL_CUSTOM(ViewLoadNoUnload,
                                    ViewLoadNoUnloadTestHandler)
