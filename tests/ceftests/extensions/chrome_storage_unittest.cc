// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/extensions/extension_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/shared/browser/extension_util.h"

#define STORAGE_TEST_GROUP_ALL(name, test_class) \
  EXTENSION_TEST_GROUP_ALL(ChromeStorage##name, test_class)
#define STORAGE_TEST_GROUP_MINIMAL(name, test_class) \
  EXTENSION_TEST_GROUP_MINIMAL(ChromeStorage##name, test_class)

namespace {

const char kExtensionPath[] = "storage-extension";
const char kSuccessMessage[] = "success";

// Base class for testing chrome.storage methods.
// See https://developer.chrome.com/extensions/storage
class StorageTestHandler : public ExtensionTestHandler {
 public:
  explicit StorageTestHandler(RequestContextType request_context_type)
      : ExtensionTestHandler(request_context_type) {
    // Only creating the extension browser.
    set_create_main_browser(false);
  }

  // CefExtensionHandler methods:
  void OnExtensionLoaded(CefRefPtr<CefExtension> extension) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_FALSE(got_loaded_);
    got_loaded_.yes();

    // Verify |extension| contents.
    EXPECT_FALSE(extension->GetIdentifier().empty());
    EXPECT_STREQ(("extensions/" + std::string(kExtensionPath)).c_str(),
                 client::extension_util::GetInternalExtensionResourcePath(
                     extension->GetPath())
                     .c_str());
    TestDictionaryEqual(CreateManifest(), extension->GetManifest());

    EXPECT_FALSE(extension_);
    extension_ = extension;

    CreateBrowserForExtension();
  }

  void OnExtensionUnloaded(CefRefPtr<CefExtension> extension) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_TRUE(extension_);
    EXPECT_TRUE(extension_->IsSame(extension));
    EXPECT_FALSE(got_unloaded_);
    got_unloaded_.yes();
    extension_ = nullptr;

    // Execute asynchronously so call stacks have a chance to unwind.
    // Will close the browser windows.
    CefPostTask(TID_UI, base::BindOnce(&StorageTestHandler::DestroyTest, this));
  }

  // CefLoadHandler methods:
  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
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
    }
  }

  // CefResourceRequestHandler methods:
  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
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
    if (message == "extension_onload") {
      // From body onLoad in the extension browser.
      EXPECT_TRUE(browser->IsSame(extension_browser_));
      EXPECT_FALSE(got_body_onload_);
      got_body_onload_.yes();
      TriggerStorageApiJSFunction();
      return true;
    }
    EXPECT_TRUE(browser->IsSame(extension_browser_));
    EXPECT_FALSE(got_success_message_);
    got_success_message_.yes();
    EXPECT_STREQ(kSuccessMessage, message.c_str());
    TriggerDestroyTest();
    return true;
  }

  void OnDestroyTest() override {
    extension_browser_ = nullptr;

    EXPECT_TRUE(got_loaded_);
    EXPECT_TRUE(got_url_request_);
    EXPECT_TRUE(got_body_onload_);
    EXPECT_TRUE(got_trigger_api_function_);
    EXPECT_TRUE(got_success_message_);
    EXPECT_TRUE(got_unloaded_);
  }

  // Create a manifest that grants access to the storage API.
  virtual CefRefPtr<CefDictionaryValue> CreateManifest() const {
    ApiPermissionsList api_permissions;
    api_permissions.push_back("storage");
    return CreateDefaultManifest(api_permissions);
  }

  // Add resources in the extension browser.
  virtual void OnAddExtensionResources(const std::string& origin) {
    extension_url_ = origin + "extension.html";
    AddResource(extension_url_, GetExtensionHTML(), "text/html");
  }

  // Returns the chrome.storage.* JS that is executed in the extension browser
  // when the triggerStorageApi() JS function is called.
  virtual std::string GetStorageApiJS() const = 0;

  // Returns the JS that will be loaded in the extension browser. This
  // implements the triggerStorageApi() JS function called from
  // TriggerStorageApiJSFunction().
  virtual std::string GetExtensionJS() const {
    return "function triggerStorageApi() {" + GetStorageApiJS() + "}";
  }

  // Returns the HTML that will be loaded in the extension browser.
  virtual std::string GetExtensionHTML() const {
    return "<html><head><script>" + GetExtensionJS() +
           "</script></head><body onLoad=" + GetMessageJS("extension_onload") +
           ">Extension</body></html>";
  }

  virtual void TriggerDestroyTest() {
    // Execute asynchronously so call stacks have a chance to unwind.
    CefPostTask(TID_UI, base::BindOnce(&StorageTestHandler::UnloadExtension,
                                       this, extension_));
  }

  CefRefPtr<CefExtension> extension() const { return extension_; }
  std::string extension_url() const { return extension_url_; }
  CefRefPtr<CefBrowser> extension_browser() const { return extension_browser_; }

  bool got_success_message() const { return got_success_message_; }

 private:
  void CreateBrowserForExtension() {
    const std::string& identifier = extension_->GetIdentifier();
    EXPECT_FALSE(identifier.empty());
    const std::string& origin =
        client::extension_util::GetExtensionOrigin(identifier);
    EXPECT_FALSE(origin.empty());

    // Add extension resources.
    OnAddExtensionResources(origin);

    // Create a browser to host the extension.
    CreateBrowser(extension_url_, request_context());
  }

  void TriggerStorageApiJSFunction() {
    EXPECT_FALSE(got_trigger_api_function_);
    got_trigger_api_function_.yes();

    extension_browser_->GetMainFrame()->ExecuteJavaScript(
        "triggerStorageApi();", extension_url_, 0);
  }

  CefRefPtr<CefExtension> extension_;
  std::string extension_url_;
  CefRefPtr<CefBrowser> extension_browser_;

  TrackCallback got_loaded_;
  TrackCallback got_url_request_;
  TrackCallback got_body_onload_;
  TrackCallback got_trigger_api_function_;
  TrackCallback got_success_message_;
  TrackCallback got_unloaded_;
};
}  // namespace

