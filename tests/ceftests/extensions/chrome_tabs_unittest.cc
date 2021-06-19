// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/extensions/extension_test_handler.h"

#include <sstream>

#include "include/cef_parser.h"

#include "tests/ceftests/test_util.h"
#include "tests/shared/browser/extension_util.h"

#define TABS_TEST_GROUP_ALL(name, test_class) \
  EXTENSION_TEST_GROUP_ALL(ChromeTabs##name, test_class)
#define TABS_TEST_GROUP_MINIMAL(name, test_class) \
  EXTENSION_TEST_GROUP_MINIMAL(ChromeTabs##name, test_class)

namespace {

const char kMainBrowserURL[] = "https://test-extensions.com/chrome-tabs";
const char kExtensionPath[] = "tabs-extension";
const char kSuccessMessage[] = "success";

// Base class for testing chrome.tabs methods.
// See https://developer.chrome.com/extensions/tabs
class TabsTestHandler : public ExtensionTestHandler {
 public:
  explicit TabsTestHandler(RequestContextType request_context_type)
      : ExtensionTestHandler(request_context_type),
        create_main_browser_first_(false),
        expect_get_active_browser_(true),
        expect_success_in_main_browser_(true),
        expected_api_call_count_(1),
        got_get_active_browser_count_(0),
        got_can_access_browser_count_(0) {}

  // CefExtensionHandler methods:
  void OnExtensionLoaded(CefRefPtr<CefExtension> extension) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_FALSE(got_extension_loaded_);
    got_extension_loaded_.yes();

    // Verify |extension| contents.
    EXPECT_FALSE(extension->GetIdentifier().empty());
    EXPECT_STREQ(("extensions/" + std::string(kExtensionPath)).c_str(),
                 client::extension_util::GetInternalExtensionResourcePath(
                     extension->GetPath())
                     .c_str());
    TestDictionaryEqual(CreateManifest(), extension->GetManifest());

    EXPECT_FALSE(extension_);
    extension_ = extension;

    if (create_main_browser_first_)
      CreateBrowserForExtensionIfReady();
    else
      CreateBrowserForExtension();
  }

  void OnExtensionUnloaded(CefRefPtr<CefExtension> extension) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_TRUE(extension_);
    EXPECT_TRUE(extension_->IsSame(extension));
    EXPECT_FALSE(got_extension_unloaded_);
    got_extension_unloaded_.yes();
    extension_ = nullptr;

    // Execute asynchronously so call stacks have a chance to unwind.
    // Will close the browser windows.
    CefPostTask(TID_UI, base::BindOnce(&TabsTestHandler::DestroyTest, this));
  }

  CefRefPtr<CefBrowser> GetActiveBrowser(CefRefPtr<CefExtension> extension,
                                         CefRefPtr<CefBrowser> browser,
                                         bool include_incognito) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_TRUE(extension_);
    EXPECT_TRUE(extension_->IsSame(extension));
    EXPECT_TRUE(main_browser_);

    EXPECT_LE(got_get_active_browser_count_, expected_api_call_count_);
    got_get_active_browser_count_++;

    // Tabs APIs will operate on the main browser.
    return main_browser_;
  }

  bool CanAccessBrowser(CefRefPtr<CefExtension> extension,
                        CefRefPtr<CefBrowser> browser,
                        bool include_incognito,
                        CefRefPtr<CefBrowser> target_browser) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_TRUE(extension_);
    EXPECT_TRUE(extension_->IsSame(extension));
    EXPECT_TRUE(main_browser_);
    EXPECT_TRUE(main_browser_->IsSame(target_browser));

    EXPECT_LE(got_can_access_browser_count_, expected_api_call_count_);
    got_can_access_browser_count_++;

    return true;
  }

  // CefLoadHandler methods:
  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (isLoading) {
      // Keep a reference to both browsers.
      if (browser->GetHost()->GetExtension()) {
        EXPECT_FALSE(extension_browser_);
        extension_browser_ = browser;
      } else {
        EXPECT_FALSE(main_browser_);
        main_browser_ = browser;
      }
    } else {
      const std::string& url = browser->GetMainFrame()->GetURL();
      if (browser->GetHost()->GetExtension()) {
        EXPECT_TRUE(browser->IsSame(extension_browser_));
        EXPECT_STREQ(extension_url_.c_str(), url.c_str());
      } else {
        EXPECT_TRUE(browser->IsSame(main_browser_));
        EXPECT_STREQ(kMainBrowserURL, url.c_str());
      }
    }
  }

  // CefResourceRequestHandler methods:
  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    const std::string& url = request->GetURL();
    if (url == kMainBrowserURL) {
      EXPECT_TRUE(browser->IsSame(main_browser_));
      EXPECT_FALSE(got_main_url_request_);
      got_main_url_request_.yes();
    } else if (url == extension_url_) {
      EXPECT_TRUE(browser->IsSame(extension_browser_));
      EXPECT_FALSE(got_extension_url_request_);
      got_extension_url_request_.yes();
    }

    // Handle the resource request.
    return RoutingTestHandler::GetResourceHandler(browser, frame, request);
  }

 protected:
  void OnAddMainBrowserResources() override {
    AddResource(kMainBrowserURL, GetMainBrowserHTML(), "text/html");
  }

  void OnCreateMainBrowser() override {
    CreateBrowser(kMainBrowserURL, request_context());
  }

  void OnLoadExtensions() override {
    LoadExtension(kExtensionPath, CreateManifest());
  }

  bool OnMessage(CefRefPtr<CefBrowser> browser,
                 const std::string& message) override {
    if (message == "main_onload") {
      // From body onLoad in the main browser.
      EXPECT_TRUE(browser->IsSame(main_browser_));
      EXPECT_FALSE(got_main_body_onload_);
      got_main_body_onload_.yes();
      if (create_main_browser_first_)
        CreateBrowserForExtensionIfReady();
      TriggerTabsApiJSFunctionIfReady();
      return true;
    }
    if (message == "extension_onload") {
      // From body onLoad in the extension browser.
      EXPECT_TRUE(browser->IsSame(extension_browser_));
      EXPECT_FALSE(got_extension_body_onload_);
      got_extension_body_onload_.yes();
      TriggerTabsApiJSFunctionIfReady();
      return true;
    }

    // The success message usually orginates from the logic in
    // GetMainBrowserSuccessHEAD/BODY(). It may occasionally originate from the
    // extension browser if we don't know how to detect success in the main
    // browser.
    if (expect_success_in_main_browser_) {
      EXPECT_TRUE(browser->IsSame(main_browser_));
    } else {
      EXPECT_TRUE(browser->IsSame(extension_browser_));
    }
    EXPECT_FALSE(got_success_message_);
    got_success_message_.yes();
    EXPECT_STREQ(kSuccessMessage, message.c_str());
    TriggerDestroyTest();
    return true;
  }

  void OnDestroyTest() override {
    main_browser_ = nullptr;
    extension_browser_ = nullptr;

    EXPECT_TRUE(got_extension_loaded_);
    EXPECT_TRUE(got_main_url_request_);
    EXPECT_TRUE(got_extension_url_request_);
    EXPECT_TRUE(got_main_body_onload_);
    EXPECT_TRUE(got_extension_body_onload_);
    EXPECT_TRUE(got_trigger_api_function_);
    EXPECT_TRUE(got_success_message_);
    EXPECT_TRUE(got_extension_unloaded_);

    if (expect_get_active_browser_) {
      EXPECT_EQ(expected_api_call_count_, got_get_active_browser_count_);
      EXPECT_EQ(0, got_can_access_browser_count_);
    } else {
      EXPECT_EQ(0, got_get_active_browser_count_);
      EXPECT_EQ(expected_api_call_count_, got_can_access_browser_count_);
    }
  }

  // Create a manifest that grants access to the tabs API.
  virtual CefRefPtr<CefDictionaryValue> CreateManifest() const {
    ApiPermissionsList api_permissions;
    api_permissions.push_back("tabs");
    return CreateDefaultManifest(api_permissions);
  }

  // Add resources in the extension browser.
  virtual void OnAddExtensionResources(const std::string& origin) {
    extension_url_ = origin + "extension.html";
    AddResource(extension_url_, GetExtensionHTML(), "text/html");
  }

  // Returns the target tabId (null, or value >= 0).
  virtual std::string GetTargetTabId() const { return "null"; }

  // Returns the logic in the main browser that triggers on success. It should
  // execute GetMessageJS(kSuccessMessage).
  virtual std::string GetMainBrowserSuccessHEAD() const {
    return std::string();
  }
  virtual std::string GetMainBrowserSuccessBODY() const {
    return std::string();
  }

  // Returns the HTML that will be loaded in the main browser.
  virtual std::string GetMainBrowserHTML() const {
    return "<html><head>" + GetMainBrowserSuccessHEAD() +
           "</head><body onLoad=" + GetMessageJS("main_onload") + ">Main" +
           GetMainBrowserSuccessBODY() + "</body></html>";
  }

  // Returns the chrome.tabs.* JS that is executed in the extension browser
  // when the triggerTabsApi() JS function is called.
  virtual std::string GetTabsApiJS() const = 0;

  // Returns the JS that will be loaded in the extension browser. This
  // implements the triggerTabsApi() JS function called from
  // TriggerTabsApiJSFunction().
  virtual std::string GetExtensionJS() const {
    return "function triggerTabsApi() {" + GetTabsApiJS() + "}";
  }

  // Returns the HTML that will be loaded in the extension browser.
  virtual std::string GetExtensionHTML() const {
    return "<html><head><script>" + GetExtensionJS() +
           "</script></head><body onLoad=" + GetMessageJS("extension_onload") +
           ">Extension</body></html>";
  }

  virtual void TriggerDestroyTest() {
    // Execute asynchronously so call stacks have a chance to unwind.
    CefPostTask(TID_UI, base::BindOnce(&TabsTestHandler::UnloadExtension, this,
                                       extension_));
  }

  CefRefPtr<CefExtension> extension() const { return extension_; }
  std::string extension_url() const { return extension_url_; }
  CefRefPtr<CefBrowser> main_browser() const { return main_browser_; }
  CefRefPtr<CefBrowser> extension_browser() const { return extension_browser_; }

  void set_create_main_browser_first(bool val) {
    create_main_browser_first_ = val;
  }
  void set_expect_get_active_browser(bool val) {
    expect_get_active_browser_ = val;
  }
  void set_expect_success_in_main_browser(bool val) {
    expect_success_in_main_browser_ = val;
  }
  void set_expected_api_call_count(int val) { expected_api_call_count_ = val; }

  bool got_success_message() const { return got_success_message_; }
  void set_got_success_message() { got_success_message_.yes(); }

 private:
  void CreateBrowserForExtensionIfReady() {
    DCHECK(create_main_browser_first_);
    if (extension_ && main_browser_)
      CreateBrowserForExtension();
  }

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

  void TriggerTabsApiJSFunctionIfReady() {
    if (got_main_body_onload_ && got_extension_body_onload_)
      TriggerTabsApiJSFunction();
  }

  void TriggerTabsApiJSFunction() {
    EXPECT_FALSE(got_trigger_api_function_);
    got_trigger_api_function_.yes();

    extension_browser_->GetMainFrame()->ExecuteJavaScript("triggerTabsApi();",
                                                          extension_url_, 0);
  }

  // If true the main browser will be created before the extension browser.
  // Otherwise the creation order is undefined.
  bool create_main_browser_first_;

  // If true we expect GetActiveBrowser() but not CanAccessBrowser() to be
  // called. Else visa-versa.
  bool expect_get_active_browser_;

  // If true we expect the success message to be delivered in the main browser.
  // Else expect it in the extension browser.
  bool expect_success_in_main_browser_;

  // Number of expected calls to GetActiveBrowser or CanAccessBrowser. This
  // should match the number of calls to chrome.tabs.* API functions in the
  // test.
  int expected_api_call_count_;

  CefRefPtr<CefExtension> extension_;
  std::string extension_url_;
  CefRefPtr<CefBrowser> main_browser_;
  CefRefPtr<CefBrowser> extension_browser_;

  TrackCallback got_extension_loaded_;
  TrackCallback got_main_url_request_;
  TrackCallback got_extension_url_request_;
  TrackCallback got_main_body_onload_;
  TrackCallback got_extension_body_onload_;
  TrackCallback got_trigger_api_function_;
  TrackCallback got_success_message_;
  TrackCallback got_extension_unloaded_;

  int got_get_active_browser_count_;
  int got_can_access_browser_count_;
};

