// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/extensions/extension_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/shared/browser/extension_util.h"

#define ALARMS_TEST_GROUP_ALL(name, test_class) \
  EXTENSION_TEST_GROUP_ALL(ChromeAlarms##name, test_class)
#define ALARMS_TEST_GROUP_MINIMAL(name, test_class) \
  EXTENSION_TEST_GROUP_MINIMAL(ChromeAlarms##name, test_class)

namespace {

const char kExtensionPath[] = "alarms-extension";
const char kSuccessMessage[] = "success";

// Base class for testing chrome.alarms methods.
// See https://developer.chrome.com/extensions/alarms
class AlarmsTestHandler : public ExtensionTestHandler {
 public:
  explicit AlarmsTestHandler(RequestContextType request_context_type)
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
    CefPostTask(TID_UI, base::BindOnce(&AlarmsTestHandler::DestroyTest, this));
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
      TriggerAlarmsApiJSFunction();
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

  // Create a manifest that grants access to the alarms API.
  virtual CefRefPtr<CefDictionaryValue> CreateManifest() const {
    ApiPermissionsList api_permissions;
    api_permissions.push_back("alarms");
    return CreateDefaultManifest(api_permissions);
  }

  // Add resources in the extension browser.
  virtual void OnAddExtensionResources(const std::string& origin) {
    extension_url_ = origin + "extension.html";
    AddResource(extension_url_, GetExtensionHTML(), "text/html");
  }

  // Returns the chrome.alarms.* JS that is executed in the extension browser
  // when the triggerAlarmsApi() JS function is called.
  virtual std::string GetAlarmsApiJS() const = 0;

  // Returns the JS that will be loaded in the extension browser. This
  // implements the triggerAlarmsApi() JS function called from
  // TriggerAlarmsApiJSFunction().
  virtual std::string GetExtensionJS() const {
    return "function triggerAlarmsApi() {" + GetAlarmsApiJS() + "}";
  }

  // Returns the HTML that will be loaded in the extension browser.
  virtual std::string GetExtensionHTML() const {
    return "<html><head><script>" + GetExtensionJS() +
           "</script></head><body onLoad=" + GetMessageJS("extension_onload") +
           ">Extension</body></html>";
  }

  virtual void TriggerDestroyTest() {
    // Execute asynchronously so call stacks have a chance to unwind.
    CefPostTask(TID_UI, base::BindOnce(&AlarmsTestHandler::UnloadExtension,
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

  void TriggerAlarmsApiJSFunction() {
    EXPECT_FALSE(got_trigger_api_function_);
    got_trigger_api_function_.yes();

    extension_browser_->GetMainFrame()->ExecuteJavaScript("triggerAlarmsApi();",
                                                          extension_url_, 0);
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

// Test for chrome.alarms.create(string name, object alarmInfo)
// and chrome.alarms.onAlarm.addListener(function callback)
class CreateAlarmTestHandler : public AlarmsTestHandler {
 public:
  explicit CreateAlarmTestHandler(RequestContextType request_context_type)
      : AlarmsTestHandler(request_context_type) {}

 protected:
  std::string GetAlarmsApiJS() const override {
    return "chrome.alarms.onAlarm.addListener(function (alarm) {" +
           GetMessageJS(kSuccessMessage) +
           "});"
           "chrome.alarms.create(\"test\", {delayInMinutes:0.01})";
  }

 private:
  IMPLEMENT_REFCOUNTING(CreateAlarmTestHandler);
  DISALLOW_COPY_AND_ASSIGN(CreateAlarmTestHandler);
};
}  // namespace

ALARMS_TEST_GROUP_ALL(CreateAlarm, CreateAlarmTestHandler)

namespace {

// Test for chrome.alarms.get(string name, function callback)
class GetAlarmTestHandler : public AlarmsTestHandler {
 public:
  explicit GetAlarmTestHandler(RequestContextType request_context_type)
      : AlarmsTestHandler(request_context_type) {}

 protected:
  std::string GetAlarmsApiJS() const override {
    return "chrome.alarms.create(\"test\", {delayInMinutes:1});"
           "setTimeout(function() {"
           "chrome.alarms.get(\"test\", function (alarm) {" +
           GetMessageJS(kSuccessMessage) + "})}, 100)";
  }

 private:
  IMPLEMENT_REFCOUNTING(GetAlarmTestHandler);
  DISALLOW_COPY_AND_ASSIGN(GetAlarmTestHandler);
};
}  // namespace

ALARMS_TEST_GROUP_MINIMAL(GetAlarm, GetAlarmTestHandler)

namespace {

// Test for chrome.alarms.getAll(function callback)
class GetAllAlarmsTestHandler : public AlarmsTestHandler {
 public:
  explicit GetAllAlarmsTestHandler(RequestContextType request_context_type)
      : AlarmsTestHandler(request_context_type) {}

 protected:
  std::string GetAlarmsApiJS() const override {
    return "chrome.alarms.create(\"alarm1\", {delayInMinutes:1});"
           "chrome.alarms.create(\"alarm2\", {delayInMinutes:1});"
           "setTimeout(function() {"
           "chrome.alarms.getAll(function (alarms) {"
           "if (alarms.length == 2) {" +
           GetMessageJS(kSuccessMessage) + "}})}, 100)";
  }

 private:
  IMPLEMENT_REFCOUNTING(GetAllAlarmsTestHandler);
  DISALLOW_COPY_AND_ASSIGN(GetAllAlarmsTestHandler);
};
}  // namespace

ALARMS_TEST_GROUP_MINIMAL(GetAllAlarms, GetAllAlarmsTestHandler)

namespace {

// Test for chrome.alarms.clear(string name, function callback)
class ClearAlarmTestHandler : public AlarmsTestHandler {
 public:
  explicit ClearAlarmTestHandler(RequestContextType request_context_type)
      : AlarmsTestHandler(request_context_type) {}

 protected:
  std::string GetAlarmsApiJS() const override {
    return "chrome.alarms.create(\"test\", {delayInMinutes:1});"
           "setTimeout(function() {"
           "chrome.alarms.clear(\"test\", function (wasCleared) {"
           "if (wasCleared) {" +
           GetMessageJS(kSuccessMessage) + "}})}, 100)";
  }

 private:
  IMPLEMENT_REFCOUNTING(ClearAlarmTestHandler);
  DISALLOW_COPY_AND_ASSIGN(ClearAlarmTestHandler);
};
}  // namespace

ALARMS_TEST_GROUP_MINIMAL(ClearAlarm, ClearAlarmTestHandler)

namespace {

// Test for chrome.alarms.clearAll(function callback)
class ClearAllAlarmsTestHandler : public AlarmsTestHandler {
 public:
  explicit ClearAllAlarmsTestHandler(RequestContextType request_context_type)
      : AlarmsTestHandler(request_context_type) {}

 protected:
  std::string GetAlarmsApiJS() const override {
    return "chrome.alarms.create(\"alarm1\", {delayInMinutes:1});"
           "chrome.alarms.create(\"alarm2\", {delayInMinutes:1});"
           "setTimeout(function() {"
           "chrome.alarms.clearAll(function (wasCleared) {"
           "if (wasCleared) {" +
           GetMessageJS(kSuccessMessage) + "}})}, 100)";
  }

 private:
  IMPLEMENT_REFCOUNTING(ClearAllAlarmsTestHandler);
  DISALLOW_COPY_AND_ASSIGN(ClearAllAlarmsTestHandler);
};
}  // namespace

ALARMS_TEST_GROUP_MINIMAL(ClearAllAlarms, ClearAllAlarmsTestHandler)