namespace {

// Test for chrome.storage.local.set(object items, function callback)
// and  for chrome.storage.local.get(string or array of string or object keys,
//                                   function callback)
class LocalStorageTestHandler : public StorageTestHandler {
 public:
  explicit LocalStorageTestHandler(RequestContextType request_context_type)
      : StorageTestHandler(request_context_type) {}

 protected:
  std::string GetStorageApiJS() const override {
    return "chrome.storage.local.set({\"local_key_1\": \"local_value_1\"}, "
           "function() {"
           "chrome.storage.local.get(\"local_key_1\", function (items) {"
           "if(items[\"local_key_1\"] == \"local_value_1\") {" +
           GetMessageJS(kSuccessMessage) +
           "}});"
           "});";
  }

 private:
  IMPLEMENT_REFCOUNTING(LocalStorageTestHandler);
  DISALLOW_COPY_AND_ASSIGN(LocalStorageTestHandler);
};
}  // namespace

STORAGE_TEST_GROUP_ALL(LocalStorage, LocalStorageTestHandler)

namespace {

// Test for chrome.storage.local.getBytesInUse(string or array of string keys,
//                                             function callback)
class LocalStorageGetBytesInUseTestHandler : public StorageTestHandler {
 public:
  explicit LocalStorageGetBytesInUseTestHandler(
      RequestContextType request_context_type)
      : StorageTestHandler(request_context_type) {}

 protected:
  std::string GetStorageApiJS() const override {
    return "chrome.storage.local.set({\"local_key_2\": \"local_value_2\"}, "
           "function() {"
           "chrome.storage.local.getBytesInUse(\"local_key_2\", function "
           "(bytesInUse) {"
           "if (bytesInUse == 26) {" +
           GetMessageJS(kSuccessMessage) +
           "}});"
           "});";
  }