//
// chrome.tabs.create tests.
//

const char kCreateBrowserURL[] =
    "https://test-extensions.com/chrome-tabs-create";
const char kTabCallbackMessage[] = "tab-callback";
const int kCreateTabIndex = 2;

// Class for chrome.tabs.create tests.
class CreateTestHandler : public TabsTestHandler {
 public:
  explicit CreateTestHandler(RequestContextType request_context_type)
      : TabsTestHandler(request_context_type) {}

  bool OnBeforeBrowser(CefRefPtr<CefExtension> extension,
                       CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefBrowser> active_browser,
                       int index,
                       const CefString& url,
                       bool foreground,
                       CefWindowInfo& windowInfo,
                       CefRefPtr<CefClient>& client,
                       CefBrowserSettings& settings) override {
    EXPECT_TRUE(extension->IsSame(this->extension()));
    EXPECT_TRUE(browser->IsSame(extension_browser()));
    EXPECT_TRUE(active_browser->IsSame(main_browser()));
    EXPECT_EQ(kCreateTabIndex, index);
    EXPECT_STREQ(kCreateBrowserURL, url.ToString().c_str());
    EXPECT_TRUE(foreground);
    EXPECT_TRUE(client);

    EXPECT_FALSE(got_on_before_browser_);
    got_on_before_browser_.yes();

    return false;
  }

  void OnAddMainBrowserResources() override {
    AddResource(kCreateBrowserURL, GetCreatedBrowserHTML(), "text/html");

    TabsTestHandler::OnAddMainBrowserResources();
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (extension_browser() && main_browser()) {
      if (isLoading) {
        // Keep a reference to the newly created browser.
        EXPECT_FALSE(created_browser_);
        created_browser_ = browser;
        return;
      } else {
        const std::string& url = browser->GetMainFrame()->GetURL();
        if (url == kCreateBrowserURL) {
          EXPECT_TRUE(browser->IsSame(created_browser_));
          return;
        }
      }
    }

    TabsTestHandler::OnLoadingStateChange(browser, isLoading, canGoBack,
                                          canGoForward);
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    const std::string& url = request->GetURL();
    if (url == kCreateBrowserURL) {
      EXPECT_TRUE(browser->IsSame(created_browser_));
      EXPECT_FALSE(got_create_browser_url_request_);
      got_create_browser_url_request_.yes();
    }

    return TabsTestHandler::GetResourceHandler(browser, frame, request);
  }

 protected:
  std::string GetTabsApiJS() const override {
    std::stringstream ss;
    ss << kCreateTabIndex;

    return "chrome.tabs.create({url: \"" + std::string(kCreateBrowserURL) +
           "\", index: " + ss.str() +
           "}, function(tab) { window.testQuery({request:'" +
           kTabCallbackMessage + ":' + JSON.stringify(tab)}); });";
  }