 private:
  IMPLEMENT_REFCOUNTING(LocalStorageGetBytesInUseTestHandler);
  DISALLOW_COPY_AND_ASSIGN(LocalStorageGetBytesInUseTestHandler);
};
}  // namespace

STORAGE_TEST_GROUP_MINIMAL(LocalStorageGetBytesInUse,
                           LocalStorageGetBytesInUseTestHandler)

namespace {

// Test for chrome.storage.local.remove(string or array of string keys, function
// callback)
class LocalStorageRemoveTestHandler : public StorageTestHandler {
 public:
  explicit LocalStorageRemoveTestHandler(
      RequestContextType request_context_type)
      : StorageTestHandler(request_context_type) {}

 protected:
  std::string GetStorageApiJS() const override {
    return "chrome.storage.local.set({\"local_key_3\": \"local_value_3\"}, "
           "function() {"
           "chrome.storage.local.remove(\"local_key_3\", function () {"
           "chrome.storage.local.get(\"local_key_3\", function(items) {"
           "if (items[\"local_key_3\"] == undefined) {" +
           GetMessageJS(kSuccessMessage) +
           "}})})"
           "});";
  }

 private:
  IMPLEMENT_REFCOUNTING(LocalStorageRemoveTestHandler);
  DISALLOW_COPY_AND_ASSIGN(LocalStorageRemoveTestHandler);
};
}  // namespace

STORAGE_TEST_GROUP_MINIMAL(LocalStorageRemove, LocalStorageRemoveTestHandler)

namespace {

// Test for chrome.storage.local.clear(function callback)
class LocalStorageClearTestHandler : public StorageTestHandler {
 public:
  explicit LocalStorageClearTestHandler(RequestContextType request_context_type)
      : StorageTestHandler(request_context_type) {}

 protected:
  std::string GetStorageApiJS() const override {
    return "var value1Cleared = false;"
           "var value2Cleared = false;"
           "function checkCleared() {"
           "if (value1Cleared && value2Cleared) {" +
           GetMessageJS(kSuccessMessage) +
           "}}"
           "chrome.storage.local.set({\"local_key_4\": \"local_value_4\","
           "\"local_key_5\": \"local_value_5\"}, function() {"
           "chrome.storage.local.clear(function () {"

           "chrome.storage.local.get(\"local_key_4\", function(items) {"
           "if (items[\"local_key_4\"] == undefined) {"
           "value1Cleared = true;"
           "checkCleared();"
           "}});"

           "chrome.storage.local.get(\"local_key_5\", function(items) {"
           "if (items[\"local_key_5\"] == undefined) {"
           "value2Cleared = true;"
           "checkCleared();"
           "}});"
           "})});";
  }

 private:
  IMPLEMENT_REFCOUNTING(LocalStorageClearTestHandler);
  DISALLOW_COPY_AND_ASSIGN(LocalStorageClearTestHandler);
};
}  // namespace

STORAGE_TEST_GROUP_MINIMAL(LocalStorageClear, LocalStorageClearTestHandler)

namespace {

// Test for chrome.storage.sync.set(object items, function callback)
// and  for chrome.storage.sync.get(string or array of string or object keys,
//                                   function callback)
class SyncStorageTestHandler : public StorageTestHandler {
 public:
  explicit SyncStorageTestHandler(RequestContextType request_context_type)
      : StorageTestHandler(request_context_type) {}

 protected:
  std::string GetStorageApiJS() const override {
    return "chrome.storage.sync.set({\"sync_key_1\": \"sync_value_1\"}, "
           "function() {"
           "chrome.storage.sync.get(\"sync_key_1\", function (items) {"
           "if (items[\"sync_key_1\"] == \"sync_value_1\") {" +
           GetMessageJS(kSuccessMessage) +
           "}});"
           "});";
  }

 private:
  IMPLEMENT_REFCOUNTING(SyncStorageTestHandler);
  DISALLOW_COPY_AND_ASSIGN(SyncStorageTestHandler);
};
}  // namespace