  bool OnMessage(CefRefPtr<CefBrowser> browser,
                 const std::string& message) override {
    if (message.find(kTabCallbackMessage) != std::string::npos) {
      EXPECT_TRUE(browser->IsSame(extension_browser()));
      EXPECT_FALSE(got_tab_callback_message_);
      got_tab_callback_message_.yes();

      // Verify the contents of the Tab object.
      const std::string& json_str =
          message.substr(strlen(kTabCallbackMessage) + 1);
      CefRefPtr<CefValue> obj = CefParseJSON(json_str, JSON_PARSER_RFC);
      EXPECT_TRUE(obj);
      EXPECT_EQ(VTYPE_DICTIONARY, obj->GetType());
      CefRefPtr<CefDictionaryValue> dict = obj->GetDictionary();

      int index = dict->GetInt("index");
      EXPECT_EQ(kCreateTabIndex, index);

      int id = dict->GetInt("id");
      int windowId = dict->GetInt("windowId");
      EXPECT_EQ(created_browser_->GetIdentifier(), id);
      EXPECT_EQ(created_browser_->GetIdentifier(), windowId);

      const std::string& url = dict->GetString("url");
      EXPECT_STREQ(kCreateBrowserURL, url.c_str());

      TriggerDestroyTestIfReady();
      return true;
    } else if (message == kSuccessMessage) {
      // Overriding default kSuccessMessage handling.
      EXPECT_TRUE(browser->IsSame(created_browser_));
      EXPECT_FALSE(got_success_message());
      set_got_success_message();
      TriggerDestroyTestIfReady();
      return true;
    }

    return TabsTestHandler::OnMessage(browser, message);
  }

  void OnDestroyTest() override {
    created_browser_ = nullptr;

    EXPECT_TRUE(got_on_before_browser_);
    EXPECT_TRUE(got_create_browser_url_request_);
    EXPECT_TRUE(got_tab_callback_message_);

    TabsTestHandler::OnDestroyTest();
  }

 private:
  std::string GetCreatedBrowserHTML() {
    return "<html><body onLoad=" + GetMessageJS(kSuccessMessage) +
           ">Created</body></html>";
  }

  void TriggerDestroyTestIfReady() {
    if (got_tab_callback_message_ && got_success_message())
      TriggerDestroyTest();
  }

  CefRefPtr<CefBrowser> created_browser_;

  TrackCallback got_on_before_browser_;
  TrackCallback got_create_browser_url_request_;
  TrackCallback got_tab_callback_message_;

  IMPLEMENT_REFCOUNTING(CreateTestHandler);
  DISALLOW_COPY_AND_ASSIGN(CreateTestHandler);
};

}  // namespace

TABS_TEST_GROUP_ALL(Create, CreateTestHandler)

namespace {

//
// chrome.tabs.executeScript tests.
//

// Base class for chrome.tabs.executeScript tests.
class ExecuteScriptTestHandler : public TabsTestHandler {
 public:
  explicit ExecuteScriptTestHandler(RequestContextType request_context_type)
      : TabsTestHandler(request_context_type) {}

 protected:
  std::string GetMainBrowserSuccessHEAD() const override {
    return "<script>function onTrigger() {" + GetMessageJS(kSuccessMessage) +
           "}</script>";
  }

  // Returns the code that will be injected as a content script.
  virtual std::string GetContentScriptJS() const {
    // Execute the onTrigger() JS function.
    return "var s = document.createElement('script');"
           "s.textContent = 'onTrigger();';"
           "document.head.appendChild(s);";
  }

  std::string GetTabsApiJS() const override {
    return "chrome.tabs.executeScript(" + GetTargetTabId() + ", {code:\"" +
           GetContentScriptJS() + "\"});";
  }
};

// Test for chrome.tabs.executeScript with a null tabId value.
class ExecuteScriptNullTabTestHandler : public ExecuteScriptTestHandler {
 public:
  explicit ExecuteScriptNullTabTestHandler(
      RequestContextType request_context_type)
      : ExecuteScriptTestHandler(request_context_type) {}

 private:
  IMPLEMENT_REFCOUNTING(ExecuteScriptNullTabTestHandler);
  DISALLOW_COPY_AND_ASSIGN(ExecuteScriptNullTabTestHandler);
};

}  // namespace

TABS_TEST_GROUP_ALL(ExecuteScriptNullTab, ExecuteScriptNullTabTestHandler)

namespace {

// Test for chrome.tabs.executeScript with an explicit tabId value.
class ExecuteScriptExplicitTabTestHandler : public ExecuteScriptTestHandler {
 public:
  explicit ExecuteScriptExplicitTabTestHandler(
      RequestContextType request_context_type)
      : ExecuteScriptTestHandler(request_context_type) {
    // Create the main browser first so we can retrieve the id.
    set_create_main_browser_first(true);
    // When a tabId is specified we should get a call to CanAccessBrowser
    // instead of GetActiveBrowser.
    set_expect_get_active_browser(false);
  }

 protected:
  std::string GetTargetTabId() const override {
    DCHECK(main_browser());
    std::stringstream ss;
    ss << main_browser()->GetIdentifier();
    return ss.str();
  }

 private:
  IMPLEMENT_REFCOUNTING(ExecuteScriptExplicitTabTestHandler);
  DISALLOW_COPY_AND_ASSIGN(ExecuteScriptExplicitTabTestHandler);
};

}  // namespace

TABS_TEST_GROUP_ALL(ExecuteScriptExplicitTab,
                    ExecuteScriptExplicitTabTestHandler)

namespace {

// Test for chrome.tabs.executeScript with a file argument loading a content
// script.
class ExecuteScriptFileTestHandler : public ExecuteScriptTestHandler {
 public:
  explicit ExecuteScriptFileTestHandler(RequestContextType request_context_type)
      : ExecuteScriptTestHandler(request_context_type) {}

  // CefExtensionHandler methods:
  bool GetExtensionResource(
      CefRefPtr<CefExtension> extension,
      CefRefPtr<CefBrowser> browser,
      const CefString& file,
      CefRefPtr<CefGetExtensionResourceCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_TRUE(this->extension());
    EXPECT_TRUE(this->extension()->IsSame(extension));

    if (file == "script.js") {
      EXPECT_FALSE(got_get_extension_resource_);
      got_get_extension_resource_.yes();

      const std::string& content = GetContentScriptJS();
      CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
          const_cast<char*>(content.data()), content.size());
      callback->Continue(stream);
      return true;
    }

    EXPECT_FALSE(true);  // Not reached.
    return false;
  }

 protected:
  std::string GetTabsApiJS() const override {
    return "chrome.tabs.executeScript(" + GetTargetTabId() +
           ", {file:\"script.js\"});";
  }

  void OnDestroyTest() override {
    ExecuteScriptTestHandler::OnDestroyTest();
    EXPECT_TRUE(got_get_extension_resource_);
  }

 private:
  TrackCallback got_get_extension_resource_;

  IMPLEMENT_REFCOUNTING(ExecuteScriptFileTestHandler);
  DISALLOW_COPY_AND_ASSIGN(ExecuteScriptFileTestHandler);
};

}  // namespace

TABS_TEST_GROUP_ALL(ExecuteScriptFile, ExecuteScriptFileTestHandler)

namespace {

// Test for chrome.tabs.executeScript with a callback argument.
class ExecuteScriptCallbackTestHandler : public ExecuteScriptTestHandler {
 public:
  explicit ExecuteScriptCallbackTestHandler(
      RequestContextType request_context_type)
      : ExecuteScriptTestHandler(request_context_type) {}

 protected:
  bool OnMessage(CefRefPtr<CefBrowser> browser,
                 const std::string& message) override {
    if (message == "callback") {
      EXPECT_FALSE(got_callback_message_);
      got_callback_message_.yes();
      EXPECT_TRUE(browser->IsSame(extension_browser()));
      TriggerDestroyTest();
      return true;
    }
    return ExecuteScriptTestHandler::OnMessage(browser, message);
  }

  std::string GetTabsApiJS() const override {
    return "chrome.tabs.executeScript(" + GetTargetTabId() + ", {code:\"" +
           GetContentScriptJS() + "\"}, function(results) {" +
           GetMessageJS("callback") + "});";
  }

  void TriggerDestroyTest() override {
    // Only destroy the test if we got both callbacks.
    if (got_callback_message_ && got_success_message()) {
      ExecuteScriptTestHandler::TriggerDestroyTest();
    }
  }

  void OnDestroyTest() override {
    ExecuteScriptTestHandler::OnDestroyTest();
    EXPECT_TRUE(got_callback_message_);
  }

 private:
  TrackCallback got_callback_message_;

  IMPLEMENT_REFCOUNTING(ExecuteScriptCallbackTestHandler);
  DISALLOW_COPY_AND_ASSIGN(ExecuteScriptCallbackTestHandler);
};

}  // namespace

TABS_TEST_GROUP_MINIMAL(ExecuteScriptCallback, ExecuteScriptCallbackTestHandler)

namespace {

// Test for chrome.tabs.executeScript with execution occuring from a separate
// resource script.
class ExecuteScriptResourceTabTestHandler : public ExecuteScriptTestHandler {
 public:
  explicit ExecuteScriptResourceTabTestHandler(
      RequestContextType request_context_type)
      : ExecuteScriptTestHandler(request_context_type) {}

  // CefResourceRequestHandler methods:
  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    const std::string& url = request->GetURL();
    if (url == resource_url_) {
      EXPECT_TRUE(browser->IsSame(extension_browser()));
      EXPECT_FALSE(got_resource_url_request_);
      got_resource_url_request_.yes();
    }

    return ExecuteScriptTestHandler::GetResourceHandler(browser, frame,
                                                        request);
  }

 protected:
  void OnAddExtensionResources(const std::string& origin) override {
    ExecuteScriptTestHandler::OnAddExtensionResources(origin);
    resource_url_ = origin + "resource.js";
    AddResource(resource_url_, GetExtensionJS(), "text/javascript");
  }

  std::string GetExtensionHTML() const override {
    return "<html><head><script src=\"resource.js\"></script></head><body "
           "onLoad=" +
           GetMessageJS("extension_onload") + ">Extension</body></html>";
  }

  void OnDestroyTest() override {
    ExecuteScriptTestHandler::OnDestroyTest();
    EXPECT_TRUE(got_resource_url_request_);
  }

 private:
  std::string resource_url_;
  TrackCallback got_resource_url_request_;