STORAGE_TEST_GROUP_ALL(SyncStorage, SyncStorageTestHandler)

namespace {

// Test for chrome.storage.sync.getBytesInUse(string or array of string keys,
//                                             function callback)
class SyncStorageGetBytesInUseTestHandler : public StorageTestHandler {
 public:
  explicit SyncStorageGetBytesInUseTestHandler(
      RequestContextType request_context_type)
      : StorageTestHandler(request_context_type) {}

 protected:
  std::string GetStorageApiJS() const override {
    return "chrome.storage.sync.set({\"sync_key_2\": \"sync_value_2\"}, "
           "function() {"
           "chrome.storage.sync.getBytesInUse(\"sync_key_2\", function "
           "(bytesInUse) {"
           "if (bytesInUse == 24) {" +
           GetMessageJS(kSuccessMessage) +
           "}});"
           "});";
  }

 private:
  IMPLEMENT_REFCOUNTING(SyncStorageGetBytesInUseTestHandler);
  DISALLOW_COPY_AND_ASSIGN(SyncStorageGetBytesInUseTestHandler);
};
}  // namespace

STORAGE_TEST_GROUP_MINIMAL(SyncStorageGetBytesInUse,
                           SyncStorageGetBytesInUseTestHandler)

namespace {

// Test for chrome.storage.sync.remove(string or array of string keys, function
// callback)
class SyncStorageRemoveTestHandler : public StorageTestHandler {
 public:
  explicit SyncStorageRemoveTestHandler(RequestContextType request_context_type)
      : StorageTestHandler(request_context_type) {}

 protected:
  std::string GetStorageApiJS() const override {
    return "chrome.storage.sync.set({\"sync_key_3\": \"sync_value_3\"}, "
           "function() {"
           "chrome.storage.sync.remove(\"sync_key_3\", function () {"
           "chrome.storage.sync.get(\"sync_key_3\", function(items) {"
           "if (items[\"sync_key_3\"] == undefined) {" +
           GetMessageJS(kSuccessMessage) +
           "}})})"
           "});";
  }

 private:
  IMPLEMENT_REFCOUNTING(SyncStorageRemoveTestHandler);
  DISALLOW_COPY_AND_ASSIGN(SyncStorageRemoveTestHandler);
};
}  // namespace

STORAGE_TEST_GROUP_MINIMAL(SyncStorageRemove, SyncStorageRemoveTestHandler)

namespace {

// Test for chrome.storage.sync.clear(function callback)
class SyncStorageClearTestHandler : public StorageTestHandler {
 public:
  explicit SyncStorageClearTestHandler(RequestContextType request_context_type)
      : StorageTestHandler(request_context_type) {}

 protected:
  std::string GetStorageApiJS() const override {
    return "var value1Cleared = false;"
           "var value2Cleared = false;"

           "function checkCleared() {"
           "if (value1Cleared && value2Cleared) {" +
           GetMessageJS(kSuccessMessage) +
           "}}"

           "chrome.storage.sync.set({\"sync_key_4\": \"sync_value_4\","
           "\"sync_key_5\": \"sync_value_5\"}, function() {"
           "chrome.storage.sync.clear(function () {"

           "chrome.storage.sync.get(\"sync_key_4\", function(items) {"
           "if (items[\"sync_key_4\"] == undefined) {"
           "value1Cleared = true;"
           "checkCleared();"
           "}});"

           "chrome.storage.sync.get(\"sync_key_5\", function(items) {"
           "if (items[\"sync_key_5\"] == undefined) {"
           "value2Cleared = true;"
           "checkCleared();"
           "}});"

           "})});";
  }

 private:
  IMPLEMENT_REFCOUNTING(SyncStorageClearTestHandler);
  DISALLOW_COPY_AND_ASSIGN(SyncStorageClearTestHandler);
};
}  // namespace

STORAGE_TEST_GROUP_MINIMAL(SyncStorageClear, SyncStorageClearTestHandler)