  IMPLEMENT_REFCOUNTING(ExecuteScriptResourceTabTestHandler);
  DISALLOW_COPY_AND_ASSIGN(ExecuteScriptResourceTabTestHandler);
};

}  // namespace

TABS_TEST_GROUP_MINIMAL(ExecuteScriptResource,
                        ExecuteScriptResourceTabTestHandler)

//
// chrome.tabs.insertCSS tests.
//

namespace {

// Base class for chrome.tabs.insertCSS tests.
class InsertCSSTestHandler : public TabsTestHandler {
 public:
  explicit InsertCSSTestHandler(RequestContextType request_context_type)
      : TabsTestHandler(request_context_type) {}

 protected:
  std::string GetMainBrowserSuccessBODY() const override {
    // We can't use a MutationObserver here because insertCSS does not modify
    // the style attribute. We could detect the change by tracking modifications
    // to document.styleSheets but that's complicated. Use a simple timer loop
    // implementation calling getComputedStyle instead.
    return "<script>var interval = setInterval(function() {"
           "if (window.getComputedStyle(document.body,null)."
           "getPropertyValue('background-color') == 'rgb(255, 0, 0)') {" +
           GetMessageJS(kSuccessMessage) +
           "clearInterval(interval);}}, 100);</script>";
  }

  // Returns the code that will be injected as a content script.
  virtual std::string GetContentScriptCSS() const {
    return "body{background-color:red}";
  }

  std::string GetTabsApiJS() const override {
    return "chrome.tabs.insertCSS(" + GetTargetTabId() + ", {code:\"" +
           GetContentScriptCSS() + "\"});";
  }
};

// Test for chrome.tabs.insertCSS with a null tabId value.
class InsertCSSNullTabTestHandler : public InsertCSSTestHandler {
 public:
  explicit InsertCSSNullTabTestHandler(RequestContextType request_context_type)
      : InsertCSSTestHandler(request_context_type) {}

 private:
  IMPLEMENT_REFCOUNTING(InsertCSSNullTabTestHandler);
  DISALLOW_COPY_AND_ASSIGN(InsertCSSNullTabTestHandler);
};

}  // namespace

TABS_TEST_GROUP_ALL(InsertCSSNullTab, InsertCSSNullTabTestHandler)

namespace {

// Test for chrome.tabs.insertCSS with an explicit tabId value.
class InsertCSSExplicitTabTestHandler : public InsertCSSTestHandler {
 public:
  explicit InsertCSSExplicitTabTestHandler(
      RequestContextType request_context_type)
      : InsertCSSTestHandler(request_context_type) {
    // Create the main browser first so we can retrieve the id.
    set_create_main_browser_first(true);
    // When a tabId is specified we should get a call to CanAccessBrowser
    // instead of GetActiveBrowser.
    set_expect_get_active_browser(false);
  }

 protected:
  std::string GetTargetTabId() const override {
    DCHECK(main_browser());
    std::stringstream ss;
    ss << main_browser()->GetIdentifier();
    return ss.str();
  }

 private:
  IMPLEMENT_REFCOUNTING(InsertCSSExplicitTabTestHandler);
  DISALLOW_COPY_AND_ASSIGN(InsertCSSExplicitTabTestHandler);
};

}  // namespace

TABS_TEST_GROUP_ALL(InsertCSSExplicitTab, InsertCSSExplicitTabTestHandler)

namespace {

// Test for chrome.tabs.insertCSS with a file argument loading a content
// script.
class InsertCSSFileTestHandler : public InsertCSSTestHandler {
 public:
  explicit InsertCSSFileTestHandler(RequestContextType request_context_type)
      : InsertCSSTestHandler(request_context_type) {}

  // CefExtensionHandler methods:
  bool GetExtensionResource(
      CefRefPtr<CefExtension> extension,
      CefRefPtr<CefBrowser> browser,
      const CefString& file,
      CefRefPtr<CefGetExtensionResourceCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_TRUE(this->extension());
    EXPECT_TRUE(this->extension()->IsSame(extension));

    if (file == "script.css") {
      EXPECT_FALSE(got_get_extension_resource_);
      got_get_extension_resource_.yes();

      const std::string& content = GetContentScriptCSS();
      CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
          const_cast<char*>(content.data()), content.size());
      callback->Continue(stream);
      return true;
    }

    EXPECT_FALSE(true);  // Not reached.
    return false;
  }

 protected:
  std::string GetTabsApiJS() const override {
    return "chrome.tabs.insertCSS(" + GetTargetTabId() +
           ", {file:\"script.css\"});";
  }

  void OnDestroyTest() override {
    InsertCSSTestHandler::OnDestroyTest();
    EXPECT_TRUE(got_get_extension_resource_);
  }

 private:
  TrackCallback got_get_extension_resource_;

  IMPLEMENT_REFCOUNTING(InsertCSSFileTestHandler);
  DISALLOW_COPY_AND_ASSIGN(InsertCSSFileTestHandler);
};

}  // namespace

TABS_TEST_GROUP_ALL(InsertCSSFile, InsertCSSFileTestHandler)

namespace {

// Test for chrome.tabs.insertCSS with a callback argument.
class InsertCSSCallbackTestHandler : public InsertCSSTestHandler {
 public:
  explicit InsertCSSCallbackTestHandler(RequestContextType request_context_type)
      : InsertCSSTestHandler(request_context_type) {}

 protected:
  bool OnMessage(CefRefPtr<CefBrowser> browser,
                 const std::string& message) override {
    if (message == "callback") {
      EXPECT_FALSE(got_callback_message_);
      got_callback_message_.yes();
      EXPECT_TRUE(browser->IsSame(extension_browser()));
      TriggerDestroyTest();
      return true;
    }
    return InsertCSSTestHandler::OnMessage(browser, message);
  }

  std::string GetTabsApiJS() const override {
    return "chrome.tabs.insertCSS(" + GetTargetTabId() + ", {code:\"" +
           GetContentScriptCSS() + "\"}, function(results) {" +
           GetMessageJS("callback") + "});";
  }

  void TriggerDestroyTest() override {
    // Only destroy the test if we got both callbacks.
    if (got_callback_message_ && got_success_message()) {
      InsertCSSTestHandler::TriggerDestroyTest();
    }
  }

  void OnDestroyTest() override {
    InsertCSSTestHandler::OnDestroyTest();
    EXPECT_TRUE(got_callback_message_);
  }

 private:
  TrackCallback got_callback_message_;

  IMPLEMENT_REFCOUNTING(InsertCSSCallbackTestHandler);
  DISALLOW_COPY_AND_ASSIGN(InsertCSSCallbackTestHandler);
};

}  // namespace

TABS_TEST_GROUP_MINIMAL(InsertCSSCallback, InsertCSSCallbackTestHandler)

namespace {

// Test for chrome.tabs.insertCSS with execution occuring from a separate
// resource script.
class InsertCSSResourceTabTestHandler : public InsertCSSTestHandler {
 public:
  explicit InsertCSSResourceTabTestHandler(
      RequestContextType request_context_type)
      : InsertCSSTestHandler(request_context_type) {}

  // CefResourceRequestHandler methods:
  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    const std::string& url = request->GetURL();
    if (url == resource_url_) {
      EXPECT_TRUE(browser->IsSame(extension_browser()));
      EXPECT_FALSE(got_resource_url_request_);
      got_resource_url_request_.yes();
    }

    return InsertCSSTestHandler::GetResourceHandler(browser, frame, request);
  }

 protected:
  void OnAddExtensionResources(const std::string& origin) override {
    InsertCSSTestHandler::OnAddExtensionResources(origin);
    resource_url_ = origin + "resource.js";
    AddResource(resource_url_, GetExtensionJS(), "text/javascript");
  }

  std::string GetExtensionHTML() const override {
    return "<html><head><script src=\"resource.js\"></script></head><body "
           "onLoad=" +
           GetMessageJS("extension_onload") + ">Extension</body></html>";
  }

  void OnDestroyTest() override {
    InsertCSSTestHandler::OnDestroyTest();
    EXPECT_TRUE(got_resource_url_request_);
  }

 private:
  std::string resource_url_;
  TrackCallback got_resource_url_request_;

  IMPLEMENT_REFCOUNTING(InsertCSSResourceTabTestHandler);
  DISALLOW_COPY_AND_ASSIGN(InsertCSSResourceTabTestHandler);
};

}  // namespace

TABS_TEST_GROUP_MINIMAL(InsertCSSResource, InsertCSSResourceTabTestHandler)

//
// chrome.tabs.setZoom/getZoom tests.
//

namespace {

// Base class for chrome.tabs.setZoom/getZoom tests.
class ZoomTestHandler : public TabsTestHandler {
 public:
  explicit ZoomTestHandler(RequestContextType request_context_type)
      : TabsTestHandler(request_context_type) {
    // We call API functions three times in this handler.
    set_expected_api_call_count(3);
  }

 protected:
  std::string GetMainBrowserSuccessHEAD() const override {
    return "<script>var orig_width = window.innerWidth;</script>";
  }

  std::string GetMainBrowserSuccessBODY() const override {
    // We can't directly detect zoom changes, so instead we look for changes
    // in window.innerWidth.
    return "<script>var interval = setInterval(function() {"
           "if (window.innerWidth != orig_width) {" +
           GetMessageJS(kSuccessMessage) +
           "clearInterval(interval);}}, 100);</script>";
  }

  std::string GetTabsApiJS() const override {
    // Results in a change to window.innerWidth.
    return "chrome.tabs.setZoom(" + GetTargetTabId() + ", 2.0);";
  }

  bool OnMessage(CefRefPtr<CefBrowser> browser,
                 const std::string& message) override {
    if (message == "restored") {
      EXPECT_TRUE(browser->IsSame(extension_browser()));
      EXPECT_FALSE(got_restored_message_);
      got_restored_message_.yes();
      // Destroy the test for real.
      TabsTestHandler::TriggerDestroyTest();
      return true;
    }
    return TabsTestHandler::OnMessage(browser, message);
  }

  void TriggerDestroyTest() override {
    // Before destroying the test we need to restore the zoom factor so that
    // it doesn't persist in the RequestContext. This also tests the callback
    // argument and the getZoom function so there's no need to do that
    // separately.
    extension_browser()->GetMainFrame()->ExecuteJavaScript(
        "chrome.tabs.setZoom(" + GetTargetTabId() + ", 1.0, function() {" +
            "chrome.tabs.getZoom(" + GetTargetTabId() +
            ", function(zoomFactor) { if (zoomFactor == 1.0) {" +
            GetMessageJS("restored") + "}})});",
        extension_url(), 0);
  }

  void OnDestroyTest() override {
    TabsTestHandler::OnDestroyTest();
    EXPECT_TRUE(got_restored_message_);
  }

 private:
  TrackCallback got_restored_message_;
};

// Test for chrome.tabs.setZoom/getZoom with a null tabId value.
class ZoomNullTabTestHandler : public ZoomTestHandler {
 public:
  explicit ZoomNullTabTestHandler(RequestContextType request_context_type)
      : ZoomTestHandler(request_context_type) {}

 private:
  IMPLEMENT_REFCOUNTING(ZoomNullTabTestHandler);
  DISALLOW_COPY_AND_ASSIGN(ZoomNullTabTestHandler);
};

}  // namespace

TABS_TEST_GROUP_ALL(ZoomNullTab, ZoomNullTabTestHandler)

namespace {

// Test for chrome.tabs.setZoom/getZoom with an explicit tabId value.
class ZoomExplicitTabTestHandler : public ZoomTestHandler {
 public:
  explicit ZoomExplicitTabTestHandler(RequestContextType request_context_type)
      : ZoomTestHandler(request_context_type) {
    // Create the main browser first so we can retrieve the id.
    set_create_main_browser_first(true);
    // When a tabId is specified we should get a call to CanAccessBrowser
    // instead of GetActiveBrowser.
    set_expect_get_active_browser(false);
  }

 protected:
  std::string GetTargetTabId() const override {
    DCHECK(main_browser());
    std::stringstream ss;
    ss << main_browser()->GetIdentifier();
    return ss.str();
  }

 private:
  IMPLEMENT_REFCOUNTING(ZoomExplicitTabTestHandler);
  DISALLOW_COPY_AND_ASSIGN(ZoomExplicitTabTestHandler);
};

}  // namespace

TABS_TEST_GROUP_ALL(ZoomExplicitTab, ZoomExplicitTabTestHandler)

//
// chrome.tabs.setZoomSettings/getZoomSettings tests.
//

namespace {

// Base class for chrome.tabs.setZoomSettings/getZoomSettings tests.
class ZoomSettingsTestHandler : public TabsTestHandler {
 public:
  explicit ZoomSettingsTestHandler(RequestContextType request_context_type)
      : TabsTestHandler(request_context_type) {
    // We call API functions two times in this handler.
    set_expected_api_call_count(2);
    // Success message will be delivered in the extension browser because we
    // don't know how to detect zoom settings changes in the main browser.
    set_expect_success_in_main_browser(false);
  }

 protected:
  std::string GetTabsApiJS() const override {
    // Set and restore the zoom settings. This also tests the callback argument
    // and the getZoomSettings function so there's no need to do that
    // separately. This is safe because zoom settings are not persisted in the
    // RequestContext across navigations.
    return "chrome.tabs.setZoomSettings(" + GetTargetTabId() +
           ", {mode: 'manual', scope: 'per-tab'}, function() {" +
           "chrome.tabs.getZoomSettings(" + GetTargetTabId() +
           ", function(zoomSettings) { if (zoomSettings.mode == 'manual' "
           "&& zoomSettings.scope == 'per-tab') {" +
           GetMessageJS(kSuccessMessage) + "}})});";
  }
};

// Test for chrome.tabs.setZoomSettings/getZoomSettings with a null tabId value.
class ZoomSettingsNullTabTestHandler : public ZoomSettingsTestHandler {
 public:
  explicit ZoomSettingsNullTabTestHandler(
      RequestContextType request_context_type)
      : ZoomSettingsTestHandler(request_context_type) {}

 private:
  IMPLEMENT_REFCOUNTING(ZoomSettingsNullTabTestHandler);
  DISALLOW_COPY_AND_ASSIGN(ZoomSettingsNullTabTestHandler);
};

}  // namespace

TABS_TEST_GROUP_ALL(ZoomSettingsNullTab, ZoomSettingsNullTabTestHandler)

namespace {

// Test for chrome.tabs.setZoomSettings/getZoomSettings with an explicit tabId
// value.
class ZoomSettingsExplicitTabTestHandler : public ZoomSettingsTestHandler {
 public:
  explicit ZoomSettingsExplicitTabTestHandler(
      RequestContextType request_context_type)
      : ZoomSettingsTestHandler(request_context_type) {
    // Create the main browser first so we can retrieve the id.
    set_create_main_browser_first(true);
    // When a tabId is specified we should get a call to CanAccessBrowser
    // instead of GetActiveBrowser.
    set_expect_get_active_browser(false);
  }

 protected:
  std::string GetTargetTabId() const override {
    DCHECK(main_browser());
    std::stringstream ss;
    ss << main_browser()->GetIdentifier();
    return ss.str();
  }

 private:
  IMPLEMENT_REFCOUNTING(ZoomSettingsExplicitTabTestHandler);
  DISALLOW_COPY_AND_ASSIGN(ZoomSettingsExplicitTabTestHandler);
};

}  // namespace

TABS_TEST_GROUP_ALL(ZoomSettingsExplicitTab, ZoomSettingsExplicitTabTestHandler)
